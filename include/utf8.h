#ifndef UTF8_H
#define UTF8_H

/* lookup tables */
extern const unsigned char base16[]; /* 1-byte values */
extern const unsigned char wspace[]; /* booleans */
extern const char *const escape[]; /* static strings */
#define base16(c) base16[(unsigned char) (c)]
#define wspace(c) wspace[(unsigned char) (c)]
#define escape(c) escape[(unsigned char) (c)]

/* format tags */
extern const char *const fmt[];
enum format_tag {
	SPOILER_L, SPOILER_R,
	CODE_L, CODE_R,
	/* [code] must come last
	 * this is enforced by static assert
	 */
	SUPPORTED_TAGS /* total count */
};

/* string library */
int atoi_s(const char *nptr);
int uintlen(unsigned long n);
char *strxtr(char *from, size_t n);
char *strins(char **loc, size_t pos, const char *src, size_t n);
char *strdup(const char *str);
char *strstr_r(const char *haystack, const char *needle);

/* UTF-8 routines */
char *utf8_rewrite(char *str);
size_t utf8_charcount(const char *str);
int utf8_sequence_length(const char c);
char *utf8_truncate(const char *src, size_t n);

/* input sanitation */
const char *time_human(size_t sec);
char *strip_whitespace(char *str);
char *xss_sanitize(char **loc);
int spam_filter(const char *str);

/* tripcode routines */
char *tripcode_pass(char **nameptr);
char *tripcode_hash(const char *pass);

#endif
