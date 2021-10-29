
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
 *    modem_timer.c  --  simple timer (will not in multithreaded env).
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */

#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <limits.h>

#include <modem_timer.h>

#if 0
#include <stdio.h>
#define WARN(fmt...) fprintf(stderr, fmt);
#else
#define WARN(fmt...)
#endif

#define MAX_TIME LONG_MAX

static struct timeval tv_init;

static struct timer_list_struct {
	struct timer *list;
	unsigned long when;
	struct sigaction old_act;
} timer_list;



unsigned long get_time()
{
	struct timeval tv_now,tv;
	gettimeofday(&tv_now,NULL);
	timersub(&tv_now,&tv_init,&tv);
	return tv.tv_sec*MODEM_HZ + tv.tv_usec/(1000000/MODEM_HZ);
}

static void set_timer(unsigned long when)
{
	struct itimerval it;
	long t;
	t = (long)(when - get_time());
	if( t <= 0) t = 1;
	it.it_interval.tv_usec = 0;
	it.it_interval.tv_sec  = 0;
	it.it_value.tv_usec = (t%MODEM_HZ)*(1000000/MODEM_HZ);
	it.it_value.tv_sec  = t/MODEM_HZ;
	setitimer(ITIMER_REAL,&it,NULL);
}


static void timer_list_handler(int num)
{
	unsigned long now;
	struct timer *t,*next;
	now = get_time();
	t = timer_list.list;
	timer_list.list = NULL;
	while(t) {
		next = t->next;
		t->next    = NULL;
		if( (long)(t->expires - now) <= 0 ) {
			t->added = 0;
			if(t->func) {
				t->func(t->data);
			}
		}
		else {
			t->next = timer_list.list;
			timer_list.list = t;
		}
		t = next;
	}
	timer_list.when = now + MAX_TIME;
	t = timer_list.list;
	while(t) {
		if((long)(timer_list.when - t->expires) > 0)
			timer_list.when = t->expires;
		t = t->next;
	}
	set_timer(timer_list.when);
}


void timer_init(struct timer *t)
{
	t->next = 0;
	t->added = 0;
	t->expires = 0;
	t->func = NULL;
	t->data = NULL;
}

int timer_add(struct timer *t)
{
	sigset_t set, old_set;
	sigemptyset(&set);
	sigaddset(&set,SIGALRM);
	sigprocmask(SIG_BLOCK,&set,&old_set);
	if(t->added) {
		WARN("timer_add: already.\n");
	}
	else {
		t->added = 1;
		t->next = timer_list.list;
		timer_list.list = t;
	}
	if((long)(timer_list.when - t->expires) > 0) {
		timer_list.when = t->expires;
		set_timer(timer_list.when);
	}
	sigprocmask(SIG_SETMASK,&old_set,NULL);
	return 0;
}

int timer_del(struct timer *t)
{
	sigset_t set, old_set;
	struct timer *p, **prev;
	sigemptyset(&set);
	sigaddset(&set,SIGALRM);
	sigprocmask(SIG_BLOCK,&set,&old_set);
	if(t->added) {
		p = timer_list.list;
		prev = &timer_list.list;
		while(p) {
			if(p == t) {
				*prev = t->next;
				t->next = NULL;
				t->added = 0;
				break;
			}
			prev = &((*prev)->next);
			p = p->next;
		}
	}
	else {
		WARN("timer_del: not added\n");
	}
	sigprocmask(SIG_SETMASK,&old_set,NULL);
	return 0;
}


int modem_timer_init()
{
	struct sigaction act;
	gettimeofday(&tv_init,NULL);
	timer_list.list = NULL;
	timer_list.when = get_time() + MAX_TIME;
	act.sa_handler = timer_list_handler;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGALRM,&act,&timer_list.old_act);
	set_timer(timer_list.when);
	return 0;
}

void modem_timer_exit()
{
	sigaction(SIGALRM,&timer_list.old_act,NULL);
}


