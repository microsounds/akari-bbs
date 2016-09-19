#ifndef UTF8_H
#define UTF8_H

/* string library */
char *strdup(const char *str);
char *strrev(char *str);

/* utf8 routines */
char *utf8_rewrite(char *str);
unsigned utf8_charcount(const char *str);

/* input sanitation */
char *strip_whitespace(char *str);
char *xss_sanitize(char **loc);

/* tripcode routines */
char *tripcode_pass(char **nameptr);
char *tripcode_hash(const char *pass);

#endif
