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
#include "macros.h"

/*
 * [core functionality]
 * board.c
 * messageboard user interface
 */

/*
	todo:
		URL rewriting
		index mode
		thread mode
		peek mode
	>>12345
	if requested ID is not found in the thread, search through /api/12345 and display that
 */

struct parameters {
	enum {
		HOMEPAGE,
		INDEX_MODE,
		THREAD_MODE,
		NOT_FOUND,
		REDIRECT
	} mode;
	char *board_id;
	long thread_id;
	long parent_id; /* redirection */
	long page_no;
};

const char *const global_template[] = {
	/* header */
	"<!DOCTYPE html>"
	"<html lang=\"en-US\">"
	"<head>"
		"<title>%s</title>" /* dynamically generate title in the future */
		"<meta charset=\"UTF-8\" />"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />"
		"<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/img/favicon.ico\" />"
		"<link rel=\"stylesheet\" type=\"text/css\" href=\"/css/style.css\" />"
		"<script src=\"/js/script.js\"></script>"
	"</head>"
	"<body>",
	/* footer */
		"<br/>"
		"<div class=\"footer\" style=\"text-align:center;\">"
			"Powered by %s rev.%d/db-%d %s<br/>"
			"All trademarks and copyrights on this page are owned by their respective parties. "
			"Comments are owned by the Poster."
		"</div>"
	"</body>"
	"</html>"
};

/* TODO:
	- html goes in separate templates.h / templates.c
	- insert post number into localStorage to emulate (You) quotes
	- add admin panel w/ login
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
	- REST api??
		- /board/peek/12345 - peek mode, with link to parent thread
		- /board/thread/12340 - thread mode
 */

/* NOTES:
 * on higher optmization levels (-O2), GCC will introduce false positive
 * Valgrind errors which make it seem like the value of *str isn't being
 * updated immediately after realloc, allowing the function to peek
 * backwards into heap memory.
 * eg. "Invalid read of size 4"
 *     "Address 0xABCDEF is 72 bytes inside a block of size 73 alloc'd"
 * Program still exhibits expected behavior regardless of optimizations.
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
	/* print all post data stored in memory */
	unsigned i;
	for (i = 0; i < res->count; i++)
	{
		/* use default name if not provided */
		const char *name = (!res->arr[i].name) ? DEFAULT_NAME : res->arr[i].name;
		const long id = res->arr[i].id; /* post id */
		enquote_comment(&res->arr[i].comment, id); /* reformat comment string */
		const char *comment = format_comment(&res->arr[i].comment);
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

void display_controls(int limit, int offset, int results)
{
	/* display controls for the user */
	fprintf(stdout, "<div class=\"nav\">");
	static const char *link =" <a href=\"/board.cgi?offset=%d\"><b>%s</b></a> ";
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
	db_resource_fetch_specific(db, &res, select, limit);
	if (!res.count)
		fprintf(stdout, "<h2>There aren't that many posts here.</h2>");
	else
	{
		long total = 0;
		long pages = (float) total / limit + 1;
		long current = (float) offset / limit + 1;
		fprintf(stdout, "<br/>Page %ld of %ld", current, pages);
		display_controls(limit, offset, res.count);
		res_display(&res);
		display_controls(limit, offset, res.count);
	}
	db_resource_free(&res);
}

void display_headers(const struct board *list, const char *board_id)
{
	/* top-most headers and rotating banners */
	static const char *const headers[] = {
		"<div id=\"boardtitle\"><b>/%s/</b> - %s</div>",
		"<div class=\"header\">"
			"<a href=\"/\"><img id=\"banner\" src=\"%s/%u.png\" alt=\"%s\">"
			"<div id=\"bannertext\">%s</div>"
		"</a>"
	};
	unsigned i, sel = rand() % BANNER_COUNT;
	for (i = 0; i < list->count; i++)
		if (!strcmp(list->id[i], board_id))
			break;
	fprintf(stdout, headers[0], board_id, list->name[i]);
	fprintf(stdout, headers[1], BANNER_LOC, sel, IDENT_FULL, IDENT_FULL);
}

void display_boardlist(const struct board *list, const char *title)
{
	/* provide links to every available board
	 * accepts an optional title field
	 */
	static const char *const navi[] = {
		"<div>",
		"<span class=\"navi boardlist\">%s[ ",
		"<a href=\"/?board=%s\" title=\"%s\">%s</a>",
		" ]</span>",
		"</div>"
	};
	unsigned i;
	for (i = 0; i < 2; i++)
		fprintf(stdout, navi[i], (!title) ? "" : title);
	for (i = 0; i < list->count; i++)
	{
		fprintf(stdout, navi[2], list->id[i], list->name[i], list->id[i]);
		if (i != list->count - 1)
			fprintf(stdout, " / ");
	}
	fprintf(stdout, navi[3]);

	/* insert repo link */
	static const char *repo = "<a href=\"%s\" title=\"%s\">%s</a>";
	for (i = 1; i < static_size(navi); i++)
	{
		if (i == 2)
			fprintf(stdout, repo, REPO_URL, LICENSE, "github");
		else
			fprintf(stdout, navi[i], "");
	}
}

void display_postform(int mode, const char *board_id, const long thread_id)
{
	/* generate post submission form */
	static const char *const postform[] = {
		"<div id=\"postbox\">"
		"<table class=\"form\" cellspacing=\"0\">"
			"<form action=\"%s\" method=\"post\" id=\"postform\">",
		/* mode-dependent options */
			"<div class=\"formtitle indexmode\">[!!] Index Mode: Start a New Thread!</div>",
			"<div class=\"formtitle threadmode\">[!!] Thread Mode: Reply to Thread No.%ld</div>",
		/* hidden fields */
			"<input type=\"hidden\" name=\"board\" value=\"%s\">"
			"<input type=\"hidden\" name=\"mode\" value=\"%s\">"
			"<input type=\"hidden\" name=\"parent\" value=\"%ld\">",
		/* visible fields */
			"<tr>"
				"<td><div class=\"desc\">Name</div></td>"
				"<td><input class=\"field\" type=\"text\" name=\"name\" maxlength=\"%d\" placeholder=\"%s\"></td>"
			"</tr>"
			"<tr>"
				"<td><div class=\"desc\">Options</div></td>"
				"<td><input class=\"field\" type=\"text\" name=\"options\" maxlength=\"%d\"></td>"
			"</tr>"
			"<tr>"
				"<td><div class=\"desc\">Subject</div></td>"
				"<td>"
					"<input class=\"field\" type=\"text\" name=\"subject\" maxlength=\"%d\">"
					"<input type=\"submit\" value=\"Submit\">"
				"</td>"
			"</tr>"
			"<tr>"
				"<td><div class=\"desc\">Comment</div></td>"
				"<td>"
					"<textarea class=\"field\" form=\"postform\" style=\"width:98%\" id=\"pBox\" name=\"comment\" "
					"rows=\"4\" maxlength=\"%d\" placeholder=\"Limit %d characters\"></textarea>"
				"</td>"
			"</tr>"
			"</form>"
		"</table>",
		/* tooltips */
		"<span class=\"help right\">"
			"<noscript>Please enable <b>JavaScript</b> for the best user experience!</br></noscript>"
			"Supported: "
			"<b>Tripcodes</b> "
			"<a class=\"tooltip\" href=\"#\" msg=\"Enter your name as &quot;name#password&quot; to generate a tripcode.\">[?]</a>, "
			"<b>Markup</b> "
			"<a class=\"tooltip\" href=\"#\" msg=\"Supported markup: [spoiler], [code]. Implicit end tags are added if missing.\">[?]</a>"
		"</span>"
		"</div>"
		"<div class=\"reset\"></div><br/>"
	};

	fprintf(stdout, postform[0], SUBMIT_SCRIPT);
	if (mode == THREAD_MODE)
	{
		fprintf(stdout, postform[2], thread_id);
		fprintf(stdout, postform[3], board_id, "reply", thread_id);
	}
	else /* INDEX_MODE */
	{
		fprintf(stdout, postform[1]);
		fprintf(stdout, postform[3], board_id, "thread", 0);
	}
	/* client-side character limits */
	fprintf(stdout, postform[4], NAME_MAX_LENGTH, DEFAULT_NAME,
		OPTIONS_MAX_LENGTH, SUBJECT_MAX_LENGTH, COMMENT_MAX_LENGTH, COMMENT_MAX_LENGTH);
	fprintf(stdout, postform[5]);
}

void homepage_mode(const struct board *list)
{
	static const char *masthead =
		"<div class=\"masthead center\">"
			"<div class=\"header\">"
				"<a href=\"/\"><img id=\"banner\" src=\"%s/%d.png\" alt=\"%s\"></a>"
				"<div class=\"title\">%s</div>"
				"<div class=\"footer\">%s</div>"
				"<div class=\"footer\">Powered by %s rev.%d/db-%d</div>"
				"<span class=\"footer\">%s</span>"
				"<span class=\"navi boardlist\">[<a href=\"%s\">github</a>]</span>"
			"</div>"
		"</div>"
		"<br/>";

	static const char *const directory[] = {
		"<div class=\"directory center\">"
			"<div class=\"dirheading\">Textboards</div>"
			"<div class=\"line\"></div>",
		/* listing */
			"<div class=\"cell\">"
				"<div class=\"navi dirname\"><a href=\"/?board=%s\">%s</a></div>"
				"<div class=\"navi dirdesc\">%s</div>"
			"</div>",
		"</div><br/>"
	};
	display_boardlist(list, "Boards: ");
	fprintf(stdout, masthead, BANNER_LOC, rand() % BANNER_COUNT, IDENT_FULL,
		    IDENT_FULL, TAGLINE, IDENT, REVISION, DB_VER, LICENSE, REPO_URL);
	unsigned i, j;
	for (i = 0; i < static_size(directory); i++)
	{
		if (i == 1)
			for (j = 0; j < list->count; j++)
				fprintf(stdout, directory[i], list->id[j], list->name[j], list->desc[j]);
		else
			fprintf(stdout, directory[i]);
	}
}

void not_found(const char *refer)
{
	static const char *error =
	"<style>h2 { color: #424242; } a { text-decoration: none; } a:hover { color: red; }</style>"
	"<div class=\"pContainer\">"
		"<span class=\"pName\">%s <i>rev.%d/db-%d</i></span>"
		"<div class=\"pComment\">"
		"<h2>404 - Not Found</h2>"
		"<span>"
			"The requested URL doesn't refer to any existing resource on this server.<br/>"
			"It may have been pruned or deleted."
		"</span>"
		"<br/>"
		"<div>[<a href=\"%s\">Go back</a>]</div>"
	"</div></div>";
	fprintf(stdout, error, IDENT, REVISION, DB_VER, (!refer) ? "/" : refer);
}

/* void index_mode(sqlite3 *db, const char *board_id, const int page) */
void index_mode(sqlite3 *db, struct board *list, struct parameters *params)
{
	display_headers(list, params->board_id);
	display_boardlist(list, NULL);
	display_postform(params->mode, params->board_id, 0);
	return;
}

/*void thread_mode(sqlite3 *db, const char *board_id, const long thread_id) */
void thread_mode(sqlite3 *db, struct board *list, struct parameters *params)
{
	display_headers(list, params->board_id);
	display_boardlist(list, NULL);
	display_postform(params->mode, params->board_id, params->thread_id);
	return;
}

struct parameters get_params(sqlite3 *db, const char *query, struct board *list)
{
	/* obtain settings from GET string */
	unsigned i;
	struct parameters params = { 0 }; /* default option is HOMEPAGE */
	char *query_str = strdup(query);
	if (query_str) /* GET parameter validation */
	{
		query_t query;
		query_parse(&query, query_str);
		free((!query_str) ? NULL : query_str);
		char *board = query_search(&query, "board");
		char *thread = query_search(&query, "thread");
		char *page = query_search(&query, "page");
		if (board)
		{
			for (i = 0; i < list->count; i++)
				if (!strcmp(list->id[i], board)) /* validate board */
					params.board_id = strdup(board);
		}
		if (!params.board_id)
			params.mode = NOT_FOUND; /* general 404 error */
		else
		{
			params.thread_id = atoi_s(thread);
			if (params.thread_id > 0)
			{
				params.parent_id = db_find_parent(db, params.board_id, params.thread_id);
				if (!params.parent_id)
					params.mode = NOT_FOUND; /* doesn't exist */
				else if (params.parent_id != params.thread_id)
					params.mode = REDIRECT;
				else
					params.mode = THREAD_MODE;
			}
			else
			{
				params.mode = INDEX_MODE;
				params.page_no = atoi_s(page);
				if (params.page_no > MAX_ACTIVE_THREADS / THREADS_PER_PAGE)
					params.page_no = 0;
			}
		}
		query_free(&query);
	}
	return params;
}

int main(void)
{
	clock_t start = clock();
	srand(time(NULL));
	int err;
	sqlite3 *db;
	fprintf(stdout, "Content-type: text/html\n");
	if ((err = sqlite3_open_v2(DATABASE_LOC, &db, 1, NULL))) /* read-only mode */
	{
		fprintf(stdout, "<h2>Cannot open database. (e%d: %s)</h2>", err, sqlite3_err[err]);
		return 1;
	}
	struct board list; /* fetch list of valid boards */
	db_board_fetch(db, &list);
	if (!list.count)
	{
		fprintf(stdout, "<h2>No available boards. Please add one.</h2>");
		return 1;
	}
	struct parameters params = get_params(db, getenv("QUERY_STRING"), &list);

	/* HTTP response */
	static const char *const response[] = {
		[REDIRECT] = "301 Moved Permanently",
		[NOT_FOUND] = "404 Not Found"
	};
	fprintf(stdout, "Status: %s\n\n",
		    (!response[params.mode]) ? "200 OK" : response[params.mode]);
	fprintf(stdout, global_template[0], IDENT_FULL); /* head */
	switch (params.mode)
	{
		case HOMEPAGE: homepage_mode(&list); break;
		case INDEX_MODE: index_mode(db, &list, &params); break;
		case THREAD_MODE: thread_mode(db, &list, &params); break;
		case NOT_FOUND: not_found(getenv("HTTP_REFERER")); break; /* goto abort; */
		case REDIRECT:
			fprintf(stdout, "<i>Redirecting to Thread No.%ld...</i>", params.parent_id);
			thread_redirect(params.board_id, params.parent_id, params.thread_id);
			goto abort;
	}

	/* debug */
	fprintf(stdout, "<br/><br/>");
	char *modes[] = { "Homepage", "Index Mode", "Thread Mode", "404 Not Found", "Redirect" };
	fprintf(stdout, "[debug] mode: %s board: %s thread: %ld page: %ld<br/>get string: \"%s\"",
		modes[params.mode], params.board_id, params.thread_id, params.page_no, getenv("QUERY_STRING"));

	/* footer
	 * version info footer with page generation time
	 * this won't work on all systems, eg. VMs
	 */
	float delta = ((float) (clock() - start) / CLOCKS_PER_SEC) * 1000;
	char pageload[100];
	sprintf(pageload, "-- completed in %.3fms.", delta);
	fprintf(stdout, global_template[1], IDENT, REVISION, DB_VER, (!delta) ? "" : pageload);

	abort: fflush(stdout);
	db_board_free(&list);
	sqlite3_close(db);
	return 0;
}
