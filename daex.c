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
 * $Id: daex.c,v 0.9 1998/05/24 09:46:03 rmooney Exp $
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
void 
fnError(int iExitCode, char *szError, ...)
/*
 * Display a string describing the error, and exit with the specified
 * status.
 *
 *    Input:  iExitCode - The exit status code of our error.
 *            szError   - The error string to be displayed.
 *  Returns:  None.
 */
/*========================================================================*/
{
  va_list stArgumentList;			/* Variable argument list   */
  char *pBuffer;				/* Temporary output buffer  */

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnError()\n");
#endif

  if ((pBuffer = calloc(1, DAEX_MAX_STRING_LEN)) == NULL) {
    fprintf(stderr, "Unable to allocate memory for argument buffer.\n");
    exit(2);
  }

  va_start(stArgumentList, szError);
  vsnprintf(pBuffer, DAEX_MAX_STRING_LEN, szError, stArgumentList);
  va_end(stArgumentList);

  if (szError != NULL)
    fprintf(stderr, "%s\n", pBuffer);

  free(pBuffer);

  exit(iExitCode);
}


/*========================================================================*/
void
fnUsage(char **pArguments) 
/*
 * Displays information on how to use DAEX from the command line.
 *
 *   Input:  pArguments - A pointer to the application arguments.
 * Returns:  None.
 */
/*========================================================================*/
{

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnUsage()\n");
#endif

  fprintf(stderr, "usage: daex [-d device] [-o outfile] [-s drive_speed] [-t track_no]\n\n");
  fprintf(stderr, "   -d device      :  ATAPI CD-ROM device. (default: /dev/wcd0c)\n");
  fprintf(stderr, "   -o outfile     :  The name of the recorded track. (default: output.wav)\n");
  fprintf(stderr, "   -s drive_speed :  The speed at which the CD audio will be read.\n");
  fprintf(stderr, "                     (default: don't attempt to set drive speed)\n\n");

  fprintf(stderr, "                       -s 0 == Maximum allowable\n");
  fprintf(stderr, "                       -s 1 == 1x (176 kbytes/sec)\n");
  fprintf(stderr, "                       -s 2 == 2x (353 kbytes/sec)\n");
  fprintf(stderr, "                       -s 3 == 3x (528 kbytes/sec)\n");
  fprintf(stderr, "                       -s 4 == 4x (706 kbytes/sec)\n\n");

  fprintf(stderr, "   -t track_no    :  The track number to extract. (default: 1)\n\n");

  exit(DAEX_EXIT_STATUS);
}


/*========================================================================*/
void
fnRetrieveArguments(int iArgc, char **szArgv, char **szDeviceName, 
                    char **szOutputFilename, int *iTrackNumber, 
                    int *iDriveSpeed)
/*
 * Parse the user arguments, and store them in the appropriate variables.
 *
 *   Input:  iArgc            - Number of arguments passed by the user.
 *           iArgv            - Actual arguments passed the user.
 *           szDeviceName     - Device from which the audio will be read.
 *           szOutputFilename - File to which the audio will be written.
 *           iTrackNumber     - Track number to extract.
 *           iDriveSpeed      - Device read speed indicator.
 *
 * Returns:  szDeviceName, szOutputFilename, iTrackNumber, iDriveSpeed
 */
/*========================================================================*/
{
  extern int  optind;		/* The current argument number - getopt()    */
  extern char *optarg;		/* Current option's arg. string - getopt()   */
  int iArgument;		/* Current argument in getopt()'s arg list   */


#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnRetrieveArguments()\n");
#endif

  /* If there are no arguments, alert the user, display the usage banner, 
   * and exit. 
   */
  if (iArgc == 1) {
    fprintf(stderr, "daex: requires at least 1 argument\n");
    fnUsage(szArgv);
  }

  /* Get the command line arguments */
  while ((iArgument = getopt(iArgc, szArgv, "d:o:s:t:")) != -1) {

    /* Make sure we're dealing with an argument that's at least 
     * somewhat sane.
     */
    if (strlen(optarg) > DAEX_MAX_STRING_LEN)
      fnError(DAEX_EXIT_STATUS, "The argument specified, \"%s\", exceeds the maximum string length (%i characters).", optarg, DAEX_MAX_STRING_LEN);

    switch(iArgument) {
      case 'd':				/* Device name                        */
        if ((*szDeviceName = strdup(optarg)) == NULL)
          fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for the device name.");
        break;

      case 'o':				/* Output filename                    */
        if ((*szOutputFilename = strdup(optarg)) == NULL)
          fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for the output file name.");
        break;

      case 's':				/* Drive speed                        */
        *iDriveSpeed = atoi(optarg);

        /* Don't allow whacked speeds */
        if (*iDriveSpeed < 0)
          fnError(DAEX_EXIT_STATUS, "The speed must be a positive integer greater than or equal to 0.");

        break;

      case 't':				/* Track number                       */
        *iTrackNumber = atoi(optarg);

        /* Don't allow whacked track numbers */
        if (*iTrackNumber <= 0)
          fnError(DAEX_EXIT_STATUS, "The track number must be a positive integer.");

        break;

      case '?':
      default:
        fnUsage(szArgv);		/* Display the usage banner, and exit */
    }
  }

  /* If there were more arguments than acceptable options,
   * display the usage banner, and exit.
   */
  if (iArgc != optind)  fnUsage(szArgv);
}


/*========================================================================*/
void
fnSanitizeArguments(char **szDevice, char **szOutputFile, int *iTrack)
/*
 * Check the specified values, and decide whether or not to use the defaults.
 *
 *   Input:  szDevice     - CD-ROM device name (path).
 *           szOutputFile - File where data will be written.
 *           iTrack       - The track being extracted.
 *
 * Returns:  None.
 */
/*========================================================================*/
{

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnSanitizeArguments()\n");
#endif

  /* If no device name is specified, we'll default to "/dev/wcd0c" */
  if (*szDevice == NULL)
    if ((*szDevice = strdup("/dev/wcd0c")) == NULL)
      fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for the device name.");

  /* If no output filename is specified, we'll default to "output.wav" */
  if (*szOutputFile == NULL)
    if ((*szOutputFile = strdup("output.wav")) == NULL)
      fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for the output file name.");

  /* If the track number wasn't specified, we'll default to track 1 */
  if (*iTrack == 0)  *iTrack = 1;
}


/*========================================================================*/
void
fnSetPrivileges(int iType, int *utSavedUID)
/*
 * Set or relinquish super user privileges based on the type (iType)
 * specified.
 *
 *   Input:  iType      - Indicates the operation to perform: set or 
 *                        relinquish.
 *           utSavedUID - The original UID.  Required to fall back once
 *                        we're done with our extended privileges.
 *
 * Returns:  None.
 */
/*========================================================================*/
{

  uid_t   utCurrentEUID;	/* Current Effective UserID                  */
#ifdef DEBUG
  uid_t   utCurrentUID;		/* Current UserID                            */
#endif

  switch(iType) {
   case DAEX_SET_PRIVILEGE:			/* Gain privileges           */

     /* Save the current UserID and Effective UserID so that we can first
      * establish, and then relinquish our superuser privileges after opening 
      * the CD-ROM device.
      */

     *utSavedUID = getuid();
     utCurrentEUID = geteuid();

#ifdef DEBUG
     fprintf(stderr, "UID (before setuid())       : %ld\n", *utSavedUID);
     fprintf(stderr, "EUID (before setuid())      : %ld\n\n", utCurrentEUID);
#endif

     /* Set our UID to the EUID.  Should be 0 (root).  Exit if we encounter 
      * an error.
      */
     if (setuid(utCurrentEUID) < 0)
       fnError(DAEX_EXIT_STATUS, "Unable to extended privileges - setuid() failed.");

#ifdef DEBUG
     utCurrentUID = getuid();
     utCurrentEUID = geteuid();

     fprintf(stderr, "UID (after setuid())        : %ld\n", utCurrentUID);
     fprintf(stderr, "EUID (after setuid())       : %ld\n\n", utCurrentEUID);
#endif

     break;

   case DAEX_REL_PRIVILEGE:			/* Relinquish privileges     */

     /* Reset our UID to the original (saved) value.  Exit if we encounter
      * an error.
      */
     if (setuid(*utSavedUID) < 0)
       fnError(DAEX_EXIT_STATUS, "Unable to return previous privileges - setuid() failed.");

#ifdef DEBUG
     utCurrentUID = getuid();
     utCurrentEUID = geteuid();

     fprintf(stderr, "UID (after open())          : %ld\n", utCurrentUID);
     fprintf(stderr, "EUID (after open())         : %ld\n\n", utCurrentEUID);
#endif

     break;

   default:
     fnError(DAEX_EXIT_STATUS, "Unknown privilege type.  Hmm.  You shouldn't be seeing this.");
  }
}


/*========================================================================*/
int
fnOpenDevice(char *szDeviceName)
/*
 * Attempt to open the CD-ROM device, and return the file descriptor on
 * success.  Exit upon failure.
 *
 *   Input:  szDeviceName - Pointer to the name of the device.
 * Returns:  The open device descriptor.
 */
/*========================================================================*/
{
  int iFileDesc;				/* Temporary file descriptor */

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnOpenDevice()\n");
#endif

  if ((iFileDesc = open(szDeviceName, O_RDONLY)) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to open CDROM device.");

  return iFileDesc;
}


/*========================================================================*/
void
fnReadTOCheader(int iFileDesc, void *pvTOCheader, int iTrack)
/*
 * Attempt to read the disc's Table Of Contents, and determine if the 
 * the user specified a valid track.  Exit upon failure.
 *
 *   Input:  iFileDesc   - CD-ROM device file descriptor.
 *           pvTOCheader - Pointer to the Table of Contents header.
 *           iTrack      - Track the user wishes to extract.
 *
 * Returns: None.
 */
/*========================================================================*/
{
  struct ioc_toc_header *stTOCheader;		/* Table of Contents header  */

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnReadTOCheader()\n");
#endif

  stTOCheader = (struct ioc_toc_header *)pvTOCheader;

  /* Attempt to read the disc's Table Of Contents.  Exit upon failure. */
  if (ioctl(iFileDesc, CDIOREADTOCHEADER, stTOCheader) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to read TOC header.");

  fprintf(stderr, "Available tracks: %i thru %i.\n\n", stTOCheader->starting_track,
                                                       stTOCheader->ending_track);

  /* If the track number specified is not within the range of tracks on
   * the disc, alert the user, and exit.
   */
  if ((iTrack < stTOCheader->starting_track) ||
      (iTrack > stTOCheader->ending_track))
    fnError(DAEX_EXIT_STATUS, "The track number you specified is not within the proper range.");
}


/*========================================================================*/
void
fnReadTOCentry(int iFileDesc, void *pvTOCheader, int iTrack, int *iLBAstart, 
               int *iLBAend)
/*
 * Attempt to read the TOC entries for each track into the array 
 * "pstTOCentryData".  Since the number of tracks differs from disc to disc,
 * we'll need to dynamically allocate memory for this array.  Once we've read
 * the entries, check to see if the last track is a data track - if so, we're
 * dealing with a CD-EXTRA disc, and need to adjust the last _audio_ track's
 * ending LBA.  See the comments below.
 *
 *   Input:  iFileDesc   - CD-ROM device file descriptor.
 *           pvTOCheader - Pointer to the Table of Contents header.
 *           iTrack      - Track the user wishes to extract.
 *           iLBAstart   - The starting LBA for the current track.
 *           iLBAend     - The ending LBA for the current track.
 *
 * Returns:  iLBAstart, iLBAend
 */
/*========================================================================*/
{
  struct ioc_read_toc_entry stTOCentry;		/* Individual TOC entry      */
  struct ioc_toc_header     *stTOCheader;	/* Table of Contents header  */
  struct cd_toc_entry       *pstTOCentryData;	/* List of TOC entries       */


#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnReadTOCentry()\n");
#endif

  stTOCheader = (struct ioc_toc_header *)pvTOCheader;

  /* Setup the TOC entry structure to return data in the Logical Block
   * Address format.  Include the starting and finishing tracks on return.
   */
  stTOCentry.address_format = CD_LBA_FORMAT;
  stTOCentry.starting_track = stTOCheader->starting_track;
  stTOCentry.data_len       = (stTOCheader->ending_track + 1)
                              * sizeof(struct cd_toc_entry);

  /* Attempt to allocate space for the individual track information.
   * If there's no enough memory available, exit.
   */
  if ((pstTOCentryData = (struct cd_toc_entry *)calloc(1, stTOCentry.data_len)) == NULL)
    fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for TOC entry data.");

  stTOCentry.data = pstTOCentryData;

  /* Attempt to read the individual track information.  Exit upon failure. */
  if (ioctl(iFileDesc, CDIOREADTOCENTRYS, &stTOCentry) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to read TOC entries.");

  /* If the specified track is a data track, alert the user and exit.  */
  if (stTOCentry.data[iTrack - 1].control & CDIO_DATA_TRACK)
    fnError(DAEX_EXIT_STATUS, "The track you specified appears to be a data track.");

  /* Convert the starting and ending LBA's to host-byte order */
  *iLBAstart = ntohl(stTOCentry.data[iTrack- 1].addr.lba);
  *iLBAend   = ntohl(stTOCentry.data[iTrack].addr.lba);

  /* If we're working with a multi-session CD-EXTRA disc, adjust the
   * the ending block address to account for the transition area.  We
   * figure approximately 2.5 minutes for the gap.
   *
   * 11250 sectors = (2.5 min) * (60 sec/min) * (75 sectors/sec)
   */
  if ((iTrack != stTOCheader->ending_track) && 
      (stTOCentry.data[iTrack].control & CDIO_DATA_TRACK)) {
     *iLBAend -= 11250;
  }

  /* free the memory used for the TOC entry array */
  free(pstTOCentryData);
}


/*========================================================================*/
void *
fnSetSpeed(int iFileDesc, int iSpeed)
/*
 * Attempt to set the drive's read speed.
 *
 *   Input:  iFileDesc - Descriptor of the CD-ROM device.
 *           iSpeed    - The speed of the drive (1 == 1X, 2 == 2X, etc...)
 * Returns:  None.
 */
/*========================================================================*/
{
  struct ioc_drive_speed stDriveSpeed;	/* Drive speed structure            */
  char *szDriveSpeed;			/* Current drive speed description  */


#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnSetSpeed()\n");
#endif

  switch (iSpeed) {
    case 0:
      stDriveSpeed.speed = 0xffff;		/* Maximum drive speed      */
      if ((szDriveSpeed = strdup("maximum speed")) == NULL)
        fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 1:
      stDriveSpeed.speed = 176;			/* 1X, or 176 kbytes/sec    */
      if ((szDriveSpeed = strdup("1x (176 kbytes/sec)")) == NULL)
        fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 2:
      stDriveSpeed.speed = 353;			/* 2X, or 353 kbytes/sec    */
      if ((szDriveSpeed = strdup("2x (353 kbytes/sec)")) == NULL)
        fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 3:
      stDriveSpeed.speed = 528;			/* 3X, or 528 kbytes/sec    */
      if ((szDriveSpeed = strdup("3x (528 kbytes/sec)")) == NULL)
        fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 4:
      stDriveSpeed.speed = 706;			/* 4X, or 706 kbytes/sec    */
      if ((szDriveSpeed = strdup("4x (706 kbytes/sec)")) == NULL)
        fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for drive speed string.");
      break;

    default:			/* Don't attempt to set the drive speed.    */
      if ((szDriveSpeed = strdup("default speed")) == NULL)
        fnError(DAEX_EXIT_STATUS, "Unable to allocate sufficient memory for drive speed string.");
      return szDriveSpeed;
  }

#ifdef DEBUG
  fprintf(stderr, "IOCTL   : CDIOSETSPEED\n");
#endif

  /* Attempt to set the drive's read speed.  Exit upon failure. */
  if (ioctl(iFileDesc, CDIOSETSPEED, &stDriveSpeed) < 0)
    fnError(DAEX_EXIT_STATUS, "Unable to set drive speed.");

  return szDriveSpeed;
}


/*========================================================================*/
void 
fnSetupWAVEheader(void *pvWavHeader, u_long lFileLength, 
                  u_long lBytesWritten)
/*
 * Create the Wave header.  If "lFileLength" and "lTotalBytesWritten" are
 * both 0, we'll assume we're just initializing the structure.  Otherwise,
 * we've (hopefully) finished recording the audio, and need to tidy up the
 * header so Wave decoders will actually process the file.
 *
 *   Input:  pvWavHeader   - Pointer to a structure containing the Wave header
 *                           fields.
 *           lFileLength   - The total file length of the output file, 
 *                           including the Wave header.
 *           lBytesWritten - The number of bytes of raw audio written
 *                           to the output file.
 *
 * Returns:  None.
 */
/*========================================================================*/
{
  struct WavFormat_t *pstWavHeader;		/* Wave header               */

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnSetupWAVEheader()\n");
#endif

  pstWavHeader = (struct WavFormat_t *)pvWavHeader;

  if ((lFileLength == 0) && (lBytesWritten == 0)) {
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

    pstWavHeader->lFileLength   = lFileLength - 8;
    pstWavHeader->lSampleLength = lBytesWritten;
  }
}


/*========================================================================*/
void
fnWriteAudioHeader(int iFileDesc, int iType, void *pvHeader, u_long lArg1, 
                   u_long lArg2)
/*
 * Attempt to write an audio header to the output file.  Exit upon error.
 *
 *   Input:  iFileDesc - The output file's file descriptor.
 *           iType     - The type of header being written.
 *           pvHeader  - Pointer to the audio header structure.
 *           lArg1     - Usually the total output file length (including 
 *                       the header)
 *           lArg2     - Usually the total number of data bytes written.
 *
 * Returns:  None.
 */
/*========================================================================*/
{ 
  struct  WavFormat_t  *pstWavHeader;		/* Wave header               */

#ifdef DEBUG
  fprintf(stderr, "\nFUNCTION: fnWriteAudioHeader()\n");
#endif

  switch (iType) {
    case DAEX_HEADER_WAVE:			/* Wave header specified     */
     pstWavHeader = (struct WavFormat_t *) pvHeader;

     /* Initialize the Wave header */
     fnSetupWAVEheader(pstWavHeader, lArg1, lArg2);

     /* If it appears the extraction has been completed (the file length
      * and data bytes written are greater than 0), seek to the beginning of 
      * the output file.
      */

     if ((lArg1 > 0) && (lArg2 > 0))
      if (lseek(iFileDesc, 0, SEEK_SET) < 0)
        fnError(DAEX_EXIT_STATUS, "Unable to seek beginning of output file.");

     /* ... and write the audio header */
     if (write(iFileDesc, pstWavHeader, sizeof(struct WavFormat_t)) != sizeof(struct WavFormat_t))
       fnError(DAEX_EXIT_STATUS, "Unable to write audio header.");

     break;

    default:
      fnError(DAEX_EXIT_STATUS, "Unknown audio type.  Hmm.  You shouldn't be seeing this.");
  }
}


/*========================================================================*/
void
fnExtractAudio(int iFileDesc, char *szFilename, int iLBAstart, int iLBAend)
/*
 * Copy the digital audio from the track specified to the output file
 * specified.  Write headers to the output file if appropriate, and deal with 
 * any errors we may run into.
 *
 *   Input:  iFileDesc  - Descriptor of the CD-ROM device.
 *           szFilename - The file to which the output will be written.
 *           iLBAstart  - The starting LBA for the current track.
 *           iLBAend    - The ending LBA for the current track.
 *
 * Returns:  None.
 */
/*========================================================================*/
{
  struct  ioc_read_cdda	stReadCDDA;  	/* CDDA (raw audio) structure        */
  struct  WavFormat_t	stWavHeader; 	/* Wave header                       */

  char    *szBuffer;		/* Raw CDDA buffer                           */
  char    szTotalBytesWritten[512]; /* String used to display the status     */

  int     iBlocksToExtract,	/* Number of blocks a track will span        */
          iBytesWritten,	/* Number of bytes written on a write()      */
          iCount,		/* Temporary counter                         */
          iErrorRecoveryCount,	/* Counter for the error recovery mechanism  */
          iOutfileDesc;		/* File descriptor for the output file       */

  int     iCurrentBlock = 0;	/* The block number we're current reading    */

#ifdef DEBUG
  int     iPrintedFlag = 0;	/* Used in determining how to print errors   */
#endif

  int     iCurrentPercentComplete,   /* Current % of extractration complete  */
          iLastPercentComplete = -1; /* Last percent complete (marker)       */

  u_long  lTotalFileLength;	/* The total file length, including headers  */

  u_long  lTotalBytesWritten = 0; /* Total bytes written thus far            */


#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnExtractAudio()\n");
#endif

  /* Attempt to allocate memory for the raw audio data that will be read
   * from the disc.
   */
  if ((szBuffer = (char *)calloc(1, CDDA_DATA_LENGTH)) == NULL)
    fnError(DAEX_EXIT_STATUS, "Unable to allocate memory for CDDA buffer.");

  /* Open the output file.  Exit upon failure. */
  if ((iOutfileDesc = open(szFilename, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0) {
    if (errno == EEXIST)
      fnError(DAEX_EXIT_STATUS, "The output file \"%s\" already exists.", szFilename);
     else
      fnError(DAEX_EXIT_STATUS, "Unable to open output file, \"%s\"...", szFilename);
  }

  /* Write the inital header - we don't know the total file length or the
   * number of bytes written, so we specify 0... (see fnWriteAudioHeader).
   */
  fnWriteAudioHeader(iOutfileDesc, DAEX_HEADER_WAVE, &stWavHeader, 0, 0);

  /* Setup the CDDA-read structure. */
  stReadCDDA.frames = 1;              /* Number of 2352 byte blocks to read */
  stReadCDDA.buffer = szBuffer;       /* Copy the buffer pointer            */

  /* Initialize the status variables for use during extraction */
  iBlocksToExtract = (iLBAend - iLBAstart);

  /* Read the individual blocks for the specified track.  Display a status
   * line, and attempt to recover from any errors we run into.
   */

  for (stReadCDDA.lba=iLBAstart; stReadCDDA.lba <= iLBAend; stReadCDDA.lba++) {
    szBuffer[0] = '\0';

    /* Reset the error recovery count for each block read */
    iErrorRecoveryCount = 0;

    /* Read the raw audio from disc.  If the read fails, increment the
     * recovery count, and try again - we'll allow up to 10 failures on the
     * current block before we decide to give up.
     */
    while ( (ioctl(iFileDesc, CDIOREADCDDA, &stReadCDDA) != 0) &&
             (iErrorRecoveryCount < 10) ) {

#ifdef DEBUG
      /* if we're displaying the status, we need to skip a line */
      if ((iErrorRecoveryCount == 0) && (iPrintedFlag == 1)) {
        fprintf(stderr, "\n");
        iPrintedFlag = 0;
      }

      fprintf(stderr, "-> Retrying block address %ld\n", stReadCDDA.lba);
#endif

      iErrorRecoveryCount++;
    }

    /* If the we've exceeded a 10 count on errors, something must be
     * wrong with the disc.  There may be a better way to handle this,
     * so it's possible this method will go away in the future.
     */ 

    if (iErrorRecoveryCount >= 10) {

#ifndef DEBUG
      fprintf(stderr, "\n");
#endif

      fnError(DAEX_EXIT_STATUS, "Too many errors encountered reading track.  Maybe the disc is scratched?  Does your drive support CDDA?");
    }

    /* Write the returned buffer of raw data to disk.  If the file system is
     * full, or if not all 2352 bytes were written to disk, display an error
     * message and exit... otherwise, increment the "iTotalBytesWritten"
     * counter. 
     */

    if ((iBytesWritten = write(iOutfileDesc, szBuffer, CDDA_DATA_LENGTH)) != CDDA_DATA_LENGTH)
      if (errno == ENOSPC)
        fnError(DAEX_EXIT_STATUS, "\nUnable to write output file.  No space left on device.");
      else
        fnError(DAEX_EXIT_STATUS, "\nIncorrect number of bytes written to output file (%i of %i).", iBytesWritten, CDDA_DATA_LENGTH);

    lTotalBytesWritten += iBytesWritten;

    /* Determine how far into the file we are (percentage wise).  We use the:
     * [(x / 100) = (# blocks / total blocks) => percent complete = (x * 100)]
     * formula.
     */
    iCurrentPercentComplete = (int) ((float)iCurrentBlock / 
                                     (float)iBlocksToExtract * 100);

    /* If the percentage indicator has increased since our last status update,
     * update the status line once again.  Allow 50 horizontal characters. 
     */
    if (iCurrentPercentComplete > iLastPercentComplete) {

      /* Update the "last known percent complete" indicator. */
      iLastPercentComplete = iCurrentPercentComplete;

      /* Print 50 backspaces. */
      for (iCount=0; iCount < 50; iCount++) putc(8, stderr);

      /* Display the actual status line. */
      snprintf(szTotalBytesWritten, sizeof(szTotalBytesWritten), "%ld of %ld blocks written (%i%%)... ", iCurrentBlock, iBlocksToExtract, iCurrentPercentComplete);
      fprintf(stderr, "%-50s", szTotalBytesWritten);

#ifdef DEBUG
      /* Set the "status line printed" flag, so that we known to print an
       * extra linefeed if we encounter an error.
       */
      iPrintedFlag = 1;
#endif
    }

    /* Increment the current block count.  Used in determining whether or
     * or not the "percent complete" count should increase.
     */
    iCurrentBlock++;
  }
  /* Dispose of the raw audio buffer */
  free(szBuffer); 

  /* Calculate the total file length written, including the audio
   * header.
   */
  lTotalFileLength = lTotalBytesWritten + sizeof(struct WavFormat_t);

  /* Rewrite the audio header with the now known values of the total file
   * length, and total number of bytes written.
   */
  fnWriteAudioHeader(iOutfileDesc, DAEX_HEADER_WAVE, &stWavHeader, 
                     lTotalFileLength, lTotalBytesWritten);

  /* Close the output file */
  close(iOutfileDesc);

  /* Display the amount of data written to the output file. */
  fprintf(stderr, "\nWrote %ld bytes of data.\n\n", lTotalFileLength); 
}


/*========================================================================*/
void
fnDispose(int iDeviceDesc, char **szDeviceName, char **szDriveSpeed, 
          char **szOutputFilename)
/*
 * Close out the remaining descriptors, and free the memory we've previously
 * allocated.
 *
 *   Input:  iDeviceDesc      - CD-ROM device descriptor.
 *           szDeviceName     - CD-ROM device name (path).
 *           szDriveSpeed     - Drive speed description.
 *           szOutputFilename - The output file name.
 *
 * Returns: None.
 */
/*========================================================================*/
{

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnDispose()\n");
#endif

  /* Close the device descriptor */
  close(iDeviceDesc);

  /* Dispose of the option arguments, and the drive speed description string */
  free(*szDeviceName);
  free(*szDriveSpeed);
  free(*szOutputFilename);
}


int
main(int argc, char **argv)
{
  struct  ioc_toc_header     stTOCheader;	/* Table of Contents header  */

  char    *szDriveSpeed;	/* Current drive speed description           */
  char    *szDeviceName = NULL,		/* Input device name                 */
          *szOutputFilename = NULL;	/* Output file name                  */

  int     iDeviceDesc,		/* File descriptor for the input device      */
          iLBAend,		/* Ending Logical Block Address for a track  */
          iLBAstart;		/* Starting LBA for a track                  */

  int     iDriveSpeed = -1,	/* CD-ROM read speed (1 == 1x, 2 == 2x, etc) */
          iTrackNumber = 0;	/* The current track number being extracted  */

  uid_t   utSavedUID;		/* Saved UserID for the current process      */


  fprintf(stderr, "DAEX v%s - The Digital Audio EXtractor.\n", kVersion);
  fprintf(stderr, "(c) Copyright 1998 Robert Mooney, All rights reserved.\n\n");

  /* Parse the user arguments and store in the appropriate variables. */
  fnRetrieveArguments(argc, argv, &szDeviceName, &szOutputFilename, 
                      &iTrackNumber, &iDriveSpeed);

#ifdef DEBUG
  fprintf(stderr, "Device        (user) : %s\n", szDeviceName);
  fprintf(stderr, "Outfile       (user) : %s\n", szOutputFilename);
  fprintf(stderr, "Track number  (user) : %i\n\n", iTrackNumber);
#endif

  /* Setup the defaults if we're missing information */
  fnSanitizeArguments(&szDeviceName, &szOutputFilename, &iTrackNumber);

#ifdef DEBUG
  fprintf(stderr, "Device       (fixed) : %s\n", szDeviceName);
  fprintf(stderr, "Outfile      (fixed) : %s\n", szOutputFilename);
  fprintf(stderr, "Track number (fixed) : %i\n\n", iTrackNumber);
#endif

  /* Set superuser privileges */
  fnSetPrivileges(DAEX_SET_PRIVILEGE, &utSavedUID);

  /* Open the CD-ROM device */
  iDeviceDesc = fnOpenDevice(szDeviceName);

  /* Relinquish superuser privileges */
  fnSetPrivileges(DAEX_REL_PRIVILEGE, &utSavedUID);

  /* Read the Table of Contents header.  We need the track number to determine
   * the actual existance of the track.
   */
  fnReadTOCheader(iDeviceDesc, &stTOCheader, iTrackNumber);

  /* Read the TOC entries, and grab the block information for the track. */
  fnReadTOCentry(iDeviceDesc, &stTOCheader, iTrackNumber, &iLBAstart, &iLBAend);

#ifdef DEBUG
  fprintf(stderr, "Start block : %i\n", iLBAstart);
  fprintf(stderr, "End block   : %i\n\n", iLBAend);
#endif

  /* Set the drive speed, and retrieve a string describing the current 
   * setting. 
   */
  szDriveSpeed = (char *)fnSetSpeed(iDeviceDesc, iDriveSpeed);

  fprintf(stderr, "Extracting track #%i to \"%s\" at %s...\n", iTrackNumber, szOutputFilename, szDriveSpeed);

  /* Copy the audio to disk. */
  fnExtractAudio(iDeviceDesc, szOutputFilename, iLBAstart, iLBAend);

  /* Close the CD-ROM device, and free up the previously allocated memory. */
  fnDispose(iDeviceDesc, &szDeviceName, &szDriveSpeed, &szOutputFilename);

  /* We're done. Yeah! */
  fprintf(stderr, "DAEX finished.\n");
}
