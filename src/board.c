#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "query.h"
#include "utf8.h"

/* global constants */
const int MAX_LENGTH = 250;
const int COOLDOWN_SEC = 30;
const int POSTS_PER_PAGE = 50;
const char *database_loc = "db/database.sqlite3";

/* html */
const char *refresh = "<meta http-equiv=\"refresh\" content=\"2\" />";
const char *header =
"<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Akari BBS</title>"
"<link rel=\"stylesheet\" type=\"text/css\" href=\"css/style.css\" /></head>"
"<body><div class=\"header\"><div id=\"logo\">Akari BBS v0.6</div><div id=\"postbox\">"
"[!!] Post a comment! (250 char max): <form action=\"board.cgi\" method=\"post\">"
"<input type=\"text\" name=\"comment\" size=\"50\" maxlength=\"250\">"
"<input type=\"submit\" value=\"Submit\"></form></div><div class=\"reset\"></div>";
const char *footer = "</body></html>";

/* todo:
	- image attachments
 */

/* generic post containers */

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

long db_total(sqlite3 *db)
{
	/* fetch total entries in database */
	long entries = 0;
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "SELECT MAX(id) FROM comments;", -1, &stmt, NULL);
	sqlite3_step(stmt);
	entries = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return entries;
}

void db_insert(sqlite3 *db, struct comment *cm)
{
	/* insert new post into database */
	cm->id = db_total(db) + 1;
	cm->time = time(NULL);
	sqlite3_stmt *stmt;
	char *insert = (char *) malloc(sizeof(char) * strlen(cm->text) + 1000);
	static const char *sql = "INSERT INTO comments VALUES(%ld, %ld, \"%s\", \"%s\");";
	sprintf(insert, sql, cm->id, cm->time, cm->ip, cm->text);
	sqlite3_prepare_v2(db, insert, -1, &stmt, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	free(insert);
}

char *sql_rowcount(const char *str)
{
	/* rewrites SQL statements to fetch row count instead */
	static const char *ins = "COUNT(*)";
	char *out = (char *) malloc(sizeof(char) * strlen(str) + strlen(ins) + 1);
	unsigned i, j = 0;
	for (i = 0; str[i]; i++)
	{
		if (str[i] == '*')
		{
			memcpy(&out[j], ins, strlen(ins));
			j += strlen(ins);
		}
		else
			out[j++] = str[i];
	}
	out[j] = '\0';
	return out;
}

void res_fetch(sqlite3 *db, struct resource *res, const char *sql)
{
	/* fetch requested SQL results into memory */
	/* this automated version is intended for COUNT(*) compatible statements */
	sqlite3_stmt *stmt;
	char *row_count = sql_rowcount(sql); /* how many rows? */
	sqlite3_prepare_v2(db, row_count, -1, &stmt, NULL);
	sqlite3_step(stmt);
	res->count = sqlite3_column_int(stmt, 0);
	res->arr = (struct comment *) malloc(sizeof(struct comment) * res->count);
	sqlite3_finalize(stmt);
	free(row_count);
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); /* fetch results */
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

void res_fetch_specific(sqlite3 *db, struct resource *res, char *sql, int limit)
{
	/* fetch requested SQL results into memory */
	/* this manual version is intended for LIMIT/OFFSET statements ONLY */
	sqlite3_stmt *stmt;
	res->count = limit; /* user-provided row count */
	res->arr = (struct comment *) malloc(sizeof(struct comment) * res->count);
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); /* fetch results */
	unsigned i;
	for (i = 0; i < res->count; i++)
	{
		int err = sqlite3_step(stmt);
		res->arr[i].id = sqlite3_column_int(stmt, 0);
		res->arr[i].time = sqlite3_column_int(stmt, 1);
		res->arr[i].ip = strdup((char *) sqlite3_column_text(stmt, 2));
		res->arr[i].text = strdup((char *) sqlite3_column_text(stmt, 3));
		if (err == SQLITE_DONE) /* early exit */
		{
			res->count = i;
			break;
		}
	}
	sqlite3_finalize(stmt);
}

void res_display(struct resource *res)
{
	/* print out SQL results stored in memory */
	fprintf(stdout,"<table class=\"posts\">");
	int i;
	for (i = 0; i < res->count; i++)
	{
		char time_str[100];
		struct tm *ts = localtime((time_t *) &res->arr[i].time);
		strftime(time_str, 100, "%a, %m/%d/%y %I:%M:%S %p", ts);
		fprintf(stdout,"<tr><td class=\"id\">id: %ld</td><td class=\"date\">%s</td><td class=\"comment\">%s</td></tr>",
					res->arr[i].id, time_str, res->arr[i].text);
	}
	fprintf(stdout,"</table>");
}

void res_freeup(struct resource *res)
{
	if (res->count)
	{
		unsigned i;
		for (i = 0; i < res->count; i++)
			free(res->arr[i].text);
		free(res->arr);
	}
}

void display_controls(int limit, int offset, int results)
{
	/* display controls for the user */
	fprintf(stdout, "<br />");
	static const char *link =" <a href=\"board.cgi?offset=%d\"><b>%s</b></a> ";
	if (offset - limit >= 0)
		fprintf(stdout, link, offset - limit, "&lt;&lt; Next");
	if (offset > 0)
		fprintf(stdout, link, 0, "[Back to Top]");
	if (results == limit)
		fprintf(stdout, link, offset + limit, "Prev &gt;&gt;");
}

void display_posts(sqlite3 *db, int limit, int offset)
{
	/* sanity check */
	limit = (limit < 0) ? -limit : limit;
	offset = (offset < 0) ? -offset : offset;
	if (offset % limit) /* display in multiples of the limit only */
	{
		fprintf(stdout, "<h2>Please request offsets in multiples of %d only.</h2>", limit);
		return;
	}
	struct resource res;
	char select[200];
	static const char *sql = "SELECT * FROM comments ORDER BY id DESC LIMIT %d OFFSET %d;";
	sprintf(select, sql, limit, offset);
	res_fetch_specific(db, &res, select, limit);
	if (!res.count)
		fprintf(stdout, "<h2>There aren't that many posts here.</h2>");
	else
	{
		long total = db_total(db);
		long pages = (float) total / limit + 1;
		long current = (float) offset / limit + 1;
		fprintf(stdout, "<br/>Page %ld of %ld", current, pages);
		display_controls(limit, offset, res.count);
		res_display(&res);
		display_controls(limit, offset, res.count);
	}
	res_freeup(&res);
}

int post_cooldown(sqlite3 *db, const char *ip_addr)
{
	/* calculates cooldown timer expressed in seconds remaining */
	const long current_time = time(NULL);
	const long delta = current_time - COOLDOWN_SEC;
	long timer = COOLDOWN_SEC;
	struct resource res;
	char sql[100]; /* fetch newest posts only */
	sprintf(sql, "SELECT * FROM comments WHERE time > %ld;", delta);
	res_fetch(db, &res, sql);
	int i;
	for (i = res.count - 1; i >= 0; i--)
	{
		if (!strcmp(ip_addr, res.arr[i].ip))
		{
			timer = current_time - res.arr[i].time;
			break;
		}
	}
	res_freeup(&res);
	return COOLDOWN_SEC - timer;
}

int get_option(const char *get_str, const char *option)
{
	/* returns requested numerical option if found */
	int opt = 0;
	if (get_str)
	{
		char *get = strdup(get_str);
		query_t query; /* parse GET options */
		query_parse(&query, get);
		char *val = query_search(&query, option);
		char *n = val;
		while (*++n && val) /* is numerical? */
			if (*n < '0' || *n > '9')
				val = NULL;
		opt = (!val) ? 0 : atoi(val);
		query_free(&query);
		free(get);
	}
	return (opt < 0) ? -opt : opt;
}

int main(void)
{
	clock_t start = clock();
	srand(time(NULL) + clock());
	sqlite3 *db;
	fprintf(stdout, "Content-type: text/html\n\n");
	if (sqlite3_open_v2(database_loc, &db, 2, NULL))
	{
		fprintf(stdout, "<h2>[!] Database missing!\nRun 'init.sh' to continue.</h2>\n");
		return 1;
	}
	fprintf(stdout, "%s", header); /* header / post box */

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

		struct comment cm; /* compose a new post */
		cm.text = query_search(&query, "comment");
		cm.ip = getenv("REMOTE_ADDR");
		if (cm.text)
		{
			utf8_rewrite(cm.text); /* ASCII => UTF-8 */
			int charcount = utf8_charcount(cm.text);
			xss_sanitize(&cm.text); /* prevent XSS attempts */
			int timer = post_cooldown(db, cm.ip); /* is this user spamming? */

			if (timer > 0)
				fprintf(stdout, "<h1>Please wait %d more seconds before posting again.</h1>%s", timer, refresh);
			else if (charcount > MAX_LENGTH || charcount < 1)
				fprintf(stdout, "<h1>Post rejected.</h1>%s", refresh);
			else
			{
				db_insert(db, &cm); /* insert */
				fprintf(stdout, "<h1>Post submitted!</h1>%s", refresh);
			}
			/* page should refresh shortly */
		}
		query_free(&query);
	}
	else /* no POST */
	{
		/* check GET options for offset value */
		int offset = get_option(getenv("QUERY_STRING"), "offset");
		display_posts(db, POSTS_PER_PAGE, offset);
		float delta = ((float) (clock() - start) / CLOCKS_PER_SEC) * 1000;
		fprintf(stdout, "<br/><sub>Page generated in %.3fms</sub>", delta);
	}
	fprintf(stdout, "%s", footer);
	sqlite3_close(db);
	return 0;
}
