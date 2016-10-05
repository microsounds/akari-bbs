#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "global.h"
#include "database.h"
#include "utf8.h"

/*
 * database.c
 * database post fetching
 */

/*
	SELECT * FROM posts WHERE parent_id = %ld ORDER BY time ASC;
 */

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
}

void res_fetch_specific(sqlite3 *db, struct resource *res, char *sql, int limit)
{
	/* fetch requested SQL results into memory
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
}

void res_free(struct resource *res)
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
}
