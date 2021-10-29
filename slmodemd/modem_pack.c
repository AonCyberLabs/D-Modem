
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
 *    modem_pack.c  --  modem packer module.
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */

#include <modem.h>
#include <modem_debug.h>

#define PACK_DBG(fmt,arg...) dprintf("%s: " fmt , m->name , ##arg)
#define PACK_ERR(fmt,arg...) eprintf("%s: " fmt , m->name , ##arg)

/* HDLC definitions */

#define HDLC_FLAG   0x7e
#define HDLC_ESCAPE (0x1f<<3)


/* bit ordering (reverse) table */
static const u8 _ordered[] = {
        0x0,0x8,0x4,0xc,0x2,0xa,0x6,0xe,
        0x1,0x9,0x5,0xd,0x3,0xb,0x7,0xf};
#define REVERSE_BITS(c) { (c) = _ordered[(c)&0xf]<<4 | _ordered[(c)>>4] ; }


/*
 *    Async packer
 *
 */

#define START_BIT   0
#define STOP_BIT    1
#if 0
#define STOP_BITS(p)    ((p)->stop_bits)
#define DATA_SIZE(p)    ((p)->data_bits)
#define CHAR_SIZE(p)    ((p)->char_size)
#define CHAR_DATA(p,ch) ( ( ~(0x1<<(CHAR_SIZE(p)-1)) & \
                            ((ch)<<(p)->stop_bits) ) | \
                          ((0x1<<(p)->stop_bits)-1) )
#else
#define STOP_BITS(p)    1
#define DATA_SIZE(p)    8
#define CHAR_SIZE(p)    10
#define CHAR_DATA(p,ch) (((ch)<<1)|0x1)
#endif

int modem_async_get_bits(struct modem *m, int nbits, u8 *bit_buf, int bit_cnt)
{
	struct bits_state *s = &m->pack;
	int ret = 0;
	u8 ch;
	do {
		/* fill bits */
		while ( bit_cnt > 0 && s->bit >= nbits) {
			*bit_buf++ = (s->data>>(s->bit-nbits));
			s->bit -= nbits;
			ret ++;
			bit_cnt--;
		}
		if (bit_cnt <= 0)
			break;
		/* get char */
		if (!m->get_chars || m->get_chars(m, (char*)&ch, 1) != 1)
			break;
		REVERSE_BITS(ch);
		s->data <<= CHAR_SIZE(m);
		s->data |=  CHAR_DATA(m,ch);
		s->bit  +=  CHAR_SIZE(m);
	} while (1);
	if (s->bit && bit_cnt > 0) {
		/* left data + stops */
		*bit_buf++ = (s->data<<(nbits-s->bit))|((0x1<<(nbits-s->bit))-1);
		ret ++;
		bit_cnt--;
		s->bit = 0;
	}
#if 0
	memcpy(bit_buf,(1<<nbits)-1,bit_cnt);
	ret += bit_cnt;
#else
	while (bit_cnt > 0) {
		/* fill stops */
		*bit_buf++ = (1<<nbits)-1;
		bit_cnt --;
		ret ++;
	}
#endif
	return ret;
}


int modem_async_put_bits(struct modem *m, int nbits, u8 *bit_buf, int bit_cnt)
{
	struct bits_state *s = &m->unpack;
	int ret = 0;
	u8 ch;
	while(bit_cnt > 0) {
		s->data<<= nbits;
		s->data |= *bit_buf++ & ((0x1<<nbits)-1);
		s->bit += nbits;
		bit_cnt --;
		ret++;
		while(s->bit && s->data&(0x1<<(s->bit-1))) /* stop bit */
			s->bit--;
		while (s->bit >= CHAR_SIZE(m)) {
			ch = s->data >>
				(s->bit-(CHAR_SIZE(m)-STOP_BITS(m))); 
			s->bit-= CHAR_SIZE(m);
			REVERSE_BITS(ch);
			if (m->put_chars)
				m->put_chars(m,(const char*)&ch,1);
		}
	}
	return ret;
}

void modem_async_start(struct modem *m)
{
	m->pack.bit  = m->unpack.bit  = 0;
	m->pack.data = m->unpack.data = 0;
        m->bit_timer_func = NULL;
	m->bit_timer = 0;
}


/*
 *    Packer detector
 *
 */

/*
 *  Detection phase description.
 *  originator - transmits ODPs until T400 timer is expired
 *               or until ADP is received.
 *
 *  answer - transmits marks(1) until: detect phase termination,
 *           ODP receipt - send at least 10 ADPs,
 *           detection of start protocol phase (continuous flags).
 * 
 *
 */

/* ODP: 0100010001 11111111 0100010011 11111111 */
#define ODP0     0x88
#define ODP1     0x89 
/* ADP v42: 0101000101 11111111 0110000101 11111111 */
/* ADP v14: 0101000101 11111111 0000000001 11111111 */
#define ADP0     0xa2
#define ADP1_V14 0x00
#define ADP1_V42 0xc2
#define ADP1 ADP1_V42

#define MIN_RX_PATTERNS 2
#define MAX_TX_PATTERNS 16
#define PATTERN_SIZE    36

#define FILL_PATTERN_ODP(pat) { (pat)[0]=ODP0 ; (pat)[1]=ODP1; }
#define FILL_PATTERN_ADP(pat) { (pat)[0]=ADP0 ; (pat)[1]=ADP1; }


static void detector_start(struct modem *m)
{
	PACK_DBG("detector start...\n");
	m->packer.detector.tx_count = 1;
}

static void detector_finish(struct modem *m)
{
	PACK_DBG("detector finished.\n");
	// fixme: modem update config
	m->cfg.ec = m->packer.detector.ec_enable;
	modem_update_status(m,STATUS_PACK_LINK);
}

int modem_detector_get_bits(struct modem *m, int nbits, u8 *bit_buf, int bit_cnt)
{
	struct bits_state *s = &m->pack;
	struct detect_state *d = &m->packer.detector;
	int ret = 0;
	do {
		/* fill bits */
		while ( bit_cnt > 0 && s->bit >= nbits) {
			*bit_buf++ = (s->data>>(s->bit-nbits));
			s->bit -= nbits;
			ret ++;
			bit_cnt--;
		}
		if (bit_cnt <= 0)
			break;
#if 0
                s->data <<= 18;
                s->data |=  (0x1ff<<9)|(d->tx_pattern[d->tx_count++%2]);
                s->bit  +=  18;
#else
                /* get char */
                s->data <<= 9;
                s->data |=  0x1ff;
                s->bit  +=  9;
		if (d->tx_count > 0) {
                        s->data <<= 9;
                        s->data |=  d->tx_pattern[d->tx_count++%2];
                        s->bit  +=  9;
                }
#endif
	} while (1);
	return ret;
}


int modem_detector_put_bits(struct modem *m, int nbits, u8 *bit_buf, int bit_cnt)
{
	struct bits_state *s = &m->unpack;
	struct detect_state *d = &m->packer.detector;
	int ret = 0;
	u8 ch;
	while(bit_cnt > 0) {
		s->data<<= nbits;
		s->data |= *bit_buf++ & ((0x1<<nbits)-1);
		s->bit += nbits;
		bit_cnt --;
		ret++;
		while(s->bit && s->data&(0x1<<(s->bit-1))) /* skip marks */
			s->bit--;
		while (s->bit >= 10 ) {
			ch = s->data >> (s->bit-9);
			s->bit-= 10;
			PACK_DBG("rx pattern: 0x%02x.\n",ch);
			if (ch == HDLC_FLAG) {
				PACK_DBG("hdlc flag detected.\n");
				d->ec_enable = 1;
				m->bit_timer_func = detector_finish;
				m->bit_timer = PATTERN_SIZE;
				continue;
			}
			if (!d->rx_count) {
				d->rx_count = (ch == d->rx_pattern[0]);
				continue;
			}
			if (d->rx_count >= 2)
				continue;
			switch(ch) {
			case ODP1:
				//PACK_DBG("rx ODP.\n");
				if (!m->caller) {
					/* start replay ADP and leave detector */ 
					d->tx_count = 1;
					d->rx_count = 2;
					d->ec_enable = 1;
					m->bit_timer_func = detector_finish;
					m->bit_timer = MAX_TX_PATTERNS*PATTERN_SIZE;
				}
				else
					d->rx_count = 0;
				break;
			case ADP1_V14:
				//PACK_DBG("rx ADP non v42.\n");
				d->rx_count = 2;
				d->ec_enable = 0;
				m->bit_timer_func = detector_finish;
				m->bit_timer = PATTERN_SIZE;
				break;
			case ADP1_V42:
				//PACK_DBG("rx ADP v42.\n");
				m->cfg.ec = 1;
				d->rx_count = 2;
				d->ec_enable = 1;
				m->bit_timer_func = detector_finish;
				m->bit_timer = PATTERN_SIZE;
				break;
			default:
				d->rx_count = 0;
				break;
			}
		}
	}
	return ret;
}

void modem_detector_start(struct modem *m) {
	struct detect_state *d = &m->packer.detector;
	int rate = 28800 ; // fixme: use real bit rate from modem
	d->tx_count = 0; /* mute transmitter */
	d->rx_count = 0;
	d->ec_enable = 0;
	if (m->caller) {
		FILL_PATTERN_ODP(d->tx_pattern);
		FILL_PATTERN_ADP(d->rx_pattern);
		m->bit_timer_func = detector_start;
		m->bit_timer = 64*8 ;
	}
	else {
		FILL_PATTERN_ADP(d->tx_pattern);
		FILL_PATTERN_ODP(d->rx_pattern);
		m->bit_timer_func = detector_finish;
		m->bit_timer = rate*3/4; /* T400 timeout */
	}
	m->pack.bit  = m->unpack.bit  = 0;
	m->pack.data = m->unpack.data = 0;
}


/*
 *    HDLC packer
 *
 */


/* static data */

/* fcs16 table */
static const
u16 fcs16_tab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

#define fcstab	fcs16_tab
#define INIT_FCS16    0xffff  /* Initial FCS16 value */
#define GOOD_FCS16    0xf0b8  /* Good final FCS16 value */

#define GET_FCS(fcs,c)	(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])
#define INIT_FCS    INIT_FCS16  /* Initial FCS value */
#define GOOD_FCS    GOOD_FCS16  /* Good final FCS value */


/* hdlc bit generators */

#define	HDLC_PUT_BYTE  { \
	for( i = 0 ; i < 8 ; i++ ) { \
		s->data<<=1; \
		s->bit++; \
		if(in&0x1) { \
			h->tx_ones++; \
		        s->data |= 0x1; \
		        if (h->tx_ones==5) { \
			        s->data<<=1; \
			        s->bit++; \
			        h->tx_ones = 0; \
		        } \
                } \
		else { \
			h->tx_ones = 0; \
                } \
		in>>=1; \
	} }


int modem_hdlc_get_bits(struct modem *m, int nbits,
			u8 *bit_buf, int bit_cnt)
{
	struct hdlc_state *h = &m->packer.hdlc;
	register struct bits_state *s = &m->pack;
	register int i, ret = 0;
	register u8 in, mask = ((1<<nbits) - 1);

	do {
		/* fill bits */
		while ( bit_cnt > 0 && s->bit >= nbits) {
			*bit_buf++ = (s->data>>(s->bit-nbits))&mask;
			s->bit -= nbits;
			ret ++;
			bit_cnt--;
		}
		if (bit_cnt <= 0)
			break;
		if (!h->tx_frame) {
			s->data <<= 8;
			s->data |= HDLC_FLAG;
			s->bit  += 8;
			if (h->get_tx_frame &&
			    (h->tx_frame = h->get_tx_frame(h->framer))) {
				h->tx_frame->fcs = INIT_FCS;
				h->tx_frame->count = 0;
			}
		}
		else if(h->tx_frame->count == h->tx_frame->size) {
			/* end frame */
			/* put fcs */
			h->tx_frame->fcs ^= 0xffff;
			in = h->tx_frame->fcs&0xff;
			HDLC_PUT_BYTE;
			in = (h->tx_frame->fcs>>8)&0xff;
			HDLC_PUT_BYTE;
			h->tx_ones = 0;
			/* frame complete callback */
			if (h->tx_complete) 
				h->tx_complete(h->framer, h->tx_frame);
			/* reset tx frame */
			h->tx_frame = NULL;
			/* put flags will done automatically */
		}
		else {
			in = h->tx_frame->buf[h->tx_frame->count];
			h->tx_frame->count++;
			/* fixme: in order to speed up
			   fcs calc: copy it to local var */
			h->tx_frame->fcs = GET_FCS(h->tx_frame->fcs,in);
			HDLC_PUT_BYTE;
		}
	} while (1);
	return ret;
}


int modem_hdlc_put_bits(struct modem *m, int nbits,
			u8 *bit_buf, int bit_cnt)
{
	struct hdlc_state *h = &m->packer.hdlc;
	register struct bits_state *s = &m->unpack;
	register int ret = 0;
	register int i;
	register u8 in, out, mask = ((0x1<<nbits)-1);
	while(bit_cnt > 0) {
		s->data <<= nbits;
		s->data |= *bit_buf++ & mask;
		s->bit += nbits;
		bit_cnt--;
		ret++;

		/* 8 char, 2 possible escapes, +8 estimated flag */
		if ( s->bit < 8 + 8 + 2)
			continue;

		out = 0;
		// fixme: probably may be optimized by handling whole
		// byte with masking
		/* mask = 0x80; */
		for ( i=0 ; i<8 ; i++ ) {
			in = s->data >> (s->bit-8) ;
			/* flag: finish frame */
			if (in == HDLC_FLAG) {
				/* end of frame */;
				if ( !h->rx_frame->count ) {
					//HDLC_DBG("empty.\n");
				}
				/* check frame for errors */
				else if (h->rx_error) {
					PACK_DBG("hdlc frame error.\n");
				}
				else if (i) {
					PACK_DBG("hdlc frame integrity error.\n");
				}
#if 1
				else if ( h->rx_frame->count < 3 ) {
#else
				else if ( h->rx_frame->count < 4 ) {
#endif
					PACK_DBG("hdlc frame size error.\n");
				}
				else if ( h->rx_frame->fcs != GOOD_FCS ) {
					PACK_DBG("hdlc frame fcs error.\n");
				}
				/* good frame */
				else if ( h->rx_complete ) {
					/* reduce fcs size */
					h->rx_frame->count -= 2;
					h->rx_complete(h->framer,h->rx_frame);
				}
				s->bit -= 8;
				h->rx_ones = 0;
				h->rx_error = 0;
				h->rx_frame->fcs   = INIT_FCS;
				h->rx_frame->count = 0;
				break;
			}
			else if(h->rx_error) {
				s->bit--;
				continue;
			}
			/* put reversed data bit */
			out >>= 1;
			if (in&0x80) {
				out |= 0x80;
				h->rx_ones++;
				/* escape or idle reached */
				if (h->rx_ones == 5) {
					if (in&0x40) {
						h->rx_error++;
						h->rx_frame->count = 0;
					}
					s->bit--;
					h->rx_ones = 0;
				}
			}
			else
				h->rx_ones = 0;
			s->bit--;
		}

		/* finish char */
		if(i==8 && !h->rx_error) {
			if (h->rx_frame->count < h->rx_frame->size) {
				h->rx_frame->buf[h->rx_frame->count] = out;
				h->rx_frame->count++;
				h->rx_frame->fcs = GET_FCS(h->rx_frame->fcs,out);
			}
			else { /* overflow */
				PACK_DBG("hdlc rx frame overflow.\n");
				h->rx_error++;
				h->rx_frame->count = 0;
			}
		}
	}

	return ret;
}


void modem_hdlc_start(struct modem *m)
{
	struct hdlc_state *h = &m->packer.hdlc;
	PACK_DBG("hdlc_start...\n");
	memset(h,0,sizeof(*h));
	/* wait for flag to clear rx error state */
	h->rx_error = 1;
	/* rx frame */
	h->rx_frame = &h->_rx_frame;
	h->rx_frame->buf  = h->_rx_buf;
	h->rx_frame->count= 0;
	h->rx_frame->size = sizeof(h->_rx_buf);
	h->rx_frame->fcs  = INIT_FCS;
	/* tx frame */
	h->tx_frame = 0;

	m->pack.bit  = m->unpack.bit  = 0;
	m->pack.data = m->unpack.data = 0;
        m->bit_timer_func = NULL;
	m->bit_timer = 0;
}

#if 0
// temp stupid things
static struct hdlc_frame *tmp_get_frame(void *framer)
{
	frame_t *f;
	struct modem *m = framer;
	if(m->get_chars) {
		f = &m->packer.hdlc._tx_frame;
		f->buf = m->packer.hdlc._tx_buf;
		f->size = m->get_chars(m,f->buf,sizeof(m->packer.hdlc._tx_buf));
		if(f->size <= 0) {
			return NULL;
		}
		PACK_DBG("get frame: %d...\n",f->size);
		return f;
	}
	return NULL;
}
static void tmp_put_frame(void *framer, frame_t *fr)
{
	struct modem *m = framer;
	if(fr->count > 0 && m->put_chars) {
		PACK_DBG("put frame: %d...\n",fr->count);
		m->put_chars(m,fr->buf,fr->count);
	}
	return;
}

void tmp_init(struct modem *m)
{
        m->packer.hdlc.get_tx_frame = tmp_get_frame;
        m->packer.hdlc.rx_complete  = tmp_put_frame;
	m->packer.hdlc.framer = m;
}
#endif


