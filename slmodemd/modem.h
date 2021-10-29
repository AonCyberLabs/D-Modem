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
 *    modem.h - modem definitions
 *
 *    Author: sashak@smlink.com
 */

#ifndef __MODEM_H__
#define __MODEM_H__

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>

#include <modem_defs.h>
#include <modem_homolog.h>
#include <modem_dp.h>
#include <modem_timer.h>


/* modem modes */
enum MODEM_MODE {
	MODEM_MODE_DATA = 0,
	MODEM_MODE_FAX  = 1,
	MODEM_MODE_VOICE = 8
};


/* modem status */
#define STATUS_OK         0x0
#define STATUS_NOCARRIER  0x0002 // 3
#define STATUS_ERROR      0x0004 // 4
#define STATUS_NODIALTONE 0x0010 // 6
#define STATUS_BUSY       0x0020 // 7
#define STATUS_NOANSWER   0x0040 // 8
#define STATUS_DP_LINK    0x0100 // 13
#define STATUS_DP_ERROR   0x0200 // 14
#define STATUS_PACK_LINK  0x0400 // 15
#define STATUS_PACK_ERROR 0x0800 // 16
#define STATUS_EC_LINK    0x1000 // 17
#define STATUS_EC_RELEASE 0x2000 // 18
#define STATUS_EC_ERROR   0x4000 // 19


/*
 *    parameters
 *
 */

#define MODEM_FORMAT MFMT_S16_LE
#define MODEM_RATE   9600 /* 8000 */
#define MODEM_FRAG   (MODEM_RATE/200)


#define MODEM_MIN_RATE 300
#define MODEM_MAX_RATE 56000

#define MODEM_DEFAULT_COUNTRY_CODE 0xb5 /* USA */

#define MODEM_CONFIG_CID   1
#define MODEM_CONFIG_VOICE 1
#define MODEM_CONFIG_FAX   1
#define MODEM_CONFIG_FAX_CLASS1 1

#define MODEM_CONFIG_RING_DETECTOR 1

/*
 *    type definitions
 *
 */


/* modem driver struct */
struct modem_driver {
	const char *name;
        int (*start)(struct modem *m);
        int (*stop )(struct modem *m);
	int (*ioctl)(struct modem *m,unsigned cmd, unsigned long arg);
};

/* modem data pumps driver struct */
struct dp_driver {
	const enum DP_ID id;
	const char *name;
	const char *code;
	struct dp_operations *op;
};


/*
 *
 *    compressor definitions
 *
 */

#define COMP_MAX_CODEWORDS 2048  /* dictionary size   */
#define COMP_MAX_STRING    34    /* max string length */

/* encoder and decoder states */
struct comp_state {
	const char *name;
	enum COMP_MODE {TRANSPARENT,COMPRESSED} mode;
	int escape;        /* escape state: decoder only */
	u16 update_at;     /* update dict. at node  */
	u16 next_free;     /* next free dict entry */
	u16 last_matched;  /* last matched dict entry */
	u16 last_added;    /* last added dict entry */
	u16 cw_size;       /* cw size */
	u16 threshold;     /* cw size threshold */
	u8  escape_char;   /* escape char value */
	int max_string;    /* max string length */
	int dict_size;     /* dictionary size */
	struct dict {
		u8  ch;
		u16 parent;
		u16 child;
		u16 next;
	} *dict;           /* dictionary */
	/* fixme: union may be used? */
	int bit_len;
	u32 bit_data;
	int flushed_len;   /* string length already sent */
	int str_len;       /* string length */
	u8  str_data[COMP_MAX_STRING];
	/* test statistics info : encoder only */
	u16 cmp_last; /* compressed bits per last TEST_SLICE chars */
	u32 cmp_bits; /* total compressed bits */
	u32 raw_bits; /* total non-compressed bits */
	/* temporary */
	int  olen;
	int  ohead;
	char obuf[1024];
};


extern int  modem_comp_init(struct modem *m);
extern int  modem_comp_config(struct modem *m, int dict_size, int max_str);
extern void modem_comp_exit(struct modem *m);

extern int  modem_comp_encode(struct modem *m, u8 ch, char *buf, int n);
extern int  modem_comp_decode(struct modem *m, u8 ch, char *buf, int n);
extern int  modem_comp_flush_encoder(struct modem *m, char *buf, int n);
extern int  modem_comp_flush_decoder(struct modem *m, char *buf, int n);

/*
 *
 *    HDLC definitions
 *
 */

typedef struct hdlc_frame {
	struct hdlc_frame *next;
	int count;
	int size;
	//u16 addr;
	//u16 ctrl;
	u16 fcs;
	u8 *buf;
} frame_t;



/*
 *    LAPM definitions
 *
 */

#define LAPM_MAX_WIN_SIZE    15  /* max supported winsize  */
#define LAPM_MAX_INFO_SIZE  128  /* max supported infosize */

#define LAPM_INFO_FRAMES    LAPM_MAX_WIN_SIZE + 1 /* number of info frames */
#define LAPM_CTRL_FRAMES      8  /* number of ctrl frames */

/* lapm state */
struct lapm_state {
        /* modem link */
        struct modem *modem;
        int state;
        /* sub-states: may be optimized */
        int config;
        /* those DATA sub-states may be optimized */
        int reject;
        int busy;
        int peer_busy;
        u8  cmd_addr;
        u8  rsp_addr;
        u8 vs;
        u8 va;
        u8 vr;
        int rtx_count; /* retransmission counter () */
        unsigned int tx_win_size;
        unsigned int rx_win_size;
        unsigned int tx_info_size;
        unsigned int rx_info_size;
        frame_t *sent_info;
        frame_t *tx_info;
        frame_t *info_list;
        frame_t *tx_ctrl;
        frame_t *ctrl_list;
        /* temporary: internal buffs */
        /* rx buf (used when enter busy state) */
        int rx_count;
        int rx_head;
        u8  rx_buf[LAPM_MAX_INFO_SIZE];
        /* temporary: frames bufs */
        struct {
                frame_t frame;
                u8 ctrl[4];
                u8 info[LAPM_MAX_INFO_SIZE];
        } info_buf[LAPM_INFO_FRAMES];
        struct {
                frame_t frame;
                u8 ctrl[4];
                u8 info[LAPM_MAX_INFO_SIZE];
        } ctrl_buf[LAPM_CTRL_FRAMES];
        /* temporary: debug counters: remove it */
        int info_count; // debug
        int tx_count;   // debug
        int sent_count; // debug
        int ctrl_count; // debug
};


/* modem struct */
struct modem {
	const char *name;
	int pty; /* pty descriptor */
	const char *dev_name, *pty_name; /* and names */
	struct termios termios;
	/* device driver */
	struct modem_driver driver;
	void *dev_data;
        /* modem states */
        unsigned modem_info;
        unsigned state;
	unsigned started;
	unsigned command;
	unsigned result_code;
        unsigned hook;
        unsigned caller;
	unsigned rx_rate;
	unsigned tx_rate;
	enum MODEM_MODE mode; /* data,fax,voice */
        /* modem events (ring,escape,waiting for connect,etc) */
	unsigned event;
	unsigned new_event;
        struct timer event_timer;
	/* modem parameters */
	unsigned min_rate;
	unsigned max_rate;
        unsigned char sregs[256];   /* s-registers */
        const struct homolog_set *homolog; /* homologation parameters */
        /* at processor */
        unsigned int at_count;
        char at_line[64];
        char at_cmd[64];
	/* ring detector */
	unsigned int ring_count;
	unsigned long ring_first;
	unsigned long ring_last;
#ifdef MODEM_CONFIG_RING_DETECTOR
	void *rd_obj;
#endif
	/* cid */
#ifdef MODEM_CONFIG_CID
	unsigned cid_requested;
	void *cid;
#endif
        /* dialer */
        char dial_string[128];
        /* escape counter */
        unsigned escape_count;
	unsigned long last_esc_check;
	/* xmit queue */
	struct xmit_queue {
		int count;
		unsigned int head;
		unsigned int tail;
		unsigned int size;
		unsigned char *buf;
	} xmit;
	/* run time configuration */
	struct modem_config {
		/* ec params */
		u8  ec;
		u8  ec_detector;
		u8  ec_tx_win_size;
		u8  ec_rx_win_size;
		u16 ec_tx_info_size;
		u16 ec_rx_info_size;
		/* compressor params */
		u8  comp;
		int comp_dict_size;
		int comp_max_string;
	} cfg;
        /* modem packer */
        int (*get_chars)(struct modem *m, char *buf, int n);
        int (*put_chars)(struct modem *m, const char *buf, int n);
	//int tx_window;
	//int rx_window;
	int bit_timer;
	void (*bit_timer_func)(struct modem *m);
	void (*packer_process)(struct modem *m, int bits);
	struct bits_state {
		int bit;
		u32 data;
	} pack,unpack;
        int (*get_bits)(struct modem *m, int nbits, u8 *buf, int n);
        int (*put_bits)(struct modem *m, int nbits, u8 *buf, int n);
	union packer_union {
		struct async_state {
			struct packer *packer;
			int data_bits;
			int stop_bits;
			int parity;
			int char_size;
		} async;
		struct detect_state {
			int tx_count;
			u8 tx_pattern[2];
			int rx_count;
			u8 rx_pattern[2];
			int ec_enable;
		} detector;
		struct hdlc_state {
			struct hdlc_frame *tx_frame;
			struct hdlc_frame *rx_frame;
			int rx_error;
			int rx_ones,tx_ones;
			/* framer interface */
			struct hdlc_frame *(*get_tx_frame)(void *framer);
			void (*tx_complete)(void *framer, frame_t *fr);
			void (*rx_complete)(void *framer, frame_t *fr);
			void *framer;
			/* temporary: internal bufs */
			struct hdlc_frame _rx_frame;
			u8 _rx_buf[512];
		} hdlc;
	} packer;
	/* error corrector */
	union {
		struct lapm_state lapm;
	} ec;
	/* compressor */
	struct comp_struct {
		// fixme: redo interface
		int head,count;
		char buf[256];
		struct comp_state encoder;
		struct comp_state decoder;
	} comp;
        /* modem data pump interface */
        unsigned srate;
        unsigned format;
        unsigned frag;
        unsigned dp_requested;
        unsigned automode_requested;
	int update_delay;
	void (*process)(struct modem *m,void *in,void *out,int cnt);
        /* process counter and sample timer */
        unsigned long count;
        unsigned long sample_timer;
	void (*sample_timer_func)(struct modem *);
        /* data pump */
        struct dp_operations **dp_ops;
        struct dp *dp;
	void *dp_runtime;
	/* dc evaluator and cleaner */
	void *dcr;
	/* dsp info data */
	struct dsp_info dsp_info;
	/* voice info */
#ifdef MODEM_CONFIG_VOICE
	void *voice_obj;
	struct voice_info voice_info;
#endif
#ifdef MODEM_CONFIG_FAX
	void *fax_obj;
#endif
};



/*
 *    prototypes
 *
 */

/* device driver --> modem */
extern struct modem *modem_create(struct modem_driver *drv, const char *dev_name);
extern void modem_delete(struct modem *m);
extern int modem_write(struct modem *m, const char *buf, int n);
extern void modem_hangup(struct modem *m);
extern void modem_update_termios(struct modem *m, struct termios *tios);
extern void modem_error  (struct modem *m);
extern void modem_ring   (struct modem *m);
extern void modem_event  (struct modem *m);
extern void modem_process(struct modem *m,void *in,void *out,int cnt);

/* packers && EC */ // FIXME: improve interface
extern void modem_async_start(struct modem *m);
extern int  modem_async_get_bits(struct modem *m,int nbits,u8 *buf,int cnt);
extern int  modem_async_put_bits(struct modem *m,int nbits,u8 *buf,int cnt);
extern void modem_detector_start(struct modem *m);
extern int  modem_detector_get_bits(struct modem *m,int nbits,u8 *buf,int cnt);
extern int  modem_detector_put_bits(struct modem *m,int nbits,u8 *buf,int cnt);
extern void modem_hdlc_start(struct modem *m);
extern int  modem_hdlc_get_bits(struct modem *m,int nbits,u8 *buf,int cnt);
extern int  modem_hdlc_put_bits(struct modem *m,int nbits,u8 *buf,int cnt);

extern int  modem_ec_init(struct modem *m);
extern void modem_ec_exit(struct modem *m);
extern void modem_ec_start(struct modem *m);
extern void modem_ec_stop(struct modem *m);

/* status & config */
extern void modem_update_status(struct modem *m, unsigned status);
extern void modem_update_config(struct modem *m, struct modem_config *cfg);

/* modem parameters access macros */
#define CRLF_CHARS(m)     ((char *)((m)->sregs+SREG_CR_CHAR))
#define MODEM_DP(m)       ((m)->sregs[SREG_DP])
#define MODEM_AUTOMODE(m) ((m)->sregs[SREG_AUTOMODE])

#endif /* __MODEM_H__ */

