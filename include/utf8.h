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
int uintlen(unsigned long n);
char *strdup(const char *str);
char *strstr_r(const char *haystack, const char *needle);

/* UTF-8 routines */
char *utf8_rewrite(char *str);
unsigned utf8_charcount(const char *str);

/* input sanitation */
char *strip_whitespace(char *str);
char *xss_sanitize(char **loc);
int spam_filter(const char *str);

/* tripcode routines */
char *tripcode_pass(char **nameptr);
char *tripcode_hash(const char *pass);

/* misc macros */
#ifndef NDEBUG
	#define static_assert(expr) typedef char STATIC_ASSERT_FAIL [(expr)?1:-1]
#else
	#define static_assert(expr)
#endif
#define static_size(p) (sizeof(p) / sizeof(*p))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define each(i, n) (i = 0; i < (n); i++)

#endif
