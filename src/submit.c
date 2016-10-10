#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "global.h"
#include "database.h"
#include "query.h"
#include "utf8.h"

/*
 * [core functionality]
 * submit.c
 * POST submission / database insert
 */

/* USAGE:
 * thread mode: board=a&mode=thread&...
 * reply mode:  board=a&mode=reply&parent=12345&...
 */

int main(void)
{
	sqlite3 *db;
	fprintf(stdout, "Content-type: text/html\n\n");
	if (sqlite3_open_v2(DATABASE_LOC, &db, 2, NULL)) /* read/write mode */
		abort_now("<h2>[!] Database missing!\nRun 'init.sh' to continue.</h2>\n");

	query_t query = { 0 }; /* obtain POST options */
	const char *request = getenv("REQUEST_METHOD");
	if (!request)
		abort_now("Not a CGI environment.\n");
	if (!strcmp(request, "POST"))
	{
		unsigned POST_len = atoi(getenv("CONTENT_LENGTH"));
		if (POST_len > 0 && POST_len < POST_MAX_PAYLOAD)
		{
			char *POST_data = (char *) malloc(sizeof(char) * POST_len + 1);
			fread(POST_data, POST_len, 1, stdin);
			POST_data[POST_len] = '\0';
			query_parse(&query, POST_data);
			free(POST_data);
		}
		else
			abort_now("<h2>Empty or abnormal POST request.</h2>");
	}
	else
		abort_now("<h2>Expected a POST request.</h2>");

	/* note:
	 * cm strings stored in query container
	 * or as static data, do not free them
	 */
	if (query.count > 0)
	{
		struct post cm = { 0 }; /* compose a new post */
		cm.board_id = query_search(&query, "board"); /* get board_id */
		if (cm.board_id)
		{
			strip_whitespace(utf8_rewrite(cm.board_id));
			xss_sanitize(&cm.board_id); /* scrub */
			if (!db_validate_board(db, cm.board_id))
				abort_now("<h2>Specified board doesn't exist.</h2>");
			if (db_status_flags(db, cm.board_id, -1) & BOARD_LOCKED)
				abort_now("<h2>This board is locked.<h2>");
		}
		else
			abort_now("<h2>No board provided.</h2>");

		const char *mode_str = query_search(&query, "mode"); /* get mode */
		enum submit_mode { THREAD_MODE, REPLY_MODE } mode;
		if (!mode_str)
			abort_now("<h2>No submit mode specified.</h2>");
		else if (!strcmp(mode_str, "thread"))
			mode = THREAD_MODE;
		else if (!strcmp(mode_str, "reply"))
			mode = REPLY_MODE;
		else
			abort_now("<h2>Invalid submit mode.</h2>");

		/* MODES
		 * thread mode - parent_id is same as post id
		 * reply mode - parent_id is provided by the client
		 */
		cm.id = db_total_posts(db, cm.board_id, -1) + 1; /* assign id/parent_id */
		char *parent_str = query_search(&query, "parent");
		if (mode == THREAD_MODE)
			cm.parent_id = cm.id;
		else if (parent_str) /* REPLY_MODE */
		{
			char *s = parent_str;
			while (*++s) /* is numerical string? */
				if (*s < '0' || *s > '9')
					abort_now("<h2>Malformed parent thread id.</h2>");
			cm.parent_id = abs(atoi(parent_str));
			if (!db_validate_parent(db, cm.board_id, cm.parent_id))
				abort_now("<h2>Specified thread doesn't exist.</h2>");
			if (db_status_flags(db, cm.board_id, cm.parent_id) & THREAD_LOCKED)
				abort_now("<h2>This thread is locked.<h2>");
		}
		else
			abort_now("<h2>No parent thread provided.</h2>");

		const char *opt = query_search(&query, "options"); /* post options */
		if (opt)
			cm.options |= (!strstr(opt, "sage")) ? 0 : POST_SAGE;

		/* user privilage options (not implemented) */
		cm.user_priv |= USER_NORMAL;
		/* deletion password (not implemented) */
		cm.del_pass = "dummy";

		cm.time = time(NULL); /* time */
		cm.ip = getenv("REMOTE_ADDR"); /* ip address */
		cm.name = query_search(&query, "name"); /* name and/or tripcode */
		if (cm.name)
			strip_whitespace(utf8_rewrite(cm.name));
		cm.trip = (!cm.name) ? NULL : tripcode_hash(tripcode_pass(&cm.name));
		cm.subject = query_search(&query, "subject"); /* subject */
		if (cm.subject)
			strip_whitespace(utf8_rewrite(cm.subject));
		cm.comment = query_search(&query, "comment"); /* comment body */
		/* continue */

		/* insert post into db
		 * thread mode:
		 * if posts in board_id > MAX_ACTIVE_THREADS
		 * find thread with oldest last_bump timestamp in active_threads and prune
		 * delete all archive posts older than ARCHIVE_SEC

		*   vv done vv
		 * reply mode:
		 * if !(cm.options & POST_SAGE)
		 		db_bump_parent(db, cm.board_id, cm.parent_id);
		 */
		query_free(&query);
	}
	sqlite3_close(db);
	return 0;
}
