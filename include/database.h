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
	THREAD_ACTIVE   = (1 << 0),
	THREAD_LOCKED   = (1 << 1),
	THREAD_STICKIED = (1 << 2)
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
	long count;
	struct post *arr;
};

void res_fetch(sqlite3 *db, struct resource *res, const char *sql);
void res_fetch_specific(sqlite3 *db, struct resource *res, char *sql, int limit);
void res_free(struct resource *res);

#endif
