#ifndef _DYNAMICFILE_H_
#define _DYNAMICFILE_H_

void* dl_open(const char* fname);
void dl_close(void* handle);
char* dl_error();
void* dl_find_api(void* handle, const char* func_name);

#endif//_DYNAMICFILE_H_