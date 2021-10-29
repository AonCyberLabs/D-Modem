
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
 *    sinus.c  --  sinus data pump.
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */

#include <modem.h>
#include <modem_debug.h>

#ifdef __KERNEL__
#define malloc(size) kmalloc(size,GFP_KERNEL)
#define free(mem)    kfree(mem)
#endif

#define SIN_AMPLITUDE 16384
#define SIN_PERIOD    (sizeof(sintab)/sizeof(sintab[0]))
static
const s16 sintab[] = {
 0, 3196, 6269, 9102, 11585, 13622, 15136, 16069,
 16384, 16069, 15136, 13622, 11585, 9102, 6269, 3196,
 0, -3196, -6269, -9102, -11585, -13622, -15136, -16069,
 -16384, -16069, -15136, -13622, -11585, -9102, -6269, -3196};

#define SIN_DBG(fmt,arg...) dprintf("sin: " fmt , ##arg)

struct sin_state {
	struct dp dp;
	int count;
	int linked;
	u8 buf[1024];
	int      amp;
	unsigned period;
	unsigned phase;
};

static struct dp *sin_create(struct modem *m, enum DP_ID id,
			     int caller, int srate, int max_frag,
			     struct dp_operations *op)
{
	struct sin_state *s;
	SIN_DBG("create...\n");
	s = malloc(sizeof(*s));
	if (!s) return NULL;
	memset(s,0,sizeof(*s));
	s->dp.id   = id;
	s->dp.modem = m;
	s->dp.op = op;
	s->dp.dp_data = s;
	s->phase = 0;
	s->amp   = SIN_AMPLITUDE;
	s->period= SIN_PERIOD;
	return &s->dp;
}

static int sin_delete(struct dp *dp)
{
	struct sin_state *s = dp->dp_data;
	SIN_DBG("delete...\n");
	free(s);
	return 0;
}

static int sin_run(struct dp *dp, void *in, void *out, int cnt)
{
	struct sin_state *s = dp->dp_data;
	s16 *smpl = out;
	int i;
	//SIN_DBG("run...\n");
	for (i=0;i<cnt;i++) {
		smpl[i]  = sintab[s->phase%SIN_PERIOD];
		s->phase = s->phase+1;
	}
	return DPSTAT_OK;
}


static struct dp_operations sin_operations = {
	name : "Sinus DP driver",
	create: sin_create,
	delete: sin_delete,
	process:sin_run
};



int dp_sinus_init(void)
{
	return modem_dp_register(DP_SINUS,&sin_operations);
}

void dp_sinus_exit(void)
{
	modem_dp_deregister(DP_SINUS,&sin_operations);
}


