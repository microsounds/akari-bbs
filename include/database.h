#ifndef DATABASE_H
#define DATABASE_H

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

struct fetch {
	long count;
	struct post *arr;
};

#endif
