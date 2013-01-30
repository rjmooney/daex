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
 * DAEX   - The Digital Audio EXtractor
 * 
 * daex.c - Extracts digital audio from an ATAPI CD-ROM and saves it in the 
 *          WAVE audio format.  Attempts to correct errors by re-reading a
 *          block up to 10 times before failing.
 *
 * Written by Robert Mooney <rmooney@iss.net>, 04/26/98
 *                              Last modified, 05/01/98
 *
 */

#include <stdio.h>
#include <sys/cdio.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include "daex.h"
#include "format.h"


/*========================================================================*/
void fnError(int iExitCode, char *szError, ...)
/*
 * Display a string describing the error, and exit with the specified
 * status.
 *
 *    Input:  szError   - The error string to be displayed.
 *            iExitCode - The exit status code of our error.
 *  Returns:  None.
 */
/*========================================================================*/
{
  va_list stArgumentList;
  char *pBuffer;

  if ((pBuffer = calloc(1, DAEX_MAX_STRING_LEN)) == NULL) {
    fprintf(stderr, "Unable to allocate memory for argument buffer.\n");
    exit(2);
  }

  va_start(stArgumentList, szError);
  vsnprintf(pBuffer, DAEX_MAX_STRING_LEN, szError, stArgumentList);
  va_end(stArgumentList);

  if (szError != NULL)
    fprintf(stderr, "%s", pBuffer);

  free(pBuffer);

  exit(iExitCode);
}

/*========================================================================*/
void fnUsage(char **pArguments) 
/*
 * Displays information on how to use DAEX from the command line.
 *
 *   Input:  pArguments - A pointer to the application arguments.
 * Returns:  None.
 */
/*========================================================================*/
{
  fprintf(stderr, "usage: daex [-d device] [-o outfile] [-s drive_speed] [-t track_no]\n\n");
  fprintf(stderr, "   -d device      :  ATAPI CD-ROM device. (default: /dev/wcd0c)\n");
  fprintf(stderr, "   -o outfile     :  The name of the recorded track. (default: output.wav)\n");
  fprintf(stderr, "   -s drive_speed :  The speed at which the CD audio will be read.\n");
  fprintf(stderr, "                     (default: max) (ex. 4 = 4x (706kb/sec)\n");
  fprintf(stderr, "   -t track_no    :  The track number to extract. (default: 1)\n\n");

  exit(DAEX_EXIT_STATUS);
}


/*========================================================================*/
void fnSetupWAVEheader(void *pvWavHeader, u_long lTotalFileLength, 
                       u_long lTotalBytesWritten)
/*
 * Create the Wave header.  If "lFileLength" and "lTotalBytesWritten" are
 * both 0, we'll assume we're just initializing the structure.  Otherwise,
 * we've (hopefully) finished recording the audio, and need to tidy up the
 * header so Wave decoders will actually process the file.
 *
 *   Input:  pvWavHeader - Pointer to a structure containing the Wave header
 *                         fields.
 *           lFileLength - The total file length of the output file, including
 *                         the Wave header.
 *           lTotalBytesWritten - The number of bytes of raw audio written
 *                                to the output file.
 *
 * Returns:  None.
 */
/*========================================================================*/
{
  struct WavFormat_t *pstWavHeader;

  pstWavHeader = (struct WavFormat_t *)pvWavHeader;

  if ((lTotalFileLength == 0) && (lTotalBytesWritten == 0)) {
    strncpy(pstWavHeader->sRiffHeader,   "RIFF", 4);
    strncpy(pstWavHeader->sWavHeader,    "WAVE", 4);
    strncpy(pstWavHeader->sFormatHeader, "fmt ", 4);
    strncpy(pstWavHeader->sDataHeader,   "data", 4);

    pstWavHeader->lFileLength     = 0;     /* We don't know the filelength   */
    pstWavHeader->lFormatLength   = 0x10;  /* Length of the format data (16) */
    pstWavHeader->nFormatTag      = 0x01;  /* PCM header                     */
    pstWavHeader->nChannels       = 2;     /* 1 = mono, 2 = stereo           */
    pstWavHeader->lSampleRate     = 44100; /* Sample rate in Khz             */
    pstWavHeader->nBitsPerSample  = 16;    /* Bits per sample 8,12,16        */

    /* Block alignment */
    pstWavHeader->nBlockAlign     = (pstWavHeader->nChannels *
                                     pstWavHeader->nBitsPerSample / 8);
    /* Bytes per second */
    pstWavHeader->lBytesPerSecond = (pstWavHeader->lSampleRate * 
                                     pstWavHeader->nBlockAlign);
    /* Length of the data block */
    pstWavHeader->lSampleLength   = 0;

  } else {

   /* We have the final file length information, so we'll fill that in
    * here.  We subtract 8 bytes from lTotalFileLength to account for the 
    * RIFF file header, and the RIFF file length field.
    */

    pstWavHeader->lFileLength   = lTotalFileLength - 8;
    pstWavHeader->lSampleLength = lTotalBytesWritten;
  }
}


void main(int argc, char **argv)
{
  struct  ioc_toc_header     stTOCheader;	/* Table of Contents header  */
  struct  ioc_read_toc_entry stTOCentry;	/* Individual TOC entry      */
  struct  ioc_drive_speed    stDriveSpeed;	/* Drive speed structure     */
  struct  ioc_read_cdda      stReadCDDA;	/* CDDA (raw audio) struct   */
  struct  WavFormat_t        stWavHeader;	/* Wave header               */
  struct  cd_toc_entry       *pstTOCentryData;  /* List of TOC entries       */

  int     iArgument,		/* Current argument in getopt()'s arg list   */
          iBlocksToExtract,     /* Number of blocks a track will span        */
          iBytesWritten,        /* Number of bytes written on a write()      */
          iCount,		/* Temporary counter                         */
          iCurrentBlock,	/* The block number we're current reading    */
          iError,		/* Return status from the various calls      */
          iErrorRecoveryCount,	/* Counter for the error recovery mechanism  */
          iFileDesc,		/* File descriptor for the input device      */
          iLBAend,		/* Ending Logical Block Address for a track  */
          iLBAstart,		/* Starting LBA for a track                  */
          iOutfileDesc,		/* File descriptor for the output file(s)    */
          iTrackNumber;		/* The current track number being extracted  */

  int     iPrintedFlag = 0; 	/* Used in determining how to print errors   */

  u_long  lPercentileMark, 	/* LBA division used to determine % complete */
          lTotalFileLength;	/* The total file length, including headers  */
  u_long  lTotalBytesWritten = 0; /* Total bytes written thus far            */

  u_short nPercent;		/* Percent of the extraction completed       */

  char    szTotalBytesWritten[512];	/* String used to display the status */
  char    *pszBuffer,			/* Raw CDDA buffer                   */
          *szDeviceName,		/* Input device name                 */
          *szOutputFilename;		/* Output file name                  */

  extern int  optind;		/* The current argument number - getopt()    */
  extern char *optarg;		/* Current option's arg. string - getopt()   */


  printf("DAEX v%s - The Digital Audio EXtractor.\n", kVersion);
  printf("(c) Copyright 1998 Robert Mooney, All rights reserved.\n\n");

  /* If there are no arguments, alert the user, display the usage banner, 
   * and exit. 
   */
  if (argc == 1) {
    fprintf(stderr, "daex: requires at least 1 argument\n");
    fnUsage(argv);
  }

  /* Get the command line arguments */
  while ((iArgument = getopt(argc, argv, "d:o:s:t:")) != -1)
   switch(iArgument) {
     case 'd':                       /* Device name                        */
       szDeviceName = strdup(optarg);
       break;
     case 'o':                       /* Output filename                    */
       szOutputFilename = strdup(optarg);
       break;
     case 's':                       /* Drive speed                        */
       fprintf(stderr, "Drive speed is not implemented. Defaulting to 4x...\n");
       break;
     case 't':                       /* Track number                       */
       iTrackNumber = atoi(optarg);
       break;
     case '?':
     default:
       fnUsage(argv);                /* Display the usage banner, and exit */
   }

  /* If there were more arguments than acceptable options,
   * display the usage banner, and exit.
   */
  if (argc != optind)  fnUsage(argv);

#ifdef DEBUG
  printf("Device       : %s\n", szDeviceName);
  printf("Outfile      : %s\n", szOutputFilename);
  printf("Track number : %i\n", iTrackNumber);
#endif

  /* If no device name is specified, we'll default to "/dev/wcd0c" */
  if (szDeviceName == NULL)
    szDeviceName = strdup("/dev/wcd0c");

  /* If no output filename is specified, we'll default to "output.wav" */
  if (szOutputFilename == NULL)
    szOutputFilename = strdup("output.wav");

  /* Attempt to open the CD-ROM device.  Exit upon failure. */
  if ((iFileDesc = open(szDeviceName, O_RDONLY)) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to open CDROM device.\n");

  /* Attempt to read the disc's Table Of Contents.  Exit upon failure. */
  if ((iError = ioctl(iFileDesc, CDIOREADTOCHEADER, &stTOCheader)) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to read TOC header.\n");

  printf("Available tracks: %i thru %i.\n\n", stTOCheader.starting_track,
                                              stTOCheader.ending_track);

  /* If the track number wasn't specified, we'll default to track 1 */
  if (iTrackNumber == 0)  iTrackNumber = 1;

  /* If the track number specified is not within the range of tracks on
   * the disc, alert the user, and exit.
   */
  if ((iTrackNumber < stTOCheader.starting_track) ||
      (iTrackNumber > stTOCheader.ending_track))
    fnError(DAEX_EXIT_STATUS, "The track number you specified is not within the proper range.\n");

  /* Setup the TOC entry structure to return data in the Logical Block
   * Address format.  Include the starting and finishing tracks on return
   */
  stTOCentry.address_format = CD_LBA_FORMAT;
  stTOCentry.starting_track = stTOCheader.starting_track;
  stTOCentry.data_len       = (stTOCheader.ending_track + 1)
                              * sizeof(struct cd_toc_entry);


  /* Attempt to allocate space for the individual track information.
   * If there's no enough memory available, exit.
   */

  if ((pstTOCentryData = (struct cd_toc_entry *)calloc(1, stTOCentry.data_len)) == NULL)
    fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for TOC entry data.\n");

  stTOCentry.data = pstTOCentryData;

  /* Attempt to read the individual track information.  Exit upon failure. */
  if ((iError = ioctl(iFileDesc, CDIOREADTOCENTRYS, &stTOCentry)) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to read TOC entries.\n");

  /* Convert the starting and ending LBA's to host-byte order */
  iLBAstart = ntohl(stTOCentry.data[iTrackNumber - 1].addr.lba);
  iLBAend   = ntohl(stTOCentry.data[iTrackNumber].addr.lba);

  /* free the memory used for the TOC entry array */
  free(pstTOCentryData);

/*  sDriveSpeed.speed = 176; */
/*  stDriveSpeed.speed = 353; */
  stDriveSpeed.speed = 706;

  /* Attempt to set the drive's read speed.  Exit upon failure. */
  if ((iError = ioctl(iFileDesc, CDIOSETSPEED, &stDriveSpeed)) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to set drive speed.\n");

  /* Attempt to allocate memory for the raw audio data that will be read
   * from the disc.
   */
  if ((pszBuffer = (char *)calloc(1, CDDA_DATA_LENGTH)) == NULL)
    fnError(DAEX_EXIT_STATUS, "Unable to allocate memory for CDDA buffer.\n");

  /* Open the output file.  Exit upon failure. */
  if ((iOutfileDesc = open(szOutputFilename, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0) {
    if (errno == EEXIST)
      fnError(DAEX_EXIT_STATUS, "The output file \"%s\" already exists.\n", szOutputFilename);
     else
      fnError(DAEX_EXIT_STATUS, "Unable to open output file, \"%s\"...\n", szOutputFilename);
  }

  printf("Extracting track #%i to \"%s\" at 4x (706kb/sec)...\n", iTrackNumber, szOutputFilename);

  /* Initialize the Wave header */
  fnSetupWAVEheader(&stWavHeader, 0, 0);

  /* Write the audio header to the output file.  Exit upon error.  */
  if (write(iOutfileDesc, &stWavHeader, sizeof(struct WavFormat_t)) != sizeof(struct WavFormat_t))
    fnError(DAEX_EXIT_STATUS, "Unable to write audio header.\n");

  /* Setup the CDDA-read structure. */
  stReadCDDA.frames = 1;              /* Number of 2352 byte blocks to read */
  stReadCDDA.buffer = pszBuffer;      /* Copy the buffer pointer            */

  /* Initialize the status variables for use during extraction */
  iBlocksToExtract = (iLBAend - iLBAstart);
  iCurrentBlock    = 0;
  lPercentileMark  = (iBlocksToExtract / 100);
  nPercent         = 0;

  /* Read the individual blocks for the specified track.  Display a status
   * line, and attempt to recover from any errors we run into.
   */

  for (stReadCDDA.lba=iLBAstart; stReadCDDA.lba <= iLBAend; stReadCDDA.lba++) {
    pszBuffer[0] = '\0';

    /* Reset the error recovery count for each block read */
    iErrorRecoveryCount = 0;

    /* Read the raw audio from disc.  If the read fails, increment the
     * recovery count, and try again - we'll allow up to 10 failures on the
     * current block before we decide to give up.
     */
    while ( (ioctl(iFileDesc, CDIOREADCDDA, &stReadCDDA) != 0) &&
             (iErrorRecoveryCount < 10) ) {

      /* we're displaying the status, so we need to skip a line */
      if ((iErrorRecoveryCount == 0) && (iPrintedFlag == 1)) {
        printf("\n");
        iPrintedFlag = 0;
      }

      fprintf(stderr, "- retrying LBA %i -\n", stReadCDDA.lba);
      iErrorRecoveryCount++;
    }

    /* If the we've exceeded a 10 count on errors, something must be
     * wrong with the disc.  There may be a better way to handle this,
     * so it's possible this method will go away in the future.
     */ 

    if (iErrorRecoveryCount >= 10)
      fnError(DAEX_EXIT_STATUS, "Unable to recover from error, giving up.  Maybe the disc is scratched?\n");

    /* Write the returned buffer of raw data to disk.  If not all 2352
     * bytes were written to disk, display an error message and exit...
     * otherwise,  increment the the iTotalBytesWritten counter. 
     */

    if ((iBytesWritten = write(iOutfileDesc, pszBuffer, CDDA_DATA_LENGTH)) != CDDA_DATA_LENGTH)
      fnError(DAEX_EXIT_STATUS, "Incorrect number of bytes written to output file (%i of %i).\n", iBytesWritten, CDDA_DATA_LENGTH);

    lTotalBytesWritten += iBytesWritten;

    /* Update the status line.  Allow 50 horizontal characters. */
    if ((iCurrentBlock % lPercentileMark) == 0) {

      /* Print 50 backspaces. */
      for (iCount=0; iCount < 50; iCount++) putc(8, stderr);

      snprintf(szTotalBytesWritten, sizeof(szTotalBytesWritten), "%ld of %ld blocks written (%i%%)... ", iCurrentBlock, iBlocksToExtract, nPercent);
      fprintf(stderr, "%-50s", szTotalBytesWritten);
      iPrintedFlag = 1;

      /* Increment the "percent complete" count */
      nPercent++;
    }

    /* Increment the current block count.  Used in determining whether or
     * or not the "percent complete" count should increase.
     */
    iCurrentBlock++;
  }
  /* Dispose of the raw audio buffer */
  free(pszBuffer); 

  /* Calculate the total file length written, including the audio
   * header.
   */
  lTotalFileLength = lTotalBytesWritten + sizeof(struct WavFormat_t);

  /* Fix up the Wave file headers */
  fnSetupWAVEheader(&stWavHeader, lTotalFileLength, lTotalBytesWritten);

  /* Seek to the begining of the output file */
  if (lseek(iOutfileDesc, 0, SEEK_SET) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to seek begining of output file.\n");

  /* ... and rewrite the audio header */
  if (write(iOutfileDesc, &stWavHeader, sizeof(struct WavFormat_t)) != sizeof(struct WavFormat_t))
    fnError(DAEX_EXIT_STATUS, "Unable to write audio header.\n");

  /* Close the device and output files */
  close(iOutfileDesc);
  close(iFileDesc);

  /* Display the amount of data written to the output file. */
  printf("\nWrote %ld bytes of data.\n\n", lTotalFileLength); 

  /* Dispose of the option arguments */
  free(szDeviceName);
  free(szOutputFilename);

  /* We're done. Yeah! */
  printf("DAEX finished.\n");
}
