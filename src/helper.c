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

/** \file helper.c
 *  \brief Implements several basic helper functions (mixed purpose)
 */
#include "helper.h"

//\cond
#include <assert.h>                     // for assert
#include <errno.h>                      // for errno
#include <fcntl.h>                      // for open, O_NOFOLLOW
#include <polarssl/sha512.h>            // for sha512
#include <stdarg.h>                     // for va_end, va_list, va_start
#include <stdbool.h>                    // for true, false, bool
#include <stdio.h>                      // for snprintf, sscanf, vsnprintf, etc
#include <stdlib.h>                     // for exit, EXIT_FAILURE, calloc, etc
#include <string.h>                     // for strerror, strlen, strdup, etc
#include <sys/mman.h>                   // for mmap, munmap, MAP_FAILED, etc
#include <sys/stat.h>                   // for stat, fstat
#include <unistd.h>                     // for close, execlp, fork, dup2, etc
#include <regex.h>
//\endcond

#include "log.h"                        // for __log, _err, _log
#include "tui.h"                        // for F_RESET, F_UNDERLINE

char* smprintf(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int required_size = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	// check for output error
	if(0 > required_size) {
		return NULL;
	}

	size_t buffer_size = ((unsigned int) required_size) + 1;

	va_list aq;
	va_start(aq, fmt);
	char *buffer = lcalloc(sizeof(char), buffer_size);
	vsnprintf(buffer, buffer_size, fmt, aq);
	va_end(aq);

	return buffer;
}

void strcrep(char *str, char s, char r) {
	while(*str) {
		if(*str == s) {
			*str = r;
		}
		str++;
	}
}

int snprint_ftime(char *buffer, size_t buffer_size, int time_secs) {
	int secs  = time_secs % 60;
	int mins  = time_secs / 60;
	int hours = mins / 60;
	mins -= hours * 60;

	if(hours) {
		return snprintf(buffer, buffer_size, "%d:%02d:%02d", hours, mins, secs);
	} else {
		return snprintf(buffer, buffer_size, "%02d:%02d", mins, secs);
	}
}

char* strstrp(char *str) {
	char *new_start = str;
	while(' ' == *new_start || '\t' == *new_start) {
		new_start++;
	}

	for(char *ptr = new_start + strlen(new_start) - 1; ptr > new_start && (' ' == *ptr || '\t' == *ptr); ptr--) {
		*ptr = '\0';
	}

	return new_start;
}

bool yank(char *text) {
	int fd[2];
	pipe(fd);

	pid_t pid = fork();
	if(-1 == pid) {
		return false;
	} else if(!pid) {
		close(fd[1]);              // close fd_out
		dup2(fd[0], STDIN_FILENO); // replace stdin with fd_in

		execlp("xsel", "xsel", "-i", (char*)NULL);

		exit(EXIT_FAILURE);
	} else {
		close(fd[0]);
		write(fd[1], text, strlen(text) + 1);
		close(fd[1]);

		// TODO: wait for child process & it's result
	}
	return true;
}

bool fork_and_run(char *cmd, char *param) {
	pid_t pid = fork();
	if(-1 == pid) {
		return false;
	} else if(!pid) {
		execlp(cmd, cmd, param, (char*)NULL);
		exit(EXIT_FAILURE);
	}
	return true;
}

#ifndef NDEBUG
static unsigned int _lmalloc_count  = 0;
static unsigned int _lcalloc_count  = 0;
static unsigned int _lrealloc_count = 0;
static unsigned int _lstrdup_count  = 0;
#endif

void* _lmalloc(char *srcfile, int srcline, const char *srcfunc, size_t size) {
	void *ptr = malloc(size);
	ONLY_DEBUG( __sync_fetch_and_add(&_lmalloc_count, 1); )
	if(!ptr) {
		__log(srcfile, srcline, srcfunc, true, "malloc(%zu) failed: %s", size, strerror(errno));
	}

	return ptr;
}

void* _lcalloc(char *srcfile, int srcline, const char *srcfunc, size_t nmemb, size_t size) {
	void *ptr = calloc(nmemb, size);
	ONLY_DEBUG( __sync_fetch_and_add(&_lcalloc_count, 1); )
	if(!ptr) {
		__log(srcfile, srcline, srcfunc, true, "calloc(%zu, %zu) failed: %s", nmemb, size, strerror(errno));
	}

	return ptr;
}

void* _lrealloc(char *srcfile, int srcline, const char *srcfunc, void *ptr, size_t size) {
	void *new_ptr = realloc(ptr, size);
	ONLY_DEBUG( __sync_fetch_and_add(&_lrealloc_count, 1); )
	if(!new_ptr) {
		__log(srcfile, srcline, srcfunc, true, "realloc(%p, %zu) failed: %s", ptr, size, strerror(errno));
	}

	return new_ptr;
}

char *_lstrdup(char *srcfile, int srcline, const char *srcfunc, const char *s) {
	void *d = strdup(s);
	ONLY_DEBUG( __sync_fetch_and_add(&_lstrdup_count, 1); )
	if(!d) {
		__log(srcfile, srcline, srcfunc, true, "strdup(\"%s\") failed: %s", s, strerror(errno));
	}

	return d;
}

#ifndef NDEBUG
void dump_alloc_counter(void) {
	_log("#{m,c,re}alloc/strdup: %u %u %u %u", _lmalloc_count, _lcalloc_count, _lrealloc_count, _lstrdup_count);
}
#endif

unsigned int parse_time_to_sec(char *str) {
	unsigned int hour, min, sec;

	if(3 == sscanf(str, " %16u : %2u : %2u ", &hour, &min, &sec)) {
		if(sec >= 60 || min >= 60) {
			return INVALID_TIME;
		}

		return 60 * (min + 60 * hour) + sec;
	}

	// check if we have a time consisting of min and sec (ab:cd)
	if(2 == sscanf(str, " %16u : %2u ", &min, &sec)) {
		return sec >= 60 ? INVALID_TIME : 60 * min + sec;
	}

	// check if we have a "second only" time
	if(1 == sscanf(str, " %16u ", &sec)) {
		return sec;
	}

	return INVALID_TIME;
}

void sha512_string(char *sha512_buf, void *inbuf, size_t inbuf_size) {

	unsigned char sha512_fingerprint[SHA512_LEN];
	sha512(inbuf, inbuf_size, sha512_fingerprint, false);

	for(size_t i = 0; i < sizeof(sha512_fingerprint); i++) {
		sprintf(sha512_buf + 3*i, "%02X%s", sha512_fingerprint[i], i+1 < sizeof(sha512_fingerprint) ? ":" : "");
	}
	sha512_buf[SHA512_LEN * 3] = '\0';
}

int lregcomp(regex_t *preg, const char *regex, int cflags) {
	int err = regcomp(preg, regex, cflags);
	if(err) {
		char err_str[2048] = {0};
		regerror(err, preg, err_str, sizeof(err_str));
		_err("regcomp: %s", err_str);
		_err("regcomp: affected string: \"%s\"", regex);
	}
	return err;
}

struct mmapped_file file_read_contents(char *path) {
	struct mmapped_file file = { .data = NULL, .size = 0 };

	int fd = open(path, O_NOFOLLOW);
	if(-1 == fd) {
		_err("open: %s", strerror(errno));
		return file;
	}

	struct stat fdstat;
	if(!fstat(fd, &fdstat)) {
		if(fdstat.st_size >= 0) {
			size_t fsize = (size_t) fdstat.st_size;
			file.data = mmap(NULL, fsize, PROT_READ, MAP_SHARED, fd, 0);
			if(MAP_FAILED != file.data) {
				file.size = fsize;
			} else {
				file.data = NULL;
				_err("mmap: %s", strerror(errno));
			}
		} else {
			_err("fstat returned a negative size for `%s`", path);
		}
	} else {
		_err("fstat: %s", strerror(errno));
	}
	close(fd);

	return file;
}

size_t add_delta_within_limits(size_t base, int delta, size_t upper_limit) {
	if(delta < 0) {
		unsigned int udelta = (unsigned int) -delta;
		return base < udelta ? 0 : base - udelta;
	} else {
		size_t res = base + (unsigned int) delta;
		return res < upper_limit ? res : upper_limit;
	}
}

size_t parse_position(char *pos_req, size_t pos_cur, size_t pos_max, size_t page_size) {
	astrdup(pos, pos_req);
	char *target = strstrp(pos);

	assert(pos_max < SIZE_MAX && "ERROR: pos_max == SIZE_MAX");

	if(streq("end", target)) {
		// special position `end`: the end of list/text/whatever
		return pos_max;
	} else if('+' == *target || '-' == *target) {
		// relative (to current position) position

		if(NULL != strchr(target, '.')) {
			float pages = 0;
			if(1 == sscanf(target, " %16f ", &pages)) {
				int delta = (int) (pages * (float) page_size);
				return add_delta_within_limits(pos_cur, delta, pos_max);
			}
		} else {
			int delta;
			if(1 == sscanf(target, " %16d ", &delta)) {
				return add_delta_within_limits(pos_cur, delta, pos_max);
			}
		}
	} else {
		// absolute position
		size_t abs;
		if(1 == sscanf(target, " %8zu ", &abs)) {
			return abs;
		}
	}

	return SIZE_MAX;
}

// suppress warning regarding the cast (const void*) to (void*)
// mmapped files are mapped ro, therefore a const can be used
// to warn about writing to that memory.
// as munmap expects a (void*), we need to get rid of that const here
#pragma GCC diagnostic ignored "-Wcast-qual"
void file_release_contents(struct mmapped_file file) {
	munmap((void*) file.data, file.size);
}
