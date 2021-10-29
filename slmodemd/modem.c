
/*
 *
 *    Copyright (c) 2002, Smart Link Ltd.
 *    Copyright (c) 2021, Aon plc
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
 *    modem.c  --  modem core module.
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <modem.h>
#include <modem_debug.h>


#define XMIT_SIZE 4096

#define MODEM_AUTHOR "Smart Link Ltd."
#define MODEM_NAME "SmartLink Soft Modem"
#define MODEM_VERSION "2.9.11"
#define MODEM_DATE __DATE__" "__TIME__

/* event mask */
#define MDMEVENT_RING_CHECK    0x01
#define MDMEVENT_ESCAPE        0x02

/* debug prints */
#define MODEM_INFO(fmt,arg...) printf(fmt , ##arg) ; dprintf(fmt , ##arg)
#define MODEM_DBG(fmt,arg...) dprintf("%s: " fmt , m->name , ##arg)
#define MODEM_ERR(fmt,arg...) eprintf("%s: " fmt , m->name , ##arg)

/* external symbols */
extern int process_at_command(struct modem *m, char *buf);
extern void *dp_runtime_create(struct modem *m);
extern void  dp_runtime_delete(void *runtime);
extern void *dcr_create();
extern void  dcr_delete(void *dcr);
extern void  dcr_process(void *dcr, void *buf, int len);
#ifdef MODEM_CONFIG_RING_DETECTOR
extern void *RD_create(struct modem *m, unsigned rate);
extern void RD_delete(void *obj);
extern int  RD_process(void *obj, void *in, int count);
extern void RD_ring_details(void *obj, long *freq, long *duration);
#endif
#ifdef MODEM_CONFIG_CID
extern void *CID_create(struct modem *m, unsigned rate, unsigned cid_val);
extern void CID_delete(void *cid);
extern int  CID_process(void *cid, void *in, int count);
#endif
#ifdef MODEM_CONFIG_VOICE
extern void *VOICE_create(struct modem *m, unsigned srate);
extern void VOICE_delete(void *obj);
extern int  VOICE_process(void *obj, void *in, void *out, int count);
extern int  VOICE_command(void *obj, enum VOICE_CMD cmd);
#endif
#ifdef MODEM_CONFIG_FAX
extern void *FAX_create(struct modem *m, unsigned caller, unsigned srate);
extern int FAX_process(void *obj, void *in, void *out, int count);
extern void FAX_delete(void *obj);
#endif

/* local prototypes */
int modem_answer(struct modem *m);
static int sregs_init(unsigned char sregs[]);
static void do_modem_change_dp(struct modem *);
static int modem_start(struct modem *);
static int modem_stop (struct modem *);
static struct dp_operations *get_dp_operations(enum DP_ID id);
static int modem_get_chars(struct modem *m, char *buf, int n);
static int modem_put_chars(struct modem *m, const char *buf, int n);
static int modem_comp_get_chars(struct modem *m, char *buf, int n);
static int modem_comp_put_chars(struct modem *m, const char *buf, int n);
#ifdef MODEM_CONFIG_CID
static int  modem_cid_start(struct modem *, unsigned timeout);
#endif
#ifdef MODEM_CONFIG_FAX
static int modem_fax_start(struct modem *m);
#endif

/* global config data */
const char *modem_default_country = NULL;


/* data definitions */
const char modem_author[] = MODEM_AUTHOR;
const char modem_name[]   = MODEM_NAME;
const char modem_version[]= MODEM_VERSION;
const char modem_date[]   = MODEM_DATE;


/*
 *    misc macros
 */

/* ring parameter */
#define RING_ON_MIN(m)   MODEM_HZ*3/20 /* 0.15 sec */
#define RING_ON_MAX(m)   MODEM_HZ*5/2  /* 2.5 sec */
#define RING_OFF_MIN(m)  MODEM_HZ*3/2  /* 1.5 sec */
#define RING_OFF_MAX(m)  MODEM_HZ*13/2 /* 6.5 sec */
#define RING_COUNT_MIN(m) 10

#define TOTAL_RINGS_COUNT(m)  ((m)->sregs[SREG_RING_COUNTER])
#define ANSWER_AFTER_RINGS(m) ((m)->sregs[SREG_RINGS_TO_AUTO_ANSWER])

#define ESCAPE_CHAR(m)        ((m)->sregs[SREG_ESCAPE_CHAR])
#define CR_CHAR(m)            ((m)->sregs[SREG_CR_CHAR])
#define LF_CHAR(m)            ((m)->sregs[SREG_LF_CHAR])
#define BS_CHAR(m)            ((m)->sregs[SREG_BS_CHAR])

#define IS_ECHO(m)            ((m)->sregs[SREG_ECHO])
#define IS_QUIET(m)           ((m)->sregs[SREG_QUIET])
#define IS_VERBOSE(m)         ((m)->sregs[SREG_VERBOSE])
#define IS_AUTOMODE(m)        MODEM_AUTOMODE(m)
#define MODEM_EC_ENABLE(m)    ((m)->sregs[SREG_EC])
#define MODEM_EC_DETECTOR(m)  ((m)->sregs[SREG_EC])
#define MODEM_COMP_ENABLE(m)  ((m)->sregs[SREG_COMP]&&MODEM_EC_ENABLE(m))
#define ESCAPE_TIMEOUT(m)     (MODEM_HZ/2)
#define ANSWER_DELAY(m)       ((m)->sregs[SREG_ANS_DELAY])
#define SPEAKER_CONTROL(m)    ((m)->sregs[SREG_SPEAKER_CONTROL])
#define SPEAKER_VOLUME(m)     ((m)->sregs[SREG_SPEAKER_VOLUME])

#define QC_SKIP_EC_DETECTION(m) ((m)->caller && (m)->dsp_info.qc_lapm)


/*
 *    dp operations drivers
 *
 */

struct dp_driver modem_dp_drivers[] = {
        {DP_CALLPROG,"CallProg"},
        {DP_DUMMY,"Dummy"},
        {DP_AUTOMODE,"Automode"},
        {DP_V8,"V8"},
        {DP_V17,"V17"},
        {DP_V21,"V21","21"},
        {DP_V22,"V22","22"},
        {DP_V23,"V23","23"},
        {DP_V22BIS,"V22bis","122"},
        {DP_V32,"V32","32"},
        {DP_V32BIS,"V32bis","132"},
        {DP_V34,"V34","34"},
        {DP_B103,"Bell103","103"},
        {DP_B212,"Bell212","212"},
        {DP_FAX,"VFax"},
        {DP_K56,"K56Flex","56"},
        {DP_V8BIS,"V8bis"},
        {DP_V90,"V90","90"},
        {DP_V92,"V92","92"},
        {DP_SINUS,"Sinus"},
        {}
};

static struct dp_driver *get_dp_driver(enum DP_ID id)
{
	struct dp_driver *p;
	for(p = modem_dp_drivers ; p->id > 0 ; p++)
		if (p->id == id) return p;
	return NULL;
}

static struct dp_operations *get_dp_operations(enum DP_ID id)
{
	struct dp_driver *p;
	for(p = modem_dp_drivers ; p->id > 0 ; p++)
		if (p->id == id) return p->op;
	return NULL;
}

int modem_dp_register(enum DP_ID id, struct dp_operations *op)
{
	struct dp_driver *p;
	int ret = -1;
	for(p = modem_dp_drivers ; p->id > 0 ; p++) {
		if (p->id == id && !p->op) {
			p->op = op;
			ret = 0;
			break;
		}
	}
	return ret;
}

void modem_dp_deregister(enum DP_ID id, struct dp_operations *op)
{
	struct dp_driver *p;
	for(p = modem_dp_drivers ; p->id > 0 ; p++)
		if (p->id == id) p->op = NULL;
}



/*
 *    state and status handling
 *
 */

enum MODEM_RESULT {
	RESULT_OK         = 0,
	RESULT_CONNECT    = 1,
	RESULT_RING       = 2,
	RESULT_NOCARRIER  = 3,
	RESULT_ERROR      = 4,
	RESULT_UNUSED_5   = 5,
	RESULT_NODIALTONE = 6,
	RESULT_BUSY       = 7,
	RESULT_NOANSWER   = 8,
};

static void modem_report_result(struct modem *m, enum MODEM_RESULT code)
{
#define MODEM_RESPONSE_TEXT(id) modem_responses[id].text
#define MODEM_RESPONSE_CODE(id) modem_responses[id].code
	/* response codes : FIXME */
	static const struct modem_response {
		enum MODEM_RESULT id;
		char *code;
		char *text;
	} modem_responses[] = {
		{RESULT_OK,         "0", "OK"},
		{RESULT_CONNECT,    "1", "CONNECT"},
		{RESULT_RING,       "2", "RING"},
		{RESULT_NOCARRIER,  "3", "NO CARRIER"},
		{RESULT_ERROR,      "4", "ERROR"},
		{RESULT_UNUSED_5,   "" , ""},
		{RESULT_NODIALTONE, "6", "NO DIALTONE"},
		{RESULT_BUSY,       "7", "BUSY"},
		{RESULT_NOANSWER,   "8", "NO ANSWER"},
	};
	static const char none_str[] = "NONE";
        const char *msg;
	u8 msg_mask;

        MODEM_DBG("modem report result: %d (%s)\n",code,MODEM_RESPONSE_TEXT(code));
        if (IS_QUIET(m))
                return;
        if (IS_VERBOSE(m)) {
		if(code==RESULT_CONNECT) {
			msg_mask = modem_get_sreg(m,SREG_CONNNECT_MSG_FORMAT);
			if(msg_mask&1) {
				struct dp_driver *dp_drv;
				modem_put_chars(m,"Modulation: ",12);
				dp_drv = get_dp_driver(m->dp->id);
				msg = (dp_drv) ? dp_drv->name : none_str;
				modem_put_chars(m,msg,strlen(msg));
				modem_put_chars(m,CRLF_CHARS(m),2);
			}
			if(msg_mask&2) {
				modem_put_chars(m,"Protocol: ",10);
				msg = m->cfg.ec ? "LAPM" : none_str;
				modem_put_chars(m,msg,strlen(msg));
				modem_put_chars(m,CRLF_CHARS(m),2);
			}
			if((msg_mask&3) == 3) {
				modem_put_chars(m,"Compression: ",13);
				msg = (m->cfg.ec && m->cfg.comp)? "V42bis" : none_str;
				modem_put_chars(m,msg,strlen(msg));
				modem_put_chars(m,CRLF_CHARS(m),2);
			}
			if(msg_mask&4 && m->tx_rate) {
				char rate_str[16];
				modem_put_chars(m,"TxRate: ",8);
				sprintf(rate_str,"%d",m->tx_rate);
				modem_put_chars(m,rate_str,strlen(rate_str));
				modem_put_chars(m,CRLF_CHARS(m),2);
			}
		}
                msg = MODEM_RESPONSE_TEXT(code);
		modem_put_chars(m,msg,strlen(msg));
		if (code==RESULT_CONNECT && m->mode == MODEM_MODE_DATA
		    && m->rx_rate && modem_get_sreg(m,SREG_X_CODE) != 0) {
			char rate_str[16];
			sprintf(rate_str," %d",m->rx_rate);
			modem_put_chars(m,rate_str,strlen(rate_str));
		}
        }
        else {
                msg = MODEM_RESPONSE_CODE(code);
		modem_put_chars(m,msg,strlen(msg));
        }
        modem_put_chars(m,CRLF_CHARS(m),2);
}


/* TDB: state lifecycle description */

/* modem state constants */
#define STATE_MODEM_IDLE      0x1
#define STATE_DP_ESTAB        0x2
#define STATE_EC_ESTAB        0x4
#define STATE_MODEM_ONLINE    0x5
#define STATE_COMMAND_ONLINE  0x6
#define STATE_EC_DISC         0x7
#define STATE_DP_DISC         0x9

#define STATE_DISC 0x100

#define STATE_ESTAB           STATE_DP_ESTAB

#define IS_STATE_LINKED(stat) ((stat) > STATE_DP_ESTAB)

#define IS_STATE_IDLE(stat) ((stat) == STATE_MODEM_IDLE)
#define IS_STATE_CONNECTING(stat) ((stat) > STATE_MODEM_IDLE && (stat) < STATE_MODEM_ONLINE)
#define IS_STATE_ONLINE(stat) ((stat) == STATE_MODEM_ONLINE || (stat) == STATE_COMMAND_ONLINE)
#define IS_STATE_DISC(stat) ((stat) == STATE_EC_DISC || (stat) == STATE_DP_DISC)


static int modem_set_state(struct modem *m, unsigned new_state)
{
        MODEM_DBG("modem set state: %x --> %x...\n",
		  m->state, new_state);
	if(m->state == new_state)
		return 0;
	switch(new_state) {
	case STATE_MODEM_IDLE:
		MODEM_DBG("new state: MODEM_IDLE\n");
		m->command = 1;
		m->get_chars = 0;
		m->put_chars = 0;
		m->get_bits  = 0;
		m->put_bits  = 0;
		break;
	case STATE_DP_ESTAB:
		MODEM_DBG("new state: DP_ESTAB\n");
		m->command = 0;
		m->get_chars = 0;
		m->put_chars = 0;
		m->get_bits  = 0;
		m->put_bits  = 0;
		break;
	case STATE_EC_ESTAB:
		MODEM_DBG("new state: EC_ESTAB\n");
		m->command = 0;
		m->get_chars = 0;
		m->put_chars = 0;
		break;
	case STATE_MODEM_ONLINE:
		MODEM_DBG("new state: MODEM_ONLINE\n");
		m->command = 0;
		m->put_chars = (m->cfg.ec && m->cfg.comp&0x1)
			? modem_comp_put_chars : modem_put_chars;
		m->get_chars = (m->cfg.ec && m->cfg.comp&0x2)
			? modem_comp_get_chars : modem_get_chars;
		m->modem_info |= TIOCM_CD;
		break;
	case STATE_COMMAND_ONLINE:
		MODEM_DBG("new state: COMMAND_ONLINE\n");
		m->command = 1;
		m->get_chars = 0;
		m->put_chars = 0;
		break;
	case STATE_EC_DISC:
		MODEM_DBG("new state: EC_DISC\n");
		m->get_chars = 0;
		m->put_chars = 0;
		break;
	case STATE_DP_DISC:
		MODEM_DBG("new state: DP_DISC\n");
		m->get_chars = 0;
		m->put_chars = 0;
		m->get_bits  = 0;
		m->put_bits  = 0;
		break;
	default:
		MODEM_ERR("invalid state: %d\n", new_state);
		return -1;
	}
        m->state = new_state;
	return 0;
}


static void run_modem_stop(struct modem *m)
{
	modem_stop(m);
}

/* FIXME */
#define schedule_modem_stop(m) { m->sample_timer_func = run_modem_stop; \
		                 m->sample_timer = m->count + 48; }


static void modem_hup(struct modem *m, unsigned local)
{
	MODEM_DBG("modem_hup...%d\n",m->state);
	switch(m->state) {
	case STATE_MODEM_IDLE:
		return;
	case STATE_DP_ESTAB:
		if(local && m->dp && m->dp->op->hangup )
			m->dp->op->hangup(m->dp);
		modem_set_state(m,STATE_DP_DISC);
		if (m->result_code == 0)
			m->result_code = RESULT_NOCARRIER;
		schedule_modem_stop(m);
		break;
	case STATE_EC_ESTAB:
	case STATE_MODEM_ONLINE:
	case STATE_COMMAND_ONLINE:
		if(local && m->cfg.ec) {
			modem_ec_stop(m);
			modem_set_state(m,STATE_EC_DISC);
		}
		else {
			if(local && m->dp && m->dp->op->hangup)
				m->dp->op->hangup(m->dp);
			modem_set_state(m,STATE_DP_DISC);
		}
		if (m->result_code == 0)
			m->result_code = RESULT_NOCARRIER;
		schedule_modem_stop(m);
#if ALREADY_DONE
		if(m->modem_info&TIOCM_CD && m->tty &&
		   !m->termios->c_cflag&CLOCAL /* !C_CLOCAL(m->tty) */) {
			MODEM_DBG("tty_hangup...\n");
			tty_hangup(m->tty);
		}
#endif
		m->modem_info &= ~TIOCM_CD;
		break;
	case STATE_EC_DISC:
		if(local && m->dp && m->dp->op->hangup )
			m->dp->op->hangup(m->dp);
		modem_set_state(m,STATE_DP_DISC);
		break;
	case STATE_DP_DISC:
	default:
		return;
	}
}


/* FIXME: find better place */
#define NONEC_DP(id) ((id) == DP_V23 || (id) == DP_B103 || (id) == DP_V21)

void modem_update_status(struct modem *m, unsigned status)
{
	MODEM_DBG("modem_update_status: %d\n", status);
	switch(status) {
	case STATUS_OK:
		break;
	case STATUS_DP_LINK:
		MODEM_DBG("--> DP LINK\n");
		if(NONEC_DP(m->dp->id))
			m->cfg.ec = 0;
		modem_set_state(m,STATE_EC_ESTAB);
		/* start packer */
		if(m->cfg.ec && m->cfg.ec_detector &&
		   !QC_SKIP_EC_DETECTION(m) ) {
			modem_detector_start(m);
			m->get_bits = modem_detector_get_bits;
			m->put_bits = modem_detector_put_bits;
		}
		else {  /* skip pack connect phase */
			modem_update_status(m,STATUS_PACK_LINK);
		}
		break;
	case STATUS_PACK_LINK:
		MODEM_DBG("--> PACK LINK\n");
		if(m->cfg.ec) {
			/* start EC */
			modem_hdlc_start(m);
			m->get_bits = modem_hdlc_get_bits;
			m->put_bits = modem_hdlc_put_bits;
			modem_ec_start(m);
		}
		else {
			/* start async, pass EC connect phase */
			modem_async_start(m);
			m->get_bits = modem_async_get_bits;
			m->put_bits = modem_async_put_bits;
			modem_update_status(m,STATUS_EC_LINK);
		}
		break;
	/* case STATUS_CONNECT: */
	case STATUS_EC_LINK:
		MODEM_DBG("--> EC LINK\n");
		modem_set_state(m,STATE_MODEM_ONLINE);
		modem_report_result(m,RESULT_CONNECT); // fixme
		if(SPEAKER_CONTROL(m) == 1)
			m->driver.ioctl(m,MDMCTL_SPEAKERVOL,0);
		break;
	case STATUS_EC_RELEASE:
	case STATUS_EC_ERROR:
		MODEM_DBG("--> EC UNLINK\n");
		m->result_code = RESULT_NOCARRIER;
		modem_hup(m,(m->state == STATE_EC_DISC));
		break;
	case STATUS_ERROR:
	case STATUS_DP_ERROR:
	case STATUS_NOCARRIER:
	case STATUS_NODIALTONE:
	case STATUS_BUSY:
	case STATUS_NOANSWER:
	default:
		MODEM_DBG("--> FINISH.\n");
		if(status == STATUS_NODIALTONE)
			m->result_code = RESULT_NODIALTONE;
		else if(status == STATUS_BUSY)
			m->result_code = RESULT_BUSY;
		else if(status == STATUS_NOANSWER)
			m->result_code = RESULT_NOANSWER;
		else
			m->result_code = RESULT_NOCARRIER;
		modem_hup(m,IS_STATE_DISC(m->state));
		break;
	}
}



void modem_update_config(struct modem *m, struct modem_config *cfg)
{
	MODEM_DBG("modem update config...\n");
#if 1
#define PRINT_CONFIG(prm) MODEM_DBG(#prm " = %d (%d)\n",cfg->prm,m->cfg.prm)
	PRINT_CONFIG(ec);
	PRINT_CONFIG(ec_detector);
	PRINT_CONFIG(ec_tx_win_size);
	PRINT_CONFIG(ec_rx_win_size);
	PRINT_CONFIG(ec_tx_info_size);
	PRINT_CONFIG(ec_rx_info_size);
	PRINT_CONFIG(comp);
	PRINT_CONFIG(comp_dict_size);
	PRINT_CONFIG(comp_max_string);
#endif
	// FIXME, FIXME, FIXME !!!
#if 0	
	if(cfg->ec) ; /* TBD: now ec reconfig is handled internally in EC */
#endif
	if(cfg->comp && m->cfg.comp) {
		m->cfg.comp = cfg->comp;
		if(!modem_comp_config(m,cfg->comp_dict_size,
				      cfg->comp_max_string)) {
			m->cfg.comp_dict_size  = cfg->comp_dict_size;
			m->cfg.comp_max_string = cfg->comp_max_string;
		}
		else { /* config failed */
			; /* TBD: probably try to renegotiate */
		}
	}
	else
		m->cfg.comp = 0;
}


/*
 *    Interrupt handlers and services
 *
 */

/* get/put bits */
int modem_get_bits(struct modem *m, int nbits, u8 *buf, int n)
{
	if(m->get_bits)
		n = m->get_bits(m,nbits,buf,n);
	else
		memset(buf,0xff,n);
	modem_debug_log_data(m,MODEM_DBG_TX_BITS,buf,n);
	/* process bit timer and packer */
	if (m->bit_timer && (m->bit_timer -= nbits*n) <= 0) {
		m->bit_timer = 0;
		m->bit_timer_func(m);
	}
	// FIXME NOW: remove packer_process from modem to EC
	if(m->packer_process)
		m->packer_process(m,nbits*n);
	return n;
}

int modem_put_bits(struct modem *m, int nbits, u8 *buf, int n)
{
	if(m->put_bits)
		n = m->put_bits(m,nbits,buf,n);
	modem_debug_log_data(m,MODEM_DBG_RX_BITS,buf,n);	
	return n;
}


static unsigned dpstat2status(unsigned dpstat)
{
	switch (dpstat) {
	case DPSTAT_OK: return STATUS_OK;
	case DPSTAT_CONNECT: return STATUS_DP_LINK;
	case DPSTAT_ERROR: return STATUS_DP_ERROR;
	case DPSTAT_NODIALTONE: return STATUS_NODIALTONE;
	case DPSTAT_BUSY: return STATUS_BUSY;
	case DPSTAT_NOANSWER: return STATUS_NOANSWER;
	case DPSTAT_CHANGEDP: return STATUS_OK;
	default: return STATUS_ERROR;
	}
}


static void modem_dp_process(struct modem *m,void *in,void *out,int count)
{
	int ret;
	unsigned cnt;
	//MODEM_DBG("modem_process %d: %d...\n", m->count,count);
	while(count && m->dp) {
		cnt = count;
		if (cnt > m->frag)
			cnt = m->frag;
		//MODEM_DBG("%d: running dp %s...\n", m->count,dp->name);
		ret = m->dp->op->process(m->dp,in,out,cnt);
		switch(ret) {
		case DPSTAT_OK:
			break;
		case DPSTAT_CONNECT:
			if(!IS_STATE_LINKED(m->state) &&
			   !IS_STATE_DISC(m->state)) {
				modem_update_status(m,STATUS_DP_LINK);
			}
			break;
		case DPSTAT_CHANGEDP:
			do_modem_change_dp(m);
			break;
		default: /* errors */
			modem_update_status(m,dpstat2status(ret));
			m->process = NULL;
			break;
		}

		count -= cnt;
		out += cnt<<MFMT_SHIFT(m->format);
		in  += cnt<<MFMT_SHIFT(m->format);
	}
	if(count)
		memset(out,0,count<<MFMT_SHIFT(m->format));
}


void modem_process(struct modem *m,void *in,void *out,int count)
{
	/* clean DC */
	dcr_process(m->dcr,in,count);
	modem_debug_log_data(m,MODEM_DBG_RX_SAMPLES,in,count<<MFMT_SHIFT(m->format));
	if(m->process)
		m->process(m,in,out,count);
	else  /* mute output */
		memset(out,0,count<<MFMT_SHIFT(m->format));
	modem_debug_log_data(m,MODEM_DBG_TX_SAMPLES,out,count<<MFMT_SHIFT(m->format));
	m->count+=count;
	if( m->sample_timer && m->count >= m->sample_timer ) {
		void (*f)(struct modem *) = m->sample_timer_func ;
                m->sample_timer = 0;
		m->sample_timer_func = NULL;
		if(f) f(m);
	}
}


/*
 *    event processing
 *
 */


static void timer_mark_event (void *data)
{
	struct modem *m = data;
#ifdef DEBUG_TIMERS
	MODEM_DBG("timer_mark_event: %x | %x : now = %lu...\n",
		  m->event, m->new_event, get_time());
#endif
	m->event |= m->new_event;
	m->new_event = 0;
}

static void schedule_event(struct modem *m, unsigned mask, unsigned long when)
{
	m->new_event |= mask;
	m->event_timer.data = m;
	m->event_timer.func = timer_mark_event;
	m->event_timer.expires = get_time() + when;
#ifdef DEBUG_TIMERS
	MODEM_DBG("schedule_event: %x | %x : now %lu + %lu = %u...\n",
		  m->new_event, mask,
		  get_time(), when, m->event_timer.expires);
#endif
	timer_add(&m->event_timer);
}


void modem_event(struct modem *m)
{
	unsigned event = m->event;
	m->event = 0;
        MODEM_DBG("modem_event: %x...\n", event);

	if(event&MDMEVENT_RING_CHECK) {
		if(m->ring_count == 0) {
			MODEM_DBG("ring cancel...\n");
			TOTAL_RINGS_COUNT(m) = 0;
		}
		else if ( time_before(m->ring_last,m->ring_first+RING_ON_MIN(m)) ||
			  time_after(m->ring_last,m->ring_first+RING_ON_MAX(m)) ||
			  m->ring_count < RING_COUNT_MIN(m)) {
			MODEM_DBG("ring reject: count %d (%lu-%lu)\n",
				  m->ring_count,m->ring_first,m->ring_last);
			TOTAL_RINGS_COUNT(m) = 0;
			m->ring_count = 0;
		}
		else {
			MODEM_DBG("ring valid.\n");
			m->ring_count = 0;
			TOTAL_RINGS_COUNT(m)++;
			if(TOTAL_RINGS_COUNT(m) == 1)
				modem_put_chars(m,CRLF_CHARS(m),2);
			modem_report_result(m,RESULT_RING);
			if (ANSWER_AFTER_RINGS(m) &&
#ifdef MODEM_CONFIG_RING_DETECTOR
			    (!m->started || m->rd_obj) &&
#else
			    !m->started &&
#endif
			    TOTAL_RINGS_COUNT(m) >= ANSWER_AFTER_RINGS(m)) {
				TOTAL_RINGS_COUNT(m) = 0;
				modem_answer(m);
			}
			else {
				schedule_event(m,MDMEVENT_RING_CHECK,
					       RING_OFF_MAX(m) + 1);
			}
		}
	}
	if(event&MDMEVENT_ESCAPE) {
		MODEM_DBG("modem_escape (count %d)...\n", m->escape_count);
		if (m->state == STATE_MODEM_ONLINE && m->escape_count == 3) {
			modem_set_state(m, STATE_COMMAND_ONLINE);
			m->xmit.head = m->xmit.tail = m->xmit.count = 0;
			modem_put_chars(m,CRLF_CHARS(m),2);
			modem_report_result(m,RESULT_OK);
			m->command = 1;
		}
		m->escape_count = 0;
	}
}

void modem_ring(struct modem *m)
{
	unsigned long now = get_time();
	if (m->state != STATE_MODEM_IDLE)
		return;
	m->ring_count++;
	if(m->ring_count == 1) {
		MODEM_DBG("modem_ring...\n");
		if(time_before(now,m->ring_last + RING_OFF_MIN(m))) {
			MODEM_DBG("bad ring: now %lu, last %lu.\n",
				  now,m->ring_last);
			m->ring_count = 0;
		}
		else {
			m->ring_first = now;
			schedule_event(m,MDMEVENT_RING_CHECK,
				       RING_ON_MAX(m) + 1);
#ifdef MODEM_CONFIG_CID
			if (TOTAL_RINGS_COUNT(m) == 0 && m->cid_requested)
				modem_cid_start(m, RING_ON_MAX(m) + RING_OFF_MIN(m));
#endif
		}
	}
	m->ring_last = now;
}


void modem_error(struct modem *m)
{
	MODEM_DBG("modem error...\n");
}



/* command mode processing */
static void modem_at_process(void *data)
{
	struct modem *m = (struct modem *)data;
	int echo = IS_ECHO(m);
	char lf = LF_CHAR(m), cr = CR_CHAR(m), bs = BS_CHAR(m);
        int ret;
	char ch;
	while(1) {
		if(modem_get_chars(m,&ch,1) <= 0)
			break;
		if (ch == '/' && m->at_count == 1 &&
		    toupper(m->at_line[0]) == 'A') {
			m->at_count = strlen(m->at_line);
			if(echo)
				modem_put_chars(m,m->at_line+1,m->at_count-1);
			ch = cr;
		}
		if (echo)
			modem_put_chars(m,&ch,1);
		if (ch == cr || ch == lf) {
			int i;
			char *p = m->at_cmd;
			m->at_line[m->at_count] = '\0';
			for(i = 0 ; i < m->at_count ; i++) {
				if( m->at_line[i] == ' ' ||
				    m->at_line[i] == '\t' )
					continue;
				*p++ = m->at_line[i];
			}
			*p = '\0';
			if (strlen(m->at_cmd) < 2 ||
			    toupper(m->at_cmd[0]) != 'A' ||
			    toupper(m->at_cmd[1]) != 'T' ) {
				memset(m->at_line,' ',m->at_count);
				if(echo) {
					modem_put_chars(m,&cr,1);
					modem_put_chars(m,m->at_line,m->at_count);
					modem_put_chars(m,&cr,1);
				}
				m->at_count = 0;
				continue;
			}
			if (echo)
				modem_put_chars(m,CRLF_CHARS(m),2);
			MODEM_DBG("run cmd: %s\n",m->at_cmd);
			ret = process_at_command(m, m->at_cmd);
			if (ret < 0)
				modem_report_result(m,RESULT_ERROR);
			else if (ret==0)
				modem_report_result(m,RESULT_OK);
			m->at_line[m->at_count] = '\0';
			m->at_count = 0;
			echo = IS_ECHO(m);
			cr = CR_CHAR(m), lf = LF_CHAR(m), bs = BS_CHAR(m);
		}
		else if (ch == bs && m->at_count) {
			m->at_count--;
			if(echo) {
				ch = ' ';
				modem_put_chars(m,&ch,1);
				modem_put_chars(m,&bs,1);
			}
		}
		else if (m->at_count == sizeof(m->at_line) - 1) {
			if (echo)
				modem_put_chars(m,CRLF_CHARS(m),2);
			m->at_count   = 0;
			m->at_line[1] = '\0';
			modem_report_result(m,RESULT_ERROR);
		}
		else {
			m->at_line[m->at_count++] = ch;
		}
	}
}


/*
 *    TTY procedures
 *
 */


/* get chars */
static int modem_get_chars(struct modem *m, char *buf, int n)
{
	int ret = 0, cnt;
        while(n) {
                cnt = n;
                if (cnt > m->xmit.count)
                        cnt = m->xmit.count;
                if (cnt > m->xmit.size - m->xmit.tail)
                        cnt = m->xmit.size - m->xmit.tail;
                if (cnt <= 0) {
                        break;
                }
                memcpy(buf, m->xmit.buf + m->xmit.tail, cnt);
                ret += cnt;
                n -= cnt;
                buf += cnt;
                m->xmit.count -= cnt;
                m->xmit.tail = (m->xmit.tail+cnt)%m->xmit.size;
        }
	//MODEM_DBG("modem_get_chars: %d...\n", n);
	return ret;
}

/* put chars */
static int modem_put_chars(struct modem *m, const char *buf, int n)
{
	int ret = write(m->pty,buf,n);
	if(ret < 0) {
		/* perror("write"); */
		ret = 0;
	}
#if 0
	if(ret>0) {
		//MODEM_DBG("modem_comp_put_chars: %d...\n",i);
		modem_debug_log_data(m,	MODEM_DBG_RX_CHARS,buf,i);
	}
#endif
	return ret;
}

/* compressor get/put chars */
/* fixme: unify interfaces: modem tx -> comp -> ec -> hdlc -->
                            modem tx -> ec -> hdlc         -->
                            modem tx -> async              -->
			    modem tx --> raw output
*/
static int modem_comp_get_chars(struct modem *m, char *buf, int n)
{
	int ret = 0, cnt;
	char ch;
	while(ret < n) {
		cnt = modem_get_chars(m,&ch,1);
		if(cnt <= 0)
			break;
		cnt = modem_comp_encode(m,ch,buf+ret,n-ret);
		if(cnt < 0) {
			break;
		}
		ret += cnt;
	}
	if(ret < n) {
		cnt =  modem_comp_flush_encoder(m,buf+ret,n-ret);
		ret += cnt;
	}
	if(ret > 0) {
		//MODEM_DBG("modem_comp_get_chars: %d(%d)...\n",ret,count);
		modem_debug_log_data(m,MODEM_DBG_TX_DATA,buf,ret);
	}
	return ret;
}

static int comp_send_output(struct modem *m)
{
	int cnt;
	if(m->comp.count > 0) {
		cnt = modem_put_chars(m,m->comp.buf+m->comp.head,m->comp.count);
		m->comp.count -= cnt;
		m->comp.head += cnt;
		if(m->comp.count > 0)
			return 0;
		else
			m->comp.head = 0;
	}
	return 1;
}

static int modem_comp_put_chars(struct modem *m, const char *buf, int n)
{
	int i=0, cnt;
	if(!comp_send_output(m))
		return 0;
	while(i<n) {
		cnt = modem_comp_decode(m,buf[i++],m->comp.buf,sizeof(m->comp.buf));
		if(cnt < 0) {
			MODEM_DBG("decoder error. %d(%d)\n",i,n);
			modem_update_status(m,STATUS_ERROR);
			break;
		}
		m->comp.count += cnt;
		if(!comp_send_output(m))
			break;
	}
	if(i==n) {
		cnt = modem_comp_flush_decoder(m,m->comp.buf,sizeof(m->comp.buf));
		m->comp.count += cnt;
		comp_send_output(m);
	}
	if(i>0) {
		//MODEM_DBG("modem_comp_put_chars: %d...\n",i);
		modem_debug_log_data(m,MODEM_DBG_RX_DATA,buf,i);
	}
	return i;
}




/*
 *    internal procedures
 *
 */

#define IS_FAST_DP(id) (id == DP_V34 || id == DP_V90 || id == DP_V92)

static void do_modem_change_dp (struct modem *m)
{
	struct dp_operations *op;
        struct dp *old;

	old = m->dp;

	if (m->mode == MODEM_MODE_FAX) {
		modem_fax_start(m);
		m->dp = NULL;
	}
	else {
		struct dp *dp = NULL;
		int dp_id,id;
		dp_id = m->dp_requested;
		m->dp_requested = 0;

		id = dp_id;
		if(dp_id == 0) {
			dp_id = MODEM_DP(m);
			id = IS_FAST_DP(dp_id) ? DP_V8 : dp_id;
		}

		MODEM_DBG("%ld: change dp: --> %d...\n", m->count, id);

		op = get_dp_operations(id);
		if (op && op->create)
			dp = op->create(m,dp_id,
					m->caller,m->srate,
					m->frag,op);
		if (!dp) {
			MODEM_ERR("change dp -> %d error.\n", id);
			modem_hup(m,1);
			return;
		}

		m->dp = dp;
		m->process = modem_dp_process;
	}

        if ( old ) {
                old->op->delete(old);
        }

}


static int modem_set_hook(struct modem *m, unsigned hook_state)
{
        int ret;
        MODEM_DBG("modem set hook: %d --> %d...\n", m->hook, hook_state);
        if ( m->hook == hook_state )
                return 0;
        ret = m->driver.ioctl(m, MDMCTL_HOOKSTATE,hook_state);
        if (!ret)
                m->hook = hook_state;
        return ret;
}

static void modem_setup_config(struct modem *m)
{
	m->cfg.ec = MODEM_EC_ENABLE(m);
	m->cfg.ec_detector = MODEM_EC_DETECTOR(m);
	m->cfg.ec_tx_win_size  = LAPM_MAX_WIN_SIZE;
	m->cfg.ec_rx_win_size  = LAPM_MAX_WIN_SIZE;
	m->cfg.ec_tx_info_size = LAPM_MAX_INFO_SIZE;
	m->cfg.ec_rx_info_size = LAPM_MAX_INFO_SIZE;
	if(m->cfg.ec && MODEM_COMP_ENABLE(m))
		m->cfg.comp = 0x3; /* bi-directional v42bis */
	else
		m->cfg.comp = 0;
	m->cfg.comp_dict_size  = COMP_MAX_CODEWORDS;
	m->cfg.comp_max_string = COMP_MAX_STRING;

	/* setup dsp data */
	m->dsp_info.qc_lapm = m->cfg.ec && m->cfg.ec_detector ;
}


static int do_modem_start(struct modem *m)
{
	int ret;
        ret = m->driver.ioctl(m, MDMCTL_SPEED, m->srate);
        ret = m->driver.ioctl(m, MDMCTL_SETFRAG, m->frag);
        m->count = 0;
        ret = m->driver.start(m);
	m->started = !ret;
	return ret;
}

static int modem_start (struct modem *m)
{
        int ret;

        MODEM_DBG("modem_start..\n");
	if(m->started && modem_stop(m))
		return -1;

	m->result_code = 0;
	modem_setup_config(m);
        modem_set_state(m, STATE_ESTAB);

	if(SPEAKER_CONTROL(m) == 1)
		m->driver.ioctl(m,MDMCTL_SPEAKERVOL,SPEAKER_VOLUME(m));
        ret = modem_set_hook(m, MODEM_HOOK_OFF);
        if (ret)
                goto error;

	m->xmit.head = m->xmit.tail = m->xmit.count = 0;
	m->command = 0;

	if( !(m->dcr= dcr_create()) ||
	    !(m->dp_runtime = dp_runtime_create(m))) {
		ret = -1;
		goto error;
	}

	if ( m->cfg.ec && m->cfg.comp &&
	     (ret = modem_comp_init(m)) )
		goto error;
	if ( m->cfg.ec &&
	     (ret = modem_ec_init(m)))
		goto error;

	/* clear rings and all events */
	TOTAL_RINGS_COUNT(m) = 0;
	m->ring_count = 0;
	m->event = m->new_event = 0;
	timer_del(&m->event_timer);

        ret = do_modem_start(m);
	if (!ret)
		return 0;

 error:
	MODEM_ERR("modem start = %d: cannot start device.\n",ret);
	m->result_code = RESULT_NOCARRIER;
	modem_stop(m);
	return ret;
}


static int modem_stop (struct modem *m)
{
        int ret = 0;
        MODEM_DBG("modem_stop..\n");

	m->process = NULL;

	if(m->started) {
		ret = m->driver.stop(m);
		m->started = ret;
		if ( ret ) {
			MODEM_ERR("modem stop = %d: cannot stop device.\n",ret);
		}
	}
        modem_set_hook(m, MODEM_HOOK_ON);
	if(SPEAKER_CONTROL(m) == 1)
		m->driver.ioctl(m,MDMCTL_SPEAKERVOL,0);
	m->caller = 0;
	m->command = 1;

	// FIXME: If ec,comp were allocated handled inside _exit() - improve?
	modem_ec_exit(m);
	modem_comp_exit(m);

        if (m->dp) {
                struct dp *dp;
                dp = m->dp;
                m->dp = 0;
                if(dp) dp->op->delete(dp);
        }
	if (m->dp_runtime) {
		dp_runtime_delete(m->dp_runtime);
		m->dp_runtime = NULL;
	}
	if (m->dcr) {
		dcr_delete(m->dcr);
		m->dcr = NULL;
	}
#ifdef MODEM_CONFIG_RING_DETECTOR
	if(m->rd_obj) {
		RD_delete(m->rd_obj);
		m->rd_obj = NULL;
	}
#endif
#ifdef MODEM_CONFIG_CID
	if(m->cid) {
		CID_delete(m->cid);
		m->cid = NULL;
	}
#endif
#ifdef MODEM_CONFIG_VOICE
	if(m->voice_obj) {
		VOICE_delete(m->voice_obj);
		m->voice_obj = NULL;
	}
#endif
#ifdef MODEM_CONFIG_FAX
	if(m->fax_obj) {
		FAX_delete(m->fax_obj);
		m->fax_obj = NULL;
	}
#endif
        modem_set_state(m, STATE_MODEM_IDLE);
	if (m->result_code) {
		modem_report_result(m, m->result_code);
		m->result_code = 0;
	}

	m->count = 0;
	return m->started;
}


/*
 *    Caller ID
 *
 */

#ifdef MODEM_CONFIG_CID

static void modem_cid_stop(struct modem *m)
{
	MODEM_DBG("modem_cid_stop...\n");
	m->sample_timer = 0;
	m->sample_timer_func = NULL;
#ifdef MODEM_CONFIG_RING_DETECTOR
	if(!m->rd_obj) {
		m->process = NULL;
		modem_stop(m);
	}
	else {
		CID_delete(m->cid);
		m->cid = NULL;
	}
#else
	m->process = NULL;
	modem_stop(m);
#endif
	/* continue with answer if need */
	if (ANSWER_AFTER_RINGS(m) &&
	    TOTAL_RINGS_COUNT(m) >= ANSWER_AFTER_RINGS(m)) {
		TOTAL_RINGS_COUNT(m) = 0;
		modem_answer(m);
	}
}

static void modem_cid_process(struct modem *m, void *in, void *out, int count)
{
	int status;
	//MODEM_DBG("modem_cid_process: %d...\n", count);
	memset(out,0,count*2);
	status = CID_process(m->cid, in, count);
	if(status) {
		if(status < 0)
			MODEM_DBG("CID failed.\n");
		modem_cid_stop(m);
	}
}

static int modem_cid_start(struct modem *m, unsigned timeout)
{
	MODEM_DBG("modem_cid_start: timeout = %d...\n", timeout);
#ifdef MODEM_CONFIG_RING_DETECTOR
	if(m->started && !m->rd_obj)
#else
	if(m->started)
#endif
		return -1;
	m->cid = CID_create(m, m->srate, m->cid_requested);
	if(!m->cid)
		return -1;
	m->sample_timer = m->count + m->srate*timeout/MODEM_HZ ;
	m->sample_timer_func = modem_cid_stop;
#ifdef MODEM_CONFIG_RING_DETECTOR
	if(m->started && m->rd_obj)
		return 0;
#endif
	m->process = modem_cid_process;
        modem_set_hook(m, MODEM_HOOK_SNOOPING);
	return do_modem_start(m);
}

#endif /* MODEM_CONFIG_CID */


/*
 *    Ring detector (internal)
 *
 */

#ifdef MODEM_CONFIG_RING_DETECTOR
static void modem_ring_detector_process(struct modem *m, void *in, void *out, int count)
{
	int ret;
	memset(out, 0, count*2);
	ret = RD_process(m->rd_obj, in, count);
	if (ret) {
		long freq, duration;
		RD_ring_details(m->rd_obj, &freq, &duration);
		MODEM_DBG("ring details: freq = %ld, duration = %ld\n",
			  freq, duration);
		if(freq == 0) {
			MODEM_DBG("report ring start...\n");
			modem_ring(m);
		}
		else if (freq > 0) {
			MODEM_DBG("report ring end...\n");
			/* ring finishing */
			m->event |= MDMEVENT_RING_CHECK;
			if (m->ring_count <= 1)
				m->ring_count = duration * freq / 1000 ;
			m->ring_last = get_time();
		}
		else
			MODEM_ERR("RD returns %ld freq. (duration %ld)\n",
				  freq, duration);
	}
#ifdef MODEM_CONFIG_CID
	if(m->cid)
		modem_cid_process(m, in, out, count);
#endif
}

int modem_ring_detector_start(struct modem *m)
{
	MODEM_DBG("modem_ring_detector_start...\n");
	if(m->rd_obj) {
		MODEM_ERR("modem_ring_detector_start: rd_obj already exists!\n");
		return -1;
	}
	m->rd_obj = RD_create(m, m->srate);
	m->process = modem_ring_detector_process;
	modem_set_hook(m, MODEM_HOOK_SNOOPING);
	return do_modem_start(m);
}
#endif /* MODEM_CONFIG_RING_DETECTOR */


/*
 *    Voice modem
 *
 */

#ifdef MODEM_CONFIG_VOICE
extern void modem_voice_stop(struct modem *m);

static void modem_voice_process(struct modem *m, void *in, void *out, int count)
{
	int status;
	//MODEM_DBG("modem_voice_process: %d...\n", count);
	status = VOICE_process(m->voice_obj, in, out, count);
	if(status) {
		MODEM_DBG("modem_voice_process: status %d\n", status);
		switch(status) {
		case VOICE_STATUS_OK:
			modem_report_result(m, RESULT_OK);
			m->command = 1;
			break;
		case VOICE_STATUS_CONNECT:
			modem_report_result(m, RESULT_CONNECT);
			m->command = 0;
			break;
		case VOICE_STATUS_ERROR:
			modem_report_result(m, RESULT_ERROR);
			m->command = 1;
			break;
		default:
			break;
		}
		if(status < 0) {
			MODEM_DBG("VOICE failed.\n");
			modem_voice_stop(m);
		}
	}
}

int modem_voice_command(struct modem *m, enum VOICE_CMD cmd)
{
	MODEM_DBG("modem_voice_command: %u...\n", cmd);
	switch(cmd) {
	case VOICE_CMD_BEEP:
	case VOICE_CMD_DTMF:
	case VOICE_CMD_STATE_RX:
	case VOICE_CMD_STATE_TX:
		if(!m->voice_obj)
			return -1;
		m->command = 0;
		VOICE_command(m->voice_obj, cmd);
		return 1;
	default:
		return -1;
	}
	return 0;
}

int modem_voice_start(struct modem *m)
{
	MODEM_DBG("modem_voice_start...\n");
	if(m->voice_obj)
		return 0;
	if(m->started) {
#ifdef MODEM_CONFIG_RING_DETECTOR
		if(m->rd_obj)
			modem_stop(m);
		else
#endif
			return -1;
	}
	m->voice_obj = VOICE_create(m, m->srate);
	if(!m->voice_obj)
		return -1;
	m->sample_timer = 0 /* m->srate*timeout/MODEM_HZ */ ;
	m->sample_timer_func = NULL /* modem_voice_stop */ ;
	m->process = modem_voice_process;
	modem_set_hook(m, MODEM_HOOK_OFF);	
	return do_modem_start(m);
}

void modem_voice_stop(struct modem *m)
{
	MODEM_DBG("modem_voice_stop...\n");
	m->process = NULL;
	m->sample_timer = 0;
	m->sample_timer_func = NULL;
	if(m->voice_obj) {
		VOICE_delete(m->voice_obj);
		m->voice_obj = NULL;
	}
	modem_stop(m);
	modem_set_hook(m, MODEM_HOOK_ON);	
}

int modem_voice_set_device(struct modem *m, unsigned device)
{
	MODEM_DBG("modem_voice_set_device: dev = %u...\n", device);
	if (device == VOICE_DEVICE_NONE) {
		if(m->voice_obj)
			modem_voice_stop(m);
		return 0;
	}
	else if (device == VOICE_DEVICE_LINE) {
		if(m->voice_obj)
			return 0;
		else if(modem_voice_start(m))
			return -1;
		return 1;
	}
	return -1;
}

int modem_voice_init(struct modem *m)
{
	m->voice_info.comp_method = 1;
	m->voice_info.sample_rate = 8000;
	m->voice_info.rx_gain = 128;
	m->voice_info.tx_gain = 128;
	m->voice_info.dtmf_symbol = 0;
	m->voice_info.tone1_freq = 933;
	m->voice_info.tone2_freq = 0;
	m->voice_info.tone_duration = 150;
	m->voice_info.inactivity_timer = 0;
	m->voice_info.silence_detect_sensitivity = 128;
	m->voice_info.silence_detect_period      = 50;
	return 0;
}

#endif /* MODEM_CONFIG_VOICE */


/*
 *    Fax modem
 *
 */

#ifdef MODEM_CONFIG_FAX

static void modem_fax_process(struct modem *m, void *in, void *out, int count)
{
	int ret;
	//MODEM_DBG("modem_fax_process: %d...\n", count);
	ret = FAX_process(m->fax_obj, in, out, count);
	if(ret) {
		MODEM_DBG("fax_process: %d\n", ret);
		switch(ret) {
		case FAX_STATUS_OK:
			modem_set_state(m, STATE_COMMAND_ONLINE);
			modem_report_result(m, RESULT_OK);
			m->command = 1;
			break;
		case FAX_STATUS_CONNECT:
			modem_set_state(m,STATE_MODEM_ONLINE);
			modem_report_result(m, RESULT_CONNECT);
			m->command = 0;
			break;
		case FAX_STATUS_NOCARRIER:
			modem_set_state(m, STATE_COMMAND_ONLINE);
			modem_report_result(m, RESULT_NOCARRIER);
			m->command = 1;
			break;
		case FAX_STATUS_ERROR:
			modem_update_status(m, STATUS_ERROR);
			break;
		default:
			break;
		}
		if(ret < 0) {
			MODEM_DBG("FAX failed.\n");
			modem_stop(m);
		}
	}
}

static int modem_fax_start(struct modem *m)
{
	MODEM_DBG("modem_fax_start...\n");
	m->fax_obj = FAX_create(m, m->caller, m->srate);
	if(!m->fax_obj)
		return -1;
	m->sample_timer = 0;
	m->sample_timer_func = NULL;
	m->process = modem_fax_process;
	return 0;
}

#endif /* MODEM_CONFIG_FAX */


/*
 *    Commands
 *
 */

int modem_send_to_tty(struct modem *m, const char *buf, int n)
{
	return modem_put_chars(m, buf, n);
}

int modem_recv_from_tty(struct modem *m, char *buf, int n)
{
	return modem_get_chars(m, buf, n);
}

int modem_answer(struct modem *m)
{
        MODEM_DBG("modem answer...\n");
        if ( m->dp ) {
                MODEM_ERR("dp %d is already exists.\n", m->dp->id);
                return -1;
        }
	if(m->started)
		modem_stop(m);
	m->dp_requested = 0;
	m->automode_requested = 0; /* MODEM_AUTOMODE(m) */
	m->sample_timer = ANSWER_DELAY(m) ?
		ANSWER_DELAY(m)*m->srate : m->srate/100;
	m->sample_timer_func = do_modem_change_dp;
	return modem_start(m);
}


static int modem_dial_start(struct modem *m)
{
	struct dp_operations *op;
        struct dp *dp = NULL;
        MODEM_DBG("modem_dial_start...\n");
	if(m->dp) {
		return -1;
	}
	if (m->started)
		modem_stop(m);
	m->caller = 1;
	op = get_dp_operations(DP_CALLPROG);
	char save = m->dial_string[0]; // hide the dial string so no DTMF is generated
	m->dial_string[0] = 0;
	if (op && op->create)
		dp = op->create(m,DP_CALLPROG,
				m->caller,m->srate,
				m->frag,op);
	m->dial_string[0] = save;
	if (!dp) {
		MODEM_ERR("cannot create CALLPROG.\n");
		return -1;
	}
        m->dp = dp;
	m->process = modem_dp_process;
	return 0;
}

int modem_dial(struct modem *m)
{
	int ret;
        MODEM_DBG("modem dial: %s...\n", m->dial_string);
	m->dp_requested = 0;
	m->automode_requested = 0;
 	ret = modem_dial_start(m);
	if(ret)
		return -1;
	return modem_start(m);
}


int modem_hook(struct modem *m, unsigned hook_state)
{
	MODEM_DBG("modem hook...\n");
        if ( m->hook == hook_state )
                return 0;
        if (!IS_STATE_IDLE(m->state))
		modem_hup(m,1);
        return modem_set_hook(m,hook_state);
}

int modem_online(struct modem *m)
{
	MODEM_DBG("modem online...\n");
        if (m->state != STATE_COMMAND_ONLINE)
                return -1;
	m->command = 0;
        modem_set_state(m, STATE_MODEM_ONLINE);
	modem_report_result(m,RESULT_CONNECT); // fixme
        return 0;
}

void modem_update_speaker(struct modem *m)
{
	//MODEM_DBG("modem update speaker...\n");
	m->driver.ioctl(m,MDMCTL_SPEAKERVOL,
			SPEAKER_CONTROL(m) == 2 ? SPEAKER_VOLUME(m):0);
}

int modem_get_sreg(struct modem *m, unsigned int num)
{
	if (num >= sizeof(m->sregs))
		return -1;
	return m->sregs[num];
}

int modem_set_sreg(struct modem *m, unsigned int num, int val)
{
	if (num >= sizeof(m->sregs))
		return -1;
	m->sregs[num] = val;
	return 0;
}

/* homolog set initialization */
int modem_homolog_init(struct modem *m, int id, const char *name)
{
	const struct homolog_set *set;
	for(set=homolog_set;set->name;set++) {
		if(set->id == id ||
		   (name && *name && !strcmp(name,set->name))) {
			m->homolog = set;
			modem_set_sreg(m,SREG_DIAL_PAUSE_TIME,
				       set->params->DialPauseTime);
			modem_set_sreg(m,SREG_FLASH_TIMER,
				       set->params->HookFlashTime);
			return 0;
		}
	}
	return -1;
}


int  modem_set_mode(struct modem *m, enum MODEM_MODE mode)
{
	MODEM_DBG("modem set mode: -> %d...\n", mode);
	if (m->mode == mode)
		return 0;
#ifdef MODEM_CONFIG_VOICE
	if (m->mode == MODEM_MODE_VOICE && m->voice_obj) {
		modem_voice_stop(m);
	}
#endif
	m->mode = mode;
	return 0;
}


int modem_reset(struct modem *m)
{
	MODEM_DBG("modem reset...\n");
	if(m->state != STATE_MODEM_IDLE)
		modem_hup(m,1);
	else if(m->started)
		modem_stop(m);
	else if(m->hook)
		modem_set_hook(m,MODEM_HOOK_ON);
	modem_set_state(m, STATE_MODEM_IDLE);
	m->command = 1;
	m->min_rate = MODEM_MIN_RATE;
	m->max_rate = MODEM_MAX_RATE;
	sregs_init(m->sregs);
	modem_homolog_init(m,m->homolog->id,NULL);
	modem_set_mode(m, MODEM_MODE_DATA);
	return 0;
}


/*
 *    Init functions
 *
 */

/* set default init values */
static int sregs_init(unsigned char sregs[])
{
        sregs[SREG_ESCAPE_CHAR]                  = '+' ; /* escape char */
        sregs[SREG_CR_CHAR]                      = '\r'; /* cr char */
        sregs[SREG_LF_CHAR]                      = '\n'; /* lf char */
        sregs[SREG_BS_CHAR]                      = '\b'; /* bs char */
        sregs[SREG_DIAL_TONE_WAIT_TIME]          =    2; /* seconds */
        sregs[SREG_WAIT_CARRIER_AFTER_DIAL]      =   60; /* seconds */
        sregs[SREG_DIAL_PAUSE_TIME]              =    2; /* seconds */
        sregs[SREG_CARRIER_DETECT_RESPONSE_TIME] =    6; /* 0.1 sec */
        sregs[SREG_CARRIER_LOSS_DISCONNECT_TIME] =    7; /* 0.1 sec */
        sregs[SREG_DTMF_DURATION]                =  100; /* ms */
        sregs[SREG_ESCAPE_PROMPT_DELAY]          =   50; /* ms */
        sregs[SREG_FLASH_TIMER]                  =   20; /* 10ms */
        sregs[SREG_ECHO]                         =    1; /* yes */
        sregs[SREG_QUIET]                        =    0; /* no  */
        sregs[SREG_VERBOSE]                      =    1; /* yes */
        sregs[SREG_TONE_OR_PULSE]                =    1; /* tone */

        sregs[SREG_X_CODE]                       =    4;

        sregs[SREG_SPEAKER_CONTROL] =  1; /* yes */
        sregs[SREG_SPEAKER_VOLUME]  =  3; /* max */
        sregs[SREG_AUTOMODE]        =  1; /* yes */
        sregs[SREG_DP]              =  DP_V92;

        sregs[SREG_ANS_DELAY]       =  2; /* seconds */

        sregs[SREG_LINE_QUALITY_CONTROL]   = 0;
        sregs[SREG_CD]                     = 0;
        sregs[SREG_FLOW_CONTROL]           = 0;
        sregs[SREG_CONNNECT_MSG_FORMAT]    = 0;
        sregs[SREG_CONNNECT_MSG_SPEED_SRC] = 0;

	/* new sregs */
	sregs[SREG_EC]   = 1;
	sregs[SREG_COMP] = 0x3;

        return 0;
}



/* ---------------------------------------------------------------- */

void modem_hangup(struct modem *m)
{
	modem_hup(m,1);
}


void modem_update_termios(struct modem *m, struct termios *tios)
{
        MODEM_DBG("update termios...\n");
        if( cfgetispeed(tios) == B0 ||
	    cfgetospeed(tios) == B0 ) {
                MODEM_DBG("modem_update_termios: hangup.\n");
                if(m->state!=STATE_MODEM_IDLE)
                        modem_hup(m,1);
		// TBD: drop DTR?
        }
        else
                m->modem_info |= TIOCM_DTR;
	m->termios = *tios;
}


/*
 *
 *    modem_write()
 *
 */


static int modem_check_escape(struct modem *m, const char *buf, int count)
{
	unsigned long now = get_time();
        int i;
        if(count + m->escape_count > 3 )
		goto noescape_out;
        for( i = 0 ; i < count ; i++) {
                if(buf[i] != ESCAPE_CHAR(m))
			goto noescape_out;
        }

        if(m->escape_count == 0) {
                if(time_before(now, m->last_esc_check + ESCAPE_TIMEOUT(m)))
                        goto noescape_out;
	}
	else if (time_after(now, m->last_esc_check + ESCAPE_TIMEOUT(m)))
		goto noescape_out;

	m->escape_count += count;
        if(m->escape_count == 3)
		schedule_event(m,MDMEVENT_ESCAPE,ESCAPE_TIMEOUT(m));

	m->last_esc_check = now;
	return m->escape_count;

 noescape_out:
	m->escape_count = 0;
        m->last_esc_check = now;
        return 0;
}



int modem_write(struct modem *m, const char *buffer, int count)
{
       	const char *buf;
	int cnt, ret = 0;
	if(IS_STATE_CONNECTING(m->state)) {
		MODEM_DBG("modem_tty_write: hangup...\n");
		modem_hup(m,1);
		return count;
	}

	if(m->state == STATE_MODEM_ONLINE)
		modem_check_escape(m,buffer,count);

	while(count) {
		cnt = count;
		if(cnt > m->xmit.size - m->xmit.count)
			cnt = m->xmit.size - m->xmit.count;
		if(cnt > m->xmit.size - m->xmit.head)
			cnt = m->xmit.size - m->xmit.head;
		if(cnt <= 0) {
			MODEM_DBG("modem_write: overflow!\n");
			break;
		}
		buf = buffer;
                memcpy(m->xmit.buf + m->xmit.head, buf, cnt);
                m->xmit.count += cnt;
                m->xmit.head = (m->xmit.head + cnt)%m->xmit.size;
		ret += cnt;
		buffer += cnt;
		count -= cnt;
	}
	if(m->command)
		modem_at_process(m);
	return ret;
}



void modem_print_version()
{
	MODEM_INFO("%s: version %s %s\n",
		   modem_name,modem_version,modem_date);
}


struct modem *modem_create(struct modem_driver *drv, const char *name)
{
	struct modem *m;

	modem_print_version();

	m = malloc(sizeof(*m));
	if (!m)
		return NULL;
	memset(m,0,sizeof(*m));

	m->name = name;
	m->driver = *drv;
        m->modem_info = 0;
	m->state  = STATE_MODEM_IDLE;
	m->command= 1;
        m->hook   = MODEM_HOOK_ON;
        m->caller = 0;
	m->min_rate = MODEM_MIN_RATE;
	m->max_rate = MODEM_MAX_RATE;

	m->modem_info |= (TIOCM_DSR|TIOCM_CTS);

        sregs_init(m->sregs);

	if(modem_homolog_init(m, MODEM_DEFAULT_COUNTRY_CODE, NULL)) {
		MODEM_ERR("bad default country code `%x'!\n",
			  MODEM_DEFAULT_COUNTRY_CODE);
		free(m);
		return NULL;
	}
	if(modem_default_country &&
	   modem_homolog_init(m, -1, modem_default_country)) {
		MODEM_INFO("bad country name `%s', using default by code!\n",
			   modem_default_country);
	}

	m->ring_last = get_time();

	timer_init(&m->event_timer);

	/* setup config */
	modem_setup_config(m);
	/* packer initializations */
	// fixme: TBD

        /* dp initialization */
        m->format = MODEM_FORMAT;
        m->srate  = MODEM_RATE;
        m->frag   = MODEM_FRAG; /* in samples */
        m->dp = NULL;

	MODEM_DBG("startup modem...\n");
	m->xmit.buf = malloc(XMIT_SIZE);
	if ( !m->xmit.buf ) {
		free(m);
		return NULL;
	}
	m->xmit.size = XMIT_SIZE;
	m->xmit.head = m->xmit.tail = m->xmit.count = 0;
	m->modem_info |= TIOCM_DTR|TIOCM_RTS;
	// TODO: update speed,DTR according to termios
#ifdef MODEM_CONFIG_VOICE
	modem_voice_init(m);
#endif

	return m;
}



void modem_delete(struct modem *m)
{
	MODEM_DBG("modem_delete...\n");
	if(m->started) {
		MODEM_DBG("shutdown modem...\n");
		if(m->state != STATE_MODEM_IDLE)
			m->result_code = RESULT_NOCARRIER;
		modem_stop(m);		
	}
	m->xmit.size = 0;
	m->xmit.count = m->xmit.tail = m->xmit.head = 0;
	free(m->xmit.buf);
	m->xmit.buf = 0;

	timer_del(&m->event_timer);
	free(m);
}




