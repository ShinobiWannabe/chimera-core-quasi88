/* system.c - the rest of QUASI88's system-dependent layer for the Chimera
 * core: events, the wait between frames, and what upstream's
 * src/sysdepend/mini/main.c gives besides main(). Modelled on
 * src/sysdepend/mini.
 *
 * Nothing here reads the host. Input reaches the machine from the driver
 * (q88-driver.c), which writes the keyboard ports before each frame; the
 * wait between frames is the frontend's, so QUASI88's own never waits; and
 * the time QUASI88's alarms count is one frame a frame. */
#include <stdio.h>

#include "quasi88.h"
#include "device.h"
#include "event.h"
#include "keyboard.h"
#include "wait.h"
#include "suspend.h"
#include "menu.h"

/* ---------------------------------------------------------------- events */

void event_init(void) {}
void event_update(void) {}
void event_exit(void) {}

void event_get_mouse_pos(int *x, int *y)
{
	*x = 0;
	*y = 0;
}

int event_keylayout_change(void) { return FALSE; }
void event_keylayout_revert(void) {}
void event_switch(void) {}
int event_get_joystick_num(void) { return 0; }

/* ---------------------------------------------------------------- the wait */

int wait_vsync_init(void) { return TRUE; }
void wait_vsync_exit(void) {}

void wait_vsync_setup(long vsync_cycle_us, int do_sleep)
{
	(void)vsync_cycle_us;
	(void)do_sleep;
}

/* Every frame is on time: QUASI88's automatic frame skip never skips. */
int wait_vsync_update(void) { return WAIT_JUST; }

void wait_get_current_time(union wait_time *time) { (void)time; }

/* One frame, in milliseconds: QUASI88's alarms (alarm.c) count it. */
int wait_calc_elasped_time_ms(const union wait_time *got_time)
{
	(void)got_time;
	return (int)(1000 / 55.4);
}

/* ---------------------------------------------------------------- main.c's */

int stateload_system(void) { return TRUE; }
int statesave_system(void) { return TRUE; }

int menu_about_osd_msg(int req_japanese, int *result_code, const char *message[])
{
	(void)req_japanese;
	(void)result_code;
	(void)message;
	return FALSE;
}
