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

int main(void)
{
	sqlite3 *db;
	fprintf(stdout, "Content-type: text/html\n\n");
	if (sqlite3_open_v2(DATABASE_LOC, &db, 2, NULL)) /* read/write mode */
	{
		fprintf(stdout, "<h2>[!] Database missing!\nRun 'init.sh' to continue.</h2>\n");
		return 1;
	}
	query_t query = { 0 };
	const char *request = getenv("REQUEST_METHOD");
	if (request && !strcmp(request, "POST"))
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
			fprintf(stdout, "<h2>Empty or abnormal POST request.</h2>");
	}
	else
		fprintf(stdout, "<h2>Not a POST request.</h2>");
	if (query.count > 0)
	{
		/* note:
		 * cm strings stored in query container
		 * or as static data, do not free them
		 */
		struct post cm; /* compose a new post */
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
