#ifndef NO_DEBUG
#define DEBUG_PRINTF(...)  fprintf(stderr, __VA_ARGS__);
#else
#define DEBUG_PRINTF(...);
#endif
