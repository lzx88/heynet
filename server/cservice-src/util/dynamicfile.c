#include <file/dynamicfile.h>

#ifdef WINDOWS

#define dl_open LoadLibrary
#define dl_close FreeLibrary
#define dl_error GetLastError
#define dl_find_api GetProcAddress


#else
#include <dlfcn.h>

void* dl_open(const char* fname){
	return dlopen(fname, RTLD_NOW);
}
#define dl_close dlclose
#define dl_error dlerror
#define dl_find_api dlsym


#endif