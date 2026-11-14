/* graph.c - QUASI88's screen (src/sysdepend/graph.h) for the Chimera core,
 * modelled on upstream's src/sysdepend/mini/graph.c.
 *
 * QUASI88 draws into a buffer of 32-bit pixels whose values it takes from
 * graph_add_color: here B, G, R and a zero byte in memory. The driver copies
 * it into the BGRA buffer Chimera reads, setting the alpha. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quasi88.h"
#include "graph.h"
#include "device.h"

/* the largest screen QUASI88 asks for: SCREEN_SIZE_FULL, no status bar or
 * toolbar (the driver's command line asks for that size) */
#define MAX_W 640
#define MAX_H 480	/* QUASI88 asks the window to allow 480: its toolbar and status bar,
			   which the command line turns off, so the screen is 640x400 */

static T_GRAPH_SPEC graph_spec;
static T_GRAPH_INFO graph_info;
static uint32_t *g_buffer;
static int g_width = MAX_W, g_height = MAX_H;

const uint32_t *chimera_screen(int *width, int *height)
{
	*width = g_width;
	*height = g_height;
	return g_buffer;
}

const T_GRAPH_SPEC *graph_init(void)
{
	graph_spec.window_max_width = MAX_W;
	graph_spec.window_max_height = MAX_H;
	graph_spec.fullscreen_max_width = 0;
	graph_spec.fullscreen_max_height = 0;
	graph_spec.touchkey_window = FALSE;
	graph_spec.touchkey_fullscreen = FALSE;
	graph_spec.touchkey_notify = NULL;
	graph_spec.forbid_half = TRUE;
	return &graph_spec;
}

const T_GRAPH_INFO *graph_setup(int width, int height, int fullscreen, double aspect)
{
	(void)fullscreen;
	(void)aspect;
	if (width > MAX_W || height > MAX_H) return NULL;
	if (!g_buffer) {
		g_buffer = (uint32_t *)calloc((size_t)MAX_W * MAX_H, sizeof *g_buffer);
		if (!g_buffer) return NULL;
	}
	g_width = width;
	g_height = height;
	graph_info.fullscreen = FALSE;
	graph_info.width = width;
	graph_info.height = height;
	graph_info.byte_per_pixel = 4;
	graph_info.byte_per_line = width * 4;
	graph_info.buffer = g_buffer;
	graph_info.nr_color = 255;
	graph_info.write_only = FALSE;
	graph_info.broken_mouse = FALSE;
	graph_info.draw_start = NULL;
	graph_info.draw_finish = NULL;
	graph_info.dont_frameskip = NULL;
	return &graph_info;
}

void graph_exit(void)
{
}

void graph_add_color(const PC88_PALETTE_T color[], int nr_color, unsigned long pixel[])
{
	int i;
	/* no alpha: QUASI88 combines the text over the graphics with bit
	 * operations on these values and takes black to be 0, as SDL's pixel
	 * values without alpha are; the driver adds the alpha as it copies */
	for (i = 0; i < nr_color; i++)
		pixel[i] = ((unsigned long)color[i].red << 16) |
		           ((unsigned long)color[i].green << 8) | (unsigned long)color[i].blue;
}

void graph_remove_color(int nr_pixel, unsigned long pixel[])
{
	(void)nr_pixel;
	(void)pixel;
}

void graph_update(int nr_rect, T_GRAPH_RECT rect[])
{
	(void)nr_rect;
	(void)rect;
}

void graph_set_window_title(const char *title)
{
	(void)title;
}

void graph_set_attribute(int mouse_show, int grab, int keyrepeat_on, int *result_show, int *result_grab)
{
	(void)mouse_show;
	(void)grab;
	(void)keyrepeat_on;
	*result_show = TRUE;
	*result_grab = FALSE;
}
