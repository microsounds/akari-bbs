#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* UTF-8 conversion and input sanitation routines */

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
	long len = strlen(str);
	char *dup = (char *) malloc(sizeof(char) * len + 1);
	strcpy(dup, str);
	return dup;
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
	/* counts significant character bytes in in UTF-8 strings */
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
