/* filename.h - the names QUASI88 gives the files it would write. The core
 * writes none of them (port/file-op.c refuses every file that is not a
 * mounted ROM or disk), but upstream's code builds the names. */
#ifndef FILENAME_H_INCLUDED
#define FILENAME_H_INCLUDED

#ifndef QUASI88_FDUMMY
#define QUASI88_FDUMMY
#endif

#define CONFIG_SUFFIX			".ini"
#define CONFIG_FILENAME			"quasi88"
#define KEYCONF_FILENAME		"keyconf"
#define STATE_SUFFIX			".sta"
#define STATE_FILENAME			"quasi88"
#define SNAPSHOT_FILENAME		"save"
#define WAVEOUT_FILENAME		"wave"

#endif /* FILENAME_H_INCLUDED */
