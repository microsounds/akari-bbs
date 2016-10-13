#ifndef MACROS_H
#define MACROS_H

/* exceptions */
#define abort_now(msg) do { fprintf(stdout, msg); fflush(stdout); exit(1); } while (0)
#define abort_now_fmt(fmt, arg) do { fprintf(stdout, fmt, arg); fflush(stdout); exit(1); } while (0)

/* static assertion */
#ifndef NDEBUG
	#define static_assert(expr) typedef char STATIC_ASSERT_FAIL [(expr)?1:-1]
#else
	#define static_assert(expr)
#endif

/* misc */
#define static_size(p) (sizeof(p) / sizeof(*p))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define each(i, n) (i = 0; i < (n); i++)

#endif
