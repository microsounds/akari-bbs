#include <stdlib.h>
#include <string.h>
#include "substr.h"
#include "utf8.h"

/*
 * substr.c
 * formatted substring extraction routines
 */

#define E_BYTE 0x7F /* extract region marker */

static unsigned substr_count(const char *str, const char *ls, const char *rs)
{
	/* count conforming substrings */
	unsigned count = 0;
	unsigned len = strlen(str);
	unsigned i;
	for (i = 0; str[i]; i++)
	{
		char *from = strstr(&str[i], ls);
		i = (!from) ? len : (unsigned) (from - str);
		if (!str[i])
			break;
		char *to = strstr(&str[i], rs);
		i = (!to) ? len : (unsigned) (to - str);
		count++;
		if (!to)
			break;
	}
	return count;
}

static void substr_newline(struct substr *ctr, const char *ls, const char *rs)
{
	/* most common use of this library will be [code] tag sanitation
	 * strips n characters between *ls and first non-newline
	 * strips n characters between *rs and last non-newline
	 */
	unsigned i, j, k;
	for (i = 0; i < ctr->count; i++)
	{
		char *str = ctr->arr[i];
		for (j = 0; j < 2; j++) /* format strings */
		{
			for (k = 0; str[k]; k++)
			{
				char *next = strstr(&str[k], (j % 2) ? rs : ls);
				k = (!next) ? strlen(str) - 1 : (unsigned) (next - str);
				if (!str[k] || !next)
					break;
				unsigned offset = strlen(ls); /* set overlap point */
				char *s = (j % 2) ? &str[k] : &str[k+offset];
				if (j % 2) /* *rs seek <-- */
					while (wspace(s[-1])) s--;
				while (wspace(*s))
					memmove(s, s + 1, strlen(s + 1) + 1);
			}
		}
		/* edge case: no *rs and trailing newlines */
		unsigned len = strlen(str) - 1;
		while (wspace(str[len]))
			str[len--] = '\0';
	}
}

struct substr *substr_extract(char *str, const char *ls, const char *rs)
{
	/* extract all substrings contained between *ls and *rs
	 * returns struct containing extracted strings
	 * extracted regions replaced with E_BYTE
	 */
	struct substr *out = (struct substr *) malloc(sizeof(struct substr));
	out->count = substr_count(str, ls, rs);
	if (out->count)
	{
		out->o_len = (unsigned *) malloc(sizeof(unsigned) * out->count);
		out->arr = (char **) malloc(sizeof(char *) * out->count);
		unsigned len = strlen(str);
		unsigned idx = 0;
		unsigned i, j, k; /* extraction */
		for (i = 0; str[i] && idx < out->count; i++)
		{
			char *from = strstr(&str[i], ls);
			i = (!from) ? len : (unsigned) (from - str);
			if (!str[i])
				break;
			char *to = strstr(&str[i], rs);
			j = (!to) ? len : (unsigned) (to - str) + strlen(rs);
			k = j - i; /* extract length */
			out->o_len[idx] = k; /* save original length */
			out->arr[idx] = (char *) malloc(sizeof(char) * k + 1);
			memcpy(out->arr[idx], &str[i], k);
			out->arr[idx++][k] = '\0';
			memset(&str[i], E_BYTE, k); /* delete */
			i = j - 1; /* jump to next */
			if (!to)
				break;
		}
	}
	substr_newline(out, ls, rs);
	return out;
}

static void substr_free(struct substr *ctr)
{
	if (ctr->count)
	{
		unsigned i;
		for (i = 0; i < ctr->count; i++)
			free(ctr->arr[i]);
		free(ctr->o_len);
		free(ctr->arr);
	}
	free(ctr);
}

void substr_restore(struct substr *ctr, char *str)
{
	/* restore extracted regions
	 * extracted strings might return shorter than before
	 */
	unsigned i;
	for (i = 0; i < ctr->count; i++)
	{
		unsigned len = strlen(ctr->arr[i]);
		unsigned j; /* seek to next extract region */
		for (j = 0; str[j] != E_BYTE; j++);
		memcpy(&str[j], ctr->arr[i], len);
		char *s = &str[j+len];
		int diff = abs(ctr->o_len[i] - len);
		if (diff)
			memmove(s, s + diff, strlen(s + diff) + 1);
	}
	substr_free(ctr);
}
