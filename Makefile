#
# Makefile for udplogd
#

PREFIX	= /usr/local
CFGFILE	= $(PREFIX)/sbin/yasul.conf
CC	= gcc
## Solaris users use:
# FLAGS	= -Wall -O2 -DCFGFILE=\"$(CFGFILE)\" -DSOLARIS
## other Unices use:
FLAGS	= -Wall -O2 -DCFGFILE=\"$(CFGFILE)\"

all:	yasul sendudp

install:    yasul sendudp
	install -m 755 yasul $(PREFIX)/sbin/yasul && \
	install -m 755 sendudp $(PREFIX)/bin/sendudp && \
	install -m 644 yasul.8 $(PREFIX)/man/man8/yasul.8 && \
	install -m 644 yasul.conf.5 $(PREFIX)/man/man5/yasul.conf.5 && \
	install -m 644 sendudp.1 $(PREFIX)/man/man1/yasul.1 && \
	install -m 644 sample-yasul.conf $(CFGFILE) && \
	echo Installation complete

server:	yasul

client:	sendudp

yasul:	yasul.c
	$(CC) $(FLAGS) -o yasul yasul.c

sendudp:	sendudp.c
	$(CC) $(FLAGS) -o sendudp sendudp.c
