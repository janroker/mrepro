#include "debug.h"

#ifndef DEBUG
#define DEBUG_PRINTF 0
#else
#define DEBUG_PRINTF 1
#endif

/* debug_printf: poziva printf ako je DEBUG=1 */
void debug_printf(char *fmt, ...)
{
    va_list args;
    if (DEBUG_PRINTF == 0)
    {
        return;
    }
    else
    {
        va_start(args, fmt);
        fprintf(stderr, "DEBUG: ");
        vfprintf(stderr, fmt, args);
        fflush(stderr);
        va_end(args);
    }
};
