#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "query.h"
#include "utf8.h"

/* board.c
 * database functionality and general user interface
 */

/* global constants */
const int NAME_MAX_LENGTH = 75;
const int COMMENT_MAX_LENGTH = 2000;
const int COOLDOWN_SEC = 30;
const int POSTS_PER_PAGE = 50;
const char *default_name = "Anonymous";
const char *database_loc = "db/database.sqlite3";

/* software name */
const char *ident = "akari-bbs";
const int rev = 9; /* revision no. */

/* html */

const char *refresh = "<meta http-equiv=\"refresh\" content=\"2\" />";
const char *header =
"<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Akari BBS</title>"
"<link rel=\"stylesheet\" type=\"text/css\" href=\"css/style.css\" />"
"<script src=\"js/script.js\"></script></head>"
"<body><a href=\"\\\"><div class=\"header\"><div id=\"logo\">Akari BBS</div></a><div id=\"postbox\">"
"[!!] Post a comment!: <form action=\"board.cgi\" method=\"post\" id=\"postform\">Name (optional):"
"<input type=\"text\" name=\"name\" size=\"25\" maxlength=\"75\" placeholder=\"Anonymous\">"
"<input type=\"submit\" value=\"Submit\"><br/>"
"<textarea id=\"pBox\" name=\"comment\" rows=\"3\" cols=\"52\" maxlength=\"2000\" "
"placeholder=\"2000 characters max.\" form=\"postform\"></textarea></form></div>"
"<div class=\"reset\"></div>";
const char *footer = "</body></html>";

/* todo:
	- >>9829 post linking
	- change offset= to page=
	- calculate page location based on post number
	- greentexting
 */

/* requested features:
	- spoiler tags
	- code tags
	- email field / sage
	- auto-updating (javascript?)
	- image attachments
 */

/* generic post containers */

struct comment {
	long id;
	long time;
	char *ip;
	char *name; /* optional */
	char *trip; /* optional */
	char *text;
};

struct resource {
	long count;
	struct comment *arr;
};

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

int db_transaction(sqlite3 *db, const char *sql)
{
	/* execute arbitrary SQL on the database */
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	int err = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return err;
}

void db_insert(sqlite3 *db, struct comment *cm)
{
	/* insert new post into database */
	cm->id = db_total(db) + 1;
	cm->time = time(NULL);
	char *insert = (char *) malloc(sizeof(char) * strlen(cm->text) + 1000);
	static const char *mandatory =
	"INSERT INTO comments(id, time, ip, text) VALUES(%ld, %ld, \"%s\", \"%s\");";
	sprintf(insert, mandatory, cm->id, cm->time, cm->ip, cm->text);
	db_transaction(db, insert);
	if (cm->name) /* insert optional fields if any */
	{
		const char *name = "UPDATE comments SET name = \"%s\" WHERE id = %ld;";
		sprintf(insert, name, cm->name, cm->id);
		db_transaction(db, insert);
	}
	if (cm->trip)
	{
		const char *trip = "UPDATE comments SET trip = \"%s\" WHERE id = %ld;";
		sprintf(insert, trip, cm->trip, cm->id);
		db_transaction(db, insert);
	}
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
		res->arr[i].name = strdup((char *) sqlite3_column_text(stmt, 3));
		res->arr[i].trip = strdup((char *) sqlite3_column_text(stmt, 4));
		res->arr[i].text = strdup((char *) sqlite3_column_text(stmt, 5));
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
		res->arr[i].name = strdup((char *) sqlite3_column_text(stmt, 3));
		res->arr[i].trip = strdup((char *) sqlite3_column_text(stmt, 4));
		res->arr[i].text = strdup((char *) sqlite3_column_text(stmt, 5));
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
	/* display SQL resulted stored in memory */
	unsigned i;
	for (i = 0; i < res->count; i++)
	{
		/* use default name if not provided */
		const char *name = (!res->arr[i].name) ? default_name : res->arr[i].name;
		const long id = res->arr[i].id; /* post id */
		fprintf(stdout, "<div class=\"pContainer\" id=\"p%ld\">", id);
		fprintf(stdout, "<span class=\"pName\">%s</span> ", name);
		if (res->arr[i].trip) /* optional field */
			fprintf(stdout, "<span class=\"pTrip\">%s</span> ", res->arr[i].trip);
		char time_str[100]; /* human readable date */
		struct tm *ts = localtime((time_t *) &res->arr[i].time);
		strftime(time_str, 100, "%a, %m/%d/%y %I:%M:%S %p", ts);
		fprintf(stdout, "<span class=\"pDate\">%s</span> ", time_str);
		fprintf(stdout, "<span class=\"pId\">"); /* post quote scripting */
		fprintf(stdout, "<a href=\"#p%ld\">No.</a>", id);
		fprintf(stdout, "<a href=\"javascript:quote('%ld');\">%ld</a>", id, id);
		fprintf(stdout, "</span>");
		fprintf(stdout, "<div class=\"pComment\">%s</div>", res->arr[i].text);
		fprintf(stdout, "</div><br/>");
	}
}

void res_freeup(struct resource *res)
{
	if (res->count)
	{
		unsigned i;
		for (i = 0; i < res->count; i++)
		{
			if (res->arr[i].name)
				free(res->arr[i].name);
			if (res->arr[i].trip)
				free(res->arr[i].trip);
			free(res->arr[i].ip);
			free(res->arr[i].text);
		}
		free(res->arr);
	}
}

void display_controls(int limit, int offset, int results)
{
	/* display controls for the user */
	fprintf(stdout, "<div class=\"nav\">");
	static const char *link =" <a href=\"board.cgi?offset=%d\"><b>%s</b></a> ";
	if (offset - limit >= 0)
		fprintf(stdout, link, offset - limit, "&lt;&lt; Next");
	if (offset > 0)
		fprintf(stdout, link, 0, "[Back to Top]");
	if (results == limit)
		fprintf(stdout, link, offset + limit, "Prev &gt;&gt;");
	fprintf(stdout, "</div>");
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

		/* note:
		 * cm strings stored in query container
		 * or as static data, do not free them
		 */
		struct comment cm; /* compose a new post */
		cm.name = query_search(&query, "name"); /* name and/or tripcode */
		if (cm.name)
			strip_whitespace(utf8_rewrite(cm.name));
		cm.trip = (!cm.name) ? NULL : tripcode_hash(tripcode_pass(&cm.name));
		cm.text = query_search(&query, "comment"); /* comment body */
		if (cm.text)
			strip_whitespace(utf8_rewrite(cm.text));
		cm.ip = getenv("REMOTE_ADDR");
		if (cm.text)
		{
			int name_count = (!cm.name) ? 0 : utf8_charcount(cm.name);
			int text_count = utf8_charcount(cm.text);
			if (cm.name) /* prevent XSS attempts */
				xss_sanitize(&cm.name);
			xss_sanitize(&cm.text);
			int timer = post_cooldown(db, cm.ip); /* is this user spamming? */

			if (timer > 0)
				fprintf(stdout, "<h1>Please wait %d more seconds before posting again.</h1>%s", timer, refresh);
			else if (name_count > NAME_MAX_LENGTH || text_count > COMMENT_MAX_LENGTH || text_count < 1)
				fprintf(stdout, "<h1>Post rejected.</h1>%s", refresh);
			else
			{
				db_insert(db, &cm); /* insert */
				fprintf(stdout, "<h1>Post submitted!</h1>%s", refresh);
			}
			/* page should refresh shortly */
		}
		else
			fprintf(stdout, "<h1>You cannot post a blank comment.</h1>%s", refresh);
		query_free(&query);
	}
	else /* no POST */
	{
		/* check GET options for offset value */
		int offset = get_option(getenv("QUERY_STRING"), "offset");
		display_posts(db, POSTS_PER_PAGE, offset);
		float delta = ((float) (clock() - start) / CLOCKS_PER_SEC) * 1000;
		fprintf(stdout, "<br/><sub>%s rev. %d</sub> ", ident, rev);
		if (delta) fprintf(stdout, "<sub>-- completed in %.3fms</sub>", delta);
	}
	fprintf(stdout, "%s", footer);
	sqlite3_close(db);
	return 0;
}
