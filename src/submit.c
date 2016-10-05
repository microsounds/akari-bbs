#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	/* check if board exists */
	sqlite3_stmt *stmt;
	unsigned exists = 0;
	static const char *sql = "SELECT COUNT(*) FROM boards WHERE id = \"%s\";";
	char *cmd = (char *) malloc(sizeof(char) * strlen(sql) + strlen(board_id) + 1);
	sprintf(cmd, sql, board_id);
	sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
	sqlite3_step(stmt);
	exists = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	free(cmd);
	return exists;
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
		abort_now("This isn't a CGI environment.\n");
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
		struct post cm; /* compose a new post */
		cm.board_id = query_search(&query, "board"); /* get board */
		if (cm.board_id)
		{
			strip_whitespace(utf8_rewrite(cm.board_id));
			xss_sanitize(&cm.board_id); /* scrub */
			if (!db_validate_board(db, cm.board_id))
				abort_now("<h2>Invalid board identifier.</h2>");
		}
		else
			abort_now("<h2>No board provided.</h2>");

		char *mode = query_search(&query, "mode"); /* get mode */


		cm.ip = getenv("REMOTE_ADDR"); /* IP address */
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
