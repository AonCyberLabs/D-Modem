/*****************************************************************************/

/*
 *
 *   Copyright (c) 2002, Smart Link Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *       1. Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *       2. Redistributions in binary form must reproduce the above
 *          copyright notice, this list of conditions and the following
 *          disclaimer in the documentation and/or other materials provided
 *          with the distribution.
 *       3. Neither the name of the Smart Link Ltd. nor the names of its
 *          contributors may be used to endorse or promote products derived
 *          from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *
 *	sysdep_common.c  --  sysdep API.
 *
 *	Author: Sasha K (sashak@smlink.com)
 *
 *
 */

/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


/* memory allocations */
void *sysdep_malloc (unsigned int size)
{
	return malloc(size);
}

void sysdep_free (void *mem)
{
	free(mem);
}

/* strings */
size_t sysdep_strlen(const char *s)
{
	return strlen(s);
}
char *sysdep_strcpy(char *d,const char *s)
{
	return strcpy(d,s);
}
char *sysdep_strcat(char *d, const char *s)
{
	return strcat(d,s);
}
int sysdep_strcmp(const char *s1,const char *s2)
{
	return strcmp(s1, s2);
}
char *sysdep_strstr(const char *s1,const char *s2)
{
	return strstr(s1,s2);
}
void *sysdep_memset(void *d,int c,size_t l)
{
	return memset(d,c,l);
}
void *sysdep_memcpy(void *d,const void *s,size_t l)
{
	return memcpy(d,s,l);
}
void *sysdep_memchr(const void *s,int c,size_t l)
{
	return memchr (s,c,l);
}

int sysdep_vsnprintf(char *str, unsigned size, const char *format, va_list ap)
{
        return vsnprintf(str, size, format, ap);
}

int sysdep_sprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;
	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);
	va_end(args);
	return i;
}

