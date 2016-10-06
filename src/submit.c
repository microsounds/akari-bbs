#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sqlite3.h>
#include "global.h"
#include "database.h"
#include "query.h"
#include "utf8.h"
#include "substr.h"

/*
 * [core functionality]
 * submit.c
 * POST submission / database insert
 */

/* USAGE:
 * thread mode: board=a&mode=thread&...
 * reply mode:  board=a&mode=reply&parent=12345&...
 */

int db_validate_board(sqlite3 *db, const char *board_id)
{
	/* check if board_id exists */
	sqlite3_stmt *stmt;
	int exists = 0;
	const char *sql = "SELECT COUNT(*) FROM boards WHERE id = \"%s\";";
	unsigned size = strlen(sql) + strlen(board_id);
	char *cmd = (char *) malloc(sizeof(char) * size + 1);
	sprintf(cmd, sql, board_id);
	sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
	sqlite3_step(stmt);
	exists = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	free(cmd);
	return exists;
}

int db_validate_parent(sqlite3 *db, const char *board_id, const long id)
{
	/* check if post id is a parent post in board_id */
	sqlite3_stmt *stmt;
	unsigned exists = 0;
	const char *sql =
	"SELECT COUNT(*) FROM threads "
		"WHERE board_id = \"%s\" AND post_id = %ld;";
	unsigned size = strlen(sql) + strlen(board_id) + (sizeof(id) * CHAR_BIT);
	char *cmd = (char *) malloc(sizeof(char) * size + 1);
	sprintf(cmd, sql, board_id, id);
	sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
	sqlite3_step(stmt);
	exists = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	free(cmd);
	return exists;
}

long db_total_posts(sqlite3 *db, const char *board_id)
{
	/* returns the total post count of board_id */
	sqlite3_stmt *stmt;
	long count = 0;
	const char *sql = "SELECT MAX(id) FROM posts WHERE board_id = \"%s\";";
	unsigned size = strlen(sql) + strlen(board_id);
	char *cmd = (char *) malloc(sizeof(char) * size + 1);
	sprintf(cmd, sql, board_id);
	sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
	sqlite3_step(stmt);
	count = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	free(cmd);
	return count;
}

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
		if (POST_len > 0 || POST_len < MAX_POST_PAYLOAD)
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
		abort_now("<h2>Not a POST request.</h2>");

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
				abort_now("<h2>Invalid board identifier.</h2>");
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

		cm.id = db_total_posts(db, cm.board_id) + 1; /* assign id/parent_id */
		char *parent_str = query_search(&query, "parent");
		if (mode == THREAD_MODE)
			cm.parent_id = cm.id;
		else if (parent_str) /* REPLY_MODE */
		{
			char *s = parent_str;
			while (*++s) /* is numerical string? */
				if (*s < '0' || *s > '9')
					abort_now("<h2>Invalid parent thread.</h2>");
			cm.parent_id = atoi(parent_str);
			if (!db_validate_parent(db, cm.board_id, cm.parent_id))
				abort_now("<h2>Specified thread doesn't exist.</h2>");
		}
		else
			abort_now("<h2>No parent thread provided.</h2>");

		/* not yet implemented */
		cm.options = 0;
		cm.user_priv = USER_NORMAL;
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
		query_free(&query);
	}
	sqlite3_close(db);
	return 0;
}
