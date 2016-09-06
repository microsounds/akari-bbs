#include <stdlib.h>
#include <string.h>
#include "query.h"

/* query.c
 * general purpose GET/POST query+value tokenization and search
 */

static void query_stripplus(char *str)
{
	/* strip '+' space delimiters */
	do
		if (*str == '+') *str = ' ';
	while (*++str);
}

static unsigned query_count(const char *str)
{
	/* name=&comment=ok */
	unsigned count = 0;
	if (str)
	{
		do
			if (*str == '=') count++;
		while (*++str);
	}
	return (!count) ? 0 : count;
}

void query_parse(query_t *self, char *str)
{
	/* create searchable container of field+value pairs
	 * strtok destroys input string so make copies
	 */
	self->count = query_count(str);
	if (!self->count) /* null string maybe? */
	{
		self->queries = NULL;
		return;
	}
	self->queries = (struct q_pair *) malloc(sizeof(struct q_pair) * self->count);
	char *tok = strtok(str, "&"); /* tokenize field + value pairs */
	unsigned i;
	for (i = 0; tok != NULL; i++)
	{
		query_stripplus(tok);
		unsigned len = strlen(tok);
		if (tok[len-1] == '=') /* field has no value */
		{
			tok[len-1] = '\0'; /* truncate field name */
			self->queries[i].field = (char *) malloc(sizeof(char) * len);
			strcpy(self->queries[i].field, tok);
			self->queries[i].value = NULL;
		}
		else /* field has value */
		{
			unsigned j; /* find split point */
			for (j = 0; j < len; j++)
				if (tok[j] == '=') break;
			self->queries[i].field = (char *) malloc(sizeof(char) * j + 1);
			strncpy(self->queries[i].field, tok, j); /* copy first j bytes to field */
			self->queries[i].field[j+1] = '\0';
			self->queries[i].value = (char *) malloc(sizeof(char) * strlen(&tok[j+1]) + 1);
			strcpy(self->queries[i].value, &tok[j+1]); /* copy the rest to value */
		}
		tok = strtok(NULL, "&");
	}
}

char *query_search(query_t *self, const char *searchstr)
{
	/* if field exists, pointer to value string is returned */
	if (self->count)
	{
		unsigned i;
		for (i = 0; i < self->count; i++)
		{
			if (!strcmp(self->queries[i].field, searchstr))
				return self->queries[i].value;
		}
	}
	return NULL;
}

void query_free(query_t *self)
{
	if (self->count)
	{
		unsigned i;
		for (i = 0; i < self->count; i++)
		{
			free(self->queries[i].field);
			if (self->queries[i].value)
				free(self->queries[i].value);
		}
		if (self->queries)
			free(self->queries);
	}
	self->count = 0;
}
