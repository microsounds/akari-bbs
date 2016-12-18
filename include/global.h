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
#define IDENT "akari-bbs"
#define IDENT_FULL "Akari BBS"
#define TAGLINE "Lightweight Messageboard System"
#define AUTHOR "microsounds"
#define LICENSE "Licensed GPL v3+"
#define REPO_URL "https://github.com/microsounds/akari-bbs"
#define REVISION 14 /* revision no. */
#define DB_VER 4

/* static resources
 * all anchor links should start with absolute / document root
 */
#define DATABASE_LOC "db/database.sqlite3"
#define BOARD_SCRIPT "/board.cgi"
#define SUBMIT_SCRIPT "/submit.cgi"

/* rotating banners */
#define BANNER_COUNT 625
#define BANNER_LOC "/img/banner"

/* thread constants */
#define THREAD_BUMP_LIMIT 350
#define MAX_ACTIVE_THREADS 150
#define MAX_REPLY_PREVIEW 5
#define THREADS_PER_PAGE 15
#define DAYS_TO_ARCHIVE 30

/* user constants */
#define DEFAULT_NAME "Anonymous"
#define COOLDOWN_SEC 30
#define IDENTICAL_POST_SEC 150
#define MAX_THREADS_PER_IP 5
#define POST_MAX_PAYLOAD 10000
#define NAME_MAX_LENGTH 75
#define OPTIONS_MAX_LENGTH 30
#define SUBJECT_MAX_LENGTH 75
#define COMMENT_MAX_LENGTH 2000
#define INSERT_MAX_RETRIES 1000

#endif
