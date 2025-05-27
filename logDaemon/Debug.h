
#include <errno.h>

#ifdef DEBUG
#  define Debug(f, v) fprintf(stderr, f, v);
#else
#  define Debug(f, v) ;
#endif

#define PERROR(m) \
  {               \
    perror(m);    \
    exit(1);      \
  }
