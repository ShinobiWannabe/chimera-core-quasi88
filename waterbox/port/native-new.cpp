/* native-new.cpp - the native reference build only, beside native-malloc.c:
 * C++'s new returns zeroed memory too. --wrap=malloc reaches only the calls
 * in the core's own objects, and operator new lives in libstdc++, whose
 * malloc is not wrapped; defining it here replaces it for the program, so
 * fmgen's objects start at zero as they do in the guest. */
#include <cstdlib>
#include <new>

static void *zeroed(std::size_t size)
{
	void *p = std::calloc(1, size ? size : 1);
	if (!p) throw std::bad_alloc();
	return p;
}

void *operator new(std::size_t size) { return zeroed(size); }
void *operator new[](std::size_t size) { return zeroed(size); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept { return std::calloc(1, size ? size : 1); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return std::calloc(1, size ? size : 1); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
