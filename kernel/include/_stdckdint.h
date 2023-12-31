#if __has_include(<stdckdint.h>)
# include <stdckdint.h>
#else
# ifdef __GNUC__
#  define ckd_add(R, A, B) __builtin_add_overflow ((A), (B), (R))
#  define ckd_sub(R, A, B) __builtin_sub_overflow ((A), (B), (R))
#  define ckd_mul(R, A, B) __builtin_mul_overflow ((A), (B), (R))
# else
#  error "we need a compiler extension for this"
# endif
#endif