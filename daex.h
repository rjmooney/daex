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
 * $Id: daex.h,v 0.5 1998/05/24 09:46:03 rmooney Exp $
 */

#undef DEBUG				/* define for DEBUG mode           */

#define CDDA_DATA_LENGTH	2352	/* CDDA data segment size          */

#define CDIO_PRE_EMPHASIS       0x01    /* ON: premphasis, OFF: no premphasis */
#define CDIO_DATA_TRACK         0x04    /* ON: data track, OFF: audio track   */
#define CDIO_COPY_PERMITTED     0x02    /* ON: allow copy, OFF: copy protect  */
#define CDIO_FOUR_CHANNEL       0x08    /* ON: 4 channel, OFF: 2 channel      */
   
#define DAEX_EXIT_STATUS	1	/* exit with this code upon error  */
#define DAEX_MAX_STRING_LEN	1024	/* maximum string length           */

#define DAEX_HEADER_WAVE	0	/* Wave header flag              */

#define DAEX_SET_PRIVILEGE	0	/* Internal set privilege flags  */
#define DAEX_REL_PRIVILEGE	1	/* Internal release priv. flag   */

#define kVersion		"0.4a"	/* current working version of DAEX */

/* EOF */
