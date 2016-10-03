#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "query.h"
#include "utf8.h"
#include "substr.h"

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

/*
 * board.c
 * end user interface and database functionality
 */

/* global constants */
const int NAME_MAX_LENGTH = 75;
const int COMMENT_MAX_LENGTH = 2000;
const int COOLDOWN_SEC = 30;
const int POSTS_PER_PAGE = 50;
const char *default_name = "Anonymous";
const char *database_loc = "db/database.sqlite3";

/* banners */
const int BANNER_COUNT = 625;
const char *banner_loc = "img/banner";

/* software name */
const char *repo_url = "https://github.com/microsounds/akari-bbs";
const char *ident = "akari-bbs";
const int rev = 14; /* revision no. */

/* html */

const char *const header[] = {
/* head */
"<!DOCTYPE html>"
"<html lang=\"en-US\">"
"<head>"
	"<title>Akari BBS</title>"
	"<meta charset=\"UTF-8\" />"
	"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />"
	"<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"img/favicon.ico\" />"
	"<link rel=\"stylesheet\" type=\"text/css\" href=\"css/style.css\" />"
	"<script src=\"js/script.js\"></script>"
"</head>",

/* banner format string */
"<body>"
	"<div class=\"header\">"
		"<a href=\"/\"><img id=\"banner\" src=\"%s/%d.png\" alt=\"Akari BBS\">"
			"<div id=\"bannertext\">Akari BBS</div>"
		"</a>",

/* post box */
		"<div id=\"postbox\">"
			"[!!] Post a comment!:"
			"<form action=\"board.cgi\" method=\"post\" id=\"postform\">"
				"Name (optional):"
				"<input type=\"text\" name=\"name\" size=\"25\" maxlength=\"75\" placeholder=\"Anonymous\">"
				"<input type=\"submit\" value=\"Submit\"><br/>"
				"<textarea form=\"postform\" id=\"pBox\" name=\"comment\" rows=\"3\" cols=\"52\" "
				"maxlength=\"2000\" placeholder=\"2000 characters max.\"></textarea>"
			"</form>"
			"<span class=\"help\" style=\"float:right;\">"
				"<noscript>Please enable <b>JavaScript</b> for the best user experience!</br></noscript>"
				"Supported: <b>Tripcodes</b> "
				"<a class=\"tooltip\" href=\"#\" msg=\"Enter your name as &quot;name#password&quot; to generate a tripcode.\">[?]</a>"
				", <b>Markup</b> "
				"<a class=\"tooltip\" href=\"#\" msg=\"Supported markup: [spoiler], [code]. Implicit end tags are added if missing.\">[?]</a>"
			"</span>"
		"</div>"
	"</div>"
	"<div class=\"reset\"></div>"
};
const char *footer = "</body></html>";
const char *refresh = "<meta http-equiv=\"refresh\" content=\"2\" />";

/* TODO:
	- insert post number into localStorage to emulate (You) quotes
	- decouple board.cgi from post.cgi and add an admin panel with login
	- db_fetch_parent() to provide correct quotelinks in the future
	- gzip compression (????)
 */

/* requested features:
	- individual threads
	- boards
	- spoiler tags
	- code tags
	- email field / sage
	- auto-updating (javascript?)
	- image attachments
	- REST api
		- /board/post/12345 - post mode, with link to parent thread
		- /board/thread/12340 - thread mode
 */

/* post containers */

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

int db_insert(sqlite3 *db, struct comment *cm)
{
	/* insert new post into database */
	cm->id = db_total(db) + 1;
	cm->time = time(NULL);
	char *insert = (char *) malloc(sizeof(char) * strlen(cm->text) + 1000);
	static const char *mandatory =
	"INSERT INTO comments(id, time, ip, text) VALUES(%ld, %ld, \"%s\", \"%s\");";
	sprintf(insert, mandatory, cm->id, cm->time, cm->ip, cm->text);
	int err = db_transaction(db, insert);
	if (cm->name) /* insert optional fields if any */
	{
		const char *name = "UPDATE comments SET name = \"%s\" WHERE id = %ld;";
		sprintf(insert, name, cm->name, cm->id);
		err = db_transaction(db, insert);
	}
	if (cm->trip)
	{
		const char *trip = "UPDATE comments SET trip = \"%s\" WHERE id = %ld;";
		sprintf(insert, trip, cm->trip, cm->id);
		err = db_transaction(db, insert);
	}
	free(insert);
	return err;
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
	/* fetch requested SQL results into memory
	 * this automated version is intended for COUNT(*) compatible statements
	 */
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
	/* fetch requested SQL results into memory
	 * this manual version is intended for LIMIT/OFFSET statements ONLY
	 */
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

/* NOTES:
 * on higher optmization levels (-O2), GCC will introduce false positive
 * Valgrind errors which make it seem like the value of *str isn't being
 * updated immediately after realloc, allowing the function to peek
 * backwards into heap memory.
 * eg. "Invalid read of size 4"
 *     "Address 0xABCDEF is 72 bytes inside a block of size 73 alloc'd"
 * Program still exhibits expected behavior regardless of optimizatons.
 */

char *enquote_comment(char **loc, const long id)
{
	/* rewrite string with quote markup and
	 * generate client-side javascript functionality
	 * - function 'popup(self, request, hover)' requires post id
	 */
	const char *gt = escape('>'), *nl = escape('\n'); /* escape codes */
	static const char *const quote[] = {
		"<span class=\"quote\">", "</span>"
	};
	static const char *const linkquote[] = {
		"<a class=\"linkquote\" href=\"#p%s\" "
		"onMouseOver=\"popup('p%ld','p%s',1)\" "
		"onMouseOut=\"popup('p%ld','p%s',0)\" "
		"onClick=\"popup('p%ld','p%s',0)\">",
		"</a>"
	};
	char *str = *loc;
	if (!strstr(str, gt)) /* no '>' found */
		return str;
	unsigned i;
	for (i = 0; str[i]; i++)
	{
		unsigned length = strlen(str);
		char *seek = strstr(&str[i], gt); /* seek to next '>' or to end */
		i = (!seek) ? length : (unsigned) (seek - str);
		if (length < i + max(strlen(gt), strlen(nl))) /* bounds check */
			break;
		/* '>>' linkquote */
		else if (!memcmp(&str[i + strlen(gt)], gt, strlen(gt)))
		{
			/* linkquote post number cannot begin with leading zero
			 * and shouldn't be longer than 20 digits
			 */
			char c = str[i + (strlen(gt) * 2)]; /* peek ahead */
			if (c >= '1' && c <= '9')
			{
				unsigned j = i + (strlen(gt) * 2);
				unsigned k = 0;
				const unsigned N_MAX = 20;
				char num[N_MAX]; /* get post number */
				char tag[300]; /* tag buffer */
				while (str[j] >= '0' && str[j] <= '9' && k < N_MAX)
					num[k++] = str[j++];
				num[k] = '\0'; /* create tag */
				sprintf(tag, linkquote[0], num, id, num, id, num, id, num);
				unsigned offset_a = strlen(tag); /* part 1 */
				str = (char *) realloc(str, strlen(str) + offset_a + 1);
				memmove(&str[i+offset_a], &str[i], strlen(&str[i]) + 1);
				memcpy(&str[i], tag, offset_a);
				j += offset_a; /* new length adjustment */

				/* index 'j' now points to the right of the post number */
				unsigned offset_b = strlen(linkquote[1]); /* part 2 */
				str = (char *) realloc(str, strlen(str) + offset_b + 1);
				memmove(&str[j+offset_b], &str[j], strlen(&str[j]) + 1);
				memcpy(&str[j], linkquote[1], offset_b);
				i += offset_a + offset_b;
			}
			else
				i += (strlen(gt) * 2);
		}
		/* '>' quote */
		else if (&str[0] == &str[i] || /* conditional bounds checking */
		!memcmp(&str[i - ((i < strlen(nl)) ? 0 : strlen(nl))], nl, strlen(nl)))
		{
			/* don't seek backwards if too close to the start of the array
			 * '>' quotes are only valid at the start of a new line
			 */
			unsigned offset_a = strlen(quote[0]); /* part 1 */
			str = (char *) realloc(str, strlen(str) + offset_a + 1);
			memmove(&str[i+offset_a], &str[i], strlen(&str[i]) + 1);
			memcpy(&str[i], quote[0], offset_a);

			/* seek to the next newline or to end */
			char *pos = strstr(&str[i], nl); /* part 2 */
			unsigned j = (!pos) ? strlen(str) : (unsigned) (pos - str);
			unsigned offset_b = strlen(quote[1]);
			str = (char *) realloc(str, strlen(str) + offset_b + 1);
			memmove(&str[j+offset_b], &str[j], strlen(&str[j]) + 1);
			memcpy(&str[j], quote[1], offset_b);
			i += offset_a + offset_b;
		}
	}
	*loc = str;
	return str;
}

char *format_comment(char **loc)
{
	/* replace matching [tags] with corresponding markup with exceptions:
	 * 1. nesting:
	 *     - nesting of [tags] is fine
	 * 2. auto-complete:
	 *     - implicit [/tag] added to end of string if none found
	 * 3. [code] blocks:
	 *     - nesting of other tags within code blocks is not allowed
	 *     - must be processed last for this reason
	 */
	static const char *const markup[] = {
		[SPOILER_L] = "<span class=\"spoiler\">", [SPOILER_R] = "</span>",
		[CODE_L] = "<div class=\"codeblock\">", [CODE_R] = "</div>"
	};
	/* some assumptions about the format tag system */
	static_assert(static_size(markup) == SUPPORTED_TAGS); /* size check */
	static_assert((SUPPORTED_TAGS % 2) == 0); /* tag count must be even */
	static_assert((SUPPORTED_TAGS - 2) == CODE_L); /* [code] must come last */

	char *str = *loc;
	struct substr *extract = substr_extract(str, fmt[CODE_L], fmt[CODE_R]);
	unsigned i, j, k, l;
	for (i = 0; i < SUPPORTED_TAGS; i += 2)
	{
		if (i == CODE_L) /* restore [code] tag regions */
			substr_restore(extract, str);
		for (j = 0; str[j]; j++)
		{
			char *from = strstr(&str[j], fmt[i]); /* left tag */
			j = (!from) ? strlen(str) : (unsigned) (from - str);
			if (!str[j])
				break;
			k = strstr(str, fmt[i+1]) - str;
			if (j >= k) /* malformed tag order */
				break;
			l = strlen(fmt[i]); /* overlap */
			memmove(&str[j], &str[j+l], strlen(&str[j+l]) + 1);
			unsigned offset_a = strlen(markup[i]);
			str = (char *) realloc(str, strlen(str) + offset_a + 1);
			memmove(&str[j+offset_a], &str[j], strlen(&str[j]) + 1);
			memcpy(&str[j], markup[i], offset_a);

			char *to = strstr(&str[j], fmt[i+1]); /* right tag */
			k = (!to) ? strlen(str) : (unsigned) (to - str);
			l = strlen(fmt[i+1]); /* overlap */
			if (to) /* overlap only if tag found */
				memmove(&str[k], &str[k+l], strlen(&str[k+l]) + 1);
			unsigned offset_b = strlen(markup[i+1]);
			str = (char *) realloc(str, strlen(str) + offset_b + 1);
			memmove(&str[k+offset_b], &str[k], strlen(&str[k]) + 1);
			memcpy(&str[k], markup[i+1], offset_b);
		}
	}
	*loc = str;
	return str;
}

void res_display(struct resource *res)
{
	/* display SQL results stored in memory */
	unsigned i;
	for (i = 0; i < res->count; i++)
	{
		/* use default name if not provided */
		const char *name = (!res->arr[i].name) ? default_name : res->arr[i].name;
		const long id = res->arr[i].id; /* post id */
		enquote_comment(&res->arr[i].text, id); /* reformat comment string */
		const char *comment = format_comment(&res->arr[i].text);
		fprintf(stdout, "<div class=\"pContainer\" id=\"p%ld\">", id);
		fprintf(stdout, "<span class=\"pName\">%s</span> ", name);
		if (res->arr[i].trip) /* optional field */
			fprintf(stdout, "<span class=\"pTrip\">%s</span> ", res->arr[i].trip);
		char time_str[100]; /* human readable date */
		struct tm *ts = localtime((time_t *) &res->arr[i].time);
		strftime(time_str, 100, "%a, %m/%d/%y %I:%M:%S %p", ts);
		fprintf(stdout, "<span class=\"pDate\">%s</span> ", time_str);
		fprintf(stdout, "<span class=\"pId\">"); /* post link scripting */
		fprintf(stdout, "<a href=\"#p%ld\" onClick=\"highlight('p%ld')\" title=\"Link to post\">No.</a>", id, id);
		fprintf(stdout, "<a href=\"javascript:quote('%ld');\" title=\"Reply to post\">%ld</a>", id, id);
		fprintf(stdout, "</span>");
		fprintf(stdout, "<div class=\"pComment\">%s</div>", comment);
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
		while (val && *++n) /* is numerical? */
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
	srand(time(NULL));
	sqlite3 *db;
	fprintf(stdout, "Content-type: text/html\n\n");
	if (sqlite3_open_v2(database_loc, &db, 2, NULL))
	{
		fprintf(stdout, "<h2>[!] Database missing!\nRun 'init.sh' to continue.</h2>\n");
		return 1;
	}
	fprintf(stdout, "%s", header[0]); /* head */
	fprintf(stdout, header[1], banner_loc, rand() % BANNER_COUNT); /* banner */
	fprintf(stdout, "%s", header[2]); /* post box */

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
			int timer = post_cooldown(db, cm.ip); /* user flooding? */

			if (spam_filter(cm.text)) /* user spamming? */
				fprintf(stdout, "<h1>This post is spam, please rewrite it.</h1>%s", refresh);
			else if (timer > 0)
				fprintf(stdout, "<h1>Please wait %d more seconds before posting again.</h1>%s", timer, refresh);
			else if (name_count > NAME_MAX_LENGTH || text_count > COMMENT_MAX_LENGTH || text_count < 1)
				fprintf(stdout, "<h1>Post rejected.</h1>%s", refresh);
			else
			{
				int err = db_insert(db, &cm); /* insert */
				if (err == SQLITE_READONLY)
					fprintf(stdout, "<h1>Post failed. DB is write-protected.</h1>%s", refresh);
				else
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

		/* footer */
		float delta = ((float) (clock() - start) / CLOCKS_PER_SEC) * 1000;
		fprintf(stdout, "<br/><div class=\"footer\">%s rev. %d ", ident, rev);
		#ifndef NDEBUG
			fprintf(stdout, "dev-build ");
		#endif
		fprintf(stdout, "<a href=\"%s\">[github]</a> ", repo_url);
		if (delta) fprintf(stdout, "-- completed in %.3fms", delta);
		fprintf(stdout, "</div>");
	}
	fprintf(stdout, "%s", footer);
	fflush(stdout);
	sqlite3_close(db);
	return 0;
}
