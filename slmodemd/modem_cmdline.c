
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
 *    modem_cmdline.c  --  simple command line processor,
 *                         define config parameters.
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <modem.h>
#include <modem_homolog.h>
#include <modem_debug.h>

#define PR_INFO(fmt...) fprintf(stderr, fmt )


/* global config data */

/* modem.c */
extern const char *modem_default_country;

/* modem_debug.c */
extern unsigned int modem_debug_level;
extern unsigned int modem_debug_logging;

/* config parameters */
const char *modem_dev_name = NULL;
const char *modem_default_dev_name = "/dev/slamr0";
const char *modem_alsa_dev_name = "modem:1";
const char *modem_exec = NULL;
unsigned int need_realtime = 1;
#ifdef MODEM_CONFIG_RING_DETECTOR
unsigned int ring_detector = 0;
#endif
unsigned int use_alsa = 0;
const char *modem_group = "dialout";
unsigned int use_short_buffer = 0;
mode_t modem_perm  = 0660;


enum {
	OPT_HELP = 0,
	OPT_USAGE,
	OPT_VERSION,
	OPT_COUNTRY,
	OPT_COUNTRYLIST,
	OPT_ALSA,
	OPT_GROUP,
	OPT_PERM,
#ifdef MODEM_CONFIG_RING_DETECTOR
	OPT_RINGDET,
#endif
	OPT_USER,
	OPT_SHORTBUF,
	OPT_DEBUG,
	OPT_LOG,
	OPT_EXEC,
	OPT_LAST
};

static struct opt {
	int  ch;
	const char *name;
	const char *desc;
	enum {NONE=0,MANDATORY,OPTIONAL} arg;
	enum {INTEGER,STRING} arg_type;
	const char *arg_val;
	unsigned found;
} opt_list[] = {
	{'h',"help","this usage"},
	{'u',"usage","this usage"},
	{'v',"version","show version and exit"},
	{'c',"country","default modem country name",MANDATORY,STRING,"USA"},
	{ 0 ,"countrylist","show list of supported countries"},
	{'a',"alsa","ALSA mode (see README for howto)"},
	{'g',"group","Modem TTY group",MANDATORY,STRING,"dialout"},
	{'p',"perm","Modem TTY permission",MANDATORY,INTEGER,"0660"},
#ifdef MODEM_CONFIG_RING_DETECTOR
	{'r',"ringdetector","with internal ring detector (software)"},
#endif
	{'n',"nortpriority","run with regular priority"},
	{'s',"shortbuffer","use short buffer (4 periods length)"},
	{'d',"debug","debug level (developers only, for ./sl...)",OPTIONAL,INTEGER,"0"},
	{'l',"log","logging mode",OPTIONAL,INTEGER,"5"},
	{'e',"exec","path to external application that transmits audio over the socket (required)",MANDATORY,STRING,""},
	{}
};



static void usage(const char *prog_name)
{
	struct opt *opt;
	PR_INFO("Usage: %s [option...] <device>\n"
		"Where 'device' is name of modem device (default `%s')\n"
		"  and 'option' may be:\n", prog_name, modem_default_dev_name);
	for (opt = opt_list ; opt->name ; opt++ ) {
		int n = 0;
		if(opt->ch)
			n = PR_INFO("  -%c, ",opt->ch);
		else
			n = PR_INFO("      ");
		n += PR_INFO("--%s%s ",opt->name,opt->arg?"=VAL":"");
		n += PR_INFO("%*s%s",24-n,"",opt->desc);
		if(opt->arg) {
			n+= PR_INFO(" (default `%s')",opt->arg_val);
		}
		PR_INFO("\n");
	}
	exit(1);
}


static void opt_parse(int argc, char *argv[])
{
	const char *prog_name;
	const char *dev_name = NULL;
	struct opt *opt;
	char *p, *arg;
	int found, i;
	
	prog_name = argv[0];

	for( i = 1 ; i < argc ; i++ ) {
		p = argv[i];
		if(*p != '-') {
			if(dev_name)
				usage(prog_name);
			dev_name = p;
			continue;
		}
		found = 0;
		arg = NULL;
		do { p++; } while( *p == '-' );
		for (opt = opt_list ; opt->name ; opt++ ) {
			if(!strncmp(p,opt->name,strlen(opt->name)) ||
			   (opt->ch && *p == opt->ch) ) {
				char *q;
				if(!strncmp(p,opt->name,strlen(opt->name)))
					q = p + strlen(opt->name);
				else
					q = p + 1;
				if( *q == '\0' )
					found = 1;
				else if (*q == '=' && opt->arg ) {
					arg = q + 1;
					found = 1;
				}
				else if(isdigit(*q) && opt->arg ) {
					arg = q;
					found = 1;
				}
			}

			if ( !found )
				continue;
			if ( arg && !opt->arg )
				usage(prog_name);

			if (!arg && opt->arg &&
			    (i < argc - 1 && *argv[i+1] != '-') &&
			    (!opt->arg_type == INTEGER || isdigit(*argv[i+1])))
				arg = argv[++i];

			if(!arg && opt->arg == MANDATORY)
				usage(prog_name);

			opt->found++;
			if(opt->arg && arg)
				opt->arg_val = arg;
			break;
		}
		if(!found)
			usage(prog_name);
	}

	if(dev_name)
		modem_dev_name = dev_name;
}



void modem_cmdline(int argc, char *argv[])
{
	const char *prog_name = argv[0];
	int val;

	opt_parse(argc,argv);

	if(opt_list[OPT_HELP].found ||
	   opt_list[OPT_USAGE].found )
		usage(prog_name);

	if(opt_list[OPT_VERSION].found) {
		extern void modem_print_version();
		modem_print_version();
		exit(0);
	}

	if(opt_list[OPT_COUNTRYLIST].found) {
		const struct homolog_set *s ;
		for( s = homolog_set; s->name ; s++ ) {
			PR_INFO("%02x: %s\n",s->id,s->name);
		}
		exit(0);
	}
	if(opt_list[OPT_COUNTRY].found)
		modem_default_country = opt_list[OPT_COUNTRY].arg_val;
	if(opt_list[OPT_ALSA].found) {
#ifndef SUPPORT_ALSA
		PR_INFO("ALSA support is not compiled in (see README for howto).\n");
		exit(1);
#endif
		use_alsa = 1;
	}
	if(opt_list[OPT_GROUP].found)
		modem_group = opt_list[OPT_GROUP].arg_val;
	if(opt_list[OPT_PERM].found) {
		val = strtol(opt_list[OPT_PERM].arg_val,NULL,8);
		if (val <= 0)
			usage(prog_name);
		modem_perm = val;
	}
#ifdef MODEM_CONFIG_RING_DETECTOR
	if(opt_list[OPT_RINGDET].found)
		ring_detector = 1;
#endif
	if(opt_list[OPT_USER].found)
		need_realtime = 0;
	if(opt_list[OPT_SHORTBUF].found)
		use_short_buffer = 1;
	if(opt_list[OPT_DEBUG].found) {
		modem_debug_level = 1 ;
		if(opt_list[OPT_DEBUG].arg_val &&
		   (val= strtol(opt_list[OPT_DEBUG].arg_val,NULL,0)) > 0 )
			modem_debug_level = val;
	}
	if(opt_list[OPT_LOG].found) {
		modem_debug_logging = 5;
		if(opt_list[OPT_LOG].arg_val &&
		   (val= strtol(opt_list[OPT_LOG].arg_val,NULL,0)) > 0 )
			modem_debug_logging = val;
	}
	if(opt_list[OPT_EXEC].found) {
		modem_exec = opt_list[OPT_EXEC].arg_val;
	} else {
		usage(prog_name);
	}
	if(!modem_dev_name) {
		modem_dev_name = use_alsa ? modem_alsa_dev_name : modem_default_dev_name;
	}
}

