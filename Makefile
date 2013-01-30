# Makefile for DAEX
#
# DAEX - The Digital Audio Extractor
# (c) 1998 Robert Mooney, All rights reserved.
#
# For more information, mail "rmooney@iss.net"...
#

INSTALLDIR= /usr/local/bin
CC= gcc
CFLAGS= -O2 -Wall -ansi

all: daex

clean:
	rm -rf *.o core daex

daex: daex.o
	$(CC) $(CFLAGS) -o daex daex.o

daex.o: daex.c format.h
	$(CC) -c daex.c

install:
	install -g bin -o bin -m 755 daex $(INSTALLDIR)

backup:
	tar cvfz daex_backup.tgz Makefile daex.c format.h atapimods_1.0.patch
