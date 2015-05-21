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

#define _XOPEN_SOURCE 500

//\cond
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
//\endcond

#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <yajl/yajl_tree.h>

#include "yajl_helper.h"
#include "track.h"
#include "log.h"
#include "helper.h"
#include "jspf.h"

#define YAJL_GEN_STRING(hand, str) yajl_gen_string(hand, (unsigned char*)str, strlen(str))

#define YAJL_GEN_ENTRY(hand, title, content) { \
	if(content) { \
		YAJL_GEN_STRING(hand, title); \
		YAJL_GEN_STRING(hand, content); \
	} \
}

#define YAJL_GEN_ENTRY_INT(hand, title, value) { \
	YAJL_GEN_STRING(hand, title); \
	yajl_gen_integer(hand, value); \
}

#define YAJL_GEN_SCOPED_ENTRY(hand, title, content) { \
	yajl_gen_map_open(hand); \
	YAJL_GEN_ENTRY(hand, title, content) \
	yajl_gen_map_close(hand); \
}

#define YAJL_GEN_SCOPED_ENTRY_INT(hand, title, content) { \
	yajl_gen_map_open(hand); \
	YAJL_GEN_ENTRY_INT(hand, title, content) \
	yajl_gen_map_close(hand); \
}

#define MY_ENCODING "utf-8"

static void jspf_filewriter(void *ctx, const char *str, size_t len);
bool jspf_write(char *file, struct track_list *list);

static void jspf_filewriter(void *ctx, const char *str, size_t len) {
	FILE *fh = (FILE*) ctx;
	fwrite(str, sizeof(char), len, fh);
}

static void write_jspf_track(yajl_gen hand, struct track *track) {
	yajl_gen_map_open(hand);

	YAJL_GEN_ENTRY    (hand, "title",      track->name);
	YAJL_GEN_ENTRY    (hand, "location",   track->stream_url);
	YAJL_GEN_ENTRY    (hand, "creator",    track->username);
	YAJL_GEN_ENTRY    (hand, "identifier", track->permalink_url);
	YAJL_GEN_ENTRY    (hand, "annotation", track->description);
	YAJL_GEN_ENTRY_INT(hand, "duration",   track->duration);

	// use meta-tags to save 'bpm' (beats per minute) and date of creation
	YAJL_GEN_STRING(hand, "meta");
	yajl_gen_array_open(hand);

	char time_buffer[256];
	strftime(time_buffer, sizeof(time_buffer), "%Y/%m/%d %H:%M:%S %z", &track->created_at);

	YAJL_GEN_SCOPED_ENTRY_INT(hand, "https://sctc.narbo.de/bpm",      track->bpm);
	YAJL_GEN_SCOPED_ENTRY_INT(hand, "https://sctc.narbo.de/user_id",  track->user_id);
	YAJL_GEN_SCOPED_ENTRY_INT(hand, "https://sctc.narbo.de/track_id", track->track_id);

	yajl_gen_map_open(hand);
	YAJL_GEN_STRING(hand, "https://sctc.narbo.de/created_at");
	YAJL_GEN_STRING(hand, time_buffer);
	yajl_gen_map_close(hand);

	yajl_gen_array_close(hand);

	yajl_gen_map_close(hand);
}

bool jspf_write(char *file, struct track_list *list) {
	FILE *fh = fopen(file, "w");

	yajl_gen hand = yajl_gen_alloc(NULL);

	yajl_gen_config(hand, yajl_gen_print_callback, jspf_filewriter, fh);

	yajl_gen_map_open(hand);
	YAJL_GEN_STRING(hand, "playlist");
	yajl_gen_map_open(hand);
	YAJL_GEN_STRING(hand, "track");

	yajl_gen_array_open(hand);
	for(int i = 0; i < list->count; i++) {
		write_jspf_track(hand, &list->entries[i]);
	}
	yajl_gen_array_close(hand);

	yajl_gen_map_close(hand);
	yajl_gen_map_close(hand);

	fclose(fh);

	return true;
}

struct track_list* jspf_read(char *file) {
	struct track_list *list = lcalloc(1, sizeof(struct track_list));

	FILE *fh = fopen(file, "r");
	if(!fh) {
		return list;
	}

	struct stat jspf_stat;
	fstat(fileno(fh), &jspf_stat);

	char buffer[jspf_stat.st_size + 1];
	fread(buffer, sizeof(char), jspf_stat.st_size, fh);
	buffer[jspf_stat.st_size] = '\0';

	fclose(fh);

	yajl_val node_root = yajl_helper_parse(buffer);
	yajl_val array = yajl_helper_get_array(node_root, "playlist", "track");

	if(array) {
		list->entries = lcalloc(array->u.array.len + 1, sizeof(struct track));
		list->count   = array->u.array.len;

		for(int i = 0; i < array->u.array.len; i++) {
			list->entries[i].name          = yajl_helper_get_string(array->u.array.values[i], "title",         NULL);
			list->entries[i].stream_url    = yajl_helper_get_string(array->u.array.values[i], "location",      NULL);
			list->entries[i].username      = yajl_helper_get_string(array->u.array.values[i], "creator",       NULL);
			list->entries[i].permalink_url = yajl_helper_get_string(array->u.array.values[i], "identifier",    NULL);
			list->entries[i].description   = yajl_helper_get_string(array->u.array.values[i], "annotation",    NULL);
			list->entries[i].duration      = yajl_helper_get_int   (array->u.array.values[i], "duration",      NULL);
			
			// TODO \todo download_url not part of cached data
			// list->entries[i].download_url  = yajl_helper_get_string(array->u.array.values[i], "download_url",  NULL);

			yajl_val node_meta = yajl_helper_get_array(array->u.array.values[i], "meta", NULL);
			for(int j = 0; j < node_meta->u.array.len; j++) {
				int val_user_id  = yajl_helper_get_int(node_meta->u.array.values[j], "https://sctc.narbo.de/user_id", NULL);
				int val_bpm      = yajl_helper_get_int(node_meta->u.array.values[j], "https://sctc.narbo.de/bpm", NULL);
				int val_track_id = yajl_helper_get_int(node_meta->u.array.values[j], "https://sctc.narbo.de/track_id", NULL);

				if(val_user_id)  list->entries[i].user_id = val_user_id;
				if(val_track_id) list->entries[i].track_id = val_track_id;
				if(val_bpm)      list->entries[i].bpm = val_bpm;

				char *date_str = yajl_helper_get_string(node_meta->u.array.values[j], "https://sctc.narbo.de/created_at", NULL);
				if(date_str) {
					char *ret = strptime(date_str, "%Y/%m/%d %H:%M:%S %z", &list->entries[i].created_at);
					if(!ret || *ret) {
						_log("strptime(\"%s\"): '%s'", date_str, strerror(errno));
					}
					free(date_str);
				}
			}
		}
	}

	return list;
}
