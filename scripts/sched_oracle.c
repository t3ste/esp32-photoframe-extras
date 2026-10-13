// Test driver for scripts/gen_sched_fixtures.py: runs the firmware's own scheduler (main/cron.c and
// main/sched_pick.c) over a day and prints, for every minute in which a schedule fires, who fires
// and who draws. Never part of the firmware.
//
// stdin lines: "tz <POSIX TZ>", "gap <minutes>", "sched <hold> <cron rule>" (an Agenda schedule, in
// priority order), "rot <cron rule>" (a rule of the photo rotation), "from <epoch>", "minutes <n>".
// stdout: "HH:MM fires[S1 R ] draws=S1" ("draws=-": every fire of that minute is held off).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cron.h"
#include "sched_pick.h"

#ifdef _WIN32
#define SET_TZ(tz) _putenv_s("TZ", tz)
#define TZ_INIT() _tzset()
#else
#define SET_TZ(tz) setenv("TZ", tz, 1)
#define TZ_INIT() tzset()
#endif

int main(void)
{
    char line[256], tz[96] = "UTC0";
    int gap = 15, n = 0, nr = 0, minutes = 0;
    time_t from = 0;
    cron_rule_t rules[8], rot[8];
    int holds[8];
    char names[8][8];
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "tz ", 3)) {
            strncpy(tz, line + 3, sizeof tz - 1);
        } else if (!strncmp(line, "gap ", 4)) {
            gap = atoi(line + 4);
        } else if (!strncmp(line, "from ", 5)) {
            from = (time_t) atoll(line + 5);
        } else if (!strncmp(line, "minutes ", 8)) {
            minutes = atoi(line + 8);
        } else if (!strncmp(line, "sched ", 6)) {
            int h = atoi(line + 6);
            const char *p = strchr(line + 6, ' ');
            if (p && n < 7 && cron_parse(p + 1, &rules[n])) {
                holds[n] = h;
                snprintf(names[n], 8, "S%d", n + 1);
                n++;
            } else {
                fprintf(stderr, "bad sched line: %s\n", line);
            }
        } else if (!strncmp(line, "rot ", 4)) {
            if (nr < 7 && cron_parse(line + 4, &rot[nr])) {
                nr++;
            } else {
                fprintf(stderr, "bad rot line\n");
            }
        }
    }
    SET_TZ(tz);
    TZ_INIT();
    sched_entity_t ent[8];
    for (int i = 0; i < n; i++) {
        ent[i].rules = &rules[i];
        ent[i].n_rules = 1;
        ent[i].hold_min = holds[i] > 0 ? holds[i] : gap;
    }
    int count = n;
    if (nr > 0) {
        ent[count].rules = rot;
        ent[count].n_rules = nr;
        ent[count].hold_min = gap;
        count++;
    }
    from -= from % 60;
    for (int m = 0; m < minutes; m++) {
        time_t t = from + (time_t) m * 60;
        int k = sched_drawn_at(ent, count, t);
        struct tm lt;
        localtime_r(&t, &lt);
        int fired = 0;
        char who[64] = "";
        for (int j = 0; j < count; j++) {
            if (sched_matches(ent, count, t, j)) {
                fired++;
                strcat(who, j < n ? names[j] : "R");
                strcat(who, " ");
            }
        }
        if (fired || k >= 0) {
            printf("%02d:%02d fires[%s] draws=%s\n", lt.tm_hour, lt.tm_min, who,
                   k < 0 ? "-" : (k < n ? names[k] : "R"));
        }
    }
    return 0;
}
