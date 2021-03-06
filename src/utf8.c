#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <crypt.h>
#include "utf8.h"
#include "substr.h"
#include "macros.h"

/*
 * utf8.c
 * lookup tables, format tags, string library,
 * UTF-8 routines, input sanitation, tripcode routines
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

/* lookup tables */
const unsigned char base16[UCHAR_MAX] = {
	['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4,
	['5'] = 5, ['6'] = 6, ['7'] = 7, ['8'] = 8, ['9'] = 9,
	['A'] = 10, ['B'] = 11, ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15,
	['a'] = 10, ['b'] = 11, ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15
};

const unsigned char wspace[UCHAR_MAX] = {
	['\a'] = 1, ['\b'] = 1, ['\t'] = 1, ['\n'] = 1,
	['\v'] = 1, ['\f'] = 1,	['\r'] = 1, [' '] = 1
};

const char *const escape[UCHAR_MAX] = {
	['\n'] = "&#013;", /* change '\n' to '\r' */
	['\"'] = "&quot;",
	['\''] = "&apos;",
	['<'] = "&lt;",
	['>'] = "&gt;",
	['&'] = "&amp;"
};

/* format tags */
const char *const fmt[] = {
	[SPOILER_L] = "[spoiler]", [SPOILER_R] = "[/spoiler]",
	[CODE_L] = "[code]", [CODE_R] = "[/code]"
};
static_assert(static_size(fmt) == SUPPORTED_TAGS); /* size check */

int atoi_s(const char *nptr)
{
	/* sanitized atoi() for numerical GET/POST options
	 * returns 0 if nptr contains garbage or is NULL
	 */
	const char *s = nptr;
	while (nptr && *++s)
		if (*s < '0' || *s > '9')
			return 0;
	return (!nptr) ? 0 : abs(atoi(nptr));
}

int uintlen(unsigned long n)
{
	/* return length of 32-bit integer
	 * faster than log10 or division
	 */
	n = abs(n);
	if (n < 10) return 1;
	if (n < 100) return 2;
	if (n < 1000) return 3;
	if (n < 10000) return 4;
	if (n < 100000) return 5;
	if (n < 1000000) return 6;
	if (n < 10000000) return 7;
	if (n < 100000000) return 8;
	if (n < 1000000000) return 9;
	return 10;
}

char *strxtr(char *src, size_t n)
{
	/* extract n characters from src
	 * return array containing extracted string
	 * extracted characters are deleted from src
	 */
	char *xtr = (char *) malloc(sizeof(char) * n + 1);
	memcpy(xtr, src, n);
	xtr[n] = '\0';
	memmove(src, &src[n], strlen(&src[n]) + 1);
	return xtr;
}

char *strins(char **loc, size_t pos, const char *src, size_t n)
{
	/* insert string src into location at &dest[pos]
	 * loc must point to mem block returned by *alloc function
	 * &dest[pos] will be realloc'd and shifted n bytes right
	 */
	char *dest = *loc; /* realloc */
	dest = (char *) realloc(dest, strlen(dest) + n + 1);
	memmove(&dest[pos + n], &dest[pos], strlen(&dest[pos]) + 1);
	memcpy(&dest[pos], src, n);
	*loc = dest;
	return dest;
}

char *strdup(const char *str)
{
	/* duplicates string */
	if (!str || !*str)
		return NULL;
	long len = strlen(str);
	char *dup = (char *) malloc(sizeof(char) * len + 1);
	strcpy(dup, str);
	return dup;
}

char *strstr_r(const char *haystack, const char *needle)
{
	/* find last occurrence of needle in haystack */
	int len_h = strlen(haystack);
	int len_n = strlen(needle);
	int i, j;
	for (i = len_h - 1; i >= 0; i--)
	{
		int matches = 0;
		for (j = len_n - 1; j >= 0; j--)
		{
			if (i + j <= len_h)
			{
				if (haystack[i + j] == needle[j])
					matches++;
			}
			else
				break;
		}
		if (matches == len_n) /* return location */
			return (char *) &haystack[i];
	}
	return NULL;
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
				str[i] = (str[i] << 4) | base16(str[i+j]);
			memmove(&str[i+1], &str[i+3], strlen(&str[i+3]) + 1);
		}
	}
	return str;
}


size_t utf8_charcount(const char *str)
{
	/* counts significant character bytes in UTF-8 strings */
	unsigned count = 0;
	while (*str++)
		if ((*str & 0xC0) != 0x80) count++;
	return count;
}

int utf8_sequence_length(const char c)
{
	/* returns sequence size of current UTF-8 byte
	 * if continuation byte, return 0
	 */
	static const unsigned char byte[] = { 0x00, 0x80, 0xC2, 0xE0, 0xF0 };
	static const int len[] = { 1, 0, 2, 3, 4 };
	unsigned i;
	for (i = 1; i < static_size(byte); i++)
		if (c >= byte[i - 1] && c < byte[i])
			break;
	return len[i - 1];
}

char *utf8_truncate(const char *src, size_t n)
{
	/* truncate UTF-8 string up to n - 1 characters
	 * return truncated string
	 */
	char *dest = (char *) calloc(strlen(src) + 1, sizeof(char));
	unsigned i;
	for (i = 0; src[i] && utf8_charcount(dest) < n; i++)
		dest[i] = src[i];
	if (n == utf8_charcount(dest)) /* limit reached */
		while (i > 0 && utf8_sequence_length(dest[i]) > 1)
			dest[i--] = '\0';
	return dest;
}

const char *time_human(size_t sec)
{
	/* takes time in seconds and returns pointer to
	 * static buffer containing human readable time string
	 * not thread safe
	 */
	static char buf[1024] = { 0 };
	static const size_t lmt[] = {
		60,
		60 * 60,
		60 * 60 * 24,
		60 * 60 * 24 * 30,
		60 * 60 * 24 * 30 * 12
	};
	static const char *const units[] = {
		"second", "minute", "hour", "day", "month", "year"
	};
	unsigned i, iters = 0, idx = 0;
	for (i = 0; i < static_size(units); i++)
		if (sec < lmt[i]) break;
	do
	{
		long time = (!i) ? sec % lmt[0] : sec % lmt[i] / lmt[i - 1];
		if (time || (!i && !iters)) /* edge case: 0 seconds */
		{
			char ins[100];
			sprintf(ins, "%ld %s%s, ", time, units[i], (time == 1) ? "" : "s");
			unsigned len = strlen(ins);
			strcpy(&buf[idx], ins);
			idx += len;
			iters++;
		}
	} while (i--);
	idx = strlen(buf) - 1;
	while (buf[idx] == ' ' || buf[idx] == ',')
		buf[idx--] = '\0';
	return buf;
}

char *strip_whitespace(char *str)
{
	/* strips excessive whitespace
	 * newlines come in pairs because '\n' is received as "\r\n"
	 */
	static const unsigned MAX_CONSECUTIVE = 3 * 2;
	/* [code] tags exempt */
	struct substr *extract = substr_extract(str, fmt[CODE_L], fmt[CODE_R]);
	unsigned count = 0;
	unsigned i;
	for (i = 0; str[i]; i++) /* --> */
	{
		if (wspace(str[i]))
			count++;
		else
		{
			if (count > MAX_CONSECUTIVE)
			{
				memmove(&str[i-count+1], &str[i], strlen(&str[i]) + 1);
				if (i - count == 0) /* replace with space or nothing */
					memmove(&str[0], &str[1], strlen(&str[1]) + 1);
				else
					str[i - count] = ' ';
			}
			count = 0;
		}
	}
	unsigned len = strlen(str) - 1;
	while (wspace(str[len])) /* <-- */
		str[len--] = '\0';
	substr_restore(extract, str);
	return str;
}

char *xss_sanitize(char **loc)
{
	/* replaces html syntax with escape codes in place
	 * char ** pointer required to update stack pointer
	 * with new string location because realloc loses the original
	 */
	char *str = *loc;
	unsigned i;
	for (i = 0; str[i]; i++)
	{
		if (escape(str[i]))
		{
			unsigned offset = strlen(escape(str[i]));
			str = (char *) realloc(str, strlen(str) + offset + 1);
			memmove(&str[i+offset], &str[i+1], strlen(&str[i+1]) + 1);
			memcpy(&str[i], escape(str[i]), offset);
		}
	}
	*loc = str;
	return str;
}

int spam_filter(const char *str)
{
	/* rudimentary spam filter
	 * blocks low-effort posts with spammy keywords
	 * eg. code tag markup spammed back to back
	 */
	const unsigned SPAM_LIMIT = 2;
	const unsigned APPROX = 20; /* space between instances */
	const char *const spam[] = { fmt[CODE_L], fmt[SPOILER_L] };
	unsigned count = 0;
	unsigned i, j;
	for (i = 0; i < sizeof(spam) / sizeof(*spam); i++)
	{
		char *prev = NULL;
		unsigned spam_len = strlen(spam[i]);
		for (j = 0; str[j]; j++)
		{
			char *next = strstr(&str[j], spam[i]);
			if (!next)
				break;
			unsigned dist = (unsigned) (next - prev);
			count += (dist <= spam_len + APPROX);
			prev = next;
			j = next - str;
		}
	}
	return (count >= SPAM_LIMIT);
}

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
	const char *p = (strlen(pass) < 3) ? NULL : pass; /* too short? */
	char salt[5] = { !p ? ' ' : p[1], !p ? ' ' : p[2], 'H', '.', '\0' };
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
	return *trip = '!', trip;
}
