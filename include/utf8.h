#ifndef UTF8_H
#define UTF8_H

/* lookup tables */
extern const unsigned char base16[];
extern const unsigned char wspace[];
extern const char *const escape[];
#define base16(c) base16[(unsigned char) (c)]
#define wspace(c) wspace[(unsigned char) (c)]
#define escape(c) escape[(unsigned char) (c)]

/* string library */
char *strdup(const char *str);
char *strstr_r(const char *haystack, const char *needle);

/* utf8 routines */
char *utf8_rewrite(char *str);
unsigned utf8_charcount(const char *str);

/* input sanitation */
char *strip_whitespace(char *str);
char *xss_sanitize(char **loc);
int spam_filter(const char *str);

/* tripcode routines */
char *tripcode_pass(char **nameptr);
char *tripcode_hash(const char *pass);

#endif
