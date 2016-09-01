#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <crypt.h>

/* utf8.c
 * UTF-8 conversion, input sanitation, syntactic sugar, tripcode routines
 */

/*
 * UTF-8 encoding
 * Binary    Hex          Comments
 * 0xxxxxxx  0x00..0x7F   Only byte of a 1-byte character encoding
 * 10xxxxxx  0x80..0xBF   Continuation bytes (1-3 continuation bytes)
 * 110xxxxx  0xC0..0xDF   First byte of a 2-byte character encoding
 * 1110xxxx  0xE0..0xEF   First byte of a 3-byte character encoding
 * 11110xxx  0xF0..0xF4   First byte of a 4-byte character encoding
 */

char *strdup(const char *str)
{
	if (str)
	{
		long len = strlen(str);
		char *dup = (char *) malloc(sizeof(char) * len + 1);
		strcpy(dup, str);
		return dup;
	}
	else
		return NULL;
}

static char hex_value(char c)
{
	c -= (c >= 'a' && c <= 'f') ? ' ' : 0; /* uppercase */
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'A' && c <= 'F')
		return c - '7';
	else
		return 0;
}

char *utf8_rewrite(char *str)
{
	/* converts ASCII escape codes to UTF-8 */
	/* %E5%88%9D%E9%9F%B3%E3%83%9F%E3%82%AF => 初音ミク */
	unsigned i, j;
	for (i = 0; str[i]; i++)
	{
		if (str[i] == '%')
		{
			for (j = 1; j <= 2; j++)
				str[i] = (str[i] << 4) | hex_value(str[i+j]);
			memmove(&str[i+1], &str[i+3], strlen(&str[i+3]) + 1);
		}
	}
	return str;
}

unsigned utf8_charcount(const char *str)
{
	/* counts significant character bytes in UTF-8 strings */
	unsigned count = 0;
	while (*str++)
		if ((*str & 0xC0) != 0x80) count++;
	return count;
}

char *xss_sanitize(char **loc)
{
	/* replaces html syntax with escape codes in place
	 * char ** pointer required to update stack pointer
	 * with new string location because realloc loses the original
	 */
	static const char *escape[UCHAR_MAX] = {
		['\"'] = "&quot;",
		['\''] = "&apos;",
		['<'] = "&lt;",
		['>'] = "&gt;",
		['&'] = "&amp;"
	};
	char *str = *loc;
	unsigned i;
	for (i = 0; str[i]; i++)
	{
		unsigned char c = str[i];
		if (escape[c])
		{
			unsigned offset = strlen(escape[c]);
			str = (char *) realloc(str, strlen(str) + offset + 1);
			memmove(&str[i+offset], &str[i+1], strlen(&str[i+1]) + 1);
			memcpy(&str[i], escape[c], offset);
		}
	}
	*loc = str;
	return str;
}

/* TODO:
	- >>9829 post linking
	- do not allow more than 2 newlines unless in a [code] tag
	- retain newline breaks in comment body
 */


char *tripcode_pass(char **nameptr)
{
	/* splits 'name#pass' string with '\0'
	 * returns a pointer to tripcode password if exists
	 */
	char *name = *nameptr;
	int len = strlen(name);
	int i, found = 0;
	for (i = len - 1; i >= 0; i--)
	{
		if (name[i] == '#' && &name[i+1] != NULL)
		{
			found = !found;
			name[i] = '\0';
			break;
		}
	}
	/* edge case: no name and only password */
	if (name[0] == '#' && found)
		*nameptr = NULL; /* invalidate name pointer */
	return (found) ? &name[i+1] : NULL;
}

char *tripcode_hash(const char *pass)
{
	/* creates unsecure futaba tripcodes
	 * this function returns pointer to static data
	 * overwritten on every call and is NOT THREAD SAFE
	 */
	if (!pass) return NULL;
	static const char *secret = "H.";
	const char *p = (strlen(pass) < 3) ? NULL : pass; /* too short? */
	char salt[5]; /* create salt */
	sprintf(salt, "%c%c%s", (!p) ? ' ' : p[1], (!p) ? ' ' : p[2], secret);
	char *s = salt;
	do /* sanitize salt */
	{
		if (*s < '.' || *s > 'z') /* clamp to './0-9A-Za-z' */
			*s = '.';
		else if (*s >= ':' && *s <= '@') /* if ':;<=>?@' */
			*s += 6; /* shift to 'ABCDEF' */
		else if (*s >= '[' && *s <= '`') /* if '[\]^_`' */
			*s += 6; /* shift to 'abcdef' */
	} while (*++s);
	char *trip = crypt(pass, salt);
	memmove(&trip[1], &trip[3], strlen(&trip[3]) + 1);
	trip[0] = '!';
	return trip;
}
