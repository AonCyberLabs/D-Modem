
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
 *    dp_dummy.c  --  dummy data pump (very stupid simulator).
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */

#include <modem.h>
#include <modem_debug.h>


#define DUMMY_SKIP     100
#define DUMMY_TIMEOUT  300

#define DUMMY_A1  0xff


struct dummy_dp {
	struct dp dp;
	unsigned long count;
	unsigned long linked;
	u8 buf[1024];
	void *dcr;
};

static struct dp *dummy_create(struct modem *m, enum DP_ID id,
			       int caller, int srate, int max_frag,
			       struct dp_operations *op)
{
	struct dummy_dp *d;
	dprintf("dummy_create...\n");
	d = malloc(sizeof(*d));
	memset(d,0,sizeof(*d));
	d->dp.id   = id;
	d->dp.modem = m;
	d->dp.op = op;
	d->dp.dp_data = d;
	d->dcr = m->dcr;
	m->dcr = NULL;
	return (struct dp *)d;
}

static int dummy_delete(struct dp *dp)
{
	struct dummy_dp *d = (struct dummy_dp *)dp;
	dprintf("dummy_delete...\n");
	dp->modem->dcr = d->dcr;
	free(d);
	return 0;
}

static int dummy_process(struct dp *dp, void *in, void *out, int cnt)
{
	struct dummy_dp *d = (struct dummy_dp *)dp;
	int n = cnt << MFMT_SHIFT(dp->modem->format);
	s16 *s;
	int i;
	d->count++;
	if (!d->linked) {
		if (d->count < DUMMY_SKIP) {
			memset(out,0,n);
			return DPSTAT_OK;
		}
		else if(d->count > DUMMY_TIMEOUT)
			return DPSTAT_ERROR;
		s = out;
		for(i = 0 ; i < cnt ; i++) {
			*s = (1-2*(i&1))*('A');
			s++;
		}
		s = in;
		for(i = 0 ; i < cnt ; i++) {
			if(abs(*s) == 'A') {
				d->linked = 1;
				return DPSTAT_CONNECT;
			}
			s++;
		}
	}
	else {
		s = in;
		for(i = 0 ; i < cnt ; i++) {
			d->buf[i] = *s&0x1;
			s++;
		}
		modem_put_bits(dp->modem,1,d->buf,cnt);
		s = out;
		modem_get_bits(dp->modem,1,d->buf,cnt);
		for(i = 0 ; i < cnt ; i++) {
			*s = d->buf[i]&1 ;
			s++;
		}
	}
	return DPSTAT_OK;
}


static struct dp_operations dummy_operations = {
	name : "Dummy DataPump driver",
	create: dummy_create,
	delete: dummy_delete,
	process:dummy_process
};



int dp_dummy_init(void)
{
	return modem_dp_register(DP_DUMMY,&dummy_operations);
}

void dp_dummy_exit(void)
{
	modem_dp_deregister(DP_DUMMY,&dummy_operations);
}


