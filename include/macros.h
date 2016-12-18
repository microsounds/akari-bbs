#ifndef MACROS_H
#define MACROS_H

/* static assertion */
#ifndef NDEBUG
	#define static_assert(expr) typedef char STATIC_ASSERT_FAIL [(expr)?1:-1]
#else
	#define static_assert(expr)
#endif

/* for-each loop */
/* usage:
	unsigned i;
	for each (i in (res->count)) { ... }
 */
#define in(x) ,x
#define x_each(i, n) (i = 0; i < (n); i++)
#define each(x) x_each(x)

/* misc */
#define static_size(p) (sizeof(p) / sizeof(*p))
#define to_seconds(n) ((60 * 60 * 24) * n)
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

#endif
