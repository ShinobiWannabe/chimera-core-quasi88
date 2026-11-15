/* config.h - QUASI88's system-dependent definitions for the Chimera core.
 *
 * QUASI88 includes "config.h" from its sysdepend folder; the Chimera core
 * has its own sysdepend folder, this one (waterbox/port), modelled on
 * upstream's src/sysdepend/mini. Sound (USE_SOUND, USE_FMGEN) is switched
 * on by q88-flags.sh, as upstream's CMakeLists.txt does. */
#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#ifndef QUASI88_CHIMERA
#define QUASI88_CHIMERA
#endif

/* x86-64 and aarch64 are both little-endian */
#define LSB_FIRST

#define Q_COMMENT		"Chimera core"

/* the picture is drawn 32 bits a pixel, B, G, R, A in memory (port/graph.c) */
#undef  SUPPORT_8BPP
#undef  SUPPORT_16BPP
#ifndef SUPPORT_32BPP
#define SUPPORT_32BPP
#endif

#ifndef INLINE
#define INLINE			static __inline__
#endif

/* Data Chimera keeps out of every savestate (emulibc's ECL_INVISIBLE),
 * for upstream code the Chimera patches touch: patches/0004 puts the trace
 * logger's disassembly scratch there. */
#define CHIMERA_INVISIBLE	__attribute__((section(".ldata.invis")))

#ifndef PI
#define PI				3.14159265358979323846
#endif
#ifndef M_PI
#define M_PI			PI
#endif

#endif /* CONFIG_H_INCLUDED */
