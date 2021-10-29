
/*
 *
 *    Copyright (c) 2002, Smart Link Ltd.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions
 *    are met:
 *
 *        1. Redistributions of source code must retain the above copyright
 *           notice, this list of conditions and the following disclaimer.
 *        2. Redistributions in binary form must reproduce the above
 *           copyright notice, this list of conditions and the following
 *           disclaimer in the documentation and/or other materials provided
 *           with the distribution.
 *        3. Neither the name of the Smart Link Ltd. nor the names of its
 *           contributors may be used to endorse or promote products derived
 *           from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *
 *    modem_test.c  --  modem simulator.
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <modem.h>
#include <modem_debug.h>

#define INFO(fmt,args...) fprintf(stderr, fmt , ##args );
#define ERR(fmt,args...) fprintf(stderr, "error: " fmt , ##args );

#define DBG(fmt,args...) if(modem_debug_level) \
                             fprintf(stderr, "main: " fmt , ##args );


//#define SIMULATE_RING 1


/* modem init externals : FIXME remove it */
extern int  dp_dummy_init(void);
extern void dp_dummy_exit(void);
extern int  dp_sinus_init(void);
extern void dp_sinus_exit(void);
extern int  prop_dp_init(void);
extern void prop_dp_exit(void);
extern int datafile_load_info(char *name,struct dsp_info *info);
extern int datafile_save_info(char *name,struct dsp_info *info);


/* global config data */
extern unsigned modem_debug_logging;


struct modem_test {
	struct modem *modem;
	struct modem_test *link;
	int in,out;
	unsigned int delay;
	unsigned int started;
	unsigned pty_closed;
	unsigned close_count;
};


#define CLOSE_COUNT_MAX 100

/* static data */
static char inbuf[4096];
static char outbuf[4096];


/* 'driver' simulation */

static int modem_test_start (struct modem *m)
{
	struct modem_test *t = m->dev_data;
	DBG("modem_test_start...\n");
	t->delay = 256;
	t->started = 1;
	memset(outbuf,0,t->delay);
	write(t->out,outbuf,t->delay);
	return 0;
}

static int modem_test_stop (struct modem *m)
{
	struct modem_test *t = m->dev_data;
	DBG("modem_test_stop...\n");
	t->started = 0;
	t->delay = 0;
	return 0;
}

static int modem_test_ioctl(struct modem *m, unsigned int cmd, unsigned long arg)
{
	struct modem_test *t = m->dev_data;
	DBG("modem_test_ioctl: cmd %x, arg %lx...\n",cmd,arg);
        switch (cmd) {
        case MDMCTL_CAPABILITIES:
                return -1;
        case MDMCTL_HOOKSTATE:
		return 0;
        case MDMCTL_SPEED:
                return 0;
        case MDMCTL_GETFMTS:
        case MDMCTL_SETFMT:
                return 0;
        case MDMCTL_SETFRAGMENT:
                return 0;
        case MDMCTL_SPEAKERVOL:
		return 0;
        case MDMCTL_CODECTYPE:
                return CODEC_UNKNOWN;
        case MDMCTL_IODELAY:
		return t->delay/2;
        default:
                break;
        }
	return -2;
}



struct modem_driver modem_test_driver = {
        .name = "modem_test driver",
        .start = modem_test_start,
        .stop = modem_test_stop,
        .ioctl = modem_test_ioctl,
};


/*
 *
 */

static volatile sig_atomic_t keep_running = 1;

void mark_termination(int signum)
{
	DBG("signal %d: mark termination.\n",signum);
	keep_running = 0;
}


static int modem_test_run(struct modem_test *modems)
{
	struct timeval tmo;
	fd_set rset,eset;
	struct termios termios;
	struct modem_test *t;
	void *in;
	int max_fd = 0;
	int count, ret;

#ifdef SIMULATE_RING
#define RING_HZ 20
#define RING_ON  1*RING_HZ
#define RING_OFF 4*RING_HZ
	unsigned rcount = RING_ON;
#endif


	while(keep_running) {

		for( t = modems ; t->modem ; t++ )
			if(t->modem->event)
				modem_event(t->modem);
#ifdef SIMULATE_RING
		tmo.tv_sec = 0;
		tmo.tv_usec= 1000000/RING_HZ;
#else
                tmo.tv_sec = 1;
                tmo.tv_usec= 0;
#endif
                FD_ZERO(&rset);
		FD_ZERO(&eset);
		max_fd = 0;

		for( t = modems ; t->modem ; t++ ) {
			FD_SET(t->in,&rset);
			FD_SET(t->in,&eset);
			if(t->in > max_fd)
				max_fd = t->in;
			if( t->pty_closed && t->close_count ) {
				if ( !t->started ||
				     t->close_count++ > CLOSE_COUNT_MAX )
					t->close_count = 0;
			}
			else if (t->modem->xmit.size - t->modem->xmit.count > 0) {
				FD_SET(t->modem->pty,&rset);
				if(t->modem->pty > max_fd)
					max_fd = t->modem->pty;
			}
		}

                ret = select(max_fd + 1,&rset,NULL,&eset,&tmo);

                if (ret < 0) {
			if (errno == EINTR) {
				continue;
			}
                        ERR("select: %s\n",strerror(errno));
                        return ret;
                }

		//DBG("select = %d\n",ret);
		if (ret == 0) {
#ifdef SIMULATE_RING 
			if(!modems->modem->started) {
				rcount++;
				if (rcount <= RING_ON)
					modem_ring(modems->modem);
				else if (rcount > RING_OFF)
					rcount = 0;
			}
#endif
			continue;
		}

		for( t = modems ; t->modem ; t++ ) {
			if(FD_ISSET(t->in,&eset)) {
				DBG("dev exception...\n");
			}
		}

		for( t = modems ; t->modem ; t++ ) {
			if(FD_ISSET(t->in,&rset)) {
				//DBG("dev read...\n");
				count = read(t->in,inbuf,sizeof(inbuf)/2);
				if(count < 0) {
					ERR("dev read: %s\n",strerror(errno));
					return -1;
				}
				else if (count == 0) {
					DBG("dev read = 0\n");
					continue;
				}
				in = inbuf;
				if(t->modem->update_delay < 0) {
					if ( -t->modem->update_delay >= count/2) {
						DBG("change delay -%d...\n", count/2);
						t->delay -= count;
						t->modem->update_delay += count/2;
						continue;
					}
					DBG("change delay %d...\n", t->modem->update_delay);
					in -= t->modem->update_delay*2;
					count += t->modem->update_delay*2;
					t->delay += t->modem->update_delay*2;
					t->modem->update_delay = 0;
				}
				if(t->started) {
					modem_process(t->modem,in,outbuf,count>>1);
				}
				else {
					memset(outbuf,0,count);
					/* ring here */
				}
				count = write(t->out,outbuf,count);
				if(count < 0) {
					ERR("dev write: %s\n",strerror(errno));
					return -1;
				}
				else if (count == 0) {
					DBG("dev write = 0\n");
				}
				if(t->modem->update_delay > 0) {
					DBG("change delay %d...\n", t->modem->update_delay);
					memset(outbuf, 0, t->modem->update_delay*2);
					count = write(t->out,outbuf,t->modem->update_delay*2);
					if(count < 0) {
						ERR("dev write: %s\n",strerror(errno));
						return -1;
					}
					t->delay += t->modem->update_delay*2;
					t->modem->update_delay = 0;
				}
			}
		}

		for( t = modems ; t->modem ; t++ ) {
			int pty = t->modem->pty;
			if(FD_ISSET(pty,&rset)) {
				//DBG("pty read...\n");
				/* check termios */
				tcgetattr(pty,&termios);
				if(memcmp(&termios,&t->modem->termios,
					  sizeof(termios))) {
					DBG("termios changed.\n");
					modem_update_termios(t->modem, &termios);
				}
				/* read data */
				count = t->modem->xmit.size - t->modem->xmit.count;
				if(count == 0)
					continue;
				if (count > sizeof(inbuf))
					count = sizeof(inbuf);
				count = read(pty,inbuf,count);
				if(count < 0) {
					if(errno == EAGAIN) {
						DBG("pty read, errno = EAGAIN\n");
						continue;
					}
					if(errno == EIO) {
						if(!t->pty_closed) {
							DBG("pty closed.\n");
							t->pty_closed = 1;
							if(termios.c_cflag&HUPCL)
								modem_hangup(t->modem);
						}
						t->close_count = 1;
						//DBG("pty read, errno=EIO\n");
						continue;
					}
					else
						ERR("pty read: %s\n",
						    strerror(errno));
					return -1;
				}
				else if (count == 0) {
					DBG("pty read = 0\n");
				}
				t->pty_closed = 0;
				//DBG("pty read %d\n",count);
				count = modem_write(t->modem,inbuf,count);
				if(count < 0) {
					DBG("modem_write failed.\n");
					return -1;
				}
			}
		}
	}

	return 0;
}


static int modem_test_init(struct modem_test *t, const char *name, int in, int out)
{
	struct termios termios;
	char *pty_name;
	int pty;
	int ret;
	
	memset(t,0,sizeof(*t));

	t->in  = in;
	t->out = out;

        pty  = getpt();
        if (pty < 0 || grantpt(pty) < 0 || unlockpt(pty) < 0) {
                ERR("getpt: %s\n",strerror(errno));
                exit(-1);
        }

        ret = tcgetattr(pty, &termios);
        /* non canonical raw tty */
        cfmakeraw(&termios);
        cfsetispeed(&termios, B115200);
        cfsetospeed(&termios, B115200);

        ret = tcsetattr(pty, TCSANOW, &termios);
        if (ret) {
                ERR("tcsetattr: %s\n",strerror(errno));
                exit(-1);
        }

	fcntl(pty,F_SETFL,O_NONBLOCK);

	pty_name = ptsname(pty);

	t->modem = modem_create(&modem_test_driver,name);
	if(!t->modem) {
		return -1;
	}
	t->modem->name = name;
	t->modem->pty = pty;
	//t->modem->dev = dev;
	t->modem->dev_name = name;
	t->modem->pty_name = pty_name;
	// datafile_load_info(basename(dev_name),&t->modem->dsp_info);
	modem_update_termios(t->modem,&termios);
	t->modem->dev_data = t;

	DBG("created %s: %s\n",t->modem->name,t->modem->pty_name);
	return 0;
}

static void modem_test_free(struct modem_test *t)
{
	int pty = t->modem->pty;
	modem_delete(t->modem);
	close(pty);
}


int modem_test()
{
	struct modem_test *ma, *mb;
	struct modem_test modems[3] = {};
	int pipe1[2], pipe2[2];
	int ret = 0;

	modem_debug_init("test");

	memset(modems,0,sizeof(modems));

	dp_dummy_init();
	dp_sinus_init();
	prop_dp_init();
	modem_timer_init();

	ma = &modems[0];
	mb = &modems[1];

	if(pipe(pipe1) < 0 || pipe(pipe2) < 0) {
		ERR("pipe: %s\n",strerror(errno));
		exit(-1);
	}

	modem_test_init(ma,"modemA",pipe1[0],pipe2[1]);
	modem_test_init(mb,"modemB",pipe2[0],pipe1[1]);

	ma->link = mb;
	mb->link = ma;

	signal(SIGINT,  mark_termination);
	signal(SIGTERM, mark_termination);

	/* main loop here */
	ret = modem_test_run(modems);

	modem_test_free(ma);
	modem_test_free(mb);

	dp_dummy_exit();
	dp_sinus_exit();
	prop_dp_exit();

	exit(ret);
	return 0;
}



int main(int argc, char *argv[])
{
	extern void modem_cmdline(int argc, char *argv[]);
	modem_debug_level = 1;
	modem_cmdline(argc,argv);
	return modem_test();
}
