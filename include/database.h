#ifndef DATABASE_H
#define DATABASE_H

/* database status flags */
enum user_priv {
	USER_NORMAL = (1 << 0),
	USER_MOD    = (1 << 1),
	USER_ADMIN  = (1 << 2)
};
enum board_status {
	BOARD_LOCKED = (1 << 0)
};
enum thread_status {
	THREAD_STICKIED = (1 << 0),
	THREAD_LOCKED   = (1 << 1)
};
enum post_options {
	POST_SAGE = (1 << 0),
	POST_WARN = (1 << 1),
	POST_BAN  = (1 << 2)
};

/* board list */

struct entry {
	char *id;
	char *name;
	char *desc;
};

struct board {
	unsigned count;
	struct entry *arr;
};

/* post container */

struct post {
	char *board_id;
	long parent_id;
	long id;
	long time;
	unsigned char options; /* flag words */
	unsigned char user_priv;
	char *del_pass;
	char *ip;
	char *name; /* optional */
	char *trip; /* optional */
	char *subject; /* optional */
	char *comment;
};

struct resource {
	unsigned count;
	struct post *arr;
};

/* SQLite3 error lookup */
extern const char *const sqlite3_err[];

/* misc */
char *sql_generate(const char *fmt, ...);
void thread_redirect(const char *board_id, long parent_id, long post_id);

/* value retrieval */
long db_retrieval(sqlite3 *db, const char *sql);
long *db_array_retrieval(sqlite3 *db, const char *sql, unsigned n);

/* state validation */
unsigned db_status_flags(sqlite3 *db, const char *board_id, const long id);
long db_find_parent(sqlite3 *db, const char *board_id, const long id);
int db_active_status(sqlite3 *db, const char *board_id, const long id);
int db_archive_status(sqlite3 *db, const char *board_id, const long id);
long db_total_posts(sqlite3 *db, const char *board_id, const long id);
long db_cooldown_timer(sqlite3 *db, const char *ip_addr);

/* insertion */
int db_post_insert(sqlite3 *db, struct post *cm);
int db_post_delete(sqlite3 *db, const char *board_id, const long id);
int db_bump_parent(sqlite3 *db, const char *board_id, const long id);
int db_archive_oldest(sqlite3 *db, const char *board_id);

/* resource fetching */
long db_board_fetch(sqlite3 *db, struct board *ls);
void db_board_free(struct board *ls);
long db_resource_fetch(sqlite3 *db, struct resource *res, const char *sql);
void db_resource_free(struct resource *res);

#endif
