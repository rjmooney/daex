# Makefile for DAEX
#
# DAEX - The Digital Audio EXtractor
# (c) 1998 Robert Mooney, All rights reserved.
#
# For more information, mail "rmooney@iss.net"...
#
# $Id: Makefile,v 0.9 1998/10/05 17:31:55 rmooney Exp $
#

DAEX_VERSION= 0.90a

CC= gcc
CFLAGS_DEBUG= -g -Wall
CFLAGS_OPTIMIZE= -O2

INSTALL= /usr/bin/install -c -g bin -o root
INSTALL_BINDIR= /usr/local/bin
INSTALL_MANDIR= /usr/local/man/man1

.ifmake daex-debug
  CFLAGS= ${CFLAGS_DEBUG}
.else
  CFLAGS= ${CFLAGS_OPTIMIZE}
.endif

all: daex
daex-debug: all

clean:
	rm -rf *.o core daex.core daex daex${DAEX_VERSION}

realclean: clean
	rm -f daex${DAEX_VERSION}.tgz

daex: daex.o cddb.o
	${CC} ${CFLAGS} -o daex daex.o cddb.o

daex.o: daex.c daex.h format.h
	${CC} ${CFLAGS} -c daex.c

cddb.o: cddb.c cddb.h
	${CC} ${CFLAGS} -c cddb.c

install:
	${INSTALL} -m 4755 daex ${INSTALL_BINDIR}
	${INSTALL} -m 0644 daex.1 ${INSTALL_MANDIR}

uninstall:
	if [ -f ${INSTALL_BINDIR}/daex ]; then \
	 rm -f ${INSTALL_BINDIR}/daex; \
	fi

	if [ -f ${INSTALL_MANDIR}/daex.1 ]; then \
	 rm -f ${INSTALL_MANDIR}/daex.1; \
	fi

dist:
	mkdir daex${DAEX_VERSION}
	cp Makefile HISTORY README THANKS TODO *.c *.h daex.1 daex${DAEX_VERSION}
	tar cfz daex${DAEX_VERSION}.tgz daex${DAEX_VERSION}
	rm -rf daex${DAEX_VERSION}
