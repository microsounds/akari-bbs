#ifndef QUERY_H
#define QUERY_H

struct q_pair {
	char *field;
	char *value;
};
struct q_container {
	struct q_pair *queries;
	unsigned count;
};

typedef struct q_container query_t;

void query_parse(query_t *self, char *str);
char *query_search(query_t *self, const char *searchstr);
void query_free(query_t *self);

#endif
