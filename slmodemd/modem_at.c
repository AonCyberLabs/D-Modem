
/*
 *
 *    Copyright (c) 2001, Smart Link Ltd.
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
 *	modem_at.c  --  modem at command processor.
 *
 *	Author: Sasha K (sashak@smlink.com)
 *		Seva	  (seva@smlink.com)
 *
 */

/********************************************************************
 *
 * 	AT commands status. (p) - partiatly
 *
 * 	1. Implemented:
 * 	
 * 	AT : A B C D E H I L M N O P Q S T V X Y Z
 *	AT&: C D E F H P R S F W
 *	AT\: A N
 * 	AT%: C E
 * 	AT+: MS FCLASS GCI GMI GMM GMR
 *      AT#: CID
 *	
 ********************************************************************
 *
 * 	2. Not implemented yet:
 *
 * 	AT+: DSC DS DS44 DR
 *           FAE FTS FRS FTM FRM FRH FTH
 * 	AT&: A K V Z
 * 	AT\: B K
 * 	AT*: B
 * 	AT#: BDR RG TL CLS SPK UD
 * 	     VBS VBT VIP VIT VLS VRA VRN 
 * 	     VRX VSD VSP VSR VSS VTD VTM 
 * 	     VTS VGT VTX
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <modem.h>
#include <modem_debug.h>

#define AT_DBG(fmt...) // dprintf("at: " fmt)


/* ------------------------------------------------------------ */
/* prototypes from modem.c */
extern const char modem_author[];
extern const char modem_name[];
extern const char modem_version[];
extern const char modem_date[];

extern struct dp_driver modem_dp_drivers[];

extern int  modem_send_to_tty(struct modem *m, const char *buf, int n);
extern int  modem_answer(struct modem *m);
extern int  modem_dial(struct modem *m);
extern int  modem_hook(struct modem *m,unsigned hook_state);
extern int  modem_online(struct modem *m);
extern int  modem_reset(struct modem *m);
extern void modem_update_speaker(struct modem *m);
extern int  modem_homolog_init(struct modem *m, int id, const char *name);
extern int  modem_set_mode(struct modem *m, enum MODEM_MODE mode);

/* ------------------------------------------------------------ */
/*
 *   local util functions
 */

/* simple toupper + strncmp */
static int toupper_strncmp(char *s1, char *s2, int n)
{
	int i;
	for(i = 0 ; i < n ; i++)
		if(toupper(s1[i]) != s2[i])
			return s1[i] - s2[i];
	return 0;
}


static int set_sreg(struct modem *m, int num, char c, int low, int hi, int *len)
{
	if (!isdigit(c)) {
		*len = 0;
		c = '0';
	}
	else
		*len = 1;
	if ( c >= low + '0' && c <= hi + '0' )
		return modem_set_sreg(m,num,c - '0');
	return -1;
}

/* ------------------------------------------------------------ */


static int process_A(struct modem *m, char *p, int *len)
{
	int ret;
	AT_DBG("AT A command...\n");
	memset(m->dial_string, 0, strlen(m->dial_string));
	ret = modem_answer(m);
	if (ret)
		return -1;
	return 1;
}

static int process_B(struct modem *m, char *p, int *len)
{
	AT_DBG("AT B command...\n");
	MODEM_DP(m) = DP_V22;
	MODEM_AUTOMODE(m) = 0;
	m->min_rate = 1200;
	m->max_rate = 1200;
	return 0;
}

static int process_D(struct modem *m, char *p, int *len)
{
	int ret;
	AT_DBG("AT D command...\n");
	switch (toupper(*p)) {
	case 'T':
		modem_set_sreg(m,SREG_TONE_OR_PULSE,1);
		break;
	case 'P':
#ifdef NO_PULSE_DIAL
		return -1;
#endif
		modem_set_sreg(m,SREG_TONE_OR_PULSE,0);
		break;
	case 'L':
		if(m->dial_string) {
			modem_send_to_tty(m,"Dialing ",8);
			modem_send_to_tty(m,m->dial_string,
						strlen(m->dial_string));
			modem_send_to_tty(m,"...",3);
			modem_send_to_tty(m,CRLF_CHARS(m),2);
		}
		p = 0;
		break;
	default:
		break;
	}
	if (p)
		strncpy(m->dial_string,p,sizeof(m->dial_string));
	ret = modem_dial(m);
	if (ret)
		return -1;
	return 1;
}

static int process_H(struct modem *m, char *p, int *len)
{
	int ret;
	AT_DBG("AT H command...\n");
	switch(*p) {
	case '1':
		*len = 1;
		ret = modem_hook(m, MODEM_HOOK_OFF);
		break;
	case '0':
		*len = 1;
		p++;
	default:
		if (isdigit(*p))
			return -1;
		ret = modem_hook(m, MODEM_HOOK_ON);
		break;
	}
	return ret ? -1 : 0;
}

static int process_I(struct modem *m, char *p, int *len)
{
	const char *s;
	int c,i;
	char *end;
	AT_DBG("AT I command...\n");
	c = strtoul (p,&end,0);
	*len = end - p;
	switch(c) {
	case 1:
	case 2:
		modem_send_to_tty(m,modem_name,strlen(modem_name));
		modem_send_to_tty(m,", ",2);
		modem_send_to_tty(m,modem_version,strlen(modem_version));
		modem_send_to_tty(m,CRLF_CHARS(m),2);
		modem_send_to_tty(m,modem_author,strlen(modem_author));
		modem_send_to_tty(m,CRLF_CHARS(m),2);
		break;
#if 0
	case 2:
		s = "Modem Manufacturer: unknown (TBD)";
		modem_send_to_tty(m,s,strlen(s));
		modem_send_to_tty(m,CRLF_CHARS(m),2);
		s = "Modem Provider: unknown (TBD)";
		modem_send_to_tty(m,s,strlen(s));
		modem_send_to_tty(m,CRLF_CHARS(m),2);		
		break;
#endif
	case 3:
		s = m->dev_name ? m->dev_name : "unknown";
		modem_send_to_tty(m, s, strlen(s));
		modem_send_to_tty(m, CRLF_CHARS(m),2);
		s = m->driver.name ? m->driver.name : "unknown";
		modem_send_to_tty(m, s, strlen(s));
		modem_send_to_tty(m, CRLF_CHARS(m),2);
		break;
	case 4:
		for (i=0;i<24;i++) {
			char str[24];
			snprintf(str,sizeof(str),"s%.2d=%.3d ",i,m->sregs[i]);
			modem_send_to_tty(m,str,strlen(str));
			if (!((i+1)%8))
				modem_send_to_tty(m, CRLF_CHARS(m),2);
		}
		modem_send_to_tty(m,CRLF_CHARS(m),2);
		break;
	case 5:
		modem_send_to_tty(m,"Stored Profile 0:",17);
		modem_send_to_tty(m,CRLF_CHARS(m),2);
#if 0
		s = load_profile(m,0);
		if (s!=NULL)
			ret = print_sregs(m,s);
#endif
		break;
	case 6:
		modem_send_to_tty(m, "Stored Profile 1:",17);
		modem_send_to_tty(m, CRLF_CHARS(m),2);
#if 0
		s = load_profile(m,1);
		if (s!=NULL)
			ret = print_sregs (m,s);
#endif
		break;
	case 7:
		s = m->homolog->name;
		modem_send_to_tty(m,"Country: ",9);
		modem_send_to_tty(m,s,strlen(s));
		modem_send_to_tty(m, CRLF_CHARS(m),2);
		break;
	case 0:
		modem_send_to_tty(m,modem_name,strlen(modem_name));
		modem_send_to_tty(m,CRLF_CHARS(m),2);
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_O(struct modem *m, char *p, int *len)
{
	int ret;
	AT_DBG("AT O command...\n");
	ret = modem_online(m);
	if (ret)
		return -1;
	return 1;
}

static int process_S(struct modem *m, char *p, int *len)
{
	unsigned n = 0;
	int val;
	char *end;
	AT_DBG("AT S command...\n");
	n = strtoul(p,&end,0);
	*len = end - p + 1;
	p = end;
	if ( n >= sizeof(m->sregs)/sizeof(m->sregs[0]) )
		return -1;
	switch(*p++) {
	case '?':
		val = modem_get_sreg(m,n);
		if (val < 0)
			return -1;
		else {
			char strval[16];
			int l = snprintf(strval,sizeof(strval),"%d",val);
			modem_send_to_tty(m, strval,l);
			modem_send_to_tty(m, CRLF_CHARS(m),2);
		}
		return 0;
	case '=':
		val = strtoul(p, &end, 0);
		*len += end - p;
		return modem_set_sreg(m,n,val);
		break;
	default:
		return -1;
	}
	return 0;
}

static int process_Z(struct modem *m, char *p, int *len)
{
	AT_DBG("AT Z command...\n");
#if 1
	switch(*p) {
	case '1':
	case '2':
	case '3':
	case '0':
		*len = 1;
		break;
	default:
		*len = 0;
		break;
	}
	modem_reset(m);
	return 0;
#else
	char *s;
	int set_num;
	set_num = strtoul (p,&s,0);
	*len = s-p;
	if (set_num < 0 || set_num > 4 )
		return -1;
	
	if (set_num == 0) { /* ATZ0 - load value defined by ATYn */
		set_num = m->sregs[SREG_DEFAULT_SETTING];
	}
	else {
		set_num--;
	}
	
	AT_DBG("AT Z command - going to load set %d\n",set_num);
	s = load_profile (m,set_num);
	AT_DBG("AT Z command - done\n");
	if (s != NULL) {
		/* FIXME: modem reset must be done instead */
		/* ... */
		memcpy (m->sregs,s,sizeof (m->sregs));
		return 0;
	}
	return -1;
#endif
}


/*
 *    AT+ commands
 */

static int process_plus_GCI(struct modem *m, char *p, int *len)
{
	static const char gci_string[] = "+GCI:";
	char strval[8];
	const struct homolog_set *h;
	char *end;
	unsigned long n;
	AT_DBG ("AT+GCI...\n");
	switch (*p) {
	case '=':
		p++;
		*len = 1;
		if (*p == '?') { /* show all country codes */
			*len += 1;
			modem_send_to_tty(m,gci_string,strlen(gci_string));
			modem_send_to_tty(m,"(",1);
			for(h=homolog_set;h->name;h++) {
				n = sprintf(strval,"%x,",h->id);
				modem_send_to_tty(m,strval,n);
			}
			modem_send_to_tty(m,")",1);
			modem_send_to_tty(m, CRLF_CHARS(m),2);
		}
		else {
			n = strtoul (p,&end,16);
			if(end == p) {
				return -1;
			}
			*len += end - p; p = end;
			/* set country n */
			if(modem_homolog_init(m,n,NULL))
				return -1;
                }
		break;
	case '?':
		n = sprintf(strval,"%x",m->homolog->id);
		modem_send_to_tty(m, "+GCI:",5);
		modem_send_to_tty(m, strval, n);
		modem_send_to_tty(m, CRLF_CHARS(m), 2);
		*len = 1;
		break;
	default:
		*len = 0;
		return -1;
	}
	return 0;
}


static int process_plus_MS(struct modem *m, char *p, int *len)
{
	char strval[32];
	struct dp_driver *drv;
	char *end;
	unsigned long n;
	unsigned int dp_id;
	int auto_mode,min_rate,max_rate;
	int is_found=0;
		
	AT_DBG ("AT+MS...\n");
	switch (*p) {
	case '=':
		p++;
		*len = 1;
		if (*p == '?') {
			*len += 1;
			modem_send_to_tty(m,"(",1);
			for(drv = modem_dp_drivers ; drv->id ; drv++ ) {
				if(!drv->code || !drv->op) continue;
				if(is_found++>0) modem_send_to_tty(m,",",1);
				sprintf(strval,"%d",drv->id);
				modem_send_to_tty(m,strval,strlen(strval));
			}
			modem_send_to_tty(m,"),(0,1),",1+7);
			sprintf(strval,"(%u-%u),",MODEM_MIN_RATE,MODEM_MAX_RATE);
			modem_send_to_tty(m,strval,strlen(strval));
			modem_send_to_tty(m,strval,strlen(strval)-1);
			modem_send_to_tty(m, CRLF_CHARS(m),2);
			break;
		}

#define PARSE_MS_ITEM(p,len,item) { \
		n = strtoul (p,&end,0); \
		if (end != p) { item = n; }  \
		*len += end - p; p = end;    \
		if (*p != ',') goto setup_ms;        \
		*len += 1; p++;              \
                }
		
		dp_id    = MODEM_DP(m);
		auto_mode= MODEM_AUTOMODE(m);
		min_rate = m->min_rate;
		max_rate = m->max_rate;

		PARSE_MS_ITEM(p,len,dp_id);
		PARSE_MS_ITEM(p,len,auto_mode);
		PARSE_MS_ITEM(p,len,min_rate);
		PARSE_MS_ITEM(p,len,max_rate);
	setup_ms:
		if(dp_id != MODEM_DP(m)) {
			for(drv = modem_dp_drivers ; drv->id ; drv++ ) {
				if(drv->op && drv->code && drv->id == dp_id) {
					is_found++;
					break;
				}
			}
			if(!is_found) return -1;
		}
		if((auto_mode != 0 && auto_mode != 1) || min_rate > max_rate ||
		   min_rate < MODEM_MIN_RATE || max_rate > MODEM_MAX_RATE )
			return -1;
		MODEM_DP(m)       = dp_id;
		MODEM_AUTOMODE(m) = auto_mode;
		m->min_rate       = min_rate;
		m->max_rate       = max_rate;
		break;
	case '?':
		n = sprintf(strval,"%d,%d,%u,%u",MODEM_DP(m),MODEM_AUTOMODE(m),
			    m->min_rate,m->max_rate);
		modem_send_to_tty(m, strval, n);
		modem_send_to_tty(m, CRLF_CHARS(m), 2);
		*len = 1;
		break;
	default:
		*len = 0;
		return -1;
	}
	return 0;
}


static int process_plus_FCLASS(struct modem *m, char *p, int *len)
{
	char ch;
	if(*p == '=' ) {
		p++;
		*len += 2;
		if(*p == '?') {
			modem_send_to_tty(m, "0" , 1);
#ifdef MODEM_CONFIG_FAX_CLASS1
			modem_send_to_tty(m, ",1" , 2);
#endif
#ifdef MODEM_CONFIG_VOICE
			modem_send_to_tty(m, ",8" , 2);
#endif
			modem_send_to_tty(m, CRLF_CHARS(m), 2);
		}
		else if (isdigit(*p)) {
			int val = *p - '0';
			if ( val != 0
#ifdef MODEM_CONFIG_FAX_CLASS1
			     && val != 1
#endif
#ifdef MODEM_CONFIG_VOICE
			     && val != 8
#endif
				)
				return -1;
			return modem_set_mode(m, val);
		}
		else
			return -1;
	}
	else if (*p == '?') {
		*len += 1;
		ch = '0' + m->mode;
		modem_send_to_tty(m, &ch , 1);
		modem_send_to_tty(m, CRLF_CHARS(m), 2);
	}
	else
		return -1;
	return 0;
}

/*
 *    Caller ID
 */

#ifdef MODEM_CONFIG_CID
static int process_CID(struct modem *m,  char *p, int *len)
{
	char ch;
	if(*p == '=' ) {
		p++;
		*len += 2;
		if(*p == '?') {
			modem_send_to_tty(m, "0,1,2" , 5);
			modem_send_to_tty(m, CRLF_CHARS(m), 2);
		}
		else if (isdigit(*p) && *p - '0' < 3) {
			m->cid_requested = *p - '0';
		}
		else
			return -1;
	}
	else if (*p == '?') {
		*len += 1;
		ch = '0' + m->cid_requested;
		modem_send_to_tty(m, &ch , 1);
		modem_send_to_tty(m, CRLF_CHARS(m), 2);
	}
	else
		return -1;
	return 0;
}
#endif


/*
 *    Voice commands
 */

#ifdef MODEM_CONFIG_VOICE

extern int modem_voice_set_device(struct modem *m, unsigned device);
extern int modem_voice_command(struct modem *m, enum VOICE_CMD cmd);
extern int modem_voice_init(struct modem *m);



struct voice_param {
	unsigned *val;
	unsigned min, max;
};

static int set_voice_params(struct modem *m, char *p, const char *cmd,
			    struct voice_param *param, int *len,
			    const char *prefix, const char *suffix)
{
	char strval[32];
	unsigned val[3];
	struct voice_param *prm;
	int i;
	char *end;

	*len += 1;
	if( *p == '=' ) {
		p++;
		if(*p == '?') {
			*len += 1;
			sprintf(strval, "+%s=", cmd);
			modem_send_to_tty(m, strval, strlen(strval));
			if(prefix)
				modem_send_to_tty(m, prefix, strlen(prefix));
			prm = param;
			while(prm->val) {
				sprintf(strval, "%d-%d", prm->min, prm->max);
				modem_send_to_tty(m, strval, strlen(strval));
				prm++;
				if(prm->val)
					modem_send_to_tty(m, ",", 1);
			}
			if(suffix)
				modem_send_to_tty(m, suffix, strlen(suffix));
			modem_send_to_tty(m, CRLF_CHARS(m), 2);
			return 0;
		}

		if(prefix) {
			if(strncmp(p,prefix,strlen(prefix)))
				return -1;
			p += strlen(prefix);
			*len += strlen(prefix);
		}

		prm = param;
		for (i = 0; prm->val && i < sizeof(val)/sizeof(val[0]) ; i++ ) {
			val[i] = strtoul(p,&end,0);
			if( (prm->min && val[i] < prm->min) ||
			    (prm->max && val[i] > prm->max) )
				return -1;
			*len += end - p;
			p = end;
			prm++;
			if(prm->val) {
				if(*p != ',')
					return -1;
				*len += 1;
				p++;
			}
		}

		if(suffix) {
			if(strncmp(p,suffix,strlen(suffix)))
				return -1;
			*len += strlen(suffix);
		}

		prm = param;
		for (i = 0; prm->val && i < sizeof(val)/sizeof(val[0]) ; i++ ) {
			*prm->val = val[i];
			prm++;
		}
		return 0;
	}
	else if ( *p == '?' ) {
		sprintf(strval, "+%s=", cmd);
		modem_send_to_tty(m, strval, strlen(strval));
		if(prefix)
			modem_send_to_tty(m, prefix, strlen(prefix));
		prm = param;
		while(prm->val) {
			sprintf(strval, "%d", *prm->val);
			modem_send_to_tty(m, strval, strlen(strval));
			prm++;
			if(prm->val)
				modem_send_to_tty(m, ",", 1);
		}
		if(suffix)
			modem_send_to_tty(m, suffix, strlen(suffix));
		modem_send_to_tty(m, CRLF_CHARS(m), 2);
		return 0;
	}
	else
		return -1;
}



static int process_voice_command(struct modem *m, char *p, int *len)
{
	AT_DBG("voice cmd: %s\n", p);
	*len += 2;
	/* AT+VIP */
	if(!toupper_strncmp(p,"IP",2)) {
		return modem_voice_init(m);
	}
	/* AT+VNH=0,1 */
	else if(!toupper_strncmp(p,"NH",2)) {
		unsigned val = 0;
		struct voice_param params[] = {
			{&val,0,1}, {} };
		return set_voice_params(m,p+2,"VNH",params,len,NULL,NULL);
	}
	/* AT+VGT=127 */
	else if(!toupper_strncmp(p,"GT",2)) {
		struct voice_param params[] = {
			{&m->voice_info.tx_gain,0,128}, {} };
		return set_voice_params(m,p+2,"VGT",params,len,NULL,NULL);
	}
	/* AT+VGR=127 */
	else if(!toupper_strncmp(p,"GR",2)) {
		struct voice_param params[] = {
			{&m->voice_info.rx_gain,0,128}, {} };
		return set_voice_params(m,p+2,"VGR",params,len,NULL,NULL);
	}
	/* AT+VIT= */
	else if(!toupper_strncmp(p,"IT",2)) {
		struct voice_param params[] = {
			{&m->voice_info.inactivity_timer,0,255}, {} };
		return set_voice_params(m,p+2,"VIT",params,len,NULL,NULL);
	}
	/* AT+VSD=128,50 */
	else if(!toupper_strncmp(p,"SD",2)) {
		struct voice_param params[] = {
			{&m->voice_info.silence_detect_sensitivity,0,255},
			{&m->voice_info.silence_detect_period,0,255},
			{}
		};
		return set_voice_params(m,p+2,"VSD",params,len,NULL,NULL);
	}
	/* AT+VSM=1,8000 */
	else if(!toupper_strncmp(p,"SM",2)) {
		struct voice_param params[] = {
			{&m->voice_info.comp_method,0,1},
			{&m->voice_info.sample_rate,8000,8000},
			{}
		};
		return set_voice_params(m,p+2,"VSM",params,len,NULL,NULL);
	}
	/* AT+VLS=1 */
	else if(!toupper_strncmp(p,"LS",2)) {
		unsigned dev = (m->voice_obj != NULL);
		struct voice_param params[] = {
			{&dev,0,1}, {} };
		int ret = set_voice_params(m,p+2,"VLS",params,len,NULL,NULL);
		if(ret)
			return ret;
		return modem_voice_set_device(m, dev);
	}
	/* AT +VRX, +VTX, +VTR */
	else if(!toupper_strncmp(p,"TX",2))
		return modem_voice_command(m,VOICE_CMD_STATE_TX);
	else if(!toupper_strncmp(p,"RX",2))
		return modem_voice_command(m,VOICE_CMD_STATE_RX);
	else if(!toupper_strncmp(p,"TR",2))
		return modem_voice_command(m,VOICE_CMD_STATE_DUPLEX);
	/* AT+VTS=[933,,150] */
	else if(!toupper_strncmp(p,"TS",2)) {
		unsigned orig_len;
		int ret;
		p += 2;
		orig_len = *len;
		{
			struct voice_param params[] = {
				{&m->voice_info.tone1_freq,0,0},
				{&m->voice_info.tone2_freq,0,0},
				{&m->voice_info.tone_duration,0,6000},
				{} };
			if(*p == '?' || (*p == '=' && *(p+1) == '?' ))
				return set_voice_params(m,p,"VTS",params,len,NULL,NULL);

			ret = set_voice_params(m,p,"VTS",params,len,"[","]");
			if(!ret)
				return modem_voice_command(m, VOICE_CMD_BEEP);
		}
		{
			struct voice_param params[] = {
				{&m->voice_info.dtmf_symbol,0,0},
				{&m->voice_info.tone_duration,0,100},
				{} };
			*len = orig_len;
			ret = set_voice_params(m,p,"VTS",params,len,"{","}");
			if(!ret)
				return modem_voice_command(m, VOICE_CMD_DTMF);
		}
		{
			struct voice_param params[] = {
				{&m->voice_info.dtmf_symbol,0,0}, {} };
			*len = orig_len;
			ret = set_voice_params(m,p,"VTS",params,len,NULL,NULL);
			if(!ret)
				return modem_voice_command(m, VOICE_CMD_DTMF);
		}
		return -1;
	}
	else
		return -1;
}
#else
#define process_voice_command(m,p,plen) (-1)
#endif /* MODEM_CONFIG_VOICE */


/*
 *    Fax commands
 */

#ifdef MODEM_CONFIG_FAX
extern int FAX_class1_command(void *obj, unsigned cmd, unsigned param);


static int process_fax_command(struct modem *m, char *p, int *len)
{
	unsigned val; char *end;
	AT_DBG("fax cmd: %s\n", p);
	*len += 2;
	/* +FTS= */
	if(!toupper_strncmp(p,"TS=",3)) {
		*len += 1;
		p += 3;
		val = strtoul(p,&end,0);
		if(p == end)
			return -1;
		*len += end - p;
		p = end;
		return FAX_class1_command(m->fax_obj, FAX_CMD_FTS, val);
	}
	/* +FRS= */
	else if(!toupper_strncmp(p,"RS=",3)) { // FIXME: abort on tty input
		*len += 1;
		p += 3;
		val = strtoul(p,&end,0);
		if(p == end)
			return -1;
		*len += end - p;
		p = end;
		return FAX_class1_command(m->fax_obj, FAX_CMD_FRS, val);
	}
	/* +FTM= */
	else if(!toupper_strncmp(p,"TM=",3)) {
		*len += 2;
		p+= 3;
		if(*p == '?') {
			modem_send_to_tty(m,"24,48,72,73,74,96,97,98,121,122,145,146",39);
			modem_send_to_tty(m, CRLF_CHARS(m), 2);
			return 0;
		}
		else {
			val = strtoul(p,&end,0);
			if(p == end)
				return -1;
			*len += end - p - 1;
			p = end;
			return FAX_class1_command(m->fax_obj, FAX_CMD_FTM, val);
		}
	}
	/* +FRM= */
	else if(!toupper_strncmp(p,"RM=",3)) {
		*len += 2;
		p+= 3;
		if(*p == '?') {
			modem_send_to_tty(m,"24,48,72,73,74,96,97,98,121,122,145,146",39);
			modem_send_to_tty(m, CRLF_CHARS(m), 2);
		}
		else {
			val = strtoul(p,&end,0);
			if(p == end)
				return -1;
			*len += end - p - 1;
			p = end;
			return FAX_class1_command(m->fax_obj, FAX_CMD_FRM, val);
		}
		return 0;
	}
	/* +FRH= */
	else if(!toupper_strncmp(p,"RH=",3)) {
		*len += 2;
		p+= 3;
		if(*p == '?') {
			modem_send_to_tty(m,"3",1);
			modem_send_to_tty(m, CRLF_CHARS(m), 2);
			return 0;
		}
		else if (isdigit(*p))
			return FAX_class1_command(m->fax_obj, FAX_CMD_FRH, *p - '0');
		return -1;
	}
	/* +FTH= */
	else if(!toupper_strncmp(p,"TH=",3)) {
		*len += 2;
		p+= 3;
		if(*p == '?') {
			modem_send_to_tty(m,"3",1);
			modem_send_to_tty(m, CRLF_CHARS(m), 2);
			return 0;
		}
		else if (isdigit(*p))
			return FAX_class1_command(m->fax_obj, FAX_CMD_FTH, *p - '0');
		return -1;
	}
	else
		return -1;
}
#else
#define process_fax_command(m,p,plen) (-1)
#endif /* MODEM_CONFIG_FAX */


/* ------------------------------------------------------------ */

/*
 *   Main AT processor
 *
 */

int process_at_command(struct modem *m, char *cmd)
{
	char c, *p = cmd;
	int len = 0, ret = -1;

	/* verify 'AT' */
	if (toupper(*p++) != 'A' || toupper(*p++) != 'T')
		return -1;

	while(*p) {
		c = toupper(*p++);
		len = 0;
		switch(c) {

		case 'A': /* ATA */
			ret = process_A(m,p,&len);
			return ret;
		case 'B': /* ATB */
			ret = process_B(m,p,&len);
			break;
		case 'D': /* ATD */
			ret = process_D(m,p,&len);
			return ret;
		case 'E': /* ATE */
			ret = set_sreg(m,SREG_ECHO,*p,0,1,&len);
			break;
		case 'H': /* ATH */
			ret = process_H(m,p,&len);
			break;
		case 'I': /* ATI */
			ret = process_I(m,p,&len);
			break;
		case 'L': /* ATL */
			ret = set_sreg(m,SREG_SPEAKER_VOLUME,*p,0,3,&len);
			if(!ret) modem_update_speaker(m);
			break;
		case 'M': /* ATM */
			ret = set_sreg(m,SREG_SPEAKER_CONTROL,*p,0,2,&len);
			if(!ret) modem_update_speaker(m);
			break;
		case 'N': /* ATN */
			ret = set_sreg(m,SREG_AUTOMODE,*p,0,1,&len);
			break;
		case 'O': /* ATO */
			ret = process_O(m,p,&len);
			break;
		case 'P': /* ATP */
#ifdef NO_PULSE_DIAL
			return -1;
#endif
			ret = modem_set_sreg(m,SREG_TONE_OR_PULSE,0);
			break;
		case 'Q': /* ATQ */
			ret = set_sreg(m,SREG_QUIET,*p,0,1,&len);
			break;
		case 'S': /* ATS */
			ret = process_S(m,p,&len);
			break;
		case 'T': /* ATT */
			ret = modem_set_sreg(m,SREG_TONE_OR_PULSE,1);
			break;
		case 'V': /* ATV */
			ret = set_sreg(m,SREG_VERBOSE,*p,0,1,&len);
			break;
		case 'X': /* ATX */
			ret = set_sreg(m,SREG_X_CODE,*p,0,4,&len);
			break;
		case 'Y': /* ATY */
			ret = set_sreg(m,SREG_DEFAULT_SETTING,*p,0,3,&len);
			break;
		case 'Z': /* ATZ */
			ret = process_Z(m,p,&len);
			return ret;

		case '#': /* AT# */
#ifdef MODEM_CONFIG_CID
			if(!toupper_strncmp(p,"CID",3)) {
				p+= 3;
				ret = process_CID(m, p, &len);
			}
			else
#endif
				ret = -1;
			break;
		case '%': /* AT% */
			c = toupper(*p++);
			switch (c) {
			case 'C': /* AT%C */
				ret = set_sreg(m,SREG_COMP,*p,0,3,&len);
				break;
			case 'E': /* AT%E */
				ret = set_sreg(m,SREG_LINE_QUALITY_CONTROL,*p,0,2,&len);
				break;
			default:
				return -1;
			} /* end AT% */
			break;
		case '&':
			c = toupper(*p++);
			switch (c) {
			case 'A': /* AT&A */
				ret = set_sreg(m,SREG_CONNNECT_MSG_FORMAT,*p,0,7,&len);
				break;
			case 'C': /* AT&C */
				ret = set_sreg(m,SREG_CD,*p,0,1,&len);
				break;
			case 'D': /* AT&D */
			case 'R': /* AT&R */
			case 'S': /* AT&S */
				len = isdigit(*p)?1:0; ret = 0;
				break;
			case 'E': /* AT&E */
				ret = set_sreg(m,SREG_CONNNECT_MSG_SPEED_SRC,*p,0,1,&len);
				break;
			case 'F': /* AT&F */
				len = isdigit(*p)?1:0; ret = 0;
				break;
			case 'H':  /* AT&H */
				ret = set_sreg(m,SREG_FLOW_CONTROL,*p,0,1,&len);
				break;
			case 'K': /* AT&K */
				ret = set_sreg(m,SREG_COMP,*p,0,3,&len);
				break;
			case 'P': /* AT&P */
				ret = set_sreg(m,SREG_PULSE_RATIO,*p,0,3,&len);
				break;
			case 'V': /* AT&V */
			case 'W': /* AT&W */
				ret = -1;
				break;
#if 0
			case 'Z': /* AT&Z */
				c = toupper(*p++);
				if (c == 'L'){
					ret = print_last_dial_string (m);
					break; /* ZL */
				}
				break; /* Z */
#endif
			default:
				return -1;
			}
			break;
		case '*': /* AT* */
			c = toupper(*p++);
			switch(c) {
			case 'B': /* AT*B */
				break;
			default:
				return -1;
			}
			break;
		case '+':
			c = toupper(*p++);
			switch (c) {
#if 0
			case 'D': /* AT+D */
				c = toupper(*p++);
				switch (c) {
				case 'C': /* AT+DC */
					break;
				case 'S': /* AT+DS */
					break;
				case 'R': /* AT+DR */
					break;
				default:
					return -1;
				}
				break;
#endif
			case 'G': /* AT+G - generic DCE cntrl */
				c = toupper(*p++);
				switch (c) {
				case 'C': /* AT+GCI[=|?|=?] */
					if (toupper(*p++) == 'I')
						ret = process_plus_GCI(m,p,&len);
					else
						return -1;
					break;
				case 'M': /* AT+GM[I|M|R]   */
					c = toupper(*p++);
					switch (c) {
					case 'I': /* AT+GMI */
						modem_send_to_tty(m,modem_author,strlen(modem_author));
						modem_send_to_tty(m,CRLF_CHARS(m),2);
						break;
					case 'M': /* AT+GMM */
						modem_send_to_tty(m,modem_name,strlen(modem_name));
						modem_send_to_tty(m,CRLF_CHARS(m),2);
						break;
					case 'R': /* AT+GMR */
						modem_send_to_tty(m,modem_version,strlen(modem_version));
						modem_send_to_tty(m,CRLF_CHARS(m),2);
						break;
					default:
						return -1;
					}
					ret = 0;
					break;
				default:
					return -1;
				}
				break;
			case 'F': /* AT+F */
				if(!toupper_strncmp(p,"CLASS",5)) {
					p+= 5;
					ret = process_plus_FCLASS(m, p, &len);
				}
				else
					ret = process_fax_command(m, p, &len);
				break;
#ifdef MODEM_CONFIG_VOICE
				/* AT+IFC=2,2 */ /* TBD: flow control - not voice command */
			case 'I':
				// FIXME
				if(!toupper_strncmp(p,"FC=2,2",6)) {
					p+= 6;
					ret = 0;
				}
				else
					ret = -1;
				break;
#endif
			case 'M': /* AT+M */
				c = toupper (*p++);
				if (c == 'S')
					ret = process_plus_MS(m,p,&len);
				break;
			case 'V':
				if(m->mode != MODEM_MODE_VOICE)
					return -1;
#ifdef MODEM_CONFIG_CID
				if(!toupper_strncmp(p,"CID",3)) {
					p+= 3;
					ret = process_CID(m, p, &len);
				}
				else
#endif
					ret = process_voice_command(m, p, &len);
				break;
			default:
				return -1;
			}
			break;
		case '\\':
			c = toupper(*p++);
			switch (c) {
			case 'A': /* AT\A */
				if(isdigit(*p)) len++;
				// ret = set_sreg(m,SREG_V42_BASEREG,*p,0,5,&len);
				break;
#if 0
			case 'B': /* AT\B */
				break;
			case 'K': /* AT\K */
				break;
#endif
			case 'N': /* AT\N */
				ret = set_sreg(m,SREG_EC,*p,0,5,&len);
				break;
			default:
				return -1;
			}
			break;
		default:
			AT_DBG("unknown command `%c` (%x).\n",c,c);
			return -1;
		}
		if (ret) return ret;
		p += len;
	}
	return 0;
}

