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
#include <assert.h>
#include <errno.h>
#include <stdarg.h>                     // for va_end, va_list, va_start
#include <stdbool.h>                    // for bool, false, true
#include <stdio.h>                      // for sscanf, snprintf, vsnprintf
#include <stdlib.h>                     // for calloc, exit, malloc, etc
#include <string.h>                     // for strlen, strdup
#include <unistd.h>                     // for close, dup2, execlp, fork, etc
//\endcond

#include <polarssl/sha512.h>            // for sha512

#include "log.h"
#include "tui.h"

char* smprintf(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int required_size = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	va_list aq;
	va_start(aq, fmt);
	char *buffer = lcalloc(sizeof(char), required_size + 1);
	vsnprintf(buffer, required_size + 1, fmt, aq);
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

void* _lmalloc(char *srcfile, int srcline, const char *srcfunc, size_t size) {
	void *ptr = malloc(size);
	if(!ptr) {
		__log(srcfile, srcline, srcfunc, "malloc(%zu) failed: %s", size, strerror(errno));
	}

	return ptr;
}

void* _lcalloc(char *srcfile, int srcline, const char *srcfunc, size_t nmemb, size_t size) {
	void *ptr = calloc(nmemb, size);
	if(!ptr) {
		__log(srcfile, srcline, srcfunc, "calloc(%zu, %zu) failed: %s", nmemb, size, strerror(errno));
	}

	return ptr;
}

void* _lrealloc(char *srcfile, int srcline, const char *srcfunc, void *ptr, size_t size) {
	void *new_ptr = realloc(ptr, size);
	if(!new_ptr) {
		__log(srcfile, srcline, srcfunc, "realloc(%p, %zu) failed: %s", ptr, size, strerror(errno));
	}

	return new_ptr;
}

char *_lstrdup(char *srcfile, int srcline, const char *srcfunc, const char *s) {
	void *d = strdup(s);
	if(!d) {
		__log(srcfile, srcline, srcfunc, "strdup(\"%s\") failed: %s", s, strerror(errno));
	}

	return d;
}

unsigned int parse_time_to_sec(char *str) {
	unsigned int hour, min, sec;

	if(3 == sscanf(str, " %u : %u : %u ", &hour, &min, &sec)) {
		if(sec >= 60 || min >= 60) {
			return INVALID_TIME;
		}

		return 60 * (min + 60 * hour) + sec;
	}

	// check if we have a time consisting of min and sec (ab:cd)
	if(2 == sscanf(str, " %u : %u ", &min, &sec)) {
		return sec >= 60 ? INVALID_TIME : 60 * min + sec;
	}

	// check if we have a "second only" time
	if(1 == sscanf(str, " %u ", &sec)) {
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

static char* string_find_next_url(char *string, char **url_end) {

	// search for `http(s)://`
	char *start = strstr(string, "http");
	if(!start) {
		return NULL;
	}

	size_t pos = 4;
	if('s' == start[pos]) pos++;

	if(strncmp(&start[pos], "://", 3)) {
		return NULL;
	}

	char *end = start;
	while(*end && ' ' != *end && '\t' != *end && '\r' != *end && '\n' != *end) {
		end++;
	}
	*url_end = end;

	return start;
}

size_t string_find_urls(char *string, char ***urls_out) {
	char **urls = NULL;
	size_t urls_size = 0;

	size_t urls_pos  = 0;

	char *string_remaining = string;
	char *url_start = NULL;
	char *url_end   = NULL;
	while( (url_start = string_find_next_url(string_remaining, &url_end)) ) {
		char old_url_end = *url_end;
		*url_end = '\0';

		if(urls_pos >= urls_size) {
			urls_size += 16;
			urls = lrealloc(urls, urls_size * sizeof(char*));
		}

		urls[urls_pos] = strdup(url_start);
		urls_pos++;

		*url_end = old_url_end;

		string_remaining = url_end;
	}

	*urls_out = urls;
	return urls_pos;
}

char* string_prepare_urls_for_display(char *string, size_t url_count) {

	size_t prep_size = strlen(string) + (2 + 16) * url_count + 1;
	size_t prep_used = 0;
	char  *prep      = lcalloc(prep_size, sizeof(char));

	size_t urls_found = 0;

	char *string_remaining = string;
	char *url_start = NULL;
	char *url_end   = NULL;
	while( (url_start = string_find_next_url(string_remaining, &url_end)) ) {

		*url_start = '\0';
		assert(prep_size >= prep_used);
		prep_used += snprintf(&prep[prep_used], prep_size - prep_used, "%s", string_remaining);
		*url_start = 'h';

		char old_url_end = *url_end;
		*url_end = '\0';

		assert(prep_size >= prep_used);
		prep_used += snprintf(&prep[prep_used], prep_size - prep_used, F_UNDERLINE"%s [%zu]"F_RESET, url_start, urls_found);
		*url_end = old_url_end;

		urls_found++;
		string_remaining = url_end;
	}
	assert(prep_size >= prep_used);
	snprintf(&prep[prep_used], prep_size - prep_used, "%s", string_remaining);

	assert(url_count == urls_found && "Failed to refind all URLs previously found!");
	return prep;
}
