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

#ifndef _COMMANDS_TEXTBOX_H
	#define _COMMANDS_TEXTBOX_H

	#include "../_hard_config.h"

	/** \brief Close the currently visible textbox
	 * 
	 *  \param unused  Ignored/Unused parameter (due to interface)
	 */
	void cmd_tb_close(const char *unused UNUSED) ATTR(nonnull);

	/** \brief Scroll a textbox (up or down)
	 * 
	 *  \param 
	 */
	void cmd_tb_scroll(const char *_hint) ATTR(nonnull);

	/** \brief
	 * 
	 *  \param 
	 */
	void cmd_tb_yank(const char *_hint) ATTR(nonnull);

	void cmd_tb_goto(const char *_hint);
	void cmd_tb_toggle(const char *unused UNUSED);
	void cmd_tb_select_all(const char *unused UNUSED);
	void cmd_tb_select_none(const char *unused UNUSED);
#endif
