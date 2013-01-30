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
 * $Id: daex.c,v 0.19 1998/10/13 20:44:57 rmooney Exp $
 */

#include "daex.h"
#include "format.h"
#include "cddb.h"


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

  if ((pBuffer = calloc(1, kiMaxStringLength)) == NULL) {
    fprintf(stderr, "Unable to allocate sufficient memory for argument buffer.\n");
    exit(2);
  }

  va_start(stArgumentList, szError);
  vsnprintf(pBuffer, kiMaxStringLength, szError, stArgumentList);
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

  fprintf(stderr, "usage: daex [-c hostname:port] [-d device] [-i filename] [-o outfile]\n");
  fprintf(stderr, "            [-s drive_speed] [-t track_no] [-y]\n\n");

  fprintf(stderr, "   -c hostname:port :  Enable CD Disc Database (CDDB) querying.\n");
  fprintf(stderr, "   -d device        :  ATAPI CD-ROM device. (default: /dev/wcd0c)\n");
  fprintf(stderr, "   -i filename      :  Dump CDDB information to the specified\n");
  fprintf(stderr, "                       filename. (requires the -c option)\n\n");

  fprintf(stderr, "   -o outfile       :  The name of the recorded track. (default: track-NN.wav\n");
  fprintf(stderr, "                       where 'NN' is the specified track number)\n\n");

  fprintf(stderr, "   -s drive_speed   :  The speed at which the CD audio will be read.\n");
  fprintf(stderr, "                       (default: don't attempt to set drive speed)\n\n");

  fprintf(stderr, "                       -s 0  == Maximum allowable\n");
  fprintf(stderr, "                       -s 1  == 1x (176 kbytes/sec)\n");
  fprintf(stderr, "                       -s 2  == 2x (353 kbytes/sec)\n");
  fprintf(stderr, "                       -s 3  == 3x (528 kbytes/sec)\n");
  fprintf(stderr, "                       -s 4  == 4x (706 kbytes/sec)\n");
  fprintf(stderr, "                       -s 8  == 8x (1.4 Mbytes/sec)\n");
  fprintf(stderr, "                       -s 16 == 16x (2.8 Mbytes/sec)\n\n");

  fprintf(stderr, "   -t track_no      :  The track number to extract. A value of 0\n");
  fprintf(stderr, "                       indicates we should copy every track.\n\n");

  fprintf(stderr, "   -y               :  Skip tracks with problems (instead of exiting) when\n");
  fprintf(stderr, "                       extracting more than one track.\n\n");

  exit(kiExitStatus_General);
}


/*========================================================================*/
void
fnRetrieveArguments(int iArgc, char **szArgv, char **szDeviceName, 
                    char **szOutputFilename, int *iTrackNumber, 
                    int *iDriveSpeed, int *iCDDBquerying, char **szCDDB_RemoteHost,
                    int *iCDDB_RemotePort, int *iSkipTracksWithErrors, char **szInfoFilename)
/*
 * Parse the user arguments, and store them in the appropriate variables.
 *
 *   Input:  iArgc                 - Number of arguments passed by the user.
 *           iArgv                 - Actual arguments passed the user.
 *           szDeviceName          - Device from which the audio will be read.
 *           szOutputFilename      - File to which the audio will be written.
 *           iTrackNumber          - Track number to extract.
 *           iDriveSpeed           - Device read speed indicator.
 *           iCDDBquerying         - CDDB querying flag.
 *           szCDDB_RemoteHost     - Remote hostname used in CDDB queries.
 *           iCDDB_RemotePort      - Remote port number used in CDDB queries.
 *           iSkipTracksWithErrors - Skip tracks with errors when extracting more
 *                                   than one track (flag).
 *           szInfoFilename        - Disc information output filename.
 *
 * Returns:  szDeviceName, szOutputFilename, iTrackNumber, iDriveSpeed, iCDDBquerying
 *           szCDDB_RemoteHost, iCDDB_RemotePort, iSkipTracksWithErrors
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
  while ((iArgument = getopt(iArgc, szArgv, "c:d:i:o:s:t:y")) != -1) {

#ifdef DEBUG
  fprintf(stderr, "DEBUG   : Argument value:  \"%c\" (%i)\n", iArgument, iArgument);
#endif

    /* Make sure we're dealing with an argument that's at least 
     * somewhat sane.
     */
    if ((optarg != NULL) && (strlen(optarg) > kiMaxStringLength))
      fnError(kiExitStatus_General, "The argument specified, \"%s\", exceeds the maximum string length (%i characters).", optarg, kiMaxStringLength);

    switch(iArgument) {
      case 'c':                         /* CDDB querying                      */
        *iCDDBquerying = 1;

        /* Store the remote hostname. */
        *szCDDB_RemoteHost = strdup(strsep(&optarg, ":"));

        /* If the separating character ":" does not exist, notify the user,
         * display the usage banner, and exit.
	 */
        if (!optarg) {
          fprintf(stderr, "daex: -c option requires an argument in the form \"hostname:port\"\n");
          fnUsage(szArgv);
        }

        /* Store the remote port. */
        *iCDDB_RemotePort = atoi(optarg);

        /* Don't allow wacked port numbers. */
        if ((*iCDDB_RemotePort < 1) || (*iCDDB_RemotePort > 65535))
          fnError(kiExitStatus_General, "The port number must be a positive integer between 1 and 65535.");

        break;

      case 'd':				/* Device name                        */
        if ((*szDeviceName = strdup(optarg)) == NULL)
          fnError(kiExitStatus_General, "Unable to allocate sufficient memory for the device name.");
        break;

      case 'i':
        if ((*szInfoFilename = strdup(optarg)) == NULL)
          fnError(kiExitStatus_General, "Unable to allocate sufficient memory for the info filename.");
        break;

      case 'o':				/* Output filename                    */
        if (strlen(optarg) > MAX_FILENAME_LENGTH)
          fnError(kiExitStatus_General, "The output filename specified exceeds the maximum allowable length (%i characters).\n", MAX_FILENAME_LENGTH);

        if ((*szOutputFilename = strdup(optarg)) == NULL)
          fnError(kiExitStatus_General, "Unable to allocate sufficient memory for the output file name.");
        break;

      case 's':				/* Drive speed                        */
        *iDriveSpeed = atoi(optarg);

        /* Don't allow wacked speeds */
        if (*iDriveSpeed < 0)
          fnError(kiExitStatus_General, "The speed must be a positive integer greater than or equal to 0.");

        break;

      case 't':				/* Track number                       */
        *iTrackNumber = atoi(optarg);

        /* Don't allow wacked track numbers */
        if (*iTrackNumber < 0)
          fnError(kiExitStatus_General, "The track number must be a positive integer greater than or equal to 0.");

        break;

    case 'y':                           /* Skip tracks with errors            */
        *iSkipTracksWithErrors = 1;
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
fnSanitizeArguments(char **szDevice)
/*
 * Check the specified values, and decide whether or not to use the defaults.
 *
 *   Input:  szDevice     - CD-ROM device name (path).
 * Returns:  None.
 */
/*========================================================================*/
{

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnSanitizeArguments()\n");
#endif

  /* If no device name is specified, we'll default to "/dev/wcd0c". */
  if (*szDevice == NULL)
    if ((*szDevice = strdup("/dev/wcd0c")) == NULL)
      fnError(kiExitStatus_General, "Unable to allocate sufficient memory for the device name.");
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
   case kiSetPrivilege:				/* Gain privileges           */

     /* Save the current UserID and Effective UserID so that we can first
      * establish, and then relinquish our superuser privileges after opening 
      * the CD-ROM device.
      */

     *utSavedUID = getuid();
     utCurrentEUID = geteuid();

#ifdef DEBUG
     fprintf(stderr, "UID (before setuid())       : %d\n", *utSavedUID);
     fprintf(stderr, "EUID (before setuid())      : %d\n\n", utCurrentEUID);
#endif

     /* Set our UID to the EUID.  Should be 0 (root).  Exit if we encounter 
      * an error.
      */
     if (setuid(utCurrentEUID) < 0)
       fnError(kiExitStatus_General, "Unable to extended privileges - setuid() failed.");

#ifdef DEBUG
     utCurrentUID = getuid();
     utCurrentEUID = geteuid();

     fprintf(stderr, "UID (after setuid())        : %d\n", utCurrentUID);
     fprintf(stderr, "EUID (after setuid())       : %d\n\n", utCurrentEUID);
#endif

     break;

   case kiRelPrivilege:				/* Relinquish privileges     */

     /* Reset our UID to the original (saved) value.  Exit if we encounter
      * an error.
      */
     if (setuid(*utSavedUID) < 0)
       fnError(kiExitStatus_General, "Unable to return previous privileges - setuid() failed.");

#ifdef DEBUG
     utCurrentUID = getuid();
     utCurrentEUID = geteuid();

     fprintf(stderr, "UID (after open())          : %d\n", utCurrentUID);
     fprintf(stderr, "EUID (after open())         : %d\n\n", utCurrentEUID);
#endif

     break;

   default:
     fnError(kiExitStatus_General, "Unknown privilege type.  Hmm.  You shouldn't be seeing this.");
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
    fnError(kiExitStatus_General, "Unable to open CDROM device.");

  return iFileDesc;
}


/*========================================================================*/
void *
fnReadTOCheader(int iFileDesc)
/*
 * Attempt to read the disc's Table Of Contents. Exit upon failure.
 *
 *   Input:  iFileDesc   - CD-ROM device file descriptor.
 * Returns:  A pointer to "struct ioc_toc_header".  (TOC header)
 */
/*========================================================================*/
{
  struct ioc_toc_header *pstTOCheader;		/* Table of Contents header  */

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnReadTOCheader()\n");
#endif

  pstTOCheader = (struct ioc_toc_header *) calloc(1, sizeof(struct ioc_toc_header));

  /* Attempt to read the disc's Table Of Contents.  Exit upon failure. */
  if (ioctl(iFileDesc, CDIOREADTOCHEADER, pstTOCheader) < 0)
    fnError(kiExitStatus_General, "Unable to read TOC header.");

  fprintf(stderr, "Available tracks: %i thru %i.\n\n", pstTOCheader->starting_track,
                                                       pstTOCheader->ending_track);

  return pstTOCheader;
}


/*========================================================================*/
void *
fnReadTOCentries(int iFileDesc, void *pvTOCheader)
/*
 * Attempt to read the TOC entries for each track into an array.
 *
 *   Input:  iFileDesc   - CD-ROM device file descriptor.
 *           pvTOCheader - Pointer to the Table of Contents header.
 *
 * Returns:  Pointer to "struct ioc_read_toc_entry".  (TOC entries)
 */
/*========================================================================*/
{
  struct ioc_toc_header     *pstTOCheader;	/* Table of Contents header  */
  struct ioc_read_toc_entry *pstTOCentries;     /* Entries' Header           */
  int i;


#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnReadTOCentries()\n");
#endif

  pstTOCheader  = (struct ioc_toc_header *) pvTOCheader;
  pstTOCentries = (struct ioc_read_toc_entry *) calloc(1, sizeof(struct ioc_read_toc_entry));

  /* Setup the TOC entry structure to return data in the Logical Block
   * Address format.  Include the starting and finishing tracks on return.
   */
  pstTOCentries->address_format = CD_MSF_FORMAT;
  pstTOCentries->starting_track = pstTOCheader->starting_track;
  pstTOCentries->data_len       = (pstTOCheader->ending_track + 1)
                                  * sizeof(struct cd_toc_entry);

  /* Attempt to allocate space for the individual track information.
   * If there's no enough memory available, exit.
   */
  pstTOCentries->data = (struct cd_toc_entry *) calloc(1, pstTOCentries->data_len);

  if (!pstTOCentries->data)
   fnError(kiExitStatus_General, "Unable to allocate sufficient memory for TOC entry data.");

  /* Attempt to read the individual track information.  Exit upon failure. */
  if (ioctl(iFileDesc, CDIOREADTOCENTRYS, pstTOCentries) < 0)
    fnError(kiExitStatus_General, "Unable to read TOC entries.");

  return pstTOCentries;
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
      if ((szDriveSpeed = strdup(kszSpeedMaximum)) == NULL)
        fnError(kiExitStatus_General, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 1:
      stDriveSpeed.speed = 176;			/* 1X, or 176 kbytes/sec    */
      if ((szDriveSpeed = strdup(kszSpeed1X)) == NULL)
        fnError(kiExitStatus_General, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 2:
      stDriveSpeed.speed = 353;			/* 2X, or 353 kbytes/sec    */
      if ((szDriveSpeed = strdup(kszSpeed2X)) == NULL)
        fnError(kiExitStatus_General, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 3:
      stDriveSpeed.speed = 528;			/* 3X, or 528 kbytes/sec    */
      if ((szDriveSpeed = strdup(kszSpeed3X)) == NULL)
        fnError(kiExitStatus_General, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 4:
      stDriveSpeed.speed = 706;			/* 4X, or 706 kbytes/sec    */
      if ((szDriveSpeed = strdup(kszSpeed4X)) == NULL)
        fnError(kiExitStatus_General, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 8:
      stDriveSpeed.speed = 1434;		/* 8X, or 1.4 Mbytes/sec    */
      if ((szDriveSpeed = strdup(kszSpeed8X)) == NULL)
        fnError(kiExitStatus_General, "Unable to allocate sufficient memory for drive speed string.");
      break;

    case 16:
      stDriveSpeed.speed = 2867;		/* 16X, or 2.8 Mbytes/sec    */
      if ((szDriveSpeed = strdup(kszSpeed16X)) == NULL)
        fnError(kiExitStatus_General, "Unable to allocate sufficient memory for drive speed string.");
      break;

    default:			 /* Don't attempt to set the drive speed.    */
      if ((szDriveSpeed = strdup(kszSpeedDefault)) == NULL)
        fnError(kiExitStatus_General, "Unable to allocate sufficient memory for drive speed string.");

      return szDriveSpeed;
  }

#ifdef DEBUG
  fprintf(stderr, "IOCTL   : CDIOSETSPEED\n");
#endif

  /* Attempt to set the drive's read speed.  Exit upon failure. */
  if (ioctl(iFileDesc, CDIOSETSPEED, &stDriveSpeed) < 0)
    fnError(kiExitStatus_General, "Unable to set drive speed.");

  return szDriveSpeed;
}


/*========================================================================*/
void 
fnSetupWAVEheader(void *pvWavHeader, u_long lFileLength, u_long lBytesWritten)
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

  pstWavHeader = (struct WavFormat_t *) pvWavHeader;

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
    case kiHeaderWAVE:			/* Wave header specified     */
     pstWavHeader = (struct WavFormat_t *) pvHeader;

     /* Initialize the Wave header */
     fnSetupWAVEheader(pstWavHeader, lArg1, lArg2);

     /* If it appears the extraction has been completed (the file length
      * and data bytes written are greater than 0), seek to the beginning of 
      * the output file.
      */

     if ((lArg1 > 0) && (lArg2 > 0))
      if (lseek(iFileDesc, 0, SEEK_SET) < 0)
        fnError(kiExitStatus_General, "Unable to seek beginning of output file.");

     /* ... and write the audio header */
     if (write(iFileDesc, pstWavHeader, sizeof(struct WavFormat_t)) != sizeof(struct WavFormat_t))
       fnError(kiExitStatus_General, "Unable to write audio header.");

     break;

    default:
      fnError(kiExitStatus_General, "Unknown audio type.  Hmm.  You shouldn't be seeing this.");
  }
}


/*========================================================================*/
int
fnExtractAudio(int iDeviceDesc, int iOutfileDesc, int iLBAstart, int iLBAend)
/*
 * Copy the digital audio from the track specified to the output file
 * specified.  Write headers to the output file if appropriate, and deal with 
 * any errors we may run into.
 *
 *   Input:  iDeviceDesc  - Descriptor of the CD-ROM device.
 *           iOutfileDesc - File descriptor for the output file.
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
          iErrorRecoveryCount;	/* Counter for the error recovery mechanism  */

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
  if ((szBuffer = (char *) calloc(1, CDDA_DATA_LENGTH)) == NULL)
    fnError(kiExitStatus_General, "DAEX: Unable to allocate sufficient memory for CDDA buffer.");

  /* Write the inital header - we don't know the total file length or the
   * number of bytes written, so we specify 0... (see fnWriteAudioHeader).
   */
  fnWriteAudioHeader(iOutfileDesc, kiHeaderWAVE, &stWavHeader, 0, 0);

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
    while ( (ioctl(iDeviceDesc, CDIOREADCDDA, &stReadCDDA) != 0) &&
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

      fprintf(stderr, "DAEX: Too many errors encountered reading track.\n");
      return -2;
    }

    /* Write the returned buffer of raw data to disk.  If the file system is
     * full, or if not all 2352 bytes were written to disk, display an error
     * message and exit... otherwise, increment the "iTotalBytesWritten"
     * counter. 
     */

    if ((iBytesWritten = write(iOutfileDesc, szBuffer, CDDA_DATA_LENGTH)) != CDDA_DATA_LENGTH) {
      if (errno == ENOSPC)
        fnError(kiExitStatus_General, "\nUnable to write output file.  No space left on device.");
      else
        fnError(kiExitStatus_General, "\nIncorrect number of bytes written to output file (%i of %i).", iBytesWritten, CDDA_DATA_LENGTH);
    }

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

      /* Print 70 backspaces. */
      for (iCount=0; iCount < 70; iCount++) putc(8, stderr);

      /* Display the actual status line. */
      snprintf(szTotalBytesWritten, sizeof(szTotalBytesWritten),
               "Statistics ...... [ %d of %d blocks written (%i%%) ]", iCurrentBlock,
               iBlocksToExtract, iCurrentPercentComplete);

      fprintf(stderr, "%-70s", szTotalBytesWritten);

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
  fnWriteAudioHeader(iOutfileDesc, kiHeaderWAVE, &stWavHeader, 
                     lTotalFileLength, lTotalBytesWritten);

  /* Close the output file */
  close(iOutfileDesc);

  /* Display the amount of data written to the output file. */
  fprintf(stderr, "\nFile Size ....... [ %ld bytes (%ld kbytes) ]\n\n",
          lTotalFileLength, lTotalFileLength / 1024);

  return 0;
}


/*========================================================================*/
int
fnProcessTrack(int iDeviceDesc, void *pvDiscInformation, int iTrackNumber)
/*
 * Validate and extract the specified track.  If the track is not within
 * the specified range, or is a data track, return an error.  Handle
 * duplicate filenames, and if all is well, extract the audio.
 *
 *   Input:  iDeviceDesc       - The CD-ROM device descriptor.
 *           pvDiscInformation - Disc information struct.
 *           iTrackNumber      - The track to extract.
 *
 * Returns:   0 - No error.
 *           -1 - Unrecoverable error.  DAEX should quit.
 *           -2 - Recoverable error.  DAEX should continue with the next track.
 */
/*========================================================================*/
{
  struct DiscInformation_t *pstDiscInformation;  /* Disc information structure            */
  struct ioc_toc_header *pstTOCheader;           /* Table of Contents header              */
  struct ioc_read_toc_entry *pstTOCentries;      /* Entries' Header                       */
  char   szTrackFilename_temp[MAX_CDDB_LINE_LENGTH]; /* Temporary filename used for dupes */
  char   *szTrackFilename_ptr;                   /* Pointer to temp filename -- for dupes */
  static int iFilenameDupeCount;                 /* Current duplicate filename count      */
  int iOutfileDesc;                              /* Audio output file (audio) descriptor  */
  int iReturnValue = 0;                          /* Return value for this function.       */


#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnProcessTrack()\n");
#endif

  pstDiscInformation = (struct DiscInformation_t *) pvDiscInformation;
  pstTOCheader  = (struct ioc_toc_header *) pstDiscInformation->pstTOCheader;
  pstTOCentries = (struct ioc_read_toc_entry *) pstDiscInformation->pstTOCentries;

  iFilenameDupeCount = 0;

  /* If the track number specified is not within the range of tracks on
   * the disc, we return an error.
   */
  if ((iTrackNumber < pstTOCheader->starting_track) ||
      (iTrackNumber > pstTOCheader->ending_track)) {
    fprintf(stderr, "DAEX: The track number you specified is not within the proper range.\n");
    return -1;
  }

  /* If the specified track is a data track, alert the user and exit. */
  if (pstTOCentries->data[iTrackNumber - 1].control & CDIO_DATA_TRACK) {
    fprintf(stderr, "DAEX: The track you specified appears to be a data track.\n");
    return -2;
  }

#ifdef DEBUG
  fprintf(stderr, "Start block : %i\n", 
          pstDiscInformation->pstTrackData[iTrackNumber - 1].iFixedLBA_start);
  fprintf(stderr, "End block   : %i\n\n", 
          pstDiscInformation->pstTrackData[iTrackNumber - 1].iFixedLBA_end);
#endif

  extract_audio:

  /* Open the output file.  Exit upon failure. */
  if ((iOutfileDesc = open(pstDiscInformation->pstTrackData[iTrackNumber - 1].szTrackFilename, 
                           O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0) {

    /* If the output file exists, determine an alternate filename. */
    if (errno == EEXIST) {

      /* If there are more than 10 dupes, exit. */
      if (iFilenameDupeCount >= 10) {
        fprintf(stderr, "DAEX: Too many attempts at finding a new name for track #%i.\n",
                iTrackNumber);
        return -2;
      }

      /* If our attempt(s) at finding a new filename was unsuccessful, determine the
       * location of the period at the end of the filename (the dupe extention).
       */
      if (iFilenameDupeCount > 0) {
        szTrackFilename_ptr =
          strrchr(pstDiscInformation->pstTrackData[iTrackNumber - 1].szTrackFilename, '.');

        /* If we're unable to locate the dupe extention (period -- '.'), exit. */
        if (!szTrackFilename_ptr) {
          fprintf(stderr, "DAEX: Unable to locate the dupe extention in the track filename.\n");
          return -1;
        }

        /* Reset the track filename pointer. */
        *szTrackFilename_ptr = 0;
      }

      /* Increase the dupe count, and create the alternate filename. */
      snprintf(szTrackFilename_temp, sizeof(szTrackFilename_temp), "%s.%i",
               pstDiscInformation->pstTrackData[iTrackNumber - 1].szTrackFilename,
               ++iFilenameDupeCount);

      /* Free the previous filename. */
      free(pstDiscInformation->pstTrackData[iTrackNumber - 1].szTrackFilename);

      /* ... and store the alternate. */
      if (! (pstDiscInformation->pstTrackData[iTrackNumber - 1].szTrackFilename =
	     strdup(szTrackFilename_temp))) {
        fprintf(stderr, "DAEX: Unable to allocate sufficient memory for track #%i's filename.\n",
                iTrackNumber);
        return -1;
      }

      /* Give the extraction another shot. */
      goto extract_audio;

    } else {
      /* Some other error... */
      fprintf(stderr, "DAEX: Unable to open output file, \"%s\".\n",
              pstDiscInformation->pstTrackData[iTrackNumber - 1].szTrackFilename);
      return -2;
    }
  }

  /* Display the first part of the status. */
  fprintf(stderr, "Current Track ... [ %i ]\n", iTrackNumber);
  fprintf(stderr, "Filename ........ [ %s ]\n",
          pstDiscInformation->pstTrackData[iTrackNumber - 1].szTrackFilename);
  fprintf(stderr, "Drive Speed ..... [ %s ]\n", pstDiscInformation->szDriveSpeed);

  /* Copy the audio to disk. */
  iReturnValue = fnExtractAudio(iDeviceDesc, iOutfileDesc,
                  pstDiscInformation->pstTrackData[iTrackNumber - 1].iFixedLBA_start,
		  pstDiscInformation->pstTrackData[iTrackNumber - 1].iFixedLBA_end);

  /* Close the outfile descriptor. */
  close(iOutfileDesc);
  return iReturnValue;
}


/*========================================================================*/
void *
fnDiscInformation(int iDeviceDesc, int iDriveSpeed, int iTrackNumber,
                  int iCDDBquerying, int iInfoRequest, char *szCDDB_RemoteHost,
                  int iCDDB_RemotePort, char *szOutputFilename)
/*
 * Fill the disc information structure. 
 *
 *   Input:  iDeviceDesc       - CD-ROM device descriptor.
 *           iDriveSpeed       - Drive read speed.
 *           iTrackNumber      - Track number user wishes to extract.
 *           iCDDBquerying     - CDDB querying flag (1 == CDDB queries, 0 == no CDDB)
 *           iInfoRequest      - Info file flag (1 == create info file, 0 == don't bother)
 *           szCDDB_RemoteHost - Remote CDDB server's hostname or IP.
 *           iCDDB_RemotePort  - Remote port the CDDB server is listening on.
 *           szOutputFilename  - Filename for the user specified track.
 *
 * Returns:  The disc information structure. (DiscInformation_t)
 */
/*========================================================================*/
{
  struct  ioc_toc_header     *pstTOCheader;        /* Table of Contents header */
  struct  ioc_read_toc_entry *pstTOCentries;       /* Track entries' header    */

  struct  DiscInformation_t  *pstDiscInformation;  /* Commonly used disc info  */
  struct  TrackInformation_t *pstTrackInformation; /* Commonly used track info */

  char    *szDriveSpeed;	                   /* Drive speed description  */
  char    szFilename_temp[kiMaxStringLength];      /* Temporary filename       */

  int     iTrackIndex;                             /* Current track counter    */


#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnDiscInformation()\n");
#endif

  /* Set the drive speed, and retrieve a string describing the current 
   * setting.
   */
  szDriveSpeed = (char *) fnSetSpeed(iDeviceDesc, iDriveSpeed);

  /* Read the Table of Contents header and retrieve starting and ending track numbers
   * so that we may determine the actual existance of the track specified by the user.
   */
  pstTOCheader = (struct ioc_toc_header *) fnReadTOCheader(iDeviceDesc);

  /* Read the TOC entries, and grab the block information for the track. */
  pstTOCentries = (struct ioc_read_toc_entry *) fnReadTOCentries(iDeviceDesc, pstTOCheader);

  /* Allocate memory for the disc information structure. */
  pstDiscInformation = (struct DiscInformation_t *)
                       calloc(1, sizeof(struct DiscInformation_t));

  if (!pstDiscInformation) {
    fprintf(stderr, "Unable to allocate sufficient memory for the disc information structure.\n");
    return NULL;
  }

  /* Allocate memory for the track information array. */
  pstTrackInformation = (struct TrackInformation_t *)
                        calloc(pstTOCheader->ending_track, sizeof(struct TrackInformation_t));

  if (!pstTrackInformation) {
    fprintf(stderr,"Unable to allocate sufficient memory for the track information structure.\n");
    return NULL;
  }

  /* Fill the disc information structure. */
  pstDiscInformation->pstTOCheader  = pstTOCheader;
  pstDiscInformation->pstTOCentries = pstTOCentries;
  pstDiscInformation->szDriveSpeed  = strdup(szDriveSpeed);

  if (!pstDiscInformation->szDriveSpeed) {
    fprintf(stderr, "DAEX: Unable to allocated sufficient memory for the drive speed string.\n");
    return NULL;
  }

  if (iCDDBquerying) {
    /* Allocate memory for the CDDB information structure. */
    pstDiscInformation->pstCDDBinformation = (struct CDDBinformation_t *) 
                                             calloc(1, sizeof(struct CDDBinformation_t));

    if (!pstDiscInformation->pstCDDBinformation) {
      fprintf(stderr, "DAEX: Unable to allocate sufficient memory for the CDDB info structure.\n");
      return NULL;
    }

    /* Store the CDDB query string, along with the total playing seconds, and each track's
     * play time, and frame offsets.
     */
    if (fnCDDB_BuildQueryString(pstDiscInformation) < 0)  return NULL;

#ifdef DEBUG
      fprintf(stderr, "DAEX: CDDB Query String: %s\n", pstDiscInformation->pstCDDBinformation->szCDDBquery);
#endif

  } else
    pstDiscInformation->pstCDDBinformation = NULL;

  pstDiscInformation->pstTrackData = pstTrackInformation;

  /* Fill the track information array. */
  for(iTrackIndex=pstTOCheader->starting_track; iTrackIndex <= pstTOCheader->ending_track;
      iTrackIndex++) {

    pstTrackInformation[iTrackIndex - 1].iTrackNumber = iTrackIndex;

    /* If we querying the CDDB server, and the user asked for an info file, or if we're 
     * not querying a CDDB server, generate the generic filename.
     *
     * We don't store the CDDB enhanced filenames when creating an info file, since we 
     * assume the user will be doing this themselves.
     */
    if ((iCDDBquerying && iInfoRequest) || (!iCDDBquerying)) { 

      /* Store the current track's generic filename.  If the filename will exceed the POSIX
       * limit, alert the user and exit.
       */
      if (snprintf(szFilename_temp, kiMaxStringLength, "track-%02d.wav", iTrackIndex) >
          MAX_FILENAME_LENGTH) {
        fprintf(stderr, "DAEX: Maximum filename length exceeded.\n");
        return NULL;
      }

      /* Allocate memory for the track's filename. Exit on error. */
      if (! (pstTrackInformation[iTrackIndex - 1].szTrackFilename = (char *)
             calloc(1, MAX_FILENAME_LENGTH + 1))) {
        fprintf(stderr, "Unable to allocate sufficient memory for the track filename.\n");
        return NULL;
      }

      /* Copy the filename from the temporary variable the memory allocated above. */
      memcpy(pstTrackInformation[iTrackIndex - 1].szTrackFilename, szFilename_temp,
             strlen(szFilename_temp));
    }
 
    /* Subtract 150 frames from the track's starting and ending Logical Block Addresses 
     * to account for the 2 second pre-gap (75 frames/sec).
     */
    pstTrackInformation[iTrackIndex - 1].iFixedLBA_start = 
           (((pstTOCentries->data[iTrackIndex - 1].addr.msf.minute * 60) +
             pstTOCentries->data[iTrackIndex - 1].addr.msf.second) * 75) +
            pstTOCentries->data[iTrackIndex - 1].addr.msf.frame - 150;

    pstTrackInformation[iTrackIndex - 1].iFixedLBA_end = 
           (((pstTOCentries->data[iTrackIndex].addr.msf.minute * 60) +
             pstTOCentries->data[iTrackIndex].addr.msf.second) * 75) +
            pstTOCentries->data[iTrackIndex].addr.msf.frame - 150;

    /* If we're working with a multi-session CD-EXTRA disc, adjust the
     * the ending block address to account for the transition area.  We
     * figure approximately 2.5 minutes for the gap.
     *
     * 11250 sectors = (2.5 min) * (60 sec/min) * (75 sectors/sec)
     */
    if ((iTrackIndex != pstTOCheader->ending_track) && 
        (pstTOCentries->data[iTrackIndex].control & CDIO_DATA_TRACK)) {
       pstTrackInformation[iTrackIndex - 1].iFixedLBA_end -= 11250;
    }
  }

  if (iCDDBquerying) {
#ifdef DEBUG
    fprintf(stderr, "DAEX: Attempting to query the CDDB server.\n");
#endif
    if (fnCDDB_DoCDDBQuery(pstDiscInformation, szCDDB_RemoteHost, iCDDB_RemotePort,
        iInfoRequest) < 0) {
      /* Handle errors... */
      return NULL;
    }
  }

 /* If we're going to be extracting the entire disc (ie, the track specified is 0), 
  * and an output filename was specified, we disregard the output filename so that
  * can use the filenames stored above.
  *
  * If we're going to be extracting a single track (ie, the track specified is not 0),
  * and the output filename was specified, use the output filename in place of the
  * filename stored above for the track specified by the user (or assigned by default).
  */

  if (iTrackNumber == 0) {
    if (szOutputFilename)  free(szOutputFilename);
  } else
      if (szOutputFilename) {
        free(pstTrackInformation[iTrackNumber - 1].szTrackFilename);
        pstTrackInformation[iTrackNumber - 1].szTrackFilename = szOutputFilename;
      }

  return pstDiscInformation;
}


/*========================================================================*/
void
fnDispose(int iDeviceDesc, char **szDeviceName, void **pvDiscInformation)
/*
 * Close out the remaining descriptor, and free the memory we've previously
 * allocated.
 *
 *   Input:  iDeviceDesc       - CD-ROM device descriptor.
 *           szDeviceName      - CD-ROM device name (path).
 *           pvDiscInformation - Disc information structure.
 *
 * Returns: None.
 */
/*========================================================================*/
{
  struct DiscInformation_t *pstDiscInformation;  /* Disc information struct. */
  int iTrackIndex;                               /* Current track count.     */


  pstDiscInformation = (struct DiscInformation_t *) *pvDiscInformation;

#ifdef DEBUG
  fprintf(stderr, "FUNCTION: fnDispose()\n");
#endif

  /* If CDDB information exists, free it. */
  if (pstDiscInformation->pstCDDBinformation)
    fnCDDB_FreeCDDBInformationStruct((void *) &pstDiscInformation->pstCDDBinformation,
                                     pstDiscInformation->pstTOCheader->ending_track);

  /* NOTE: If the output filename was specified (szOutputFilename), it will be freed 
   * along with the track filenames (see fnDiscInformation() for more info).
   */

  /* Dispose of the track filenames. */
  for (iTrackIndex = pstDiscInformation->pstTOCheader->starting_track;
       iTrackIndex <= pstDiscInformation->pstTOCheader->ending_track;
       iTrackIndex++) {
    free(pstDiscInformation->pstTrackData[iTrackIndex - 1].szTrackFilename);
    pstDiscInformation->pstTrackData[iTrackIndex - 1].szTrackFilename = NULL;
  }

  /* Dispose of the track data array. */
  free(pstDiscInformation->pstTrackData);
  pstDiscInformation->pstTrackData = NULL;

  /* Dispose of the drive speed string. */
  free(pstDiscInformation->szDriveSpeed);
  pstDiscInformation->szDriveSpeed = NULL;

  /* Dispose of the TOC entries (the individual track information). */
  free(pstDiscInformation->pstTOCentries->data);
  pstDiscInformation->pstTOCentries->data = NULL;

  /* Dispose of the TOC entries header. */
  free(pstDiscInformation->pstTOCentries);
  pstDiscInformation->pstTOCentries = NULL;

  /* Dispose of the TOC header. */
  free(pstDiscInformation->pstTOCheader);
  pstDiscInformation->pstTOCheader = NULL;

  /* Dispose of the disc information structure. */
  free(pstDiscInformation);
  pstDiscInformation = NULL;

  /* Dispose of the device name string. */
  free(*szDeviceName);
  *szDeviceName = NULL;

  /* Close the device descriptor. */
  close(iDeviceDesc);
}


int
main(int argc, char **argv)
{
  struct DiscInformation_t *pstDiscInformation;  /* Disc information structure      */

  char    *szDeviceName = NULL,	       /* Input device name                         */
          *szOutputFilename = NULL,    /* Output file name                          */
          *szCDDB_RemoteHost = NULL,   /* Hostname used in CDDB queries             */
          *szInfoFilename = NULL;      /* Disc information output filename          */

  int     iDeviceDesc,		       /* File descriptor for the input device      */
          iTrackIndex,                 /* Current track count                       */
          iReturnValue;                /* Functin return value                      */
  int     iDriveSpeed = -1,	       /* CD-ROM read speed (1 == 1x, 2 == 2x, etc) */
          iTrackNumber = -1,	       /* The current track number being extracted  */
          iCDDBquerying = 0,           /* CDDB querying flag (1 == yes, 0 == no)    */
          iCDDB_RemotePort = 0,        /* Port number used in CDDB queries          */
          iSkipTracksWithErrors = 0;   /* Skip tracks with errors (don't exit)      */

  uid_t   utSavedUID;		       /* Saved UserID for the current process      */


  fprintf(stderr, "DAEX v%s - The Digital Audio EXtractor.\n", kszVersion);
  fprintf(stderr, "(c) Copyright 1998 Robert Mooney, All rights reserved.\n\n");

  /* Parse the user arguments and store in the appropriate variables. */
  fnRetrieveArguments(argc, argv, &szDeviceName, &szOutputFilename, 
                      &iTrackNumber, &iDriveSpeed, &iCDDBquerying, 
                      &szCDDB_RemoteHost, &iCDDB_RemotePort, &iSkipTracksWithErrors,
                      &szInfoFilename);

#ifdef DEBUG
  fprintf(stderr, "Device               (user) : %s\n", szDeviceName);
  fprintf(stderr, "Outfile              (user) : %s\n", szOutputFilename);
  fprintf(stderr, "Drive speed          (user) : %i\n", iDriveSpeed);
  fprintf(stderr, "Track number         (user) : %i\n", iTrackNumber);
  fprintf(stderr, "CDDB querying        (user) : %i\n", iCDDBquerying);
  fprintf(stderr, "CDDB remote host     (user) : %s\n", szCDDB_RemoteHost);
  fprintf(stderr, "CDDB remote port     (user) : %i\n", iCDDB_RemotePort);
  fprintf(stderr, "Skip tracks w/errors (user) : %i\n", iSkipTracksWithErrors);
  fprintf(stderr, "Disc info filename   (user) : %s\n\n", szInfoFilename);
#endif


  if (szInfoFilename && !iCDDBquerying)
    fnError(kiExitStatus_General, "You must specify CDDB querying to dump CDDB information.");

  if ((iTrackNumber < 0) && (! (szInfoFilename && iCDDBquerying)))
    fnError(kiExitStatus_General, "You must specify a track number to extract.");

  /* Setup the defaults if we're missing information. */
  fnSanitizeArguments(&szDeviceName);

#ifdef DEBUG
  fprintf(stderr, "Device (corrected)          : %s\n", szDeviceName);
#endif

  /* Set superuser privileges */
  fnSetPrivileges(kiSetPrivilege, &utSavedUID);

  /* Open the CD-ROM device */
  iDeviceDesc = fnOpenDevice(szDeviceName);

  /* Relinquish superuser privileges */
  fnSetPrivileges(kiRelPrivilege, &utSavedUID);

  /* Gather information about the disc, and the track(s) we are going to extract 
   * and store it in the pstDiscInformation structure.
   */
  pstDiscInformation = 
    (struct DiscInformation_t *) fnDiscInformation(iDeviceDesc, iDriveSpeed,
    iTrackNumber, iCDDBquerying, szInfoFilename ? 1 : 0, szCDDB_RemoteHost, iCDDB_RemotePort, 
    szOutputFilename);

  if (!pstDiscInformation)
    fnError(kiExitStatus_General, "Unable to retrieve disc information.");

  /* If the user requested CDDB querying and a CDDB dump file, dump the information
   * gather from fnDiscInformation().  Exit on error.
   */
  if (iCDDBquerying && szInfoFilename)
    if (fnCDDB_DumpCDDBInfo(pstDiscInformation, szInfoFilename) < 0)
      fnError(kiExitStatus_General, "DAEX: Unrecoverable error.");

  /* If we're extracting a track (we're not just extracting the disc CDDB information),
   * start processing.
   */
  if (iTrackNumber >= 0) {
    fprintf(stderr, "DAEX: Beginning the extraction process.\n\n");

    /* If the track number specified was 0, we must be extracting the whole disc. */
    if (iTrackNumber == 0) {

     /* Cycle through the available tracks and extract them. */
     for (iTrackIndex = pstDiscInformation->pstTOCheader->starting_track; 
          iTrackIndex <= pstDiscInformation->pstTOCheader->ending_track;
          iTrackIndex++)

       /* Process the track. */
       if ((iReturnValue = fnProcessTrack(iDeviceDesc, pstDiscInformation, iTrackIndex)) < 0) {

         /* fnProcessTrack() will return one of the following:
	  *
	  *   -1  Unrecoverable error.  DAEX should quit.
	  *   -2  Recoverable error.  DAEX should continue with the next track.
	  */
         switch(iReturnValue) {
          case -1: break;
          case -2: {
            if (iSkipTracksWithErrors)  {
              fprintf(stderr, "DAEX: Skipping the current track (#%i).\n\n", iTrackIndex);
              continue;
            }
            break;
          }
          default: break;
         }

         fnError(kiExitStatus_General, "DAEX: Unrecoverable error.");
       }

    } else
      /* Extract the specified track. */
      if (fnProcessTrack(iDeviceDesc, pstDiscInformation, iTrackNumber) < 0)
        fnError(kiExitStatus_General, "DAEX: Unrecoverable error.");
  }

  /* Reset the drive speed to the maximum attainable speed, assuming
   * we actually set it above.
   */
  if (strcmp(pstDiscInformation->szDriveSpeed, kszSpeedDefault) != 0)
    free( fnSetSpeed(iDeviceDesc, 0) );

  /* Close the CD-ROM device, and free up the previously allocated memory. */
  fnDispose(iDeviceDesc, &szDeviceName, (void *) &pstDiscInformation);

  /* We're done. Yeah! */
  fprintf(stderr, "DAEX: Finished.\n");

  exit(0);
}
