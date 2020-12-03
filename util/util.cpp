// string util
#include <ctype.h>
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

int xstrsplitws(char* str, char **pointer_array)
{
	char c = *str;

	int num_indices = 0;

	bool bAdd = true;

	while(c != '\0')
	{
		c = *str;

		if(bAdd)
		{
			pointer_array[num_indices] = str;
			num_indices++;
			bAdd = false;
		}

		if( isspace(c) )
		{
			bAdd = true;
			*str = '\0'; // make null-string
		}

		str++;
	}

	return num_indices;
}