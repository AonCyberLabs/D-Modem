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
 *	modem_debug.c  --  modem debug module (dummy).
 *
 *	Author: Sasha K (sashak@smlink.com)
 *
 *
 */

/*****************************************************************************/

#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <limits.h>

#include <modem.h>
#include <modem_debug.h>

unsigned int modem_debug_level=0;
unsigned int modem_debug_logging=0;
unsigned int dsplibs_debug_level=0;

static const char *modem_debug_logfile = "slmodem.log";

static int modem_log_fd;

/* logging */

int modem_debug_log_data(struct modem *m, unsigned id, const void *data, int count)
{
	struct sllog_header hdr;
	int ret = 0;
	if(id <= modem_debug_logging && modem_log_fd > 0) {
		hdr.modem_id = (unsigned)m ;
                hdr.id = id;
                hdr.length = count;
		ret = write(modem_log_fd,&hdr,sizeof(hdr));
		if(ret < 0) {
			fprintf(stderr,"log_data: cannot write header: %s\n",
				strerror(errno));
			return ret;
		}
		ret = write(modem_log_fd,data,count);
		if(ret < 0) {
			fprintf(stderr,"log_data: cannot write data: %s\n",
				strerror(errno));
			return ret;
		}
	}
	return ret;
}



/* printing */

static int debug_vprintf(unsigned level, const char *fmt, va_list args)
{
	static char debug_temp[512];
        struct timeval tv;
        int i, len;
	gettimeofday(&tv,NULL);
	i = snprintf(debug_temp,sizeof(debug_temp),"<%03ld.%06ld> ",
		     (tv.tv_sec % 1000), tv.tv_usec);
        len = vsnprintf(debug_temp + i, sizeof(debug_temp) - i, fmt, args);
	if(modem_debug_logging)
		modem_debug_log_data(0,
				     MODEM_DBG_PRINT_MSG,debug_temp,len + i);
	if(level <= modem_debug_level)
		fprintf( stderr, "%s", debug_temp);
        return len;
}


int dsplibs_debug_printf(const char *fmt, ...)
{
        va_list args;
        int len;
	va_start(args, fmt);
	len = debug_vprintf(dsplibs_debug_level, fmt, args);
        va_end(args);
        return len;
}

int modem_debug_printf(const char *fmt, ...)
{
        va_list args;
        int len;
	va_start(args, fmt);
	len = debug_vprintf(1, fmt, args);
        va_end(args);
        return len;
}


int modem_debug_init(const char *suffix)
{
	dsplibs_debug_level = modem_debug_level;
	if(modem_debug_logging) {
		char path_name[PATH_MAX];
		const char *name;
		if(suffix) {
			snprintf(path_name, sizeof(path_name),
				"%s.%s", modem_debug_logfile, suffix);
			name = path_name;
		}
		else
			name = modem_debug_logfile;
		modem_log_fd = creat(name,S_IREAD|S_IWRITE);
		if(modem_log_fd < 0) {
			fprintf(stderr,"cannot create `%s': %s\n",
				name,strerror(errno));
			return -1;
		}
		if(dsplibs_debug_level < 3)
			dsplibs_debug_level = 3;
	}
	return 0;
}

void modem_debug_exit()
{
	if(modem_log_fd > 0) {
		close(modem_log_fd);
		modem_log_fd = 0;
	}
}

