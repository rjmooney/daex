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
 * daex.h - Header for the DAEX package.
 *
 * $Id: daex.h,v 0.14 1998/10/13 20:41:38 rmooney Exp $
 */

#include <stdio.h>
#include <sys/cdio.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#undef  DEBUG				/* Define for DEBUG mode                   */

#define CDDA_DATA_LENGTH	2352	/* CDDA data segment size                  */
#define MAX_FILENAME_LENGTH     255     /* Max filename length defined by POSIX    */

#define CDIO_PRE_EMPHASIS       0x01    /* ON: premphasis, OFF: no premphasis      */
#define CDIO_DATA_TRACK         0x04    /* ON: data track, OFF: audio track        */
#define CDIO_COPY_PERMITTED     0x02    /* ON: allow copy, OFF: copy protect       */
#define CDIO_FOUR_CHANNEL       0x08    /* ON: 4 channel, OFF: 2 channel           */
   
#define kiExitStatus_General	1	/* Exit with this code upon error          */
#define kiMaxStringLength	1024	/* Maximum string length                   */

#define kiHeaderWAVE		0	/* Wave header flag                        */

#define kiSetPrivilege		0	/* Internal set privilege flags            */
#define kiRelPrivilege		1	/* Internal release priv. flag             */

/* Data rate descriptions */
#define kszSpeedDefault		"Default"
#define kszSpeedMaximum		"Maximum"
#define kszSpeed1X		"1x (176 kbytes/sec)"
#define kszSpeed2X		"2x (353 kbytes/sec)"
#define kszSpeed3X		"3x (528 kbytes/sec)"
#define kszSpeed4X		"4x (706 kbytes/sec)"
#define kszSpeed8X		"8x (1.4 Mbytes/sec)"
#define kszSpeed16X		"16x (2.8 Mbytes/sec)"

#define kszVersion		"0.90a"	     /* Current working version of DAEX    */


/* Disc structure which contains pointers to commonly used structures and variables
 * within DAEX.
 */
struct DiscInformation_t {
  struct ioc_toc_header     *pstTOCheader;   /* Table of Contents header           */
  struct ioc_read_toc_entry *pstTOCentries;  /* Track entries' header              */
  struct CDDBinformation_t  *pstCDDBinformation; /* Disc information structure     */
  struct TrackInformation_t *pstTrackData;   /* Individual track information       */

  char *szDriveSpeed;                        /* Drive speed description string     */
};

/* Track structure which contains various information used in the extraction
 * of the specified tracks.
 */
struct TrackInformation_t {
  int iTrackNumber;                 /* The track number to extract                 */
  char *szTrackFilename;            /* Track's output file name                    */

  int iFixedLBA_start,              /* Track's starting LBA                        */
      iFixedLBA_end;                /* Track's ending LBA                          */
};

/* EOF */
