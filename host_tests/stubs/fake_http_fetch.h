#ifndef FAKE_HTTP_FETCH_H
#define FAKE_HTTP_FETCH_H

// Control block of the scriptable http_fetch_report() stub in fake_http_fetch.c.
#ifdef __cplusplus
extern "C" {
#endif

#define FAKE_REPORT_MAX_CALLS 4

typedef struct {
    int calls;                               // requests seen so far
    char url[FAKE_REPORT_MAX_CALLS][512];    // per request: the URL asked
    char body[FAKE_REPORT_MAX_CALLS][2048];  // per request: the REPORT body sent
    int status[FAKE_REPORT_MAX_CALLS];       // per request: answer (0 = 207)
    const char *response;                    // body returned with a 207
} fake_report_t;

extern fake_report_t fake_report;
void fake_report_reset(void);

#ifdef __cplusplus
}
#endif

#endif
