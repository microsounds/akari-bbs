#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sqlite3.h>
#include "global.h"
#include "database.h"
#include "query.h"
#include "utf8.h"
#include "macros.h"

/*
 * [core functionality]
 * submit.c
 * POST submission, database insert, housekeeping
 */

/* USAGE:
 * thread mode: board=a&mode=thread&...
 * reply mode:  board=a&mode=reply&parent=12345&...
 */

static const char *const html[] = {
	/* header */
	"<!DOCTYPE html>"
	"<html lang=\"en-US\">"
	"<head>"
		"<title>Submit - %s</title>"
		"<meta charset=\"UTF-8\" />"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />"
		"<meta name=\"theme-color\" content=\"#DC8B9A\" />"
		"<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"img/favicon.ico\" />"
		"<link rel=\"stylesheet\" type=\"text/css\" href=\"css/style.css\" />"
	"</head>"
	"<body>"
		"<div class=\"pContainer\">"
			"<span class=\"pName\">%s <i>rev.%d/db-%d</i></span>"
			"<div class=\"pComment\">",
	/* footer */
			"</div>"
		"</div>"
	"</body>"
	"</html>",
	/* backlink */
	"<div class=\"navi controls\">[<a href=\"%s\">Go back</a>]</div>"
};

static void abort_now(const char *fmt, ...)
{
	/* exception handling
	 * print formatted error, add backlink and exit
	 */
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	const char *refer = getenv_s("HTTP_REFERER");
	fprintf(stdout, html[2], (!refer) ? "/" : refer);
	fprintf(stdout, html[1]); /* footer */
	exit(1);
}

int main(void)
{
	int err;
	FILE *fp;
	sqlite3 *db;
	fprintf(stdout, "Content-type: text/html\n\n");
	fprintf(stdout, html[0], IDENT_FULL, IDENT, REVISION, DB_VER);
	if ((fp = fopen("POSTING_DISABLED", "r"))) /* maintenance lockout */
		abort_now("<h2>Posting disabled, check back later.</h2>");
	if ((err = sqlite3_open_v2(DATABASE_LOC, &db, 2, NULL))) /* read/write mode */
		abort_now("<h2>Cannot open database. (e%d: %s)</h2>", err, sqlite3_err[err]);

	query_t query = { 0 }; /* obtain POST options */
	const char *request = getenv_s("REQUEST_METHOD");
	if (!request)
		abort_now("<h2>Not a valid CGI environment.</h2>");
	if (!strcmp(request, "POST"))
	{
		const char *size = getenv_s("CONTENT_LENGTH");
		unsigned POST_len = (!size) ? 0 : atoi(size);
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

		unsigned timer;
		cm.ip = getenv_s("REMOTE_ADDR"); /* ip address */
		if ((timer = db_cooldown_timer(db, cm.ip))) /* post cooldown */
			abort_now("<h2>Please wait %s before posting again.</h2>", time_human(timer));
		cm.board_id = query_search(&query, "board"); /* get board_id */
		if (cm.board_id)
		{
			strip_whitespace(utf8_rewrite(cm.board_id));
			xss_sanitize(&cm.board_id); /* scrub */
			struct board list;
			db_board_fetch(db, &list);
			unsigned i, valid = 0;
			for (i = 0; i < list.count && !valid; i++)
				if (!strcmp(list.arr[i].id, cm.board_id)) /* validate board */
					valid = 1;
			if (!valid)
				abort_now("<h2>Specified board doesn't exist.</h2>");
			if (db_status_flags(db, cm.board_id, -1) & BOARD_LOCKED)
				abort_now("<h2>Posting is disabled on this board.<h2>");
			db_board_free(&list);
		}
		else
			abort_now("<h2>No board provided.</h2>");

		/* MODES
		 * thread mode - parent_id is same as post id
		 * reply mode - parent_id is provided by the client
		 */
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

		cm.time = time(NULL); /* assign timestamp */
		cm.id = db_total_posts(db, cm.board_id, -1) + 1; /* assign post id */
		char *parent_str = query_search(&query, "parent");
		if (mode == THREAD_MODE)
		{
			cm.parent_id = cm.id;
			if (db_user_threads(db, cm.board_id, cm.ip) > MAX_THREADS_PER_IP)
				abort_now("<h2>You can only have %d active threads at a time.</h2>",
				          MAX_THREADS_PER_IP);
		}
		else if (parent_str) /* REPLY_MODE */
		{
			cm.parent_id = atoi_s(parent_str);
			if (cm.parent_id != db_find_parent(db, cm.board_id, cm.parent_id))
				abort_now("<h2>Specified parent thread doesn't exist.</h2>");
			if (db_archive_status(db, cm.board_id, cm.parent_id))
				abort_now("<h2>You cannot reply to this thread anymore.</h2>");
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

		/* user input field valiation
		 * sanitation/character escapes come last as they interfere
		 * with character count/tripcode passwords
		 */
		cm.name = query_search(&query, "name"); /* name and/or tripcode */
		cm.subject = query_search(&query, "subject"); /* subject */
		cm.comment = query_search(&query, "comment"); /* comment body */
		if (!cm.comment)
			abort_now("<h2>You cannot post a blank comment.</h2>");

		/* three star pointer needed to assign realloc'd char arrays
		 * back to their original post container
		 */
		char **field[] = { &cm.name, &cm.subject, &cm.comment };
		static const unsigned limit[] = {
			NAME_MAX_LENGTH, SUBJECT_MAX_LENGTH, COMMENT_MAX_LENGTH
		};
		unsigned i;
		for (i = 0; i < static_size(field); i++)
		{
			if (*field[i])
			{
				strip_whitespace(utf8_rewrite(*field[i])); /* UTF-8 */
				if (utf8_charcount(*field[i]) > limit[i]) /* length limit */
					abort_now("<h2>One or more fields are too long.</h2>");
			}
		}
		/* generate tripcode from #password if name provided */
		cm.trip = (!cm.name) ? NULL : tripcode_hash(tripcode_pass(&cm.name));

		for (i = 0; i < static_size(field); i++)
			if (*field[i]) xss_sanitize(field[i]); /* sanitize inputs */

		if ((timer = db_duplicate_post(db, cm.comment, cm.ip))) /* flooding */
			abort_now("<h2>Wait %s before making an identical post.</h2>", time_human(timer));
		if (spam_filter(cm.comment)) /* spammy behavior */
			abort_now("<h2>This post is spam. Please rewrite it.</h2>");

		int attempts = 0;
		reassign: if (attempts++ > 0) /* reattempt insert operation */
		{
			sleep(1);
			cm.time = time(NULL); /* reassign post id's */
			cm.id = db_total_posts(db, cm.board_id, -1) + 1;
			if (mode == THREAD_MODE)
				cm.parent_id = cm.id;
		}
		if (!(err = db_post_insert(db, &cm))) /* insert post / push new thread */
		{
			db_archive_oldest(db, cm.board_id); /* prune stale threads */
			if (mode == REPLY_MODE)
			{
				if (!(cm.options & POST_SAGE)) /* and bump the parent */
					db_bump_parent(db, cm.board_id, cm.parent_id);
				fprintf(stdout, "<h2>Reply to Thread No.%ld<br/>", cm.parent_id);
				fprintf(stdout, ">>> Post No.%ld submitted!</h2>", cm.id);
			}
			else /* THREAD_MODE */
				fprintf(stdout, "<h2>Thread No.%ld created!</h2>", cm.id);
			thread_redirect(cm.board_id, cm.parent_id, cm.id); /* redirect */
		}
		else if (attempts < INSERT_MAX_RETRIES) /* post number collision? */
			goto reassign;
		else
			abort_now("<h2>Post failed. (e%d: %s)</h2>", err, sqlite3_err[err]);
		query_free(&query);
	}
	fprintf(stdout, html[1]); /* footer */
	sqlite3_close(db);
	return 0;
}
