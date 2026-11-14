/* native-malloc.c - the native reference build only (build-native.sh links
 * it with -Wl,--wrap=malloc): every malloc returns zeroed memory, as a
 * guest's fresh memory is, so memory QUASI88 allocates and reads before it
 * writes cannot make the native and the sandboxed runs differ. */
#include <stdlib.h>

void *__wrap_malloc(size_t size)
{
	return calloc(1, size ? size : 1);
}
