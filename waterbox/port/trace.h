/* trace.h - the trace logger (port/trace.c): what the driver calls. */
#ifndef CHIMERA_TRACE_H
#define CHIMERA_TRACE_H

/* the line buffer, allocated once, in Init (before the seal) */
void chimera_trace_init(void);

#endif
