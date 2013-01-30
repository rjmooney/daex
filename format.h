/* format.h  -  Headers for various audio file formats
 *
 * This package, all included text and source are
 * (c) Copyright 1998 Robert Mooney, All rights reserved.
 *
 * Written by Robert Mooney <rmooney@iss.net>, v.01a 04/26/98
 *
 */

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
