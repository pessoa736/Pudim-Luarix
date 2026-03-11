#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

typedef long time_t;
typedef long clock_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#define CLOCKS_PER_SEC 1000

time_t time(time_t* t);
clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm* tm);
struct tm* gmtime(const time_t* timer);
struct tm* localtime(const time_t* timer);
size_t strftime(char* s, size_t max, const char* format, const struct tm* tm);

#endif
