#ifndef GLOBAL_H
#define GLOBAL_H

/*
 *  akari-bbs - Lightweight Messageboard System
 *  Copyright (C) 2016 microsounds <https://github.com/microsounds>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* software name */
const char *ident = "akari-bbs";
const char *repo_url = "https://github.com/microsounds/akari-bbs";
#define REVISION 14 /* revison no. */

/* global constants */
#define NAME_MAX_LENGTH 75
#define COMMENT_MAX_LENGTH 2000
#define COOLDOWN_SEC 30
#define POSTS_PER_PAGE 50
const char *default_name = "Anonymous";
const char *database_loc = "db/database.sqlite3";

/* banners */
#define BANNER_COUNT 625
const char *banner_loc = "img/banner";

/* database status flags */
enum user_priv {
	USER_NORMAL = (1 << 0),
	USER_MOD    = (1 << 1),
	USER_ADMIN  = (1 << 2)
};

enum board_status {
	BOARD_LOCKED = (1 << 0)
};

enum thread_status {
	THREAD_ACTIVE   = (1 << 0),
	THREAD_LOCKED   = (1 << 1),
	THREAD_STICKIED = (1 << 2)
};

enum post_options {
	POST_SAGE = (1 << 0)
};

#endif
