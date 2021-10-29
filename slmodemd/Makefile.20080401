###########################################################################
#
#
#       Makefile  --  modem application Makefile.
#
#       Copyright(c) 2003, Smart Link Ltd. (www.smlink.com)
#	All rights reserved.
#
#       Author: Sasha K (sashak@smlink.com)
#
#
###########################################################################
#
###########################################################################

ARCH64:=$(shell uname -m | sed -e '/amd64/s//x86_64/' | grep -c x86_64)

ifeq ($(ARCH64), 0)
CC:= gcc
else
# SUPPORT_ALSA:=1
CC:= gcc -m32
endif

RM:= rm -f

CFLAGS+= -Wall -g -O -I. -DCONFIG_DEBUG_MODEM

modem-objs:= \
	modem.o modem_datafile.o modem_at.o modem_timer.o \
	modem_pack.o modem_ec.o modem_comp.o \
	modem_param.o modem_debug.o homolog_data.o
dp-objs:= dp_sinus.o dp_dummy.o
sysdep-objs:= sysdep_common.o
all-objs:= modem_cmdline.o $(modem-objs) $(dp-objs) dsplibs.o $(sysdep-objs) 

all: slmodemd modem_test

slmodemd: modem_main.o $(all-objs)
modem_test: modem_test.o $(all-objs)

ifdef SUPPORT_ALSA
CFLAGS+= -DSUPPORT_ALSA=1
ifeq ($(ARCH64), 0)
LFLAGS+= /usr/lib/libasound.so
else
# LFLAGS+= /usr/lib32/libasound.so , replaced by -l commands search for asound
LFLAGS+= -lasound
endif
endif

slmodemd:
	$(CC) -o slmodemd modem_main.o $(all-objs) $(LFLAGS)

modem_test:
	$(CC) -o modem_test modem_test.o $(all-objs) $(LFLAGS)

clean:
	$(RM) slmodemd modem_test modem_main.o modem_cmdline.o modem_test.o $(modem-objs) $(dp-objs) $(sysdep-objs)
	$(RM) *~ *.orig *.rej

.PHONY: all dep generic-dep clean clean-build-profile


#
# rules
#

%.o: %.c .build_profile
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -o $@ -c $<

dep: generic-dep
generic-dep:
	$(RM) .depend
	$(foreach src,$(wildcard *.c),$(CC) -M $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$(src:.c=.o)) -c $(src) >> .depend ;)

-include .depend

# build_profile
-include .build_profile

.build_profile::
ifneq ($(CFLAGS),$(PROFILE_CFLAGS))
	@if [ "$(CFLAGS) $(EXTRA_CFLAGS)" != "$(PROFILE_CFLAGS)" ] ; then \
	  echo "rebuild profile..." ; \
	  echo "PROFILE_CFLAGS=$(CFLAGS) $(EXTRA_CFLAGS)" > $(@) ; fi
endif

clean: clean-build-profile
clean-build-profile:
	@$(RM) -f .build_profile

