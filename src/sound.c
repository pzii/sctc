/*
	SCTC - the soundcloud.com client
	Copyright (C) 2015   Christian Eichler

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>
*/


#include "_hard_config.h"
#include "sound.h"

#include <ao/ao.h>                      // for ao_sample_format, ao_close, etc
#include <mpg123.h>                     // for mpg123_close, mpg123_delete, etc

//\cond
#include <errno.h>                      // for errno
#include <pthread.h>                    // for pthread_create, etc
#include <semaphore.h>                  // for sem_post, sem_wait, etc
#include <stddef.h>                     // for NULL, size_t
#include <stdlib.h>                     // for free
#include <string.h>                     // for strerror, strdup
#include <sys/stat.h>                   // for off_t
//\endcond

#include "cache.h"                      // for cache_track_get, etc
#include "config.h"
#include "downloader.h"
#include "helper.h"                     // for lmalloc
#include "http.h"                       // for http_response, etc
#include "log.h"                        // for _log
#include "network/network.h"            // for network_conn
#include "soundcloud.h"                 // for soundcloud_connect_track
#include "track.h"                      // for track, FLAG_CACHED
#include "tui.h"                        // for F_BOLD, F_RESET, etc
#include "state.h"

#define BUFFER_SIZE 256 * 1024 * 1024

#define SEEKPOS_NONE ((unsigned int) ~0)

struct io_handle {
	size_t                 position;   //< the current position
	struct download_state *download_state;     //< contains the maximum (currently possible) position
};

static sem_t sem_stopped;
static pthread_t thread_play; // thread decoding and playing downloaded data

static sem_t sem_data_available;

static mpg123_handle *mh = NULL;
struct io_handle *m_iohandle = NULL;

static volatile struct track *track       = NULL;
static volatile unsigned int  current_pos = 0;

static volatile unsigned int  seek_to_pos = SEEKPOS_NONE;

/** The amount of data for the whole track.
    As soon as we reach this position during playback the end of track has been reached */
static volatile bool          stopped     = false;
static volatile bool          terminate   = false;

static struct download_state *state = NULL;

static void (*time_callback)(int);

static void sound_finalize();

static char* ao_strerror(int err) {
	switch(err) {
		case AO_ENODRIVER:   return "no driver with given id exists";
		case AO_ENOTLIVE:    return "not live";
		case AO_EBADOPTION:  return "bad option";
		case AO_EOPENDEVICE: return "failed to open device";
		case AO_EFAIL:       return "error unknown";
		default:             return "<unknown ao error>";
	}
}

static ssize_t _io_read(void *_iohandle, void *mpg123buffer, size_t count) {
	struct io_handle *iohandle    = (struct io_handle*) _iohandle;
	struct download_state *dlstat = iohandle->download_state;

	size_t bytes_available = 0;
	if(dlstat->bytes_recvd > iohandle->position) {
		bytes_available = dlstat->bytes_recvd - iohandle->position;
	}
	size_t bytes_copied = count < bytes_available ? count : bytes_available;

	memcpy(mpg123buffer, &dlstat->buffer[iohandle->position], bytes_copied);
	iohandle->position += bytes_copied;

	return bytes_copied;
}

static off_t _io_seek(void *_iohandle, off_t offset, int whence) {
	struct io_handle *iohandle    = (struct io_handle*) _iohandle;
	struct download_state *dlstat = iohandle->download_state;

	size_t abs_offset = 0;
	switch(whence) {
		case SEEK_SET: abs_offset = offset;                       break;
		case SEEK_CUR: abs_offset = iohandle->position  + offset; break;
		case SEEK_END: abs_offset = dlstat->bytes_total + offset; break;
		default: {
			_log("invalid value for whence: %i", whence);
			return (off_t) -1;
		}
	}

	if(abs_offset >= dlstat->bytes_recvd) {
		return (off_t) -1;
	}

	iohandle->position = abs_offset;
	return abs_offset;
}

static void _io_cleanup(void *iohandle) {
	free(iohandle);
	_log("cleanup called");
}

/** \brief Reinitialize libmpg123
 *
 *  \param mh  The old handle to close (may be NULL)
 *  \return    A pointer to the new handle (or NULL in case of failure)
 */
static mpg123_handle* mpg123_init_playback(mpg123_handle *mh, struct download_state *download_state) {
	// close the old mh if available
	if(mh) {
		mpg123_close(mh);
		mpg123_delete(mh);
	}

	mpg123_handle *new_mh = mpg123_new(NULL, NULL);
	if(!new_mh) {
		_log("mpg123_new failed");
		return NULL;
	}

	if(MPG123_OK != mpg123_replace_reader_handle(new_mh, _io_read, _io_seek, _io_cleanup)) {
		_log("mpg123_replace_reader_handle: %s", mpg123_strerror(new_mh));
		return NULL;
	}

	struct io_handle *iohandle = lmalloc( sizeof(struct io_handle) );
	iohandle->position       = 0;
	iohandle->download_state = download_state;

	m_iohandle = iohandle;

	if(MPG123_OK != mpg123_open_handle(new_mh, iohandle)) {
		_log("mpg123_open_handle: %s", mpg123_strerror(new_mh));
		free(iohandle);
		return NULL;
	}

	if(MPG123_OK != mpg123_param(new_mh, MPG123_FLAGS, MPG123_QUIET, 0.0)) {
		_log("mpg123_param: %s", mpg123_strerror(new_mh));
		free(iohandle);
		return NULL;
	}

	// set the new values for the equalizer obtained from configuration
	for(int i = 0; i < 32; i++) {
		mpg123_eq(new_mh, MPG123_LR, i, config_get_equalizer(i));
	}

	return new_mh;
}

static void io_callback(struct download_state *state) {
	sem_post(&sem_data_available);

	if(state->finished) {
		_log("download of `%s` is finished, saving track to cache", track->name);
		if(cache_track_save((struct track*) track, (void*) state->buffer, state->bytes_recvd)) {
			track->flags |= FLAG_CACHED;
		}

		//free(state->buffer);
		//free(state);
		//state = NULL;
	}
}

/** \brief main function for playback thread.
*
*  \param unused  Unused parameter (never read), required due to pthread interface
*  \return NULL   Unused return value, required due to pthread interface
*/
static void* _thread_play_function(void *unused) {
	ao_device *dev = NULL;

	/** \todo 'quiet' is not quiet at all... */
	ao_option *ao_opt = NULL;
	ao_append_option(&ao_opt, "quiet", NULL);
	ao_append_global_option("quiet", "true");

	double time_per_frame = 0;

	do {
		_log("waiting for data to play");
		sem_wait(&sem_data_available);
		if(!terminate) {
			_log("starting playback");
			mh = mpg123_init_playback(mh, state);
			_log("mpg123_init_playback returned %p", (void*)mh);
		}

		size_t done;

		unsigned int last_reported_pos = ~0;

		bool playback_done = false;

		off_t frame_offset;
		unsigned char *audio = NULL;

		while(!terminate && !stopped && !playback_done && track) {
			int err = mpg123_decode_frame(mh, &frame_offset, &audio, &done);
			switch(err) {
				case MPG123_NEW_FORMAT: {
					ao_sample_format format;
					int channels, encoding;
					long rate;

					mpg123_getformat(mh, &rate, &channels, &encoding);
					format.bits = mpg123_encsize(encoding) * 8; // 8 bit per Byte
					format.rate = rate;
					format.channels = channels;
					format.byte_format = AO_FMT_NATIVE;
					format.matrix = 0;

					time_per_frame = mpg123_tpf(mh);

					ao_info *info = ao_driver_info(ao_default_driver_id());
					_log("libao: opening default output '%s' with: rate: %i, %i channels, %i bits per sample | %fs per frame", info->name, format.rate, format.channels, format.bits, time_per_frame);
					if(dev) {
						ao_close(dev);
					}
					dev = ao_open_live(ao_default_driver_id(), &format, ao_opt);
					if(!dev) {
						_log("ao_open_live: %s", ao_strerror(errno));
					}
					break;
				}

				case MPG123_OK:
					ao_play(dev, (char*)audio, done);

					current_pos = (unsigned int) (time_per_frame * mpg123_tellframe(mh));
					// only report position of playback if it has changed
					// meant to reduce the number of redraws possibly issued by time_callback
					if(current_pos != last_reported_pos) {
						last_reported_pos = current_pos;
						time_callback(current_pos);
					}
					break;

				default:
					break;
			}

			// do seeking to specified position if required
			if(SEEKPOS_NONE != seek_to_pos) {
				_log("requested seek to %i", seek_to_pos);
				off_t target_frame_off = mpg123_timeframe(mh, seek_to_pos);

				off_t *offsets;
				size_t offsets_size;
				off_t  step_per_idx;
				if(MPG123_OK == mpg123_index(mh, &offsets, &step_per_idx, &offsets_size)) {
					size_t idx = target_frame_off / step_per_idx;
					if(idx < offsets_size) {
						_log("going to bytepos %zu", offsets[idx]);
					}
				}

				// reset seek_to_pos to avoid seeking multiple times
				seek_to_pos = SEEKPOS_NONE;
			}
		}

		if(stopped) {
			sem_post(&sem_stopped);
		}
	} while(!terminate);
	ao_free_options(ao_opt);
	if(dev) {
		ao_close(dev);
	}

	return NULL;
}

bool sound_init(void (*_time_callback)(int)) {
	time_callback = _time_callback;

	_log("initializing libao...");
	ao_initialize();

	mpg123_init();

	if(sem_init(&sem_data_available, 0, 0)) {
		_log("sem_init failed: %s", strerror(errno));
		return false;
	}

	sem_init(&sem_stopped, 0, 0);

	pthread_create(&thread_play, NULL, _thread_play_function, NULL);

	if(atexit(sound_finalize)) {
		_log("atexit: %s", strerror(errno));
	}

	return true;
}

static void sound_finalize() {
	terminate = true;

	_log("waiting for threads to terminate...");

	_log("thread_play...");
	sem_post(&sem_data_available);
	pthread_join(thread_play, NULL);

	// cleanup libmpg123
	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();

	// cleanup libao
	ao_shutdown();
}

#define SEM_SET_TO_ZERO(S) {int sval; while(!sem_getvalue(&S, &sval) && sval) {sem_wait(&S);}}

bool sound_stop() {
	stopped = true;

	_log("waiting for threads to stop playback");

	// if stop() is called by the thread doing the playback,
	// for instance caused by the `time_callback`, then we may not block
	if(pthread_self() != thread_play) {
		sem_post(&sem_data_available);

		sem_wait(&sem_stopped);
	}

	track   = NULL;
	stopped = false;

	SEM_SET_TO_ZERO(sem_data_available)

	_log("stopping done");

	return true;
}

unsigned int sound_get_current_pos() {
	return current_pos;
}

void sound_seek(unsigned int pos) {
	seek_to_pos = pos;
}

bool sound_play(struct track *_track) {

	if(track) {
		sound_stop();
	}

	track       = _track;
	current_pos = 0;

	seek_to_pos = _track->current_position;
/*
	size_t cache_track_size;
	if( ( cache_track_size = cache_track_get((struct track*) track, (void*) buffer) ) ) {
		_log("using file from cache for '%s' by '%s'", track->name, track->username);
		track_size = cache_track_size;

		sem_post(&sem_data_available);
	} else {
		state = downloader_queue_buffer(track, io_callback);
	}
*/
	state = downloader_queue_buffer(track, io_callback);
	return true;
}

