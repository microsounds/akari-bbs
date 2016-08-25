#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "query.h"
#include "utf8.h"

/* global specifics */
static const char *DBLOC = "db/database.sqlite3";
const int MAX_LENGTH = 250;
const int COOLDOWN_SEC = 30;
const char *refresh = "<meta http-equiv=\"refresh\" content=\"2\" />";
const char *postbox = "[!!] Post a comment! (250 char max): <form action=\"board.cgi\" "
				"method=\"post\"><input type=\"text\" name=\"comment\" size=\"50\"maxlength=\"250\">"
				"<input type=\"submit\" value=\"Submit\"></form><br/>";

struct comment {
	long id;
	long time;
	char *ip;
	char *text;
};

struct resource {
	long count;
	struct comment *arr;
};

char *random_text(unsigned len)
{
	char *lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ";
	char *out = (char *) malloc(sizeof(char) * len + 1);
	unsigned i;
	for (i = 0; i < len; i++)
    	out[i] = lookup[rand() % 53];
	out[len] = '\0';
	return out;
}

void insert(sqlite3 *db, const char *ip, const char *str)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM comments;", -1, &stmt, NULL);
	sqlite3_step(stmt);
	unsigned id = sqlite3_column_int(stmt, 0) + 1;
	sqlite3_finalize(stmt);
	char *insert = (char *) malloc(sizeof(char) * strlen(str) + 1000); /* good enough */
	sprintf(insert, "INSERT INTO comments VALUES(%d, %ld, \"%s\", \"%s\");", id, (long) time(NULL), ip, str);
	sqlite3_prepare_v2(db, insert, -1, &stmt, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	free(insert);
}

void populate(sqlite3 *db, struct resource *res)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM comments;", -1, &stmt, NULL);
	sqlite3_step(stmt);
	res->count = sqlite3_column_int(stmt, 0);
	res->arr = (struct comment *) malloc(sizeof(struct comment) * res->count);
	sqlite3_finalize(stmt);
	sqlite3_prepare_v2(db, "SELECT * FROM comments;", -1, &stmt, NULL);
	unsigned i;
	for (i = 0; i < res->count; i++)
	{
		sqlite3_step(stmt);
		res->arr[i].id = sqlite3_column_int(stmt, 0);
		res->arr[i].time = sqlite3_column_int(stmt, 1);
		res->arr[i].ip = strdup((char *) sqlite3_column_text(stmt, 2));
		res->arr[i].text = strdup((char *) sqlite3_column_text(stmt, 3));
	}
	sqlite3_finalize(stmt);
}

void display(struct resource *res)
{
	int i;
	for (i = res->count - 1; i >= 0; i--)
	{
		char time_str[100];
		struct tm *ts = localtime((time_t *) &res->arr[i].time);
		strftime(time_str, 100, "%a, %m/%d/%y %I:%M:%S %p", ts);
		fprintf(stdout, "<br/>id: %ld\ndate: %s\ncomment: %s\n",
					res->arr[i].id, time_str, res->arr[i].text);
	}
}

void freeup(struct resource *res)
{
	if (res->count)
	{
		unsigned i;
		for (i = 0; i < res->count; i++)
			free(res->arr[i].text);
		free(res->arr);
	}
}

int post_cooldown(sqlite3 *db, const char *ip_addr)
{
	/* post cooldown timer */
	int timer = COOLDOWN_SEC;
	struct resource res;
	populate(db, &res);
	int i;
	for (i = res.count - 1; i >= 0; i--)
	{
		if (!strcmp(ip_addr, res.arr[i].ip))
		{
			timer = time(NULL) - res.arr[i].time;
			goto exit;
		}
	}
	exit: freeup(&res);
	return COOLDOWN_SEC - timer;
}

int main(int argc, char **argv)
{
	srand(time(NULL) + clock());
	sqlite3 *db;
	fprintf(stdout, "Content-type: text/html\n\n");
	fprintf(stdout, "<meta charset=\"UTF-8\">");
	if (sqlite3_open_v2(DBLOC, &db, 2, NULL))
	{
		fprintf(stdout, "<h2>[!] Database missing!\nRun 'init.sh' to continue.</h2>\n");
		return 1;
	}
	fprintf(stdout, "%s", postbox); /* post box */

	char *is_cgi = getenv("REQUEST_METHOD"); /* is this a CGI environment? */
	unsigned POST_len = (!is_cgi) ? 0 : atoi(getenv("CONTENT_LENGTH"));

	if (POST_len > 0) /* POST data received */
	{
		char *POST_data = (char *) malloc(sizeof(char) * POST_len + 1);
		fread(POST_data, POST_len, 1, stdin);
		POST_data[POST_len] = '\0';
		query_t query;
		query_parse(&query, POST_data);
		free(POST_data);
		char *comment = query_search(&query, "comment");
		if (comment)
		{
			utf8_rewrite(comment); /* ASCII => UTF-8 */
			int charcount = utf8_charcount(comment);
			xss_sanitize(&comment); /* prevent XSS attempts */

			char *ip_addr = getenv("REMOTE_ADDR");
			int timer = post_cooldown(db, ip_addr); /* is this user spamming? */

			if (timer > 0)
				fprintf(stdout, "<h1>Please wait %d more seconds before posting again.</h1>%s", timer, refresh);
			else if (charcount > MAX_LENGTH || charcount < 1)
				fprintf(stdout, "<h1>Post rejected.</h1>%s", refresh);
			else
			{
				insert(db, ip_addr, comment); /* insert */
				fprintf(stdout, "<h1>Post submitted!</h1>%s", refresh);
			}
			/* page should refresh shortly */
		}
		query_free(&query);
	}
	else /* no POST */
	{
		struct resource res;
		populate(db, &res);
		display(&res);
		freeup(&res);
	}
	sqlite3_close(db);
	return 0;
}
