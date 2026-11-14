/* clock.c - the time QUASI88's calendar clock reads, for the Chimera core.
 *
 * pc88main.c's calendar clock (the uPD1990A) asks the host for the time
 * with time(), localtime() and mktime(). q88-flags.sh compiles that file
 * with those names changed to these, so it gets the Start Time setting plus
 * the emulated time since power-on instead (q88-driver.c), and reads it in
 * UTC, which is what the setting is written in: the clock shows the setting
 * exactly as written, whatever the time zone of the host, and the native and
 * sandboxed builds agree. */
#define _DEFAULT_SOURCE
#include <time.h>

#include "chimera-port.h"

time_t q88_time(time_t *t)
{
	time_t now = (time_t)chimera_time_now();
	if (t) *t = now;
	return now;
}

struct tm *q88_localtime(const time_t *t)
{
	static struct tm tm;
	return gmtime_r(t, &tm);
}

time_t q88_mktime(struct tm *tm)
{
	return timegm(tm);
}
