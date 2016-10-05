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

/* database fetch containers */
struct post {
	char *board_id;
	long parent_id; /* 0 if parent */
	long id;
	long time;
	unsigned options; /* flag words */
	unsigned user_priv;
	char *del_pass;
	char *ip;
	char *name; /* optional */
	char *trip; /* optional */
	char *comment;
};

struct resource {
	long count;
	struct post *arr;
};

/* extern from board.c */
void res_fetch(sqlite3 *db, struct resource *res, const char *sql);
void res_fetch_specific(sqlite3 *db, struct resource *res, char *sql, int limit);
void res_freeup(struct resource *res);

#endif