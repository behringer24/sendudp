#
# Makefile for udplogd
#

PREFIX	= /usr/local
CFGFILE	= $(PREFIX)/sbin/udplogd.conf
CC	= gcc
## Solaris users use:
# FLAGS	= -Wall -O2 -DCFGFILE=\"$(CFGFILE)\" -DSOLARIS
## other Unices use:
FLAGS	= -Wall -O2 -DCFGFILE=\"$(CFGFILE)\"

all:	udplogd sendudp

install:	udplogd sendudp
	install -m 755 udplogd $(PREFIX)/sbin/udplogd && \
	install -m 755 sendudp $(PREFIX)/bin/sendudp && \
	install -m 644 udplogd.8 $(PREFIX)/man/man8/udplogd.8 && \
	install -m 644 udplogd.conf.5 $(PREFIX)/man/man5/udplogd.conf.5 && \
	install -m 644 sendudp.1 $(PREFIX)/man/man1/sendudp.1 && \
	install -m 644 sample-udplogd.conf $(CFGFILE) && \
	echo Installation complete

server:	udplogd

client:	sendudp

udplogd:	udplogd.c
	$(CC) $(FLAGS) -o udplogd udplogd.c

sendudp:	sendudp.c
	$(CC) $(FLAGS) -o sendudp sendudp.c
