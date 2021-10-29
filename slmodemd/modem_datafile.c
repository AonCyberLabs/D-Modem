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
 *	modem_datafile.c  --  modem data file stuff.
 *
 *	Author: Sasha K (sashak@smlink.com)
 *
 */

/*****************************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <modem_defs.h>
#include <modem_debug.h>

#define DBG(fmt,args...) dprintf( fmt , ##args );


struct datafile_record {
	unsigned minor;
	struct dsp_info dsp_info;
	unsigned size;
};



int datafile_load_info(char *file_name,struct dsp_info *info)
{
	struct datafile_record *dr;
	int ret = -1;
	int fd;

	DBG("open file: %s...\n",file_name);
	fd = open(file_name, 0, 0);
	if(fd < 0) {
		DBG("cannot open '%s': %s\n",
		    file_name,strerror(errno));
		return -errno;
	}

        dr = (struct datafile_record *)malloc(sizeof(*dr));
        if (!dr) {
		close(fd);
		return -errno;
	}

	ret = read(fd,(char *)dr,sizeof(*dr));
	if (ret < 0) {
		free(dr);
		close(fd);
		return -errno;
	}

	/* simple validation */
	if(dr->size != sizeof(*dr)) {
		free(dr);
		close(fd);
		return -1;
	}

	memcpy(info,&dr->dsp_info,sizeof(*info));

	free(dr);
	close(fd);
	return ret;
}


int datafile_save_info(char *file_name,struct dsp_info *info)
{
	struct datafile_record *dr;
	int ret = -1;
	int fd;

	fd = open(file_name,O_CREAT|O_WRONLY,(S_IRUSR|S_IWUSR));
	if(fd < 0)
		return -errno;

        dr = (struct datafile_record *)malloc(sizeof(*dr));
        if (!dr) {
		return -errno;
		close(fd);
	}

	memcpy(&dr->dsp_info,info,sizeof(*info));
	dr->size = sizeof(*dr);

	ret = write(fd,(char *)dr,sizeof(*dr));

	free(dr);
	close(fd);
	return ret;
}



