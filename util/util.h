#ifndef UTIL_H
#define UTIL_H

void	FixPathSlashes(char* pathbuff);
bool	mkdirRecursive(const char* path, bool includeDotPath = false);

char*	varargs(const char* fmt, ...);
int		xstrsplitws(char* str, char** pointer_array);

#ifndef _WIN32	// POSIX
#define _mkdir(str)		mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#define stricmp			strcasecmp
#endif

#endif // UTIL_H