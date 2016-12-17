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
		stress test thread cycling
		stress test thread pruning and archival
		enquote_comment is corrupting strings at random
		archived thread viewer
		peek mode
		rewrite enquote_comment to get correct thread and link

	>>12345
	if requested ID is not found in the thread, search through /api/12345 and display that
 */

struct parameters {
	enum {
		HOMEPAGE,
		INDEX_MODE,
		THREAD_MODE,
		ARCHIVE_MODE,
		ARCHIVE_VIEWER,
		PEEK_MODE,
		NOT_FOUND,
		REDIRECT
	} mode;
	char *board_id;
	long thread_id;
	long parent_id; /* redirection */
	long page_no;
	long active_threads; /* statistics */
	long archived_threads;
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

/* PRE-REWRITE TODO:
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

char *post_digest(sqlite3 *db, const char *board_id, const long id, unsigned len)
{
	/* returns post preview
	 * non-zero length returns truncated plaintext
	 */
	static const char *sql =
		"SELECT * FROM posts WHERE board_id = \"%s\" AND id = %ld;";
	char *cmd = sql_generate(sql, board_id, id);
	char *dest = NULL;
	struct resource res;
	if (db_resource_fetch(db, &res, cmd))
	{
		struct post *p = &res.arr[0];
		if (len) /* truncated preview */
		{
			char *src = (!p->subject) ? p->comment : p->subject;
			dest = utf8_truncate(src, len);
		}
		else /* formatted rich text */
		{
			char *subj = (!p->subject) ? NULL : sql_generate("<b>%s</b>: ", p->subject);
			dest = (!subj) ? strdup(p->comment) : sql_generate("%s%s", subj, p->comment);
			free(subj);
		}
	}
	db_resource_free(&res);
	free(cmd);
	return dest;
}


char *generate_pagetitle(sqlite3 *db, struct parameters *params, struct board *list)
{
	/* generate short page title
	 * not thread safe, returns pointer to static buffer
	 */
	static const char *const pat[] = {
		[HOMEPAGE] = "%s - Home",
		[INDEX_MODE] = "/%s/ - %s - Page %ld - %s",
		[THREAD_MODE] =	"/%s/ - %s - %s - %s",
		[ARCHIVE_VIEWER] = "/%s/ - Archive - %s",
		[PEEK_MODE] =	"Post No.%ld on /%s/",
		[NOT_FOUND] = "%s - 404 Not Found",
		[REDIRECT] = "%s - 301 Moved Permanently"
	};
	static char buf[1024];
	char *digest; /* thread preview */
	unsigned i = 0; /* board description */
	if (params->board_id)
	{
		for (; i < list->count; i++)
			if (!strcmp(list->arr[i].id, params->board_id))
				break;
	}
	switch (params->mode)
	{
		case HOMEPAGE: sprintf(buf, pat[params->mode], IDENT_FULL); break;
		case INDEX_MODE:
				sprintf(buf, pat[params->mode], params->board_id, list->arr[i].name,
				        params->page_no + 1, IDENT_FULL); break;
		case ARCHIVE_MODE:
		case THREAD_MODE:
				digest = post_digest(db, params->board_id, params->thread_id, 45);
				sprintf(buf, pat[THREAD_MODE], params->board_id, digest,
				        list->arr[i].name, IDENT_FULL);
				free(digest); break;
		case ARCHIVE_VIEWER:
				sprintf(buf, pat[params->mode], params->board_id, IDENT_FULL); break;
		case PEEK_MODE: sprintf(buf, pat[params->mode], params->thread_id,
		                        params->board_id); break;
		case NOT_FOUND:
		case REDIRECT: sprintf(buf, pat[params->mode], IDENT_FULL); break;
	}
	return buf;
}

void display_headers(const struct board *list, const char *board_id)
{
	/* top-most headers and rotating banners */
	static const char *const headers[] = {
		"<div id=\"boardtitle\"><b>/%s/</b> - %s</div>",
		"<div class=\"header\">"
			"<a href=\"/\"><img id=\"banner\" src=\"%s/%u.png\" title=\"%s\">"
			"<div id=\"bannertext\">%s</div>"
		"</a>"
	};
	unsigned i, sel = rand() % BANNER_COUNT;
	for (i = 0; i < list->count; i++)
		if (!strcmp(list->arr[i].id, board_id))
			break;
	fprintf(stdout, headers[0], board_id, list->arr[i].name);
	fprintf(stdout, headers[1], BANNER_LOC, sel, "Go to Homepage", IDENT_FULL);
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
		struct entry *board = &list->arr[i];
		fprintf(stdout, navi[2], board->id, board->name, board->id);
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
		"<div id=\"postbox\">",
		/* postform preamble */
		"<table class=\"form\" cellspacing=\"0\">"
			"<form action=\"%s\" method=\"post\" id=\"postform\">"
			"<input type=\"hidden\" name=\"board\" value=\"%s\">"
			"<input type=\"hidden\" name=\"mode\" value=\"%s\">"
			"<input type=\"hidden\" name=\"parent\" value=\"%ld\">",
		/* mode-dependent options */
			"<div class=\"formtitle indexmode\">[!!] Index Mode: Start a New Thread!</div>",
			"<div class=\"formtitle threadmode\">[!!] Thread Mode: Reply to Thread No.%ld</div>",
			"<div class=\"formtitle archivemode\">[!!] Viewing Archived Thread No.%ld</div>"
			"<h2>You cannot reply to this thread anymore.</h2>",
		/* postform input fields */
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
		"</table>"
		/* tooltips */
		"<span class=\"help right\">"
			"<noscript>Please enable <b>JavaScript</b> for the best user experience!</br></noscript>"
			"Supported: "
			"<b>Tripcodes</b> "
			"<a class=\"tooltip\" href=\"#\" msg=\"Enter your name as &quot;name#password&quot; to generate a tripcode.\">[?]</a>, "
			"<b>Markup</b> "
			"<a class=\"tooltip\" href=\"#\" msg=\"Supported markup: [spoiler], [code]. Implicit end tags are added if missing.\">[?]</a>"
		"</span>",
		"</div>"
		"<div class=\"reset\"></div>"
		"</div><br/>"
	};

	fprintf(stdout, postform[0]);
	switch (mode) /* preamble */
	{
		case INDEX_MODE:
			fprintf(stdout, postform[1], SUBMIT_SCRIPT, board_id, "thread", 0);
			fprintf(stdout, postform[2]); break;
		case THREAD_MODE:
			fprintf(stdout, postform[1], SUBMIT_SCRIPT, board_id, "reply", thread_id);
			fprintf(stdout, postform[3], thread_id); break;
		case ARCHIVE_MODE:
			fprintf(stdout, postform[4], thread_id); goto end;
	}
	/* postform body and character limits */
	fprintf(stdout, postform[5], NAME_MAX_LENGTH, DEFAULT_NAME,
		OPTIONS_MAX_LENGTH, SUBJECT_MAX_LENGTH, COMMENT_MAX_LENGTH, COMMENT_MAX_LENGTH);
	end: fprintf(stdout, postform[6]);
}

void display_navigation(const struct parameters *params, int bottom)
{
	/* display navigation bar */
	static const char *const navi[] = {
		"<div class=\"line\"></div>",
		"<span class=\"navi controls\">",
			"[<a href=\"#bottom\" id=\"top\">Bottom</a>] ",
			"[<a href=\"#top\" id=\"bottom\">Top</a>] ",
		/* thread mode */
			"[<a href=\"%s?board=%s\">Return</a>] ",
			"Thread No.%ld",
		/* index mode */
			"[<a href=\"%s?board=%s&page=%u\">&lt;&lt;</a>] ",
			"[<a href=\"%s?board=%s\">&lt;&lt;</a>] ",
			"[<a href=\"%s?board=%s&page=%u\">&gt;&gt;</a>] ",
			"[<b>%u</b>] ",
			"[<a href=\"%s?board=%s\">%u</a>] ",
			"[<a href=\"%s?board=%s&page=%u\">%u</a>] ",
			"Page %u of %u",
		"</span>"
	};
	/* functionally identical */
	int mode = (params->mode == ARCHIVE_MODE) ? THREAD_MODE : params->mode;
	unsigned i;
	for (i = 0; i < 2; i++)
		fprintf(stdout, navi[i]);
	if (mode == THREAD_MODE) /* return button */
		fprintf(stdout, navi[4], BOARD_SCRIPT, params->board_id);
	fprintf(stdout, (!bottom) ? navi[2] : navi[3]); /* top / bottom */
	unsigned pages = params->active_threads / THREADS_PER_PAGE;
	if (params->active_threads % THREADS_PER_PAGE)
		pages++; /* ceiling */
	unsigned current_page = params->page_no + 1;
	if (mode == INDEX_MODE)
	{
		/* 1-indexed auto pagination
		 * URLs pointing to Page 1 are implicitly omitted
		 */
		if (current_page > 2) /* << */
			fprintf(stdout, navi[6], BOARD_SCRIPT, params->board_id, current_page - 1);
		else if (current_page == 2) /* Page 1 */
			fprintf(stdout, navi[7], BOARD_SCRIPT, params->board_id);
		for (i = 1; i <= pages; i++)
		{
			if (i == current_page)
				fprintf(stdout, navi[9], i);
			else if (i == 1) /* Page 1 */
				fprintf(stdout, navi[10], BOARD_SCRIPT, params->board_id, i);
			else
				fprintf(stdout, navi[11], BOARD_SCRIPT, params->board_id, i, i);
		}
		if (current_page < pages) /* >> */
			fprintf(stdout, navi[8], BOARD_SCRIPT, params->board_id, current_page + 1);
		fprintf(stdout, navi[12], current_page, pages);
	}
	else if (mode == THREAD_MODE) /* thread info */
		fprintf(stdout, navi[5], params->thread_id);
	fprintf(stdout, "%s%s", navi[13], navi[0]);
}

void display_statistics(struct parameters *params, long replies, long thread_id)
{
	/* display thread statistics
	 * INDEX_MODE requires explicit thread id for URL links
	 */
	static const char *const ins[] = {
		"<div class=\"navi controls\">",
		    "\xF0\x9F\x93\x8E", /* Unicode U+1F4CE */
			"%s No repl%s yet. ",
			"%s %ld repl%s. ",
			"%s %ld repl%s, %ld post%s omitted. ",
			"[<a href=\"%s?board=%s&thread=%ld\">Click here</a>] to view.",
		"</div>"
	};
	int mode = params->mode;
	switch (mode) /* functionally identical */
	{
		case ARCHIVE_MODE:
		case PEEK_MODE: mode = THREAD_MODE;
	}
	const char *p1 = (replies == 1) ? "y" : "ies"; /* plurals */
	const char *p2 = (replies == 1) ? "" : "s";
	fprintf(stdout, ins[0]);
	if (replies > THREAD_BUMP_LIMIT)
		fprintf(stdout, "Bump limit reached. ");
	if (!replies)
		fprintf(stdout, ins[2], ins[1], p1);
	else if (mode == INDEX_MODE)
	{
		long omitted = 0;
		if (replies > MAX_REPLY_PREVIEW)
			omitted = replies - MAX_REPLY_PREVIEW;
		if (!omitted)
			fprintf(stdout, ins[3], ins[1], replies, p1);
		else
			fprintf(stdout, ins[4], ins[1], replies, p1, omitted, p2);
		fprintf(stdout, ins[5], BOARD_SCRIPT, params->board_id, thread_id);
	}
	else if (mode == THREAD_MODE)
	{
		if (!replies)
			fprintf(stdout, ins[2], ins[1], p1);
		else
			fprintf(stdout, ins[3], ins[1], replies, p1);
	}
	fprintf(stdout, ins[6]);
}

void display_resource(struct resource *res, int mode, int offset)
{
	/* print all post data stored in post container
	 * starting from given offset value
	 */
	unsigned i;
	for (i = offset; i < res->count; i++)
	{
		/* use default name if not provided */
		const char *name = (!res->arr[i].name) ? DEFAULT_NAME : res->arr[i].name;
		const long id = res->arr[i].id; /* post id */
		const long parent_id = res->arr[i].parent_id; /* parent id */
		unsigned is_parent = (id == parent_id); /* OP post */
		enquote_comment(&res->arr[i].comment, id); /* reformat comment string */
		const char *comment = format_comment(&res->arr[i].comment);
		const char *op = (is_parent) ? " parent" : ""; /* opening post */
		if (!is_parent) /* arrow marker wrapper */
			fprintf(stdout, "<div><div class=\"navi marker\">&gt;&gt;</div>");
		fprintf(stdout, "<div class=\"pContainer%s\" id=\"p%ld\">", op, id);
		if (res->arr[i].options & POST_SAGE)
			fprintf(stdout, "<span class=\"pSubject\">[sage]</span> ");
		if (res->arr[i].subject)
			fprintf(stdout, "<span class=\"pSubject\">%s</span> ", res->arr[i].subject);
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
		if (mode == INDEX_MODE && is_parent) /* reply link */
			fprintf(stdout, " <span class=\"navi controls\">"
			                "[<a href=\"%s?board=%s&thread=%ld\">Reply</a>]</span>",
		                     BOARD_SCRIPT, res->arr[i].board_id, res->arr[i].parent_id);
		fprintf(stdout, "</span>");
		fprintf(stdout, "<div class=\"pComment\">%s</div>", comment);
		fprintf(stdout, "</div>%s", (!is_parent) ? "</div>" : ""); /* end wrapper */
	}
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
		{
			for (j = 0; j < list->count; j++)
			{
				struct entry *board = &list->arr[j];
				fprintf(stdout, directory[i], board->id, board->name, board->desc);
			}
		}
		else
			fprintf(stdout, directory[i]);
	}
}

void not_found(const char *refer)
{
	static const char *error =
	"<div class=\"pContainer\">"
		"<span class=\"pName\">%s <i>rev.%d/db-%d</i></span>"
		"<div class=\"pComment\">"
		"<h2>404 - Not Found</h2>"
		"<span>"
			"The requested URL doesn't refer to any existing resource on this server.<br/>"
			"It may have been pruned or deleted."
		"</span>"
		"<br/>"
		"<div class=\"navi controls\">[<a href=\"%s\">Go back</a>]</div>"
	"</div></div>";
	fprintf(stdout, error, IDENT, REVISION, DB_VER, (!refer) ? "/" : refer);
}

void index_mode(sqlite3 *db, struct board *list, struct parameters *params)
{
	/* compute index ranking
	 * display thread previews from selected page
	 */
	display_headers(list, params->board_id);
	display_boardlist(list, NULL);
	display_postform(params->mode, params->board_id, 0);
	display_navigation(params, 0);
	static const char *const sql[] = {
		"SELECT post_id FROM active_threads WHERE "
			"board_id = \"%s\" ORDER BY last_bump DESC;",
		"SELECT * FROM posts WHERE "
			"board_id = \"%s\" AND parent_id = %ld ORDER BY id ASC;"
	};
	long thread_count = params->active_threads;
	char *rank = sql_generate(sql[0], params->board_id);
	long *index = db_array_retrieval(db, rank, thread_count);
	long offset = params->page_no * THREADS_PER_PAGE;
	long limit = min(offset + THREADS_PER_PAGE, thread_count);
	if (!thread_count)
		fprintf(stdout, "<h2>There aren't any threads yet.</h2>");
	else if (offset > thread_count) /* sanity check */
		fprintf(stdout, "<h2>There aren't that many threads here.</h2>");
	else
	{
		unsigned i;
		for (i = offset; i < limit; i++)
		{
			if (i != offset)
				fprintf(stdout, "<div class=\"line\"></div>");
			struct resource res; /* fetch thread */
			char *current = sql_generate(sql[1], params->board_id, index[i]);
			long replies = db_resource_fetch(db, &res, current) - 1;
			long omitted = 0;
			if (replies > MAX_REPLY_PREVIEW)
				omitted = replies - MAX_REPLY_PREVIEW;
			struct resource parent = { 1 , res.arr }; /* OP */
			display_resource(&parent, params->mode, 0);
			display_statistics(params, replies, index[i]);
			display_resource(&res, params->mode, omitted + 1); /* replies */
			db_resource_free(&res);
			free(current);
		}
	}
	display_navigation(params, 1);
	free((!index) ? NULL : index);
	free(rank);
}

void thread_mode(sqlite3 *db, struct board *list, struct parameters *params)
{
	/* display requested thread with
	 * thread statistics inserted between OP and replies
	 */
	display_headers(list, params->board_id);
	display_boardlist(list, NULL);
	display_postform(params->mode, params->board_id, params->thread_id);
	display_navigation(params, 0);
	static const char *sql =
		"SELECT * FROM posts WHERE "
			"board_id = \"%s\" AND parent_id = %ld ORDER BY id ASC;";
	struct resource res; /* fetch thread */
	char *cmd = sql_generate(sql, params->board_id, params->thread_id);
	int replies = db_resource_fetch(db, &res, cmd) - 1;
	struct resource parent = { 1, res.arr }; /* OP */
	display_resource(&parent, params->mode, 0);
	display_statistics(params, replies, 0);
	display_resource(&res, params->mode, 1); /* replies */
	display_navigation(params, 1);
	db_resource_free(&res);
	free(cmd);
}

void peek_mode(sqlite3 *db, struct parameters *params)
{
	/* preview a single post
	 * display reply count if parent post
	 */
	static const char *sql =
		"SELECT * FROM posts WHERE board_id = \"%s\" AND parent_id = %ld;";
	struct resource res;
	char *cmd = sql_generate(sql, params->board_id, params->parent_id);
	long total_posts = db_resource_fetch(db, &res, cmd);
	unsigned i;
	for (i = 0; i < total_posts; i++)
	{
		if (res.arr[i].id == params->thread_id)
		{
			struct resource post = { 1, &res.arr[i] };
			display_resource(&post, params->mode, 0);
			if (res.arr[i].id == res.arr[i].parent_id) /* OP */
				display_statistics(params, total_posts - 1, 0);
			break;
		}
	}
	db_resource_free(&res);
	free(cmd);
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
		const char *board = query_search(&query, "board"),
		           *thread = query_search(&query, "thread"),
		           *page = query_search(&query, "page"),
		           *archive = query_search(&query, "archive"),
		           *peek = query_search(&query, "peek");
		if (board)
		{
			for (i = 0; i < list->count && !params.board_id; i++)
				if (!strcmp(list->arr[i].id, board)) /* validate board */
					params.board_id = list->arr[i].id;
		}
		if (!params.board_id)
			params.mode = NOT_FOUND; /* general 404 error */
		else
		{
			/* get general board statistics */
			static const char *const sql[] = {
				"SELECT COUNT(*) FROM active_threads WHERE board_id = \"%s\";",
				"SELECT COUNT(*) FROM archived_threads WHERE board_id = \"%s\";"
			};
			char *cmd[2] = { 0 };
			for (i = 0; i < 2; i++)
				cmd[i] = sql_generate(sql[i], params.board_id);
			params.active_threads = db_retrieval(db, cmd[0]);
			params.archived_threads = db_retrieval(db, cmd[1]);
			for (i = 0; i < 2; i++)
				free(cmd[i]);

			params.thread_id = atoi_s(thread);
			if (params.thread_id > 0)
			{
				params.parent_id = db_find_parent(db, params.board_id, params.thread_id);
				if (!params.parent_id)
					params.mode = NOT_FOUND; /* doesn't exist */
				else if (atoi_s(peek))
					params.mode = PEEK_MODE;
				else if (params.parent_id != params.thread_id)
					params.mode = REDIRECT;
				else if (db_archive_status(db, params.board_id, params.parent_id))
					params.mode = ARCHIVE_MODE;
				else
					params.mode = THREAD_MODE;
			}
			else if (atoi_s(archive))
				params.mode = ARCHIVE_VIEWER;
			else
			{
				params.mode = INDEX_MODE;
				params.page_no = atoi_s(page);
				if (params.page_no > 0) /* pages 0-indexed internally */
					params.page_no -= 1;
				long max_pages = params.active_threads / THREADS_PER_PAGE;
				if (params.active_threads % THREADS_PER_PAGE)
					max_pages++; /* ceiling */
				if (params.active_threads && params.page_no >= max_pages)
					params.mode = NOT_FOUND;
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
	/* head */
	fprintf(stdout, global_template[0], generate_pagetitle(db, &params, &list));
	switch (params.mode)
	{
		case HOMEPAGE: homepage_mode(&list); break;
		case INDEX_MODE: index_mode(db, &list, &params); break;
		case THREAD_MODE:
		case ARCHIVE_MODE: thread_mode(db, &list, &params); break;
		case ARCHIVE_VIEWER: break;
		case PEEK_MODE: peek_mode(db, &params); goto abort;
		case NOT_FOUND: not_found(getenv("HTTP_REFERER")); goto abort;
		case REDIRECT:
			fprintf(stdout, "<i>Redirecting to Thread No.%ld...</i>", params.parent_id);
			thread_redirect(params.board_id, params.parent_id, params.thread_id);
			goto abort;
	}

	/* debug */
	fprintf(stdout, "<br/><br/>");
	char *modes[] = {
		"Homepage", "Index Mode", "Thread Mode", "Archive Mode",
		"Archive Viewer", "Peek Mode", "404 Not Found", "Redirect"
	};
	fprintf(stdout, "[debug] mode: %s board: %s thread: %ld page: %ld<br/>active/archived: %ld/%ld get string: \"%s\"",
		modes[params.mode], params.board_id, params.thread_id, params.page_no, params.active_threads,
		params.archived_threads, getenv("QUERY_STRING"));

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
