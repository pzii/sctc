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

/** \file helper.h
 *  \brief Several basic helper functions.
*/

#ifndef _HELPER_H
	#define _HELPER_H

	#include "_hard_config.h"

	//\cond
	#include <stdbool.h>                    // for bool
	#include <stddef.h>                     // for size_t
	#include <regex.h>
	//\endcond

	#ifdef NDEBUG
		#define ONLY_DEBUG(X)
	#else
		#define ONLY_DEBUG(X) X
	#endif

	#define SHA512_LEN 64

	#define astrdup(VNEW, VOLD) char VNEW[strlen(VOLD) + 1]; strcpy(VNEW, VOLD);

	#define streq(S1, S2) ( 0 == strcmp(S1, S2) )

	/** \brief printf style function with automated allocation of buffer.
	 *
	 *  \param sha512_buf  Buffer receiving the SHA512 string (required size: SHA512_LEN * 3 + 1 = 193)
	 *  \param inbuf       The buffer to be hashed
	 *  \param inbuf_size  The number of Bytes in `inbuf` to be hashed
	 */
	void sha512_string(char *sha512_buf, void *inbuf, size_t inbuf_size) ATTR(nonnull);

	/** \brief printf style function with automated allocation of buffer.
	 *
	 *  \param fmt  The formatstring, as known from `printf`, see `man 3 printf`
	 *  \return     Pointer to allocated memory (needs to be `free`'d) or `NULL` if an error occured
	 */
	char* smprintf(char *fmt, ...) ATTR(format (printf, 1, 2), nonnull);

	/** \brief Write a formated version of time_secs to buffer.
	 *
	 *  If time_secs is above one hour the format "%d:%02d:%02d" is used, otherwise "%02d:%02d" is used.\n
	 *  At most buffer_size bytes are written to buffer. If `buffer_size - 1` is returned your output is most likely truncated
	 *  and thus the used buffer should be resized.
	 *
	 *  \param  buffer       The buffer receiving the formated time.
	 *  \param  buffer_size  The size of the buffer.
	 *  \param  time_secs    The seconds to be formated.
	 *  \return              The number of bytes written to buffer (excluding the terminating '\0')
	 */
	int snprint_ftime(char *buffer, size_t buffer_size, int time_secs) ATTR(nonnull);

	/** \brief replace a char with another char in a given string
	 *
	 *  \param str  The string to search in, points to a buffer that will be *modified*
	 *  \param s    The char to `s`earch for
	 *  \param r    The char to `r`eplace with
	 */
	void strcrep(char *str, char s, char r) ATTR(nonnull);

	/** \brief Strip whitespaces at the beginning and at the end of the string
	 *
	 *  \param str  The string to strip, points to a buffer that will be *modified*
	 *  \return     Pointer to the new beginning
	 */
	char* strstrp(char *str) ATTR(nonnull, returns_nonnull);

	/** \brief Executes a provided command (including argument)
	 *
	 *  \param cmd    The executable
	 *  \param param  The parameters
	 *  \return       `true` on success, `false` otherwise
	 */
	bool fork_and_run(char *cmd, char *param);

	/** \brief Yank the provided text
	 *
	 *  \param text  The text to yank
	 *  \return      `true` on success, `false` otherwise
	 */
	bool yank(char *text) ATTR(nonnull);


	#define INVALID_TIME ((unsigned int) ~0) ///< value indicating an invalid time

	/** \brief Parse a string to seconds
	 *
	 *  Parses a string in format `hour:min:sec`, `min:sec` or `sec` to a number of seconds.
	 *  If `str` does not match any of the formats shown above INVALID_TIME is returned.
	 *
	 *  \param str  The string to be parsed
	 *  \return     The corresponding number of seconds or INVALID_TIME in case of invalid format
	 */
	unsigned int parse_time_to_sec(char *str) ATTR(nonnull);

	/** \brief Logging wrapper for malloc().
	 *
	 *  Behaves like malloc(), but writes a message to the logfile in case of an error.
	 *  lmalloc() does **not terminate** the execution.
	 *
	 *  Always use this macro instead of directly calling _lmalloc().
	 */
	#define lmalloc(S)    _lmalloc (__FILE__, __LINE__, __func__, S)

	/** \brief Logging wrapper for calloc().
	 *
	 *  Behaves like calloc(), but writes a message to the logfile in case of an error.
	 *  lcalloc() does **not terminate** the execution.
	 *
	 *  Always use this macro instead of directly calling _lrealloc().
	 */
	#define lcalloc(N,S)  _lcalloc (__FILE__, __LINE__, __func__, N, S)

	/** \brief Logging wrapper for realloc().
	 *
	 *  Behaves like realloc(), but writes a message to the logfile in case of an error.
	 *  lrealloc() does **not terminate** the execution.
	 *
	 *  Always use this macro instead of directly calling _lrealloc().
	 */
	#define lrealloc(P,S) _lrealloc(__FILE__, __LINE__, __func__, P, S)

	/** The internal implementation for lmalloc.
	 *
	 *  **Warning**: This function is not intended to be called directly by the user.
	 *  Use lmalloc() instead, as this macro inserts the correct position of the call to _lmalloc().
	 *
	 *  \param srcfile  The file executing the call to _lmalloc(); filled by macro lmalloc(), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to _lmalloc(); filled by macro lmalloc(), do not use "by hand"
	 *  \param srcfunc  The function calling _lmalloc(); filled by macro lmalloc(), do not use "by hand"
	 *  \param size     The number of bytes to be allocated
	 */
	void* _lmalloc (char *srcfile, int srcline, const char *srcfunc, size_t size) ATTR(warn_unused_result);

	/** The internal implementation for lcalloc.
	 *
	 *  **Warning**: This function is not intended to be called directly by the user.
	 *  Use lcalloc() instead, as this macro inserts the correct position of the call to _lcalloc().
	 *
	 *  \param srcfile  The file executing the call to _lcalloc(); filled by macro lcalloc(), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to _lcalloc(); filled by macro lcalloc(), do not use "by hand"
	 *  \param srcfunc  The function calling _lcallocg(); filled by macro lcalloc(), do not use "by hand"
	 *  \param nmemb    The number of elements to be allocated
	 *  \param size     The size of each single element to be allocated
	 */
	void* _lcalloc (char *srcfile, int srcline, const char *srcfunc, size_t nmemb, size_t size) ATTR(warn_unused_result);

	/** The internal implementation for lrealloc.
	 *
	 *  **Warning**: This function is not intended to be called directly by the user.
	 *  Use lrealloc() instead, as this macro inserts the correct position of the call to _lrealloc().
	 *
	 *  \param srcfile  The file executing the call to _lrealloc(); filled by macro lrealloc(), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to _lrealloc(); filled by macro lrealloc(), do not use "by hand"
	 *  \param srcfunc  The function calling _lrealloc(); filled by macro lrealloc(), do not use "by hand"
	 *  \param ptr      Pointer to the memory to resize
	 *  \param size     The number of bytes to be reallocated
	 */
	void* _lrealloc(char *srcfile, int srcline, const char *srcfunc, void *ptr, size_t size) ATTR(warn_unused_result);

	/** \brief Logging wrapper for strdup().
	 *
	 *  Behaves like strdup(), but writes a message to the logfile in case of an error.
	 *  lstrdup() does **not terminate** the execution.
	 *
	 *  Always use this macro instead of directly calling _lstrdup().
	 */
	#define lstrdup(S) _lstrdup(__FILE__, __LINE__, __func__, S)

	/** The internal implementation for lstrdup.
	 *
	 *  **Warning**: This function is not intended to be called directly by the user.
	 *  Use lstrdup() instead, as this macro inserts the correct position of the call to _lstrdup().
	 *
	 *  \param srcfile  The file executing the call to _lstrdup(); filled by macro lstrdup), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to _lstrdup(); filled by macro lstrdup(), do not use "by hand"
	 *  \param srcfunc  The function calling _lstrdup(); filled by macro lstrdup(), do not use "by hand"
	 *  \param s        The string to be duplicated
	 */
	char *_lstrdup(char *srcfile, int srcline, const char *srcfunc, const char *s) ATTR(nonnull, warn_unused_result);

	ONLY_DEBUG( void dump_alloc_counter(void); )

	struct mmapped_file {
		const void *data;
		size_t      size;
	};

	struct mmapped_file file_read_contents(char *file);
	void file_release_contents(struct mmapped_file file);

	int lregcomp(regex_t *preg, const char *regex, int cflags);

	size_t add_delta_within_limits(size_t base, int delta, size_t upper_limit);

	/** \brief Parse a string containing a relative or absolute position
	 *
	 *  Expected format for **relative positions**:
	 *   - prefixed with either `+` or `-`
	 *   - followed by:
	 *     - a positive integer: returns pos_cur + integer (capped to `[0; pos_max]`)
	 *     - a positive float: returns pos_cur + page_size * float (capped to `[0; pos_max]`)
	 *
	 *  Expected format for **absolute positions**:
	 *   - any positive integer (will be capped to `[0; pos_max]`)
	 *   - "end" (`pos_max` will be returned)
	 *
	 *  Any leading or trailing whitespaces will be ignored.
	 *
	 *  \param pos_req    The string to be parsed
	 *  \param pos_cur    The current position (used as base for relative positions)
	 *  \param pos_max    The maximum allowed position (to avoid going `behind` the list/...), `< SIZE_MAX`
	 *  \param page_size  The size of one page (in case of a relative float position)
	 *  \return           The resulting absolute position in [0; pos_max], or `SIZE_MAX` if parsing failed
	 */
	size_t parse_position(char *pos_req, size_t pos_cur, size_t pos_max, size_t page_size);
#endif /* _HELPER_H */
