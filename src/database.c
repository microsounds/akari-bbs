#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "global.h"
#include "database.h"
#include "utf8.h"

/*
 * database.c
 * database validation, insertion, and retrieval
 */

/*
	SELECT * FROM posts WHERE parent_id = %ld ORDER BY time ASC;
 */

static int db_transaction(sqlite3 *db, const char *sql)
{
	/* 1-shot SQL INSERT/UPDATE transaction
	 * returns error code
	 */
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	int err = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return err;
}

static int db_retrieval(sqlite3 *db, const char *sql)
{
	/* 1-shot SQL SELECT value retrieval
	 * returns first value from first column
	 */
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	sqlite3_step(stmt);
	int value = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return value;
}

int db_validate_board(sqlite3 *db, const char *board_id)
{
	/* check if board_id exists */
	const char *sql = "SELECT COUNT(*) FROM boards WHERE id = \"%s\";";
	unsigned size = strlen(sql) + strlen(board_id);
	char *cmd = (char *) malloc(sizeof(char) * size + 1);
	sprintf(cmd, sql, board_id);
	int exists = db_retrieval(db, cmd);
	free(cmd);
	return exists;
}

int db_validate_parent(sqlite3 *db, const char *board_id, const long id)
{
	/* check if post id is a parent post in board_id */
	const char *sql =
	"SELECT COUNT(*) FROM active_threads "
		"WHERE board_id = \"%s\" AND post_id = %ld;";
	unsigned size = strlen(sql) + strlen(board_id) + uintlen(id);
	char *cmd = (char *) malloc(sizeof(char) * size + 1);
	sprintf(cmd, sql, board_id, id);
	int exists = db_retrieval(db, cmd);
	free(cmd);
	return exists;
}

long db_total_posts(sqlite3 *db, const char *board_id)
{
	/* returns total post count of board_id */
	const char *sql = "SELECT MAX(id) FROM posts WHERE board_id = \"%s\";";
	unsigned size = strlen(sql) + strlen(board_id);
	char *cmd = (char *) malloc(sizeof(char) * size + 1);
	sprintf(cmd, sql, board_id);
	int post_count = db_retrieval(db, cmd);
	free(cmd);
	return post_count;
}

long db_cooldown_timer(sqlite3 *db, const char *ip_addr)
{
	/* calculates cooldown timer expressed in seconds remaining
	 * cooldown timer is global across all boards
	 */
	const long current_time = time(NULL);
	const long delta = current_time - COOLDOWN_SEC;
	long timer = COOLDOWN_SEC;
	struct resource res;
	char sql[100]; /* fetch newest posts only */
	sprintf(sql, "SELECT * FROM posts WHERE time > %ld;", delta);
	db_resource_fetch(db, &res, sql);
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

static unsigned post_length(struct post *cm)
{
	/* get approximate length of all data in struct */
	unsigned len = 0;
	len += strlen(cm->board_id);
	len += uintlen(cm->parent_id);
	len += uintlen(cm->id);
	len += uintlen(cm->time);
	len += uintlen(cm->options);
	len += uintlen(cm->user_priv);
	len += strlen(cm->del_pass);
	len += strlen(cm->ip);
	len += (!cm->name) ? 0 : strlen(cm->name);
	len += (!cm->trip) ? 0 : strlen(cm->trip);
	len += (!cm->subject) ? 0 : strlen(cm->subject);
	len += strlen(cm->comment);
	return len;
}

int db_post_insert(sqlite3 *db, struct post *cm)
{
	/* insert new post into database */
	static const char *const sql[] = {
		/* mandatory fields */
		"INSERT INTO "
			"posts(board_id, parent_id, id, time, options, "
			      "user_priv, del_pass, ip, comment) "
		"VALUES(\"%s\", %ld, %ld, %ld, %d, %d, \"%s\", \"%s\");",
		/* subject */
		"UPDATE posts SET subject = \"%s\" WHERE id = %ld;",
		/* name */
		"UPDATE posts SET name = \"%s\" WHERE id = %ld;",
		/* trip */
		"UPDATE posts SET trip = \"%s\" WHERE id = %ld;"
	};
	unsigned len = post_length(cm);
	char *insert = (char *) malloc(sizeof(char) * strlen(sql[0]) + len + 1);
	sprintf(insert, sql[0], cm->board_id, cm->parent_id, cm->id, cm->time,
	        cm->options, cm->user_priv, cm->del_pass, cm->ip, cm->comment);
	int err = db_transaction(db, insert);

	/* optional fields, if any */
	const char *const field[] = { cm->subject, cm->name, cm->trip };
	unsigned i;
	for (i = 0; i < static_size(field); i++)
	{
		if (field[i])
		{
			sprintf(insert, sql[i+1], field[i], cm->id);
			err = db_transaction(db, insert);
		}
	}
	free(insert);
	return err;
}

int db_bump_parent(sqlite3 *db, const char *board_id, const long id)
{
	/* bump parent thread by updating it's last_bump timestamp
	 * returns non-zero if thread bumped
	 */
	static const char *const sql[] = {
		/* reply count */
		"SELECT COUNT(*) FROM posts "
			"WHERE board_id = \"%s\" AND parent_id = %ld",
		/* bump thread */
		"UPDATE active_threads SET last_bump = %ld "
			"WHERE board_id = \"%s\" AND post_id = %ld;"
	};
	int bumped = 0;
	if (db_validate_parent(db, board_id, id)) /* valid thread? */
	{
		unsigned size = strlen(sql[0]) + strlen(board_id) + uintlen(id);
		char *cmd = (char *) malloc(sizeof(char) * size + 1);
		sprintf(cmd, sql[0], board_id, id);
		if (db_retrieval(db, cmd) <= THREAD_BUMP_LIMIT) /* eligible? */
		{
			const long tm = time(NULL);
			size = strlen(sql[1]) + strlen(board_id) + uintlen(id);
			cmd = (char *) realloc(cmd, size + uintlen(tm) + 1);
			sprintf(cmd, sql[1], tm, board_id, id);
			bumped = !db_transaction(db, cmd);
		}
		free(cmd);
	}
	return bumped;
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

long db_resource_fetch(sqlite3 *db, struct resource *res, const char *sql)
{
	/* fetch requested SQL row matches into memory
	 * returns number of items fetched
	 * this automated version is intended for COUNT(*) compatible statements
	 */
	sqlite3_stmt *stmt;
	char *row_count = sql_rowcount(sql); /* how many rows? */
	sqlite3_prepare_v2(db, row_count, -1, &stmt, NULL);
	sqlite3_step(stmt);
	res->count = sqlite3_column_int(stmt, 0);
	res->arr = (struct post *) malloc(sizeof(struct post) * res->count);
	sqlite3_finalize(stmt);
	free(row_count);
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
	return res->count;
}

long db_resource_fetch_specific(sqlite3 *db, struct resource *res, char *sql, int limit)
{
	/* fetch requested SQL row matches into memory
	 * returns number of items fetched
	 * this manual version is intended for LIMIT/OFFSET statements ONLY
	 */
	sqlite3_stmt *stmt;
	res->count = limit; /* user-provided row count */
	res->arr = (struct post *) malloc(sizeof(struct post) * res->count);
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); /* fetch results */
	unsigned i;
	for (i = 0; i < res->count; i++)
	{
		int err = sqlite3_step(stmt);
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
		if (err == SQLITE_DONE) /* early exit */
		{
			res->count = i;
			break;
		}
	}
	sqlite3_finalize(stmt);
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
			free(res->arr[i].subject);
			free(res->arr[i].comment);
		}
		free(res->arr);
	}
	res->count = 0;
}
