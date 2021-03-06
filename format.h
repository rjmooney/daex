/*
 * Copyright (c) 1998 Robert Mooney
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
 * DAEX      - The Digital Audio EXtractor
 * 
 * format.h  - Headers for various audio file formats
 * 
 * $Id: format.h,v 0.1 1998/10/11 04:38:46 rmooney Exp $
 */

/* WAV format */
struct WavFormat_t {
  u_char   sRiffHeader[4];     /* RIFF chunk header                 */
  u_long   lFileLength;        /* (lFileLength - 8)                 */

  u_char   sWavHeader[4];      /* WAV chunk header                  */

  u_char   sFormatHeader[4];   /* Sample format header              */
  u_long   lFormatLength;      /* Length of format data (16 bytes)  */
  u_short  nFormatTag;         /* Format tag, 1 = PCM               */
  u_short  nChannels;          /* Channels, 1 = mono, 2 = stereo    */
  u_long   lSampleRate;        /* Sample rate (hz)                  */
  u_long   lBytesPerSecond;    /* (lSampleRate * nBlockAlign)       */
  u_short  nBlockAlign;        /* (nChannels * nBitsPerSample / 8 ) */
  u_short  nBitsPerSample;     /* Bits per sample, 8 or 16          */

  u_char   sDataHeader[4];     /* Data chunk header                 */
  u_long   lSampleLength;      /* Sample data length                */
};

/* EOF */
