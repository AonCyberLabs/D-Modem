
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
 *    modem_comp.c  --  modem compressor module.
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */


#include <modem.h>
#include <modem_debug.h>


/*
 *  TODO list:
 *  - modify interface: clean temporary out buffs
 *  - char char encoding/decoding
 *  - simplify and clean code:
 *    - decoder
 *    - encoder test compression case, do it after string finishing
 *  - preformance trace and optimize
 *
 */

//#define COMP_TEST  1
//#define COMP_DEBUG 1

/* V42bis parameters limitation */
#define MIN_DICT_SIZE 512   /* min dictionary size enabled by protocol */
#define MIN_STR_SIZE    6   /* min string size enabled by protocol     */
#define MAX_STR_SIZE  250   /* max string size enabled by protocol     */

/* V42bis constants */
#define CHAR_SIZE      8                     /* char size (bits)          N3 */
#define TOTAL_CHARS  256                     /* number of chars           N4 */
#define TOTAL_CTRLS    3                     /* number of control cw      N6 */
#define FIRST_ENTRY TOTAL_CTRLS              /* first dictionary entry       */
#define FIRST_FREE  TOTAL_CHARS+FIRST_ENTRY  /* initial val of free entry N5 */

/* control codewords */
#define CCW_ETM        0   /* Enter transparent mode */
#define CCW_FLUSH      1   /* Flash data             */
#define CCW_STEPUP     2   /* Step up codeword size  */
/* command codes */
#define CC_ECM         0   /* Enter compression mode    */
#define CC_EID         1   /* Escape charactrer in data */
#define CC_RESET       2   /* Force reinitialization    */

#define ESCAPE_STEP    51  /* escape char value step */
#define TEST_SLICE    256  /* compression test slice (bytes) */


/* fixme: use get pages */
#define COMP_ALLOC(size) malloc(size)
#define COMP_FREE(mem)   free(mem)

/* debug stuff */
#if COMP_DEBUG
#define INLINE_FUNC static
#define COMP_DBG(fmt,arg...) dprintf("comp %d/%d: " fmt , \
                                     c->cmp_bits , c->raw_bits , ##arg)
#if 1
#define COMP_DBG1(fmt,arg...)
#else
#define COMP_DBG1(fmt,arg...) COMP_DBG(fmt , ##arg)
static char *show_string(struct comp_state *c, u16 cw)
{
	static char _str[COMP_MAX_STRING+1];
	int j;
	_str[COMP_MAX_STRING] = 0;
	for(j = COMP_MAX_STRING-1; cw && j >= 0 ; j--) {
		_str[j] = c->dict[cw].ch;
		cw = c->dict[cw].parent;
	}
	return _str+j+1;
}
#endif // 0
#else  /* !COMP_DEBUG */
#define INLINE_FUNC extern inline
#define COMP_DBG(fmt,arg...)
#define COMP_DBG1(fmt,arg...)
#endif /* !COMP_DEBUG */

#define COMP_ERR(fmt,arg...) eprintf("err: " fmt , ##arg)


/*
 *  common procedures
 *
 */

/* initialize dictionary and params */
static void dict_init(struct comp_state *c)
{
	int i;
	for(i=0;i<c->dict_size;i++) {
		c->dict[i].parent = 0;
		c->dict[i].child  = 0;
		c->dict[i].next   = 0;
	}
	c->next_free    = FIRST_FREE;
	c->last_matched = 0;
	c->update_at	= 0;
	c->last_added   = 0;
	c->cw_size      = CHAR_SIZE + 1;
	c->threshold    = TOTAL_CHARS<<1;
	c->bit_len      = 0;
	c->flushed_len  = 0;
	c->str_len      = 0;
	c->escape_char  = 0;
	c->cmp_last     = TEST_SLICE<<3;
	c->mode         = TRANSPARENT;
	c->escape	= 0;
}

/* match one char */
INLINE_FUNC u16 match_char(struct comp_state *c, u16 at, u8 ch)
{
	register u16 e;
	if (!at)
		return ch + FIRST_ENTRY;
	e = c->dict[at].child;
	while(e) {
		if (c->dict[e].ch == ch)
			return e;
		e = c->dict[e].next;
	}
	return 0;
}

/* add one char */
INLINE_FUNC u16 add_char(struct comp_state *c, u16 at, u8 ch)
{
	register u16 new = c->next_free;
	register u16 next;
	/* add */
	c->dict[new].ch = ch;
	c->dict[new].parent = at;
	c->dict[new].child = 0;
	c->dict[new].next = c->dict[at].child;
	c->dict[at].child = new;
	/* update next free */;
	next = new;
	do {
		if (++next == c->dict_size)
			next = FIRST_FREE;
	} while(c->dict[next].child);
	if (c->dict[next].parent) {
		u16 e = c->dict[next].parent;
		if(c->dict[e].child == next)
			c->dict[e].child = c->dict[next].next;
		else {
			e = c->dict[e].child;
			while(c->dict[e].next != next)
				e = c->dict[e].next;
			c->dict[e].next = c->dict[next].next;
		}
	}
	c->next_free = next;
	return new;
}




/*
 *  decoder
 *
 */

/* out decoded string */
INLINE_FUNC int send_string(struct comp_state *c)
{
	int ret;
#if 0
	c->str_data[c->str_len] = 0;
	COMP_DBG("out string %d: '%s'\n",c->last_matched,c->str_data);
#endif
	memcpy(c->obuf+c->olen,c->str_data,c->str_len);
	ret = c->str_len;
	c->olen += ret;
	c->str_len = 0;
	c->flushed_len = 0;
	return ret;
}

/* receive encoded data */
INLINE_FUNC int recv_data(struct comp_state *c, u16 cw)
{
	int i,ret = 0;
	u16 p = cw;
	COMP_DBG1("dec recv %d: '%s'\n",cw,show_string(c,cw));
	for (ret = 0 ; p ; ret++) {
		p = c->dict[p].parent;
	}
	p = cw;
	i = c->str_len + ret - 1;
	while(p) {
		c->str_data[i--] = c->dict[p].ch;
		p = c->dict[p].parent;
	}
	c->str_len += ret;
	return ret;
}

/* flush decoder */
int comp_flush_decoder(struct comp_state *c)
{
	int ret, total_len;
	total_len = c->flushed_len + c->str_len;
	ret = send_string(c);
	c->flushed_len = total_len;
	if (ret) {
		COMP_DBG("flush_decoder: %d...\n", ret);
	}
	return ret;
}

/* decoder */
int comp_decode(struct comp_state *c, u8 *in, int n)
{
	register int i, ret = 0;
	register u16 cw, p;
	u8 ch;
	for (i = 0 ; i<n ;) {
		switch (c->mode) {
		case COMPRESSED:
			/* get cw from input */
			while (c->bit_len < c->cw_size && i<n) {
				c->bit_data |= *in++ << c->bit_len;
				c->bit_len += 8;
				i++;
			}
			if (c->bit_len < c->cw_size)
				break;

			cw = c->bit_data&((1<<c->cw_size)-1);
			c->bit_data >>= c->cw_size;
			c->bit_len -= c->cw_size;

			/* process cw */
			switch (cw) {
				/* control cw */
			case CCW_ETM: /* enter transparent mode */
				COMP_DBG("C decoder: ETM\n");
				c->bit_len = 0;
				c->mode = TRANSPARENT;
				c->last_matched = 0;
				c->last_added = 0;
				break;
			case CCW_FLUSH: /* flush signal */
				COMP_DBG("C decoder: FLUSH\n");
				c->bit_len = 0;
				break;
			case CCW_STEPUP: /* increase cw size */
				c->cw_size++;
				c->threshold <<= 1;
				COMP_DBG("C decoder: STEPUP %d\n", c->cw_size);
				if (c->cw_size > (c->dict_size>>3)) { /* C-ERROR */
					COMP_ERR("fatal: cw size too big!\n");
					return -1;
				}
				break;
			default:       /* regular codeword */
				if (cw == c->next_free) { /* C-ERROR */
					COMP_ERR("fatal: next free!\n");
					return -1;
				}

				ret += recv_data(c,cw);
				if (c->update_at) {
					ch = c->str_data[0];
					p = match_char(c,c->update_at,ch);
					if (!p) {
						// fixme: prevent case add 303 at 303
						c->last_added = add_char(c,c->update_at,ch);
						COMP_DBG1("C dec add %d at %d: %d(%c) '%s'\n",
							 c->last_added,c->update_at,
							 ch,ch,show_string(c,c->last_added));
						if (cw == c->next_free) { /* C-ERROR */
							COMP_ERR("fatal: empty cw!\n");
							return -1;
					}
					}
					else if (p == c->last_added)
						c->last_added = 0;
				}
				if (c->str_len + c->flushed_len == c->max_string) {
					c->update_at = 0;
				}
				else {
					c->update_at = cw;
				}
				/* process escape */
				// fixme: separate it in func (send_string ?)
				{	int j;
					for (j=0;j<c->str_len;j++) {
						if(c->str_data[j]==c->escape_char) {
							COMP_DBG("C dec: ESCAPE %d\n", c->escape_char);
							c->escape_char+=ESCAPE_STEP;
						}
					}
				}
				send_string(c);
				break;
			}
			break;
		case TRANSPARENT:
			if (c->escape) { /* command */
				c->escape = 0;
				switch(*in) {
				case CC_ECM: /* enter compressed mode */
					COMP_DBG("T decoder: ECM\n");
					ret += send_string(c);
					c->mode = COMPRESSED;
					c->update_at = c->last_matched;
					c->last_matched = 0;
					in++;i++;
					continue;
					//break;
				case CC_EID: /* escape symbol */
					COMP_DBG("T decoder: EID\n");
					// fixme: don't rewrite in buf
					*in = c->escape_char;
					c->escape_char += ESCAPE_STEP;
					break;
				case CC_RESET: /* reset dict */
					COMP_DBG("T decoder: RESET\n");
					// TBD
					ret += send_string(c);
					dict_init(c);
					in++;i++;
					continue;
					//break;
				default: /* C-ERROR */
					COMP_ERR("fatal: invalid cmd!\n");
					return -1;
				}
			}
			else if (*in == c->escape_char) {
				COMP_DBG("T dec ESCAPE %d\n", *in);
				c->escape = 1;
				in++; i++;
				break;
			}

			//COMP_DBG1("dec rcv: %d(%c)\n", *in, *in);

			if (c->update_at) {
				if(!match_char(c,c->update_at,*in)) {
					c->last_added = add_char(c,c->update_at,*in);
					COMP_DBG1("T dec add %d at %d: %d(%c) '%s'\n",
						 c->last_added,c->update_at,
						 *in,*in,show_string(c,c->last_added));
				}
				c->update_at = 0;
			}

			cw = match_char(c,c->last_matched,*in);
			if(!cw) {
				c->update_at = c->last_matched;
				/* out  string */
				ret += send_string(c);
				c->last_matched = 0;
			}
			else if (cw == c->last_added) {
				c->last_added = 0;
				/* out string */
				ret += send_string(c);
				c->last_matched = 0;
			}
			else {
				c->last_matched = cw;
				c->str_data[c->str_len++] = *in++;
				if (c->str_len + c->flushed_len == c->max_string) {
					/* out string */
					ret += send_string(c);
					c->last_matched = 0;
				}
				i++;
			}
			break;
		default:
			return -1;
		}
	}
	return ret;
}


/*
 *  encoder
 *
 */

#define SEND_CW(c,cw)   { (c)->bit_data |= (cw) << (c)->bit_len; \
			  (c)->bit_len += (c)->cw_size; }
#define ALIGN_BITS(c)	{ if((c)->bit_len%8) \
				(c)->bit_len += 8 - (c)->bit_len%8; }
#define SEND_BITS(c)	{ while((c)->bit_len >= 8) { \
				(c)->obuf[c->olen++]=(c)->bit_data&0xff; \
				(c)->bit_data >>= 8; \
				(c)->bit_len -= 8; ret++; \
			  } }



/* send encoded data */
static int send_data(struct comp_state *c, u16 cw)
{
	register int i, ret = 0;

	/* update test info */
	c->raw_bits += c->str_len<<3;
	c->cmp_bits += c->cw_size;
	c->cmp_last += c->cw_size - c->cmp_last*c->str_len/TEST_SLICE;

	switch(c->mode) {
	case TRANSPARENT:
		for (i = 0;i<c->str_len;i++) {
			c->obuf[c->olen++] = c->str_data[i];
			ret++;
			if (c->str_data[i] == c->escape_char) {
				COMP_DBG("T enc ESCAPE %d\n", c->escape_char);
				c->obuf[c->olen++] = CC_EID;
				ret++;
				c->escape_char += ESCAPE_STEP;
			}
			COMP_DBG1("enc send: %d(%c)\n",c->str_data[i],c->str_data[i]);
		}
		break;
	case COMPRESSED:
		/* process escape */
		for (i = 0;i<c->str_len;i++) {
			if (c->str_data[i] == c->escape_char) {
				COMP_DBG("C enc ESCAPE %d\n", c->escape_char);
				c->escape_char += ESCAPE_STEP;
			}
		}
		/* send codeword */
		/* check cw size and send STEPUP if need */
		while (cw >= c->threshold) {
			SEND_CW(c,CCW_STEPUP);
			SEND_BITS(c);
			c->threshold <<= 1;
			c->cw_size++;
			COMP_DBG("C enc STEPUP %d\n",c->cw_size);
		}
		SEND_CW(c,cw);
		SEND_BITS(c);
		COMP_DBG1("enc send %d: '%s'\n",cw,show_string(c,cw));
		break;
	}
	c->str_len = 0;
	c->flushed_len = 0;
	return ret;
}

/* test compression */
static int test_mode(struct comp_state *c)
{
	int ret = 0;
	switch(c->mode) {
	case TRANSPARENT:
		/* fixme: why 11 ? */
		if (c->cmp_last < (TEST_SLICE<<3) - 11) {
			/* switch to compressed mode */
			COMP_DBG("encoder %d/%d %d: --> COMPRESSED mode.\n",
				 c->cmp_bits,c->raw_bits,c->cmp_last);
			if (c->last_matched) {
				c->update_at = c->last_matched;
				ret += send_data(c,c->last_matched);
				c->last_matched = 0;
			}
			c->obuf[c->olen++] = c->escape_char;
			c->obuf[c->olen++] = CC_ECM;
			ret += 2;
			c->bit_data = 0;
			c->mode = COMPRESSED;
		}
#if 0
		else if (0) { /* fixme: when RESET? */
			COMP_DBG("encoder RESET\n");
		}
#endif
		break;
	case COMPRESSED:
		if (c->cmp_last > (TEST_SLICE<<3)) {
			/* switch to transparent mode */
			COMP_DBG("encoder %d/%d %d: --> TRANSPARENT mode.\n",
				 c->cmp_bits,c->raw_bits,c->cmp_last);
			if (c->last_matched) {
				c->update_at = c->last_matched;
				ret += send_data(c,c->last_matched);
				c->last_matched = 0;
			}
			c->last_added = 0;
			SEND_CW(c,CCW_ETM);
			ALIGN_BITS(c);
			SEND_BITS(c);
			c->mode = TRANSPARENT;
		}
		break;
	}
	return ret;
}

/* flush encoder data */
int comp_flush_encoder(struct comp_state *c)
{
	int ret = 0;
	//COMP_DBG1("flush_encoder...\n");
#if 1 /* fixme: use something better */
	if (c->update_at) {
		return 0;
	}
#endif
	if (c->last_matched) {
		int total_len = c->flushed_len + c->str_len;
		ret = send_data(c,c->last_matched);
		c->flushed_len = total_len;
	}
	if(c->mode == COMPRESSED) {
		c->update_at = c->last_matched;
		c->last_matched = 0;
		c->flushed_len = 0;
		//if (c->bit_len > 0) {
			COMP_DBG("flush_encoder: send FLUSH\n");
			/* send FLUSH and align by zeros */
			SEND_CW(c,CCW_FLUSH);
			ALIGN_BITS(c);
			SEND_BITS(c);
			//}
	}
	if (ret)
		COMP_DBG("flush_encoder %d...\n", ret);
	return ret;
}


/* encoder */
#define NEW_ENCODER 1
int comp_encode(struct comp_state *c, u8 *in, int n)
{
	register int i, ret=0;
	register u16 cw = 0;

	for(i=0;i<n;) {
		if (c->update_at) {
			if (!match_char(c,c->update_at,*in)) {
				c->last_added = add_char(c,c->update_at,*in);
				COMP_DBG1("enc add %d at %d: %d(%c) '%s'\n",
					 c->last_added,c->update_at,
					 *in,*in,show_string(c,c->last_added));
			}
			c->update_at = 0;
		}
#if NEW_ENCODER
		/* match string */
		while(i<n) {
			cw = match_char(c,c->last_matched,*in);
			if (!cw) {
				c->update_at = c->last_matched;
				ret += send_data(c,c->last_matched);
				c->last_matched = 0;
				break;
			}
			if (cw == c->last_added) {
				c->last_added = 0;
				ret += send_data(c,c->last_matched);
				c->last_matched = 0;
				break;
			}
			c->last_matched = cw;
			/* collect char */
			c->str_data[c->str_len++] = *in++; i++;
			if ( c->str_len + c->flushed_len == c->max_string ) {
				ret += send_data(c,c->last_matched);
				c->last_matched = 0;
				break;
			}
		}
		ret += test_mode(c);
	}
#else
		/* match char */
		cw = match_char(c,c->last_matched,*in);
		if (!cw) {
			/* add char */
			c->update_at = c->last_matched;
			/* send and reset string */
			ret += send_data(c,c->last_matched);
			c->last_matched = 0;
		}
		else if (cw == c->last_added) {
			c->last_added = 0;
			/* send and reset string */
			ret += send_data(c,c->last_matched);
			c->last_matched = 0;
		}
		else {
			c->last_matched = cw;
			/* collect char */
			c->str_data[c->str_len++] = *in++;
			if ( c->str_len + c->flushed_len == c->max_string ) {
				/* send and reset string */
				ret += send_data(c,c->last_matched);
				c->last_matched = 0;
			}
			i++;
		}
	}
	ret += test_mode(c);
#endif
	return ret;
}


#if COMP_TEST
/*
 * Test encoder procedures
 *
 */

/* force reset */
int comp_force_reset(struct comp_state *c)
{
	int ret = 0;
	if (c->mode != TRANSPARENT)
		return 0;
	COMP_DBG("comp_reset...\n");
	if (c->last_matched) {
		ret = send_data(c,c->last_matched);
		c->last_matched = 0;
	}
	c->obuf[c->olen++] = c->escape_char;
	c->obuf[c->olen++] = CC_RESET;
	ret += 2;
	c->bit_data = 0;
	dict_init(c);
	return ret;
}

/* force change compression mode */
int comp_force_change_mode(struct comp_state *c, enum COMP_MODE mode)
{
	int ret = 0;
	if (mode == c->mode)
		return 0;
	COMP_DBG("change_mode...\n");
	if (c->last_matched) {
		c->update_at = c->last_matched;
		ret += send_data(c,c->last_matched);
		c->last_matched = 0;
	}
	/* switch to compressed mode */
	if (c->mode == TRANSPARENT) {
		COMP_DBG("encoder %ld/%ld %d: switch to COMPRESSED mode.\n",
			 c->cmp_bits,c->raw_bits,c->cmp_last);
		c->obuf[c->olen++] = c->escape_char;
		c->obuf[c->olen++] = CCW_ECM;
		ret += 2;
		c->bit_data = 0;
		c->mode = COMPRESSED;
	}
	/* switch to transparent mode */
	else if (c->mode == COMPRESSED) {
		COMP_DBG("encoder %ld/%ld %d: switch to TRANSPARENT mode.\n",
			 c->cmp_bits,c->raw_bits,c->cmp_last);
		SEND_CW(c,CCW_ETM);
		ALIGN_BITS(c);
		SEND_BITS(c);
		c->mode = TRANSPARENT;
	}
	return ret;
}
#endif


/*
 *  init procedures
 *
 */


int comp_init(struct comp_state *c,int dict_size,int max_str)
{
	struct dict *d;
	int i;
	memset(c,0,sizeof(*c));
	if (dict_size < MIN_DICT_SIZE ||
	    max_str < MIN_STR_SIZE || max_str > MAX_STR_SIZE)
		return -1;
	/* alloc dict */
	d = COMP_ALLOC(dict_size*sizeof(*d));
	if (!d)
		return -1;
	COMP_DBG("comp_init: dict size %d, max str %d (dict %d bytes).\n",
			dict_size,max_str,dict_size*sizeof(*d));
	memset(d,0,dict_size*sizeof(*d));
	c->max_string= max_str;
	c->dict_size = dict_size;
	c->dict = d;
	/* init dict */
	for(i=0;i<TOTAL_CHARS;i++) {
		c->dict[i+FIRST_ENTRY].ch = i;
	}
	dict_init(c);
	/* temporary */
	c->olen = 0;
	return 0;
}

int comp_config(struct comp_state *c,int dict_size,int max_str)
{
	if (dict_size < MIN_DICT_SIZE ||
	    max_str < MIN_STR_SIZE || max_str > MAX_STR_SIZE)
		return -1;
	if (dict_size > c->dict_size || max_str > sizeof(c->str_data))
		return -1;
	c->max_string= max_str;
	c->dict_size = dict_size;
	//dict_init(c);
	/* temporary */
	//c->olen = 0;
	return 0;
}

int comp_exit(struct comp_state *c)
{
	COMP_DBG("comp exit: %d/%d %d\n",
		 c->cmp_bits,c->raw_bits,c->cmp_last);
	c->dict_size = 0;
	if(c->dict) {
		COMP_FREE(c->dict);
		c->dict = NULL;
	}
	return 0;
}

/*
 *  modem init procedures
 *
 */


/* exported functions prototypes */
extern int comp_init(struct comp_state *c,int dict_size,int max_str);
extern int comp_config(struct comp_state *c,int dict_size,int max_str);
extern int comp_exit(struct comp_state *c);
extern int comp_encode(struct comp_state *c, u8 *in, int n);
extern int comp_decode(struct comp_state *c, u8 *in, int n);
extern int comp_flush_encoder(struct comp_state *c);
extern int comp_flush_decoder(struct comp_state *c);


int modem_comp_init(struct modem *m)
{
	int ret;
	ret = comp_init(&m->comp.encoder,
			m->cfg.comp_dict_size,m->cfg.comp_max_string);
	if(ret)
		return ret;
	ret = comp_init(&m->comp.decoder,
			m->cfg.comp_dict_size,m->cfg.comp_max_string);
	if (ret) {
		comp_exit(&m->comp.encoder);
		return ret;
	}
	return 0;
}

int modem_comp_config(struct modem *m,int dict_size,int max_str)
{
	int ret = 0;
	if ((ret = comp_config(&m->comp.encoder,dict_size,max_str)) ||
	    (ret = comp_config(&m->comp.decoder,dict_size,max_str)) )
		COMP_ERR("modem_comp_config: setup failed (%d/%d).\n",
			 dict_size,max_str);
	return ret;
}


void modem_comp_exit(struct modem *m)
{
	comp_exit(&m->comp.encoder);
	comp_exit(&m->comp.decoder);
	// temproary
	m->comp.head = m->comp.count = 0;
}


// FIXME: remove this stupest things
static int get_odata(struct comp_state *c, char *buf, int n)
{
        int ret = 0;
        if(c->olen > 0) {
                ret = c->olen - c->ohead < n ?
                        c->olen - c->ohead : n;
                memcpy(buf,c->obuf+c->ohead,ret);
                c->ohead += ret;
                if (c->ohead == c->olen) {
                        c->ohead = 0;
                        c->olen = 0;
                }
        }
        return ret;
}

int  modem_comp_encode(struct modem *m, u8 ch, char *buf, int n)
{
	int ret;
	ret = get_odata(&m->comp.encoder,buf,n);
	if (ret == n) return ret;
	if(comp_encode(&m->comp.encoder,&ch,1)<0)
		return -1;
	ret += get_odata(&m->comp.encoder,buf+ret,n-ret);
	return ret;
}

int  modem_comp_decode(struct modem *m, u8 ch, char *buf, int n)
{
	int ret;
	ret = get_odata(&m->comp.decoder,buf,n);
	if(ret == n) return ret;
	if(comp_decode(&m->comp.decoder,&ch,1)<0)
		return -1;
	ret += get_odata(&m->comp.decoder,buf+ret,n-ret);
	return ret;
}

int  modem_comp_flush_encoder(struct modem *m, char *buf, int n)
{
	int ret;
	ret = get_odata(&m->comp.encoder,buf,n);
	if (ret == n) return ret;
	if(comp_flush_encoder(&m->comp.encoder) < 0)
		return -1;
	ret += get_odata(&m->comp.encoder,buf+ret,n-ret);
	return ret;
}

int  modem_comp_flush_decoder(struct modem *m, char *buf, int n)
{
	int ret;
	ret = get_odata(&m->comp.decoder,buf,n);
	if (ret == n) return ret;
	if(comp_flush_decoder(&m->comp.decoder) < 0)
		return -1;
	ret += get_odata(&m->comp.decoder,buf+ret,n-ret);
	return ret;
}
