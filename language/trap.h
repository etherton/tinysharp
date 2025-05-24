#ifdef _NDEBUG
#define trapf(...)
#define trapge(...)
#define trapgt(...)
#define traple(...)
#define traplt(...)
#else
#include <assert.h>
#define trap() __builtin_trap()
#define trapf(c,...) assert(c)
#define trapge(a,b) do if ((a)>=(b)) trap(); while(0)
#define trapgt(a,b) do if ((a)>(b)) trap(); while(0)
#define traple(a,b) do if ((a)<=(b)) trap(); while(0)
#define traplt(a,b) do if ((a)<(b)) trap(); while(0)
#endif
