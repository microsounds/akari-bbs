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
	POST_SAGE = (1 << 0)
};

/* database fetch container */
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


/* validation */
int db_validate_board(sqlite3 *db, const char *board_id);
int db_validate_parent(sqlite3 *db, const char *board_id, const long id);
long db_total_posts(sqlite3 *db, const char *board_id);
long db_cooldown_timer(sqlite3 *db, const char *ip_addr);

/* insertion */
int db_post_insert(sqlite3 *db, struct post *cm);
int db_bump_parent(sqlite3 *db, const char *board_id, const long id);

/* resource fetching */
long db_resource_fetch(sqlite3 *db, struct resource *res, const char *sql);
long db_resource_fetch_specific(sqlite3 *db, struct resource *res, char *sql, int limit);
void db_resource_free(struct resource *res);

#endif
