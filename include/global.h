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
#define IDENT_FULL "Akari BBS"
#define IDENT "akari-bbs"
#define REPO_URL "https://github.com/microsounds/akari-bbs"
#define REVISION 14 /* revision no. */
#define DB_VER 4

/* constants */
#define DEFAULT_NAME "Anonymous"
#define DATABASE_LOC "db/database.sqlite3"

/* banners */
#define BANNER_COUNT 625
#define BANNER_LOC "img/banner"

/* thread constants */
#define THREAD_BUMP_LIMIT 350
#define MAX_ACTIVE_THREADS 150
#define THREADS_PER_PAGE 15
#define ARCHIVE_SEC ((60 * 60 * 24) * 30) /* 1 mo */

/* user constants */
#define COOLDOWN_SEC 30
#define MAX_THREADS_PER_IP 5
#define POST_MAX_PAYLOAD 10000
#define NAME_MAX_LENGTH 75
#define SUBJECT_MAX_LENGTH 75
#define COMMENT_MAX_LENGTH 2000

/* deprecated xxx */
#define POSTS_PER_PAGE 50

#endif
