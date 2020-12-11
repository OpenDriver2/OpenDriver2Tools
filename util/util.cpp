// string util

#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32

#include <direct.h>

#define CORRECT_PATH_SEPARATOR			'\\'
#define INCORRECT_PATH_SEPARATOR		'/'

#else // POSIX

#define CORRECT_PATH_SEPARATOR			'/'
#define INCORRECT_PATH_SEPARATOR		'\\'

#include <sys/stat.h>
#include <errno.h>

#endif

void FixPathSlashes(char* pathbuff)
{
	while (*pathbuff)
	{
		if (*pathbuff == INCORRECT_PATH_SEPARATOR) // make unix-style path
			*pathbuff = CORRECT_PATH_SEPARATOR;
		pathbuff++;
	}
}

bool mkdirRecursive(const char* path, bool includeDotPath)
{
	char temp[1024];
	char folder[265];

	strcpy(temp, path);
	FixPathSlashes(temp);
	
	char* end = (char*)strchr(temp, CORRECT_PATH_SEPARATOR);

	while (end != NULL)
	{
		int result;

		strncpy(folder, temp, end - temp);
		folder[end - temp] = 0;

		// stop on file extension?
		if (strchr(folder, '.') != NULL && !includeDotPath)
			break;
		
		result = _mkdir(folder); 

		if (result > 0 && result != EEXIST)
			return false;

		if (*end == 0)
			break;
		
		char* next = strchr(++end, CORRECT_PATH_SEPARATOR);
		
		if (next)
			end = next;
		else
			end += strlen(end);
	}

	return true;
}

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