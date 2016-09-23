#ifndef UTF8_H
#define UTF8_H

/* string library */
char *strdup(const char *str);

/* utf8 routines */
char *utf8_rewrite(char *str);
unsigned utf8_charcount(const char *str);

/* [code] tag extraction */
char *codetag_extract(char *str);
void codetag_restore(char *str, char *extract);

/* input sanitation */
char *strip_whitespace(char *str);
char *xss_sanitize(char **loc);
int spam_filter(const char *str);

/* tripcode routines */
char *tripcode_pass(char **nameptr);
char *tripcode_hash(const char *pass);

#endif
