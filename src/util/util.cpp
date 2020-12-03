// string util
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

char* varargs(const char* fmt, ...)
{
	va_list		argptr;

	static int index = 0;
	static char	string[4][4096];

	char* buf = string[index];
	index = (index + 1) & 3;

	memset(buf, 0, 4096);

	va_start(argptr, fmt);
	vsnprintf(buf, 4096, fmt, argptr);
	va_end(argptr);

	return buf;
}
