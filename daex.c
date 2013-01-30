/* daex.c - Extracts CDDA from an ATAPI CD-ROM and saves in the WAVE
 *          audio format.  Attempts to correct errors by re-reading
 *          a block up to 10 times before failing.
 *
 * This package, all included text and source are
 * (c) Copyright 1998 Robert Mooney, All rights reserved.
 * 
 * Written by Robert Mooney <rmooney@iss.net>, v.01a 04/26/98
 *
 */

#undef DEBUG			/* define for DEBUG mode */

#include <stdio.h>
#include <sys/cdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include "format.h"

#define kVersion  ".01a"	/* Current working version of DAEX */

void fnUsage(char **argv) {
  fprintf(stderr, "usage: daex [-d device] [-o outfile] [-s drive_speed] [-t track_no]\n\n");
  fprintf(stderr, "   -d device      :  ATAPI CD-ROM device. (default: /dev/wcd0c)\n");
  fprintf(stderr, "   -o outfile     :  The name of the recorded track. (default: output.wav)\n");
  fprintf(stderr, "   -s drive_speed :  The speed at which the CD audio will be read.\n");
  fprintf(stderr, "                     (default: max) (ex. 4 = 4x (706kb/sec)\n");
  fprintf(stderr, "   -t track_no    :  The track number to extract. (default: 1)\n\n");

  exit(1);
}

void main(int argc, char **argv)
{
  int iCount, iError, iErrorRecoveryCount, iFileDesc, iBytesWritten, 
      iOutfileDesc, iPrintedFlag = 0;
  int iLBAstart, iLBAend;
  struct ioc_toc_header stTOCheader;
  struct ioc_read_toc_entry stTOCentry;
  struct cd_toc_entry *pstTOCentryData;
  struct ioc_drive_speed stDriveSpeed;
  struct ioc_play_track stPlayList;
  struct ioc_read_cdda stReadCDDA;
  struct WavFormat_t stWavHeader;
  u_long lTotalBytesWritten = 0;
  char *pszBuffer;
  char szTotalBytesWritten[512];
  int iBlocksToExtract, iCurrentBlock;
  u_long lPercentileMark, lTotalFileLength;
  u_short nPercent;
  extern char *optarg;
  extern int optind;
  int iArgument, iTrackNumber;
  char *szDeviceName, *szOutputFilename;

  printf("DAEX v%s - The Digital Audio EXtractor.\n", kVersion);
  printf("(c) Copyright 1998 Robert Mooney, All rights reserved.\n\n");

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
  if ((iFileDesc = open(szDeviceName, O_RDONLY)) < 0) {
    fprintf(stderr, "Unable to open CDROM device.\n");
    exit(1);
  }

  /* Attempt to read the disc's Table Of Contents.  Exit upon failure. */
  if ((iError = ioctl(iFileDesc, CDIOREADTOCHEADER, &stTOCheader)) < 0) {
    fprintf(stderr, "Unable to read TOC header.\n");
    exit(1);
  }

  printf("Available tracks: %i thru %i.\n\n", stTOCheader.starting_track,
                                              stTOCheader.ending_track);

  /* If the track number wasn't specified, we'll default to track 1 */
  if (iTrackNumber == 0)  iTrackNumber = 1;

  /* If the track number specified is not within the range of tracks on
   * the disc, alert the user, and exit.
   */
  if ((iTrackNumber < stTOCheader.starting_track) ||
      (iTrackNumber > stTOCheader.ending_track)) {
    fprintf(stderr, "The track number you specified is not within the proper range.\n");
    exit(1);
  }

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

  if ((pstTOCentryData = (struct cd_toc_entry *)calloc(1, stTOCentry.data_len)) == NULL) {
    fprintf(stderr, "Unable to allocate sufficient memory for TOC entry data.\n");
    exit(1);
  }

  stTOCentry.data = pstTOCentryData;

  /* Attempt to read the individual track information.  Exit upon failure. */
  if ((iError = ioctl(iFileDesc, CDIOREADTOCENTRYS, &stTOCentry)) < 0) {
    fprintf(stderr, "Unable to read TOC entries.\n");
    exit(1);
  }

  /* Convert the starting and ending LBA's to host-byte order */
  iLBAstart = ntohl(stTOCentry.data[iTrackNumber - 1].addr.lba);
  iLBAend   = ntohl(stTOCentry.data[iTrackNumber].addr.lba);

  printf("Extracting track #%i to \"%s\" at 4x (706kb/sec)...\n", iTrackNumber, szOutputFilename);

/*  sDriveSpeed.speed = 176; */
/*  stDriveSpeed.speed = 353; */
  stDriveSpeed.speed = 706;

  /* Attempt to set the drive's read speed.  Exit upon failure. */
  if ((iError = ioctl(iFileDesc, CDIOSETSPEED, &stDriveSpeed)) < 0) {
    fprintf(stderr, "Unable to set drive speed.\n");
    exit(1);
  }

  /* Attempt to allocate memory for the raw audio data that will be read
   * from the disc.
   */
  if ((pszBuffer = (char *)calloc(1, 2352)) == NULL) {
    fprintf(stderr, "Unable to allocate memory for CDDA buffer.\n");
    exit(1);
  }

  /* Open the output file.  Exit upon failure. */
  if ((iOutfileDesc = open(szOutputFilename, O_WRONLY | O_CREAT, 0644)) < 0) {
    fprintf(stderr, "Unable to open output file, \"%s\"...\n", szOutputFilename);
    exit(1);
  }

  /* Setup the RIFF/WAVE header */
  strncpy(stWavHeader.sRiffHeader,   "RIFF", 4);
  strncpy(stWavHeader.sWavHeader,    "WAVE", 4);
  strncpy(stWavHeader.sFormatHeader, "fmt ", 4);
  strncpy(stWavHeader.sDataHeader,   "data", 4);

  stWavHeader.lFileLength     = 0;       /* We don't know the filelength yet */
  stWavHeader.lFormatLength   = 0x10;    /* Length of the format data (16)   */
  stWavHeader.nFormatTag      = 0x01;    /* PCM header                       */
  stWavHeader.nChannels       = 2;       /* 1 = mono, 2 = stereo             */
  stWavHeader.lSampleRate     = 44100;   /* Sample rate in Khz               */
  stWavHeader.nBitsPerSample  = 16;      /* Bits per sample 8,12,16          */

  /* Block alignment */
  stWavHeader.nBlockAlign     = (stWavHeader.nChannels *
                                 stWavHeader.nBitsPerSample / 8);
  /* Bytes per second */
  stWavHeader.lBytesPerSecond = (stWavHeader.lSampleRate * 
                                 stWavHeader.nBlockAlign);
  /* Length of the data block */
  stWavHeader.lSampleLength   = 0;

  /* Write the audio header to the output file.  Exit upon error.  */
  if (write(iOutfileDesc, &stWavHeader, sizeof(struct WavFormat_t)) != sizeof(struct WavFormat_t)) {
    fprintf(stderr, "Unable to write audio header.\n");
    exit(1);
  }

  /* Setup the CDDA-read structure. */
  stReadCDDA.frames = 1;              /* Number of 2352 byte blocks to read */
  stReadCDDA.buffer = pszBuffer;      /* Copy the buffer point              */

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

    if (iErrorRecoveryCount >= 10) {
      fprintf(stderr, "Unable to recover from error, giving up.  Maybe the disc is scratched?\n");
      exit(1);
    }

    /* Write the returned buffer of raw data to disk.  If not all 2352
     * bytes were written to disk, display an error message and exit...
     * otherwise,  increment the the iTotalBytesWritten counter. 
     */

    if ((iBytesWritten = write(iOutfileDesc, pszBuffer, 2352)) != 2352) {
      fprintf(stderr, "Incorrect number of bytes written to output file (%i of %i).\n", iBytesWritten, 2352);
      exit(1);
    }
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

  /* Fix up the file headers */
  stWavHeader.lFileLength   = lTotalFileLength - 8;
  stWavHeader.lSampleLength = lTotalBytesWritten;

  /* Seek to the begining of the output file and rewrite the audio header */
  lseek(iOutfileDesc, 0, SEEK_SET);
  write(iOutfileDesc, &stWavHeader, sizeof(struct WavFormat_t));

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
