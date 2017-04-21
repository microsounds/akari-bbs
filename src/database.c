#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>
#include <sqlite3.h>
#include "global.h"
#include "database.h"
#include "utf8.h"
#include "macros.h"

/*
 * database.c
 * database retrieval, validation, flood control, insertion, and resource fetching
 */

/* SQLite3 error lookup */
#define enum_string(e) [e] = #e
const char *const sqlite3_err[UCHAR_MAX] = {
	enum_string(SQLITE_OK), enum_string(SQLITE_ERROR),
	enum_string(SQLITE_INTERNAL), enum_string(SQLITE_PERM),
	enum_string(SQLITE_ABORT), enum_string(SQLITE_BUSY),
	enum_string(SQLITE_LOCKED), enum_string(SQLITE_NOMEM),
	enum_string(SQLITE_READONLY), enum_string(SQLITE_INTERRUPT),
	enum_string(SQLITE_IOERR), enum_string(SQLITE_CORRUPT),
	enum_string(SQLITE_NOTFOUND), enum_string(SQLITE_FULL),
	enum_string(SQLITE_CANTOPEN), enum_string(SQLITE_PROTOCOL),
	enum_string(SQLITE_EMPTY), enum_string(SQLITE_SCHEMA),
	enum_string(SQLITE_TOOBIG), enum_string(SQLITE_CONSTRAINT),
	enum_string(SQLITE_MISMATCH), enum_string(SQLITE_MISUSE),
	enum_string(SQLITE_NOLFS), enum_string(SQLITE_AUTH),
	enum_string(SQLITE_FORMAT), enum_string(SQLITE_RANGE),
	enum_string(SQLITE_NOTADB), enum_string(SQLITE_NOTICE),
	enum_string(SQLITE_WARNING), enum_string(SQLITE_ROW),
	enum_string(SQLITE_DONE)
};
#undef enum_string


char *sql_generate(const char *fmt, ...)
{
	/* self-allocating sprintf
	 * allocates space for integer and string arguments only
	 */
	va_list args;
	va_start(args, fmt);
	unsigned size = strlen(fmt);
	char *s; long n; /* supported types */
	const char *f = fmt;
	do
	{
		if (*f == '%')
		{
			switch (f[1])
			{
				case 'd':
				case 'l':
				case 'u':
					n = va_arg(args, long);
					size += uintlen(n);
					break;
				case 's':
					s = va_arg(args, char *);
					size += strlen(s);
					break;
			}
		}
	} while (*++f);
	va_end(args);
	va_start(args, fmt);
	char *out = (char *) malloc(sizeof(char) * size + 1);
	vsprintf(out, fmt, args);
	va_end(args);
	return out;
}

static char *sql_rowcount(const char *str)
{
	/* rewrites SQL statements to fetch row count instead */
	static const char *ins = "COUNT(*)";
	static const unsigned offset = static_size(ins);
	char *out = (char *) malloc(sizeof(char) * strlen(str) + offset + 1);
	unsigned i, j = 0;
	for (i = 0; str[i]; i++)
	{
		if (str[i] == '*')
		{
			memcpy(&out[j], ins, offset);
			j += offset;
		}
		else
			out[j++] = str[i];
	}
	out[j] = '\0';
	return out;
}

static int db_transaction(sqlite3 *db, const char *sql)
{
	/* 1-shot SQL INSERT/UPDATE transaction
	 * returns error code
	 */
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	int err = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (err == SQLITE_DONE) ? SQLITE_OK : err;
}

long db_retrieval(sqlite3 *db, const char *sql)
{
	/* 1-shot SQL SELECT integer value retrieval
	 * returns first value from first column
	 */
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_step(stmt);
	int value = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return value;
}

long *db_array_retrieval(sqlite3 *db, const char *sql, unsigned n)
{
	/* 1-shot SQL SELECT integer array retrieval
	 * retrieves n integer values from first column
	 */
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_step(stmt);
	long *arr = (long *) malloc(sizeof(long) * n);
	unsigned i;
	for (i = 0; i < n; i++)
	{
		arr[i] = sqlite3_column_int(stmt, 0);
		sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	return arr;
}

void thread_redirect(const char *board_id, long parent_id, long post_id)
{
	/* blind redirect to a specific thread
	 * assuming inputs are already validated
	 * if post_id > parent_id, append as a permalink
	 */
	const char *redir = "<meta http-equiv=\"refresh\" content=\"%ld; url=%s\">";
	const char *redir_link =
		"<div class=\"navi controls\">"
			"If you are not redirected shortly, please [<a href=\"%s\">click here</a>]."
		"</div>";
	const char *url = "%s?board=%s&thread=%ld";
	const char *permalink = "#p%ld";
	char *buf[4]  = { 0 }; /* storage */
	buf[0] = sql_generate(url, BOARD_SCRIPT, board_id, parent_id);
	if (post_id > parent_id) /* concatenate permalink */
	{
		buf[1] = sql_generate(permalink, post_id);
		unsigned size = strlen(buf[0]) + strlen(buf[1]);
		buf[0] = (char *) realloc(buf[0], sizeof(char) * size + 1);
		strcat(buf[0], buf[1]);
	}
	buf[2] = sql_generate(redir, REDIRECT_SEC, buf[0]);
	buf[3] = sql_generate(redir_link, buf[0]);
	fprintf(stdout, "%s%s", buf[2], buf[3]);
	unsigned i;
	for (i = 0; i < static_size(buf); i++)
		free((!buf[i]) ? NULL : buf[i]);
}

unsigned db_status_flags(sqlite3 *db, const char *board_id, const long id)
{
	/* returns board status flags
	 * if parent_id is provided, thread status flags are returned instead
	 */
	static const char *const sql[] = {
		"SELECT status FROM boards WHERE id = \"%s\";",
		"SELECT status FROM active_threads "
			"WHERE board_id = \"%s\" AND post_id = %ld;"
	};
	unsigned opt = (id < 0) ? 0 : 1; /* select mode */
	const char *stmt = sql[opt];
	char *cmd = (!opt) ? sql_generate(stmt, board_id)
	                   : sql_generate(stmt, board_id, id);
	unsigned status = db_retrieval(db, cmd);
	free(cmd);
	return status;
}

long db_find_parent(sqlite3 *db, const char *board_id, const long id)
{
	/* return parent_id of requested post number
	 * if post doesn't exist, return 0
	 */
	static const char *sql =
		"SELECT parent_id FROM posts "
			"WHERE board_id = \"%s\" AND id = %ld;";
	char *cmd = sql_generate(sql, board_id, id);
	long parent_id = db_retrieval(db, cmd);
	free(cmd);
	return parent_id;
}

int db_active_status(sqlite3 *db, const char *board_id, const long id)
{
	/* return non-zero if requested thread_id is active */
	static const char *sql =
		"SELECT COUNT(*) FROM active_threads "
			"WHERE board_id = \"%s\" AND post_id = %ld;";
	char *cmd = sql_generate(sql, board_id, id);
	int exists = db_retrieval(db, cmd);
	free(cmd);
	return exists;
}

int db_archive_status(sqlite3 *db, const char *board_id, const long id)
{
	/* return non-zero if requested thread_id is archived */
	static const char *sql =
		"SELECT COUNT(*) FROM archived_threads "
			"WHERE board_id = \"%s\" AND post_id = %ld;";
	char *cmd = sql_generate(sql, board_id, id);
	long status = db_retrieval(db, cmd);
	free(cmd);
	return status;
}

long db_total_posts(sqlite3 *db, const char *board_id, const long id)
{
	/* returns lifetime post count of board_id
	 * if parent_id is provided, post count of that thread is returned
	 */
	static const char *const sql[] = {
		"SELECT MAX(id) FROM posts WHERE board_id = \"%s\";",
		"SELECT COUNT(*) FROM posts "
			"WHERE board_id = \"%s\" AND parent_id = %ld;"
	};
	unsigned opt = (id < 0) ? 0 : 1; /* select mode */
	const char *stmt = sql[opt];
	char *cmd = (!opt) ? sql_generate(stmt, board_id)
	                   : sql_generate(stmt, board_id, id);
	long post_count = db_retrieval(db, cmd);
	free(cmd);
	return post_count;
}

#ifdef NDEBUG /* flood control */

long db_user_threads(sqlite3 *db, const char *board_id, const char *ip_addr)
{
	/* returns number of active threads from this IP */
	static const char *sql =
		"SELECT COUNT(*) FROM active_threads "
			"INNER JOIN posts ON active_threads.post_id = posts.id "
			"WHERE posts.board_id = \"%s\" AND posts.ip = \"%s\";";
	char *cmd = sql_generate(sql, board_id, ip_addr);
	long count = db_retrieval(db, cmd);
	free(cmd);
	return count;
}

long db_duplicate_post(sqlite3 *db, const char *text, const char *ip_addr)
{
	/* determines elapsed seconds since user made an identical post
	 * identical post detection is global across all boards
	 */
	const long current_time = time(NULL);
	const long time_frame = current_time - IDENTICAL_POST_SEC;
	long timer = IDENTICAL_POST_SEC;
	struct resource res;
	static const char *sql =
		"SELECT * FROM posts WHERE time > %ld AND ip = \"%s\" "
			"ORDER BY time DESC;";
	char *cmd = sql_generate(sql, time_frame, ip_addr);
	unsigned posts = db_resource_fetch(db, &res, cmd);
	free(cmd);
	unsigned i;
	for (i = 0; i < posts; i++)
	{
		if (!strcmp(res.arr[i].comment, text))
		{
			timer = current_time - res.arr[i].time;
			break;
		}
	}
	db_resource_free(&res);
	return IDENTICAL_POST_SEC - timer;
}

long db_cooldown_timer(sqlite3 *db, const char *ip_addr)
{
	/* determines elapsed seconds since last user transaction
	 * cooldown timer is global across all boards
	 */
	const long current_time = time(NULL);
	const long time_frame = current_time - COOLDOWN_SEC;
	long timer = COOLDOWN_SEC;
	struct resource res; /* fetch newest posts only */
	static const char *sql = "SELECT * FROM posts WHERE time > %ld;";
	char *cmd = sql_generate(sql, time_frame);
	db_resource_fetch(db, &res, cmd);
	free(cmd);
	int i;
	for (i = res.count - 1; i >= 0; i--)
	{
		if (!strcmp(ip_addr, res.arr[i].ip))
		{
			timer = current_time - res.arr[i].time;
			break;
		}
	}
	db_resource_free(&res);
	return COOLDOWN_SEC - timer;
}

#endif

int db_post_insert(sqlite3 *db, struct post *cm)
{
	/* insert new post into database
	 * if parent_id and id are the same, create new thread
	 * returns error code
	 */
	static const char *const sql[] = {
		"INSERT INTO active_threads VALUES(\"%s\", %ld, %ld, %d);",
		"INSERT INTO "
			"posts(board_id, parent_id, id, time, options, "
			      "user_priv, del_pass, ip, comment) "
		"VALUES(\"%s\", %ld, %ld, %ld, %d, %d, \"%s\", \"%s\", \"%s\");",
		/* optional fields */
		"UPDATE posts SET subject = \"%s\" "
			"WHERE board_id = \"%s\" AND id = %ld;",
		"UPDATE posts SET name = \"%s\" "
			"WHERE board_id = \"%s\" AND id = %ld;",
		"UPDATE posts SET trip = \"%s\" "
			"WHERE board_id = \"%s\" AND id = %ld;",
	};
	char *cmd[static_size(sql)] = { 0 }; /* buffer */
	int err = 0;
	if (cm->parent_id == cm->id) /* new thread creation */
	{
		cmd[0] = sql_generate(sql[0], cm->board_id, cm->id, cm->time, 0);
		if ((err = db_transaction(db, cmd[0])))
			goto end;
	}
	cmd[1] = sql_generate(sql[1],
		cm->board_id, cm->parent_id, cm->id, cm->time,
		cm->options, cm->user_priv, cm->del_pass, cm->ip, cm->comment
	);
	if ((err = db_transaction(db, cmd[1]))) /* mandatory fields */
		goto end;
	const char *const field[] = { cm->subject, cm->name, cm->trip };
	unsigned i;
	for (i = 0; i < static_size(field); i++) /* optional fields */
	{
		if (field[i])
		{
			cmd[i+2] = sql_generate(sql[i+2], field[i], cm->board_id, cm->id);
			err = db_transaction(db, cmd[i+2]);
		}
	}
	end: for (i = 0; i < static_size(sql); i++)
		free((!cmd[i]) ? NULL : cmd[i]);
	return err;
}

int db_post_delete(sqlite3 *db, const char *board_id, const long id)
{
	/* delete post
	 * if a parent thread, delete all child posts
	 * returns non-zero if successful
	 */
	static const char *const sql[] = {
		"DELETE FROM posts WHERE board_id = \"%s\" AND id = %ld;",
		/* parent thread delete */
		"SELECT * FROM posts WHERE board_id = \"%s\" AND id = %ld;",
		"DELETE FROM active_threads WHERE board_id = \"%s\" AND post_id = %ld;",
		"DELETE FROM archived_threads WHERE board_id = \"%s\" AND post_id = %ld;",
		"DELETE FROM posts WHERE board_id = \"%s\" AND parent_id = %ld;"
	};
	char *cmd[static_size(sql)] = { 0 };
	unsigned i, success = 0;
	for (i = 0; i < static_size(sql); i++)
		cmd[i] = sql_generate(sql[i], board_id, id);
	struct resource res;
	db_resource_fetch(db, &res, cmd[1]);
	if (res.count)
	{
		if (res.arr[0].id == res.arr[0].parent_id) /* parent thread */
		{
			int is_active = db_active_status(db, board_id, id);
			success = !db_transaction(db, (is_active) ? cmd[2] : cmd[3]);
			success = !db_transaction(db, cmd[4]);
		}
		else
			success = !db_transaction(db, cmd[0]); /* single post */
	}
	db_resource_free(&res);
	for (i = 0; i < static_size(sql); i++)
		free((!cmd[i]) ? NULL : cmd[i]);
	return success;
}

int db_bump_parent(sqlite3 *db, const char *board_id, const long id)
{
	/* bump parent thread by updating it's last_bump timestamp
	 * returns non-zero if thread bumped
	 */
	static const char *sql =
		"UPDATE active_threads SET last_bump = %ld "
			"WHERE board_id = \"%s\" AND post_id = %ld;";
	int bumped = 0;
	if (db_active_status(db, board_id, id)) /* eligible for bump? */
	{
		if (db_total_posts(db, board_id, id) <= THREAD_BUMP_LIMIT)
		{
			char *cmd = sql_generate(sql, time(NULL), board_id, id);
			bumped = !db_transaction(db, cmd);
			free(cmd);
		}
	}
	return bumped;
}

int db_archive_oldest(sqlite3 *db, const char *board_id)
{
	/* archive stale threads from active_threads
	 * delete archived_threads past their expiration date
	 * return non-zero if operations performed
	 */
	static const char *const sql[] = {
		/* find stale threads */
		"SELECT COUNT(*) FROM active_threads WHERE board_id = \"%s\";",
		"SELECT post_id FROM active_threads WHERE board_id = \"%s\" "
			"ORDER BY last_bump ASC;",
		/* move thread to archive */
		"INSERT INTO archived_threads VALUES(\"%s\", %ld, %ld);",
		"DELETE FROM active_threads WHERE board_id = \"%s\" AND post_id = %ld;",
		/* find expired archive threads */
		"SELECT COUNT(*) FROM archived_threads WHERE board_id = \"%s\" AND expiry < %ld;",
		"SELECT post_id FROM archived_threads WHERE board_id = \"%s\" AND expiry < %ld;"
	};
	char *cmd[static_size(sql)] = { 0 };
	time_t now = time(NULL);
	time_t expire_time = now + to_seconds(DAYS_TO_ARCHIVE);
	unsigned i, j, success = 0;
	for (i = 0; i < 2; i++)
		cmd[i] = sql_generate(sql[i], board_id);
	long thread_count = db_retrieval(db, cmd[0]);
	if (thread_count > MAX_ACTIVE_THREADS)
	{
		long stale = thread_count - MAX_ACTIVE_THREADS;
		long *post_id = db_array_retrieval(db, cmd[1], stale);
		for (i = 0; i < stale; i++)
		{
			/* archive and set expiration date */
			cmd[2] = sql_generate(sql[2], board_id, post_id[i], expire_time);
			cmd[3] = sql_generate(sql[3], board_id, post_id[i]);
			for (j = 2; j < 4; j++)
			{
				success = !db_transaction(db, cmd[j]);
				free(cmd[j]);
				cmd[j] = NULL;
			}
		}
		free((!post_id) ? NULL : post_id);
	}
	for (i = 4; i < 6; i++)
		cmd[i] = sql_generate(sql[i], board_id, now);
	long expired_count = db_retrieval(db, cmd[4]);
	if (expired_count)
	{
		long *post_id = db_array_retrieval(db, cmd[5], expired_count);
		for (i = 0; i < expired_count; i++)
			success = db_post_delete(db, board_id, post_id[i]);
		free((!post_id) ? NULL : post_id);
	}
	for (i = 0; i < static_size(sql); i++)
		free(cmd[i]);
	return success;
}

long db_board_fetch(sqlite3 *db, struct board *ls)
{
	/* fetch enumerated boardlist with description information
	 * returns number of items fetched
	 */
	static const char *const sql[] = {
		"SELECT COUNT(id) FROM boards;",
		"SELECT id, name, desc FROM boards ORDER BY id;"
	};
	ls->count = db_retrieval(db, sql[0]);
	if (ls->count)
	{
		ls->arr = (struct entry *) malloc(sizeof(struct entry) * ls->count);
		sqlite3_stmt *stmt;
		sqlite3_prepare_v2(db, sql[1], -1, &stmt, NULL);
		unsigned i;
		for (i = 0; i < ls->count; i++)
		{
			sqlite3_step(stmt);
			ls->arr[i].id = strdup((char *) sqlite3_column_text(stmt, 0));
			ls->arr[i].name = strdup((char *) sqlite3_column_text(stmt, 1));
			ls->arr[i].desc = strdup((char *) sqlite3_column_text(stmt, 2));
		}
		sqlite3_finalize(stmt);
	}
	return ls->count;
}

void db_board_free(struct board *ls)
{
	if (ls->count)
	{
		unsigned i;
		for(i = 0; i < ls->count; i++)
		{
			free((!ls->arr[i].id) ? NULL : ls->arr[i].id);
			free((!ls->arr[i].name) ? NULL : ls->arr[i].name);
			free((!ls->arr[i].desc) ? NULL : ls->arr[i].desc);
		}
		free(ls->arr);
		ls->count = 0;
	}
}

long db_resource_fetch(sqlite3 *db, struct resource *res, const char *sql)
{
	/* fetch enumerated post container that match requested params
	 * returns number of items fetched
	 * this automated version is intended for COUNT(*) compatible statements
	 */
	char *row_count = sql_rowcount(sql);
	res->count = db_retrieval(db, row_count); /* how many rows? */
	free(row_count);
	if (res->count)
	{
		res->arr = (struct post *) malloc(sizeof(struct post) * res->count);
		sqlite3_stmt *stmt;
		sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); /* fetch results */
		unsigned i;
		for (i = 0; i < res->count; i++)
		{
			sqlite3_step(stmt);
			res->arr[i].board_id = strdup((char *) sqlite3_column_text(stmt, 0));
			res->arr[i].parent_id = sqlite3_column_int(stmt, 1);
			res->arr[i].id = sqlite3_column_int(stmt, 2);
			res->arr[i].time = sqlite3_column_int(stmt, 3);
			res->arr[i].options = (unsigned char) sqlite3_column_int(stmt, 4);
			res->arr[i].user_priv = (unsigned char) sqlite3_column_int(stmt, 5);
			res->arr[i].del_pass = strdup((char *) sqlite3_column_text(stmt, 6));
			res->arr[i].ip = strdup((char *) sqlite3_column_text(stmt, 7));
			res->arr[i].name = strdup((char *) sqlite3_column_text(stmt, 8));
			res->arr[i].trip = strdup((char *) sqlite3_column_text(stmt, 9));
			res->arr[i].subject = strdup((char *) sqlite3_column_text(stmt, 10));
			res->arr[i].comment = strdup((char *) sqlite3_column_text(stmt, 11));
		}
		sqlite3_finalize(stmt);
	}
	return res->count;
}

void db_resource_free(struct resource *res)
{
	if (res->count)
	{
		unsigned i;
		for (i = 0; i < res->count; i++)
		{
			free(res->arr[i].board_id);
			free(res->arr[i].ip);
			free(res->arr[i].del_pass);
			if (res->arr[i].name)
				free(res->arr[i].name);
			if (res->arr[i].trip)
				free(res->arr[i].trip);
			if (res->arr[i].subject)
				free(res->arr[i].subject);
			free(res->arr[i].comment);
		}
		free(res->arr);
	}
	res->count = 0;
}
