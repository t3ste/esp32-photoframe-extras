// Windows (mingw) stand-in for the POSIX localtime_r() that main/cron.c and main/sched_pick.c use;
// only for scripts/gen_sched_fixtures.py (passed with `gcc -include`).
#pragma once
#ifdef _WIN32
#include <time.h>
static inline struct tm *localtime_r(const time_t *t, struct tm *out)
{
    return localtime_s(out, t) == 0 ? out : 0;
}
#endif
