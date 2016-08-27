#include <stdlib.h>
#include <string.h>
#include "query.h"

/* general purpose query+value container for GET/POST requests */

static void query_deplus(char *str)
{
	do
		if (*str == '+') *str = ' ';
	while (*++str);
}

static unsigned query_count(const char *str)
{
	if (!str)
		return 0;
	long len = strlen(str);
	unsigned count = 0;
	unsigned i;
	for (i = 1; i < len; i++)
	{
		if (str[i] == '&')
		{
			if (str[i - 1] == '=') /* empty/incomplete fields */
				return 0;
			else
				count++;
		}
	}
	/* field1=&field2=&field3= */
	return (str[len - 1] == '=') ? 0 : count + 1;
}

void query_parse(query_t *self, char *str)
{
	/* create searchable container of query value pairs
	 * strtok destroys input string so make copies
	 */
	self->count = query_count(str);
	if (!self->count) /* if fields empty or incomplete */
	{
		self->queries = NULL;
		return;
	}
	self->queries = (struct q_pair *) malloc(sizeof(struct q_pair) * self->count);
	char *tok = strtok(str, "&="); /* alternating field + value pairs */
	unsigned i = 0, j = 0; /* indices */
	while (tok != NULL)
	{
		unsigned len = strlen(tok);
		char *ptr = (char *) malloc(sizeof(char) * len + 1);
		if (i % 2)
			self->queries[j].value = ptr;
		else
			self->queries[j].field = ptr;
		strcpy(ptr, tok);
		ptr[len] = '\0';
		query_deplus(ptr);
		tok = strtok(NULL, "&=");
		j += (i++ % 2); /* increment j every 2 iterations of i */
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
			free(self->queries[i].value);
		}
		if (self->queries)
			free(self->queries);
	}
	self->count = 0;
}
