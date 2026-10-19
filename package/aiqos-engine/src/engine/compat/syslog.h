#ifndef AIQOS_COMPAT_SYSLOG_H
#define AIQOS_COMPAT_SYSLOG_H
/* Minimal syslog stub for Windows host unit-test builds */
#define LOG_ERR    3
#define LOG_WARNING 4
#define LOG_INFO   6
#define LOG_DEBUG  7
static inline void syslog(int pri, const char *fmt, ...) { (void)pri; (void)fmt; }
static inline void openlog(const char *id, int opt, int fac) { (void)id; (void)opt; (void)fac; }
static inline void closelog(void) {}
#endif