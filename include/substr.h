#ifndef SUBSTR_H
#define SUBSTR_H

struct substr {
	unsigned count;
	unsigned *o_len;
	char **arr;
};

struct substr *substr_extract(char *str, const char *ls, const char *rs);
void substr_restore(struct substr *ctr, char *str);

#endif
