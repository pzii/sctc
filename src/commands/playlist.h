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

#ifndef _COMMANDS_PLAYLIST_H
	#define _COMMANDS_PLAYLIST_H

	#include "../_hard_config.h"

	void cmd_pl_add           (const char *_list) ATTR(nonnull);
	void cmd_pl_command_input (const char *unused UNUSED);
	void cmd_pl_play          (const char *unused UNUSED);
	void cmd_pl_goto          (const char *_hint) ATTR(nonnull);
	void cmd_pl_del           (const char *unused UNUSED);
	void cmd_pl_details       (const char *unused UNUSED);
	void cmd_pl_search_next   (const char *unused UNUSED);
	void cmd_pl_search_prev   (const char *unused UNUSED);
	void cmd_pl_search_start  (const char *unused UNUSED);
	void cmd_pl_open_user     (const char *_user) ATTR(nonnull);

	void cmd_pl_list_new      (const char *_name) ATTR(nonnull);
	void cmd_pl_write_playlist(const char *_file) ATTR(nonnull);
	void cmd_pl_list          (const char *list)  ATTR(nonnull);
	void cmd_pl_download      (const char *unused UNUSED);
	void cmd_pl_exit          (const char *unused UNUSED) ATTR(noreturn);
	void cmd_pl_help          (const char *unused UNUSED);
	void cmd_pl_yank          (const char *unused UNUSED);
	void cmd_pl_subscriptions (const char *_user);
#endif
