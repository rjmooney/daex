/*
 * Copyright (c) 1988 Robert Mooney
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * DAEX   - The Digital Audio EXtractor
 * 
 * cddb.h - Header for the CDDB portion of the DAEX package.
 *
 * $Id: cddb.h,v 1.6 1998/10/11 04:38:45 rmooney Exp $ 
 */

#include <stdio.h>
#include <sys/cdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#undef  CDDB_DEBUG                    /* Define for CDDB DEBUG */
#define MAX_CDDB_LINE_LENGTH 256      /* Maximum CDDB line length as defined in the CDDB
                                       * specifications. */

/* Session status flags */
#define SESSION_CONNECTING  0         /* Session is currently connecting. */
#define SESSION_SENT_HELLO  1         /* Initial handshake has been sent. */
#define SESSION_SENT_QUERY  2         /* CDDB "query" has been sent.      */
#define SESSION_SENT_READ   3         /* CDDB "read" has been sent.       */


/* CDDB structure which contains the disc and individual track titles. */
struct CDDBinformation_t {
  char *szDiscTitle;                  /* Disc title                               */
  char **szTrackTitle;                /* Array of track titles                    */

  int  iDiscTotalSeconds;             /* Total playing seconds on the disc        */
  int  *iTrackSeconds;                /* Array of each track's play time          */
  int  *iTrackFrameOffset;            /* Array of each track's frame offset       */

  char szDiscID[9];                   /* Disc's CDDB Disc ID                      */
  char *szDiscCategory;               /* Disc's CDDB category                     */

  char *szCDDBquery;                  /* CDDB query string                        */
};

/* CDDB function prototypes. */
int  fnCDDB_BuildQueryString(void *pvDiscInformation);
int  fnCDDB_DoCDDBQuery(void *pvDiscInformation, char *szCDDB_RemoteHost,
                        int CDDB_RemotePort, int iCDDB_StoreFilenames);
int  fnCDDB_DumpCDDBInfo(void *pvDiscInformation, char *szFilename);
void fnCDDB_FreeCDDBInformationStruct(void **pvCDDBinfo, int iTracksExpected);

/* EOF */
