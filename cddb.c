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
 * cddb.c - Routines to support connecting to, and storing information retrieved
 *          from remote CD Disc Database (CDDB) servers.
 *
 * $Id: cddb.c,v 1.7 1998/10/13 20:41:38 rmooney Exp $ 
 */

#include "daex.h"
#include "cddb.h"


/*========================================================================*/
int
fnCDDB_TrackHash(int iTrack_RelativeSeconds)
/*
 * Hashing function as defined in CDDB specifications (cddb.howto).
 *
 *   Input:  iTrack_RelativeSeconds - The total number of seconds for the
 *                                    the current track.
 * Returns:  iReturn                - The generated hash.
 */
/*========================================================================*/
{
  int iReturn = 0;

  while (iTrack_RelativeSeconds > 0) {
    iReturn = iReturn + (iTrack_RelativeSeconds % 10);
    iTrack_RelativeSeconds = iTrack_RelativeSeconds / 10;
  }

  return (iReturn);
}


/*========================================================================*/
int
fnCDDB_BuildQueryString(void *pvDiscInformation)
/*
 * Build the query string that will be used to lookup the disc information.
 * Store the total playing seconds on the disc, the CDDB Disc ID, the track
 * frame offsets, the individual track playing times, and the query string.
 *
 *   Input:  pvDiscInformation - Pointer to the Disc Information structure.
 * Returns:  pvDiscInformation - Total playing seconds, CDDB Disc ID, track
 *                               frame offsets, track playing times, and
 *                               the CDDB query string.
 */
/*========================================================================*/
{
  struct DiscInformation_t  *pstDiscInformation; /* Commonly used disc info              */
  struct ioc_toc_header     *pstTOCheader;	 /* Table of Contents header             */
  struct cd_toc_entry       *pstTOCentryData;    /* Individual Track TOC entries         */
  struct CDDBinformation_t  *pstCDDBinformation; /* CDDB relevant information            */
  char   szDiscID[9];                            /* CDDB DiscID + terminating NULL       */
  char   szQuery[1024],                          /* CDDB query string                    */
         szTemp0[10],                            /* Frame offset character array         */
         szTemp1[500];                           /* String of frame offsets              */
  int    iTrackCount,                            /* Current track count                  */
         iTrackFrameOffset;                      /* Current track frame offset           */
  int    iCDDBhash = 0,                          /* The CDDB disc hash                   */
         iDisc_TotalSeconds = 0;                 /* Total playing seconds on the disc +2 */


  pstDiscInformation = (struct DiscInformation_t *) pvDiscInformation;  

  /* Grab the relevant information from pstDiscInformation. */
  pstTOCheader        = pstDiscInformation->pstTOCheader;
  pstTOCentryData     = pstDiscInformation->pstTOCentries->data;
  pstCDDBinformation  = pstDiscInformation->pstCDDBinformation;

  /* Determine the CDDB hash for the disc.  Cycle through each of the tracks, and call
   * the hashing function as defined in the CDDB specifications (cddb.howto).
   */
  for (iTrackCount=0; iTrackCount < pstTOCheader->ending_track; iTrackCount++)
    iCDDBhash += fnCDDB_TrackHash((pstTOCentryData[iTrackCount].addr.msf.minute * 60) + 
                                  pstTOCentryData[iTrackCount].addr.msf.second);

  /* Determine the total number of play seconds on the disc. */
  iDisc_TotalSeconds = ((pstTOCentryData[pstTOCheader->ending_track].addr.msf.minute * 60) +
                         pstTOCentryData[pstTOCheader->ending_track].addr.msf.second) -
                       ((pstTOCentryData[0].addr.msf.minute * 60) + 
			pstTOCentryData[0].addr.msf.second);

  /* Store the total play seconds on the disc ... add two seconds to account for the
   * lead-in.
   */
  pstCDDBinformation->iDiscTotalSeconds = iDisc_TotalSeconds + 2;

  /* Dump the Disc ID into a string for use in our query. */
  snprintf(szDiscID, sizeof(szDiscID), "%08x",
          ((iCDDBhash % 0xff) << 24 | iDisc_TotalSeconds << 8 | (pstTOCheader->ending_track)));

  /* Copy the CDDB disc ID into the CDDB info structure. */
  strncpy(pstCDDBinformation->szDiscID, szDiscID, 9);

  /* Clear our return and temporary variables. */
  memset(&szQuery, 0, sizeof(szQuery));
  memset(&szTemp1, 0, sizeof(szTemp1));

  /* Allocate memory for the track frame offsets. */
  pstCDDBinformation->iTrackFrameOffset = (int *) calloc(pstTOCheader->ending_track, sizeof(int));

  if (!pstCDDBinformation->iTrackFrameOffset) {
    fprintf(stderr, "DAEX: Unable to allocate memory for the track frame offset array.\n");
    return -1;
  }

  /* Allocate memory for the track play time array. */
  pstCDDBinformation->iTrackSeconds = (int *) calloc(pstTOCheader->ending_track, sizeof(int));

  if (!pstCDDBinformation->iTrackSeconds) {
    fprintf(stderr, "DAEX: Unable to allocate memory for the track play time array.\n");
    return -1;
  }

  /* Determine the frame offset for each track and create a temporary string to be
   * used in the query string.  This forumula assumes 75 frames per second.
   */
  for (iTrackCount=0; iTrackCount < pstTOCheader->ending_track; iTrackCount++) {
    memset(&szTemp0, 0, sizeof(szTemp0));

    iTrackFrameOffset = (((pstTOCentryData[iTrackCount].addr.msf.minute * 60) +
                          pstTOCentryData[iTrackCount].addr.msf.second) * 75) +
                         pstTOCentryData[iTrackCount].addr.msf.frame;

    snprintf(szTemp0, sizeof(szTemp0), "%d ", iTrackFrameOffset);

    /* ... allow for the trailing NULL */
    strncat(szTemp1, szTemp0, sizeof(szTemp1) - strlen(szTemp1) - 1);

    /* Store the current track's frame offset. */
    pstCDDBinformation->iTrackFrameOffset[iTrackCount] = iTrackFrameOffset;

    /* Store the current track's play time. */
    pstCDDBinformation->iTrackSeconds[iTrackCount] =
               ((pstTOCentryData[iTrackCount + 1].addr.msf.minute * 60) +
                 pstTOCentryData[iTrackCount + 1].addr.msf.second) -
               ((pstTOCentryData[iTrackCount].addr.msf.minute * 60) + 
                 pstTOCentryData[iTrackCount].addr.msf.second);
  }

  /* Build the query string. */
  snprintf(szQuery, sizeof(szQuery), "cddb query %s %d %s%d", szDiscID, 
          pstTOCheader->ending_track, szTemp1, pstCDDBinformation->iDiscTotalSeconds);

  if (! (pstCDDBinformation->szCDDBquery = strdup(szQuery))) {
    fprintf(stderr, "DAEX: Unable to allocate sufficient memory for the CDDB query string.\n");
    return -1;
  }

  return 0;
}


/*========================================================================*/
int
fnReadChar(int iSocketFD, char *cBuffer)
/*
 * Set a static pointer to the first character in a static array. Read 
 * characters from the socket's input buffer, and store them in the buffer.
 * Each time the function is called, return the next character in the buffer.
 * If no more characters are available, attempt to fill the buffer once again 
 * through another call to "read".  Exit on error.
 *
 *   Input:  iSocketFD  - A connected socket.
 *           cBuffer    - The memory location where we'll place the next character read
 *                        from the buffer.
 *
 * Returns:  -1 on error, 1 on success.
 *
 *           cBuffer    - A character from the static read buffer.
 */
/*========================================================================*/
{
  static int iCharactersRead = 0;
  static char *pBuffer;
  static char szBuffer[MAX_CDDB_LINE_LENGTH];


  if (iCharactersRead <= 0) {

   repeat:
    if ((iCharactersRead = read(iSocketFD, &szBuffer, sizeof(szBuffer))) < 0) {
      if (errno == EINTR)  goto repeat;
      else  return -1;

    } else if (iCharactersRead == 0)  return 0;

    pBuffer = szBuffer;
  }

  iCharactersRead--;
  *cBuffer = *pBuffer++;

  return 1;
}


/*========================================================================*/
int
fnReadLine(int iSocketFD, char *szBuffer, int iMaxSize)
/*
 * Read characters from fnReadChar() until the buffer provided is filled,
 * a newline is returned, there are no more characters, or we encounter an 
 * error.
 *
 *   Input:  iSocketFD - A connected socket.
 *           szBuffer  - Buffer to store the line read.
 *           iMaxSize  - Maximum size of szBuffer.
 *
 * Returns:  Number of bytes stored in the buffer.
 *
 *           szBuffer  - Characters read from the socket.
 */
/*========================================================================*/
{
  int iCharactersRead,
      iCounter;
  char cTemp;
  char *pBuffer;


  pBuffer = szBuffer;

  for (iCounter=1; iCounter < iMaxSize; iCounter++) {
    if ((iCharactersRead = fnReadChar(iSocketFD, &cTemp)) < 0) {
      return -1;   /* Error. */

    } else if (iCharactersRead == 0) {
      if (iCounter == 1)
        return 0;  /* EOF, no data read. */
      else 
        break;     /* EOF, terminate and return data received. */
    }

    *pBuffer++ = cTemp;
    if (cTemp == '\n')  break;
  }

  *pBuffer = 0;

  return iCounter;
}


/*========================================================================*/
int
fnCDDB_ReadEntry(int iSocketFD, void *pvDiscInformation)
/*
 * Read the disc title and track title information, and store in the
 * appropriate locations in the DiscInformation_t struct (pvDiscInformation).
 *
 *   Input:  iSocketFD         - A connected socket.
 *           pvDiscInformation - Pointer to the disc information structure.
 *
 * Returns:  pvDiscInformation - Disc and track title information 
 *
 *            0 - No error.
 *           -1 - Memory allocation error.
 *           -2 - Socket read error.
 *           -3 - Remote end closed the connection.
 */
/*========================================================================*/
{
  struct DiscInformation_t *pstDiscInfo;     /* Disc information structure.           */
  struct ioc_toc_header *pstTOCheader;       /* Disc's Table Of Contents header.      */
  struct CDDBinformation_t *pstCDDBinfo;     /* Returned CDDB information             */
  char szInputBuffer[MAX_CDDB_LINE_LENGTH],  /* Input buffer read from the file desc. */
       *szTempTitle,                         /* Temporary (disc|track) title          */
       *szTempTitle_Realloced;               /* Reallocated (disc|track) title        */
  int iTrackTitleIndex = 1;                  /* Index for the track title array       */ 
  int iTrack,                                /* Track returned by the query. Not used */
      iCharactersRead;                       /* Number of char's return by fnReadLine */


  pstDiscInfo  = (struct DiscInformation_t *) pvDiscInformation;
  pstTOCheader = pstDiscInfo->pstTOCheader;
  pstCDDBinfo  = pstDiscInfo->pstCDDBinformation;

  if (! (pstCDDBinfo->szTrackTitle = (char **) calloc(pstTOCheader->ending_track, sizeof(char *)))) {
    fprintf(stderr, "Unable to allocate sufficient memory for the track titles array.\n");
    return -1;
  }

  if (! (szTempTitle = (char *) calloc(1, 80))) {
    fprintf(stderr, "Unable to allocate sufficient memory for the temporary title.\n");
    return -1;
  }

  /* Read the contents of the query. */
  while((iCharactersRead = fnReadLine(iSocketFD, szInputBuffer, MAX_CDDB_LINE_LENGTH)) > 0) {

#ifdef CDDB_DEBUG
    /* Display the server's response. */
    fprintf(stderr, "CDDBD: %s", szInputBuffer);
#endif

    /* If a period is found on a line by itself, we break -- CDDBP uses this as
     * a terminating marker.
     */
    if (strncmp(szInputBuffer, ".\r\n", 3) == 0) {
#ifdef CDDB_DEBUG
      fprintf(stderr, "DAEX: Received terminating marker.\n"); 
#endif
      break; 
    }

    /*********************************************************************
     * Read the disc title and store it in the CDDB info structure as
     * szDiscTitle.
     *********************************************************************/

    if (sscanf(szInputBuffer, "DTITLE=%79[^\r\n]\r\n", szTempTitle) > 0) {

      /* Prevent memory leaks in the event that there are multiple DTITLE
       * lines.
       */
      if (pstCDDBinfo->szDiscTitle)  free(pstCDDBinfo->szDiscTitle);

      /* We attempt to use only the space necessary to store the disc's title. */
      if ((szTempTitle_Realloced = (char *) realloc(szTempTitle, strlen(szTempTitle) + 1))) {

        /* Attempt to copy the temporary title into the CDDB info structure. */
        if (! (pstCDDBinfo->szDiscTitle = (char *) strdup(szTempTitle_Realloced))) {
          fprintf(stderr, "Unable to allocate sufficient memory for the disc title.\n");
          return -1;
        }
   
	/* Attempt to reset the temporary title to 80 bytes. If realloc()'ing fails,
	 * free the memory once used, and re-malloc() it.  If this fails, exit.
         */
        if (! (szTempTitle = (void *) realloc(szTempTitle_Realloced, 80))) {
          free(szTempTitle_Realloced);
          szTempTitle_Realloced = NULL;

          if (! (szTempTitle = (void *) malloc(80))) {
            fprintf(stderr, "Unable to allocate sufficient memory for the temporary title.\n");
            return -1;
          }
	}

      } else
	 /* The reallocation failed -- copy the title to the CDDB info structure as it 
          * was.  Exit upon failure.
          */
         if (! (pstCDDBinfo->szDiscTitle = (char *) strdup(szTempTitle))) {
           fprintf(stderr, "Unable to allocate sufficient memory for the disc title.\n");
           return -1;
         }

       /* Clear the temporary title for reuse. */
       memset(szTempTitle, 0, 80);

       continue;
    }

    /*********************************************************************
     * Read the track title, and store in the appropriate location in the
     * track title array.
     *********************************************************************/

    if (sscanf(szInputBuffer, "TTITLE%u=%79[^\r\n]\r\n", &iTrack, szTempTitle) > 0) {

      /* Check for any inconsistency between the number of tracks expected and the
       * number of tracks returned.
       */
      if (iTrackTitleIndex > pstTOCheader->ending_track) {
        fprintf(stderr, "DAEX: Too many titles returned (%d expected, %d returned).\n",
                pstTOCheader->ending_track, iTrackTitleIndex);
        return -1;
      }

      /* We attempt to use only the space necessary to store the track's title. */
      if ((szTempTitle_Realloced = (char * ) realloc(szTempTitle, strlen(szTempTitle) + 1))) {

        /* Attempt to copy the temporary title into the CDDB info structure's track
         * title array. 
         */
        if (! (pstCDDBinfo->szTrackTitle[iTrackTitleIndex - 1] = 
	                               (char *) strdup(szTempTitle_Realloced))) {
          fprintf(stderr, "Unable to allocate sufficient memory for the track title.\n");
          return -1;
        }

	/* Attempt to reset the temporary title to 80 bytes. If realloc()'ing fails,
	 * free the memory once used, and re-malloc() it.  If this fails, exit.
         */
        if (! (szTempTitle = (void *) realloc(szTempTitle_Realloced, 80))) {
          free(szTempTitle_Realloced);
          szTempTitle_Realloced = NULL;

          if (! (szTempTitle = (void *) malloc(80))) {
            fprintf(stderr, "Unable to allocate sufficient memory for the temporary title.\n");
            return -1;
          }
	}

      } else
	 /* The reallocation failed -- copy the track title to the CDDB info structure's
          * track title array as it was.  Exit upon failure.
          */
         if (! (pstCDDBinfo->szTrackTitle[iTrackTitleIndex - 1] = (char *) strdup(szTempTitle))) {
           fprintf(stderr, "Unable to allocate sufficient memory for the track title.\n");
           return -1;
         }

      /* Clear the temporary title for reuse. */
      memset(szTempTitle, 0, 80);

      /* Increase the track title index. */
      iTrackTitleIndex++;

      continue;
    }
  }

  /* Clean up the temporary buffers. */
  free(szTempTitle);

  /* If there was an error reading from the socket, or the remote end closed the
   * connection, alert the user.
   */
  if (iCharactersRead < 0) {
    fprintf(stderr, "DAEX: Error reading from socket: %s.\n", strerror(errno));
    return -2;

  } else if (iCharactersRead == 0) {
    fprintf(stderr, "DAEX: Session terminated prematurely.\n");
    return -3;
  }

  /* Check for any inconsistency between the number of tracks expected and the
   * number of tracks returned.
   */
  if (iTrackTitleIndex - 1 < pstTOCheader->ending_track) {
    fprintf(stderr, "DAEX: Too few titles returned (%d expected, %d returned).\n", 
            pstTOCheader->ending_track, iTrackTitleIndex - 1);
    return -2;
  }

  return 0;
}


/*========================================================================*/
int
fnCDDB_DoDNSlookup(char *szArg1, void *pvInetAddress)
/*
 * Do a lookup on the hostname or IP provided, and return the resulting
 * network address.
 *
 *   Input:  szArg1        - Hostname or IP address (a.b.c.d)
 *           pvInetAddress - Pointer to the network address.
 *
 * Returns:  pvInetAddress - Pointer to the network address.
 *
 *            0 - No error.
 *           -1 - Error resolving the address.
 */
/*========================================================================*/
{
  struct hostent *pstHostEntry;               /* Host entry structure for gethostbyname() */
  struct in_addr *pstInetAddress;             /* InetAddr structure for the remote IP.    */


  pstInetAddress = (struct in_addr *) pvInetAddress;

  /* We first attempt to convert the argument as a numeric IP address, and store it in 
   * sInetAddress as a network compatible address.  If this fails, we attempt to convert
   * the argument as a hostname, and store the primary address returned in sInetAddress as
   * a network compatible address.  On error, we return -1.
   */
  if (!inet_aton(szArg1, pstInetAddress)) {
    if (! (pstHostEntry = gethostbyname(szArg1))) {
      fprintf(stderr, "DAEX: Hostname/IP lookup failed: %s.\n", hstrerror(h_errno));
      return -1;
    }

   /* Store the primary network address (the first returned, at any rate). */
   *pstInetAddress = * (struct in_addr *) pstHostEntry->h_addr_list[0];
  }

  return 0;
}


/*========================================================================*/
int
fnCDDB_ConnectToServer(char *szHostname, int iPort)
/*
 * Connect to the host/port specified and return the connected socket.
 *
 *   Input: szHostname - Hostname/IP of the CDDB server.
 *          iPort      - Port on which the CDDB server listens.
 *
 * Returns: The connected socket on success, or -1 on error.
 */
/*========================================================================*/
{
  int iSocketFD;                              /* The socket used to connect.             */
  struct sockaddr_in sSocket;                 /* Socket address structure for connect(). */
  struct in_addr *pstInetAddress;             /* InetAddr structure for the remote IP.   */


  /* Attempt to allocate memory for the address structure.  Return with error on error. */
  pstInetAddress = (struct in_addr *) calloc(1, sizeof(struct in_addr));

  if (!pstInetAddress) {
    fprintf(stderr, "Unable to allocate sufficient memory for the address structure.\n");
    return -1;
  }

  /* Attempt to lookup the hostname specified by the user.  Return with error on error. */
  if (fnCDDB_DoDNSlookup(szHostname, pstInetAddress) < 0) {
    return -1;
  }

  /* Create the socket we'll use to connect to the CDDB server. Return with error on
   * error. 
   */
  if ((iSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "DAEX: Unable to create socket: %s.\n", strerror(errno));
    return -1;
  }

  /* Initialize the socket address structure, and fill in the port and address information. */
  memset(&sSocket, 0, sizeof(struct sockaddr_in));

  sSocket.sin_family        = AF_INET;
  sSocket.sin_port          = htons(iPort);
  sSocket.sin_addr          = *pstInetAddress;

  /* Free up the previously allocated memory. */
  free(pstInetAddress);

  /* Connect to the CDDB server. Return with error on error. */
  if (connect(iSocketFD, (struct sockaddr *) &sSocket, sizeof(struct sockaddr_in)) < 0) {
    fprintf(stderr, "DAEX: Unable to connect to CDDB server: %s.\n", strerror(errno));
    return -1;
  }

  /* Return the connected socket. */
  return iSocketFD;
}


/*========================================================================*/
void
fnCDDB_SessionTerminate(int iSocketFD, int iType)
/*
 * Close the current session by closing the socket, and/or sending the
 * CDDB "quit" command.
 *
 *   Input:  iSocketFD - A connected socket.
 *           iType     - How to go about closing the socket.  1 == send the
 *                       CDDB "quit" command first, 0 == don't.
 *
 * Returns:  None.
 */
/*========================================================================*/
{
  if (iType == 1) {
    write(iSocketFD, "quit\r\n", 6);

#ifdef CDDB_DEBUG
    fprintf(stderr, "DAEX: Sent QUIT command.\n");
#endif
  }

  close(iSocketFD);

  fprintf(stderr, "DAEX: Connection closed.\n");
}


/*========================================================================*/
int
fnCDDB_SendRequest(int iSocketFD, void *pvDiscInformation, int *iSessionState,
                   char *szBuffer)
/*
 * Provide functionality for the client end of DAEX.  Our requests to the
 * CDDB server are sent from this function (the request sent depends on the 
 * current session state).
 *
 *   Input:  iSocketFD         - A connected socket.
 *           pvDiscInformation - Pointer to the disc information structure.
 *           iSessionState     - The current session state.
 *           szBuffer          - Line of input from the server.
 *
 * Returns:  -1 on error, 0 on success.
 *
 *           The CDDB category for this disc is returned through the
 *           disc information strucuture (pvDiscInformation).
 */
/*========================================================================*/
{
  struct DiscInformation_t *pstDiscInformation; /* Pointer to the disc information struct */
  struct CDDBinformation_t *pstCDDBinformation; /* Pointer to the CDDB information struct */
  char szCategory[25],                          /* Local category (ie, rock, jazz, etc).  */
       szDiscID[9],                             /* Local DiscID (from the server).        */
       szHostname[MAXHOSTNAMELEN],              /* Hostname of the current machine.       */
       szSendBuffer[MAX_CDDB_LINE_LENGTH];      /* Buffer sent to the remote CDDB server. */
  char *szLoginName;                            /* Username of the current user.          */


  pstDiscInformation = (struct DiscInformation_t *) pvDiscInformation;
  pstCDDBinformation = pstDiscInformation->pstCDDBinformation;

  switch(*iSessionState) {
   case SESSION_CONNECTING: {  /* We're connected, initiate the handshake. */

     /* Attempt to grab the current user's username.  If this fails, set the username
      * passed to CDDB server to "unknown".  If the grab was successful, allocate a
      * new buffer to store the username.  We do this to avoid a strcmp() after building
      * the send buffer -- instead we just free our variable, szLoginName (the value 
      * returned from getlogin() is a pointer to a static buffer).
      */
     if (! (szLoginName = getlogin())) {
       if (! (szLoginName = strdup("unknown"))) {
         fprintf(stderr, "DAEX: Unable to allocate sufficient memory for the current username.\n");
         return -1;
       }
     } else
       if (! (szLoginName = strdup(szLoginName))) {
         fprintf(stderr, "DAEX: Unable to allocate sufficient memory for the current username.\n");
         return -1;
       }

     /* Attempt to grab the hostname.  If this fails, create a generic one. */
     if (gethostname(szHostname, MAXHOSTNAMELEN) < 0) {
       snprintf(szHostname, MAXHOSTNAMELEN, "unknown.hostname");
     }

     /* Create the send buffer. */
     snprintf(szSendBuffer, sizeof(szSendBuffer),
              "cddb hello %s %s DAEX %s\r\n", szLoginName, szHostname, kszVersion);

     free(szLoginName);

     /* Send the buffer containing our CDDB HELLO command. */
     write(iSocketFD, szSendBuffer, strlen(szSendBuffer));

#ifdef CDDB_DEBUG
     fprintf(stderr, "DAEX: Sent HELLO command.\n");
#endif

     *iSessionState = SESSION_SENT_HELLO;

     break;
   }

   case SESSION_SENT_HELLO: {  /* The handshake was successful, initiate the query. */

      /* Check to be sure there's enough room available in the send buffer string for our
       * query.
       */
      if ((strlen(pstCDDBinformation->szCDDBquery) + 3) > sizeof(szSendBuffer)) {
        fprintf(stderr, "DAEX: Query string exceeds the size of the send buffer.\n");
        return -1;
      }

      /* Create the send buffer. */
      snprintf(szSendBuffer, sizeof(szSendBuffer), "%s\r\n", pstCDDBinformation->szCDDBquery);

#ifdef CDDB_DEBUG
      fprintf(stderr, "DAEX: Query string follows:\n%s", szSendBuffer);
#endif

      /* Send the query string to the server. */
      write(iSocketFD, szSendBuffer, strlen(szSendBuffer));

#ifdef CDDB_DEBUG
      fprintf(stderr, "DAEX: Sent QUERY command.\n");
#endif

      *iSessionState = SESSION_SENT_QUERY;
      break;
   }

   case SESSION_SENT_QUERY: {  /* The query was successful, read the response. */

     if (sscanf(szBuffer, "%*u %24s %8s %*s\r\n", szCategory, szDiscID) == 2) {

        /* NULL terminate the variables read. */
        szCategory[sizeof(szCategory) - 1] = 0;
        szDiscID[sizeof(szDiscID) - 1] = 0;

        /* Copy the disc's category in the CDDB information structure. */
        if (! (pstCDDBinformation->szDiscCategory = strdup(szCategory))) {
          fprintf(stderr, "DAEX: Unable to allocate sufficient memory for the disc category.\n");
          return -1;
	}

        /* Create the send buffer. */
        snprintf(szSendBuffer, sizeof(szSendBuffer), "cddb read %s %s\r\n", 
                 szCategory, szDiscID);

        /* Send the query string to the server. */
        write(iSocketFD, szSendBuffer, strlen(szSendBuffer));

#ifdef CDDB_DEBUG
        fprintf(stderr, "DAEX: Sent READ command.\n");
#endif

        *iSessionState = SESSION_SENT_READ;

     } else
       /* send quit */
       return -1;

     break;
   }

   default: {  /* Unknown session state. */
#ifdef CDDB_DEBUG
     fprintf(stderr, "DAEX: Unknown session state (fnCDDB_SendRequest).\n");
#endif
     return -1;
   }
  }

  return 0;
}


/*========================================================================*/
int
fnCDDB_DoCDDBLookup(int iSocketFD, void *pvDiscInformation, int *iSessionState)
/*
 * Parse input from the CDDB server and send the appropriate response.
 *
 *   Input: iSocketFD         - A connected socket.
 *          pvDiscInformation - Pointer to the disc information structure.
 *          iSessionState     - The current session state.
 *
 * Returns:  0 - Successful query.
 *          -1 - Memory allocation error.
 *          -2 - No match, Entry corrupt, Server error.
 *          -3 - Negotiation error.
 *          -4 - No handshake.
 */
/*========================================================================*/
{
  struct DiscInformation_t *pstDiscInformation; /* Pointer to disc information structure. */
  char szInputBuffer[MAX_CDDB_LINE_LENGTH];     /* Input buffer for server's response.    */
  int iResponseCode,                            /* CDDB response code                     */
      iCharactersRead;                          /* Characters read from the socket.       */


  pstDiscInformation = (struct DiscInformation_t *) pvDiscInformation;

  /* Read data from the socket until we encouter an error. */
  while ((iCharactersRead = fnReadLine(iSocketFD, szInputBuffer, MAX_CDDB_LINE_LENGTH)) > 0) {

#ifdef CDDB_DEBUG
    /* Display the server's response. */
    fprintf(stderr, "CDDBD: %s", &szInputBuffer[0]);
#endif

    /* We assume that any response from the remote server will contain at least
     * one linefeed, and that we're reading the maximum line length from the
     * socket (which is defined as 256 characters in the CDDB 1.3.1 server docs).
     * If a linefeed is not found in the returned buffer, we label it as an error.
     */
    if (!strchr(szInputBuffer, '\n')) {
      fprintf(stderr, "DAEX: Input buffer does not contain a newline.  Exiting.\n");
      return -2;
    }

    /* Parse the response code from the buffer... see the CDDB Server Protocol
     * Specifcations for more information.
     */
    if (sscanf(szInputBuffer, "%u %*s", &iResponseCode) > 0) {

      /* Session states can be one of the following:
       *
       *    SESSION_CONNECTING  (sign-on)
       *    SESSION_SENT_HELLO  (initial client-server handshake)
       *    SESSION_SENT_QUERY  (CDDB query)
       *    SESSION_SENT_READ   (CDDB read)
       *
       *  The states are used to determine the exact meaning of the server's reponse
       *  code.  Each code has a different meaning, depending on which command invoked
       *  the response.
       */

      switch (*iSessionState) {
       case SESSION_CONNECTING: {
         switch (iResponseCode) {
	  case 200: {  /* OK, read/write allowed. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Command OK.  Read/Write allowed.\n");
#endif
            break;
	  }

	  case 201: {  /* OK, read only. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Command OK.  Read only.\n");
#endif
            break;
	  }

	  case 432: {  /* No connections allowed: permission denied. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: No connections allowed.  Permission denied.\n");
#endif
            return -3;
	  }

          case 433: {  /* No connections allowed: X users allowed, Y currently active. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: No connections allowed.  Too many users.\n");
#endif
            return -3;
	  }

          case 434: {  /* No connections allowed: system load too high. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: No connections allowed.  System load too high.\n");
#endif
            return -3;
	  }

	  default: {   /* Unknown server response. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Unknown server reponse.\n");
#endif
            return -2;
	  }
	 }

         break;
       }

       case SESSION_SENT_HELLO: {
	 switch(iResponseCode) {
	  case 200: {  /* Handshake successful. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Handshake successful.\n");
#endif
            break;
	  }

	  case 402: {  /* Already shook hands. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Already shook hands.\n");
#endif
            break;
	  }

	  case 431: {  /* Handshake not successful, closing connection. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "Handshake not successful, closing connection.\n");
#endif
            return -3;
	  }

	  default: {   /* Unknown server response. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Unknown server reponse.\n");
#endif
            return -2;
	  }
	 }

         break;
       }

       case SESSION_SENT_QUERY: {
         switch(iResponseCode) {
	  case 200: {  /* Found exact match. */
            fprintf(stderr, "DAEX: Found exact match.\n");
            break;
	  }

	  case 202: {  /* No match found. */
            fprintf(stderr, "DAEX: No match found.\n");
            return -2;
	  }

	  case 211: {  /* Found inexact matches, list follows (until terminating marker). */
            fprintf(stderr, "DAEX: Found inexact matches.\n");
            fprintf(stderr," DAEX: Unfortunately, this is unimplemented.\n");
            /* break; */
            return -2;
	  }

	  case 403: {  /* Database entry is corrupt. */
            fprintf(stderr, "DAEX: Database entry is corrupt.\n");
            return -2;
	  }

	  case 409: {  /* No handshake. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: No handshake.\n");
#endif
            return -4;
	  }

	  default: {   /* Unknown server response. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Unknown server reponse.\n");
#endif
            return -2;
	  }
	 }

         break;
       }

       case SESSION_SENT_READ: {
         switch(iResponseCode) {
	  case 210: {  /* OK, CDDB entry follows (until terminating marker) */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: OK. CDDB entry follows.\n");
#endif
            /* Return to the main loop to begin processing of the CDDB entry. */
            return 0;
	  }

	  case 401: {  /* Specified CDDB entry not found. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Specified CDDB entry not found.\n");
#endif
            return -2;
	  }

	  case 402: {  /* Server error. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Server error.\n");
#endif
            return -2;
	  }

	  case 403: {  /* Database entry is corrupt. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Database entry is corrupt.\n");
#endif
            return -2;
	  }

	  case 409: {  /* No handshake. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: No handshake.\n");
#endif
            return -4;
	  }

	  default: {   /* Unknown server response. */
#ifdef CDDB_DEBUG
            fprintf(stderr, "DAEX: Unknown server reponse.\n");
#endif
            return -2;
	  }
         }

         break;
       }

       default: {  /* Unknown session state. */
#ifdef CDDB_DEBUG
         fprintf(stderr, "DAEX: Unknown session state (fnCDDB_DoCDDBLookup).\n");
#endif
         return -2;
       }
      }

      /* Process the next request.  If there is an error in processing, return an
       * error to this function's caller so that we may disconnect.
       */
      if (fnCDDB_SendRequest(iSocketFD, pstDiscInformation, iSessionState, 
                             szInputBuffer) < 0) {
        return -2;
      }
    }
  }

  /* If there was an error reading from the socket, alert the user. */
  if (iCharactersRead < 0) {
    fprintf(stderr, "DAEX: Error reading from socket: %s.\n", strerror(errno));
    return -2;
  }

  /* ... or if the remote end closed the connection, alert the user. */
  fprintf(stderr, "DAEX: Session terminated prematurely.\n");
  return -3;
}


/*========================================================================*/
int
fnCDDB_SessionWrapper(int iSocketFD, void *pvDiscInformation, int *iTerminationType)
/*
 * Handle the lookup and retrieval of the disc's title and track information.
 *
 *   Input:  iSocketFD         - A connected socket.
 *           pvDiscInformation - Pointer to the disc information struct.
 *           iTerminationType  - How to go about terminating the session.
 *                                 0 == don't send the "quit" command.
 *                                 1 == send the "quit" command.
 *
 * Returns:  -1 on error, 0 otherwise.
 */
/*========================================================================*/
{
  struct DiscInformation_t *pstDiscInformation;  /* Disc information structure.          */
  struct ioc_toc_header    *pstTOCheader;        /* Disc's TOC header.                   */
  int iSessionState,                             /* Current session state.               */
      iReturnValue;                              /* Lookup and retrieval function return
                                                    values. */
  int iNegotiationCounter = 1;                   /* CDDB session negotiation counter     */


  pstDiscInformation = (struct DiscInformation_t *) pvDiscInformation;
  pstTOCheader = pstDiscInformation->pstTOCheader;

  /* The default session termination type is set to 1.
   *
   *   0 == don't send the "quit" command before closing the connection.
   *   1 == send the "quit" command before closing the connection.
   */
  *iTerminationType = 1;

  fprintf(stderr, "DAEX: Negotiating session.\n");

  connection_negotiate:

  /* Set the initial state of the session.  We'll use these states to determine the
   * meaning of the server's reponse code.
   */
  iSessionState = SESSION_CONNECTING;

  /* Do the initial queries, so that we may retrieve the disc and track title information.
   * On error, fnCDDB_DoCDDBLookup() will return one of the following values:
   *
   *   -1   Memory allocation error.
   *   -2   No match, Entry corrupt, Server error.
   *   -3   Negotiation error.
   *   -4   No handshake.
   */
  if ((iReturnValue = fnCDDB_DoCDDBLookup(iSocketFD, pstDiscInformation, &iSessionState)) < 0) {

    switch(iReturnValue) {
     case -1:
     case -2: break;
     case -3: { 
       *iTerminationType = 0;
       break; 
     }
     case -4: {
       /* The server didn't receive our handshake.  If we've exceeded 5 negotiation
	* attempts, close the connection.
        */
       if (iNegotiationCounter++ > 5) {
#ifdef CDDB_DEBUG
         fprintf(stderr, "DAEX: Too many renegotiation attempts.\n");
#endif
         break;
       }

       goto connection_negotiate;
     }
    }

    return -1;
  }

  fprintf(stderr, "DAEX: Reading CDDB information.\n");

  /* Read the disc and track title information.  On error, fnCDDB_ReadEntry() will return
   * one of the following values:
   *
   *   -1  Memory allocation error.
   *   -2  Socket read error.
   *   -3  Remote end closed the connection.
   */
  if ((iReturnValue = fnCDDB_ReadEntry(iSocketFD, pstDiscInformation)) < 0) {

    switch(iReturnValue) {
     case -1:
     case -2: break;
     case -3: { 
       *iTerminationType = 0;
       break; 
     }
    }

    fprintf(stderr, "DAEX: Error reading CDDB information.\n");
    return -1;
  }

  return 0;
}


/*========================================================================*/
char *
fnCDDB_ConvertTitleToFilename(char *szTitle, int iTitleTypeFlag)
/*
 * Convert a CDDB title to something which could be used as a filename.
 *
 *   Input:  szTitle        - The CDDB title.
 *           iTitleTypeFlag - Type of title we're converting. 0 == track, 1 == disc.
 *
 * Returns:  Pointer to the filename, or NULL on error.
 */
/*========================================================================*/
{
  char *szFilename,                  /* The converted filename.                     */
       *szTempFilename,              /* Temporary filename based on the CDDB title. */
       *szTempFilename_Realloced;    /* The realloced title.                        */
  int  iCounter,                     /* Current character counter.                  */
       iPreviouslyModified,          /* Flag to indicate previously modified chars. */
       iFilenameIndex,               /* Character counter for the actual filename.  */
       iTitleLength;                 /* The original title's length.                */


  iTitleLength = strlen(szTitle);

  /* Allocate memory for the filename, including the terminating NULL.  Once the 
   * conversion is complete, we copy the pointer from the temporary variable to the
   * "permanent" variable (permanent for this function at any rate). :)
   */
  if (!(szTempFilename = (char *) calloc(1, iTitleLength + 1))) {
    fprintf(stderr, "Unable to allocate sufficient memory for the track filename.\n");
    return NULL;
  }

  /* Cycle through characters in the title. */
  for (iCounter = iFilenameIndex = iPreviouslyModified = 0; iCounter < iTitleLength; 
       iCounter++) {

    /* If we're currently working with the disc title string, and we encounter a
     * forward slash, convert it to a dash to separate the group from title.
     */
    if ((iTitleTypeFlag) && (szTitle[iCounter] == 47)) {
      if (iPreviouslyModified)  iFilenameIndex--;

      szTempFilename[iFilenameIndex++] = '-';
      iPreviouslyModified = 1;

    /* Characters other than [0-9A-Za-z\(\)\.\,] are converted to an underscore. */
    } else if (! (((szTitle[iCounter] >= 48) && (szTitle[iCounter] <= 57)) ||
                  ((szTitle[iCounter] >= 65) && (szTitle[iCounter] <= 90)) ||
                  ((szTitle[iCounter] >= 97) && (szTitle[iCounter] <= 122)) ||
                   (szTitle[iCounter] == 40) || (szTitle[iCounter] == 41) || 
                   (szTitle[iCounter] == 44) || (szTitle[iCounter] == 46))) {

      /* If the character that will appear before the one we're about to replace has
       * already been modified, we skip the current replacement.
       */
      if (!iPreviouslyModified) {
        szTempFilename[iFilenameIndex++] = '_';
        iPreviouslyModified = 1;
      }

    } else {
      /* ... any other characters besides those that are converted above are copied
       * into the temporary filename string.
       */
      iPreviouslyModified = 0;
      szTempFilename[iFilenameIndex++] = szTitle[iCounter];
    }
  }

  /* If the converted filename is shorter than it's original length, we attempt to
   * reallocate it's memory usage so that it only uses the memory it requires. Otherwise,
   * it would appear that no changes to the title have been made in it's conversion to
   * a filename.
   */
  if (iFilenameIndex < iTitleLength) {

    /* We attempt to use only the space necessary to store the track's filename. */
    if ((szTempFilename_Realloced = (char *) realloc(szTempFilename, iFilenameIndex+1))) {
      szTempFilename_Realloced[iFilenameIndex] = 0;  /* Terminate the string. */

      /* Copy the pointer from the reallocated temporary filename to the track filename 
       * string. 
       */
      szFilename = szTempFilename_Realloced;

    } else {
      /* The reallocation failed -- copy the pointer from the temporary filename to the 
       * track filename string.
       */
      szFilename = szTempFilename;
    }

  } else
    /* The temporary filename was the same length as the title -- apparently, no changes 
     * were made.  Therefore, we just copy the pointer from the temporary variable to the
     * track filename string.
     */
     szFilename = szTempFilename;

  /* Return the converted filename. */
  return szFilename;
}


/*========================================================================*/
int
fnCDDB_StoreFilenames(void *pvDiscInformation)
/*
 * Convert the CDDB titles to filenames, and store them in the disc
 * information structure.
 *
 *   Input: pvDiscInformation - Disc information structure.
 * Returns: -1 on error, 0 otherwise.
 */
/*========================================================================*/
{
  struct DiscInformation_t *pstDiscInformation;   /* Disc information structure   */
  struct ioc_toc_header *pstTOCheader;            /* Disc TOC header              */
  struct TrackInformation_t *pstTrackInformation; /* Track information structure  */
  struct CDDBinformation_t *pstCDDBinformation;   /* CDDB information structure   */
  int iCounter;                                   /* Current track counter        */
  char *szFilename_title,                         /* Disc title as a filename     */
       *szFilename_track;                         /* Track title as a filename    */
  char szFilename_temp[kiMaxStringLength];        /* Temporary assembled filename */


  pstDiscInformation  = (struct DiscInformation_t *) pvDiscInformation;
  pstTOCheader        = pstDiscInformation->pstTOCheader;
  pstCDDBinformation  = pstDiscInformation->pstCDDBinformation;
  pstTrackInformation = pstDiscInformation->pstTrackData;

  if (! (szFilename_title = 
         fnCDDB_ConvertTitleToFilename(pstCDDBinformation->szDiscTitle, 1))) {
    return -1;
  }

  for (iCounter=1; iCounter <= pstTOCheader->ending_track; iCounter++) {
    if (! (szFilename_track = 
	   fnCDDB_ConvertTitleToFilename(pstCDDBinformation->szTrackTitle[iCounter - 1], 0))) {
      return -1;
    }

    /* Store the current track's CDDB-derived filename. */
    if (snprintf(szFilename_temp, kiMaxStringLength, "%s-%02d-%s.wav", 
                 szFilename_title, iCounter, szFilename_track) > MAX_FILENAME_LENGTH) {
      fprintf(stderr, "DAEX: Maximum filename length exceeded.\n");
      return -1;
    }

    if (! (pstTrackInformation[iCounter - 1].szTrackFilename = (char *)
	   calloc(1, MAX_FILENAME_LENGTH + 1))) {
      fprintf(stderr, "Unable to allocate sufficient memory for the track filename.\n");
      return -1;
    }

    memcpy(pstTrackInformation[iCounter - 1].szTrackFilename, szFilename_temp,
           strlen(szFilename_temp));

    free(szFilename_track);
  }

  free(szFilename_title);
  return 0;
}


/*========================================================================*/
void
fnCDDB_FreeCDDBInformationStruct(void **pvCDDBinfo, int iTracksExpected)
/*
 * Free up the memory used by the CDDB information structure.
 *
 *   Input:  pvCDDBinfo     - CDDB information structure.
 *           iTrackExpected - Number of tracks on the disc, used to free
 *                            the track frame offsets, total seconds, and
 *                            file names.
 *
 * Returns:  None.
 */
/*========================================================================*/
{
  int iCounter;                           /* Current track counter.      */
  struct CDDBinformation_t *pstCDDBinfo;  /* CDDB information structure. */


  pstCDDBinfo = (struct CDDBinformation_t *) *pvCDDBinfo;

  /* Dispose of the CDDB query string. */
  if (pstCDDBinfo->szCDDBquery) {
    free(pstCDDBinfo->szCDDBquery);
    pstCDDBinfo->szCDDBquery = NULL;
  }

  /* Dispose of the CDDB disc category. */
  if (pstCDDBinfo->szDiscCategory) {
    free(pstCDDBinfo->szDiscCategory);
    pstCDDBinfo->szDiscCategory = NULL;
  }

  /* Free the frame offset array. */
  if (pstCDDBinfo->iTrackFrameOffset) {
    free(pstCDDBinfo->iTrackFrameOffset);
    pstCDDBinfo->iTrackFrameOffset = NULL;
  }

  /* Free the track play time array. */
  if (pstCDDBinfo->iTrackSeconds) {
    free(pstCDDBinfo->iTrackSeconds);
    pstCDDBinfo->iTrackSeconds = NULL;
  }

  /* Cycle through the disc's tracks and free the track titles. */
  for(iCounter=0; iCounter < iTracksExpected; iCounter++)
    if (pstCDDBinfo->szTrackTitle[iCounter]) {
      free(pstCDDBinfo->szTrackTitle[iCounter]);
      pstCDDBinfo->szTrackTitle[iCounter] = NULL;
    }

  /* Free the track title array. */
  if (pstCDDBinfo->szTrackTitle) {
    free(pstCDDBinfo->szTrackTitle);
    pstCDDBinfo->szTrackTitle = NULL;
  }

  /* Free the disc title. */
  if (pstCDDBinfo->szDiscTitle) {
    free(pstCDDBinfo->szDiscTitle);
    pstCDDBinfo->szDiscTitle = NULL;
  }

  /* Free the CDDB information structure. */
  if (pstCDDBinfo) {
    free(pstCDDBinfo);
    pstCDDBinfo = NULL;
  }
}


/*========================================================================*/
int
fnCDDB_DumpCDDBInfo(void *pvDiscInformation, char *szFilename)
/*
 * Create the CDDB information file.  Output includes the disc's total
 * play time, total tracks, artist's name, album name, CDDB disc ID, CDDB
 * genre, and each track's frame offset, play time, and title.
 *
 *   Input:  pvDiscInformation - Disc information structure.
 *           szFilename        - Output filename.
 *
 * Returns:  -1 on error, 0 otherwise.
 */
/*========================================================================*/
{
  FILE   *fOutfile;                              /* Output file stream.         */
  struct DiscInformation_t *pstDiscInformation;  /* Disc information structure. */
  char   *szDiscTitle_temp,                      /* Temporary disc title.       */
         *pDiscTitle,                            /* Pointer to szDiscTitle_temp */
         *szDiscArtist,                          /* Disc artist string.         */
         *szDiscTitle;                           /* Disc title string.          */
  int    iDiscArtistLen,                         /* Length of szDiscArtist      */
         iCount;                                 /* Current track counter.      */
  time_t ttCurrentTime;                          /* Current time value.         */


  pstDiscInformation = (struct DiscInformation_t *) pvDiscInformation;

  fprintf(stderr, "DAEX: Dumping CDDB data to the specified info file.\n");

  /* Open the output file for writing. Return on error. */
  if (! (fOutfile = fopen(szFilename, "w"))) {
    fprintf(stderr, "DAEX: Unable to create info file: %s.\n", strerror(errno));
    return -1;
  }

  /* Dump the DAEX header. */
  fprintf(fOutfile, "# DAEX v%s - The Digital Audio EXtractor.\n", kszVersion);
  fprintf(fOutfile, "# (c) Copyright 1998 Robert Mooney, All rights reserved.\n");
  fprintf(fOutfile, "#\n");

  /* Grab the current time. */
  ttCurrentTime = time(NULL);

  /* Continue with the header, and convert the current time to something readable. */
  fprintf(fOutfile, "# Disc information file created on %s", ctime(&ttCurrentTime));
  fprintf(fOutfile, "#\n\n");

  /* Dump initial information on the disc. */
  fprintf(fOutfile, "Type                 : Audio CD\n");
  fprintf(fOutfile, "Total time (seconds) : %d\n", pstDiscInformation->pstCDDBinformation->iDiscTotalSeconds);
  fprintf(fOutfile, "Total tracks         : %d\n", pstDiscInformation->pstTOCheader->ending_track);

  /* Copy the temporary disc title from the disc title stored in the disc information
   * structure.
   */
  szDiscTitle_temp = strdup(pstDiscInformation->pstCDDBinformation->szDiscTitle);

  if (!szDiscTitle_temp) {
    fprintf(stderr, "DAEX: Unable to allocate sufficient memory for the temporary disc title.\n");
    return -1;
  }

  /* Set the pointer to the temporary disc title. */
  pDiscTitle = szDiscTitle_temp;

  /* Copy the disc's artist into a more permanent variable. */
  szDiscArtist = strdup(strsep(&szDiscTitle_temp, "/"));

  if (!szDiscArtist) {
    fprintf(stderr, "DAEX: Unable to allocated sufficient memory for the disc artist.\n");
    return -1;
  }

  /* Remove the trailing space (if any) from the disc artist string. */
  iDiscArtistLen = strlen(szDiscArtist);
  if (szDiscArtist[iDiscArtistLen - 1] == ' ')  szDiscArtist[iDiscArtistLen - 1] = 0;

  /* If there's more after the "/" in the CDDB Disc title, this must be the actual disc
   * title.  Skip past the first space (if it exists).  If there's nothing after the "/",
   * or if the "/" doesn't exist, set the disc's title to "-".
   */
  if (szDiscTitle_temp) {
    if (szDiscTitle_temp[0] == ' ') {
      if (strlen(szDiscTitle_temp) > 1)  szDiscTitle_temp++;
       else szDiscTitle_temp[0] = '-';
    }

  } else {
    free(pDiscTitle);

    if (! (szDiscTitle_temp = strdup("-"))) {
      fprintf(stderr, "DAEX: Unable to allocate sufficient memory for the temporary disc title.\n");
      return -1;
    }

    pDiscTitle = szDiscTitle_temp;
  }

  /* Copy the resulting disc title. */
  szDiscTitle = szDiscTitle_temp;

  /* Dump the artist/album information. */
  fprintf(fOutfile, "Artist               : %s\n", szDiscArtist);
  fprintf(fOutfile, "Album                : %s\n", szDiscTitle);

  /* Free up the previously allocated memory. */
  free(szDiscArtist);
  free(pDiscTitle);

  /* Dump the CDDB Disc ID and Genre. */
  fprintf(fOutfile, "CDDB Disc ID         : %s\n", pstDiscInformation->pstCDDBinformation->szDiscID);
  fprintf(fOutfile, "CDDB Disc Genre      : %s\n\n", pstDiscInformation->pstCDDBinformation->szDiscCategory);

  /* Dump additional information on the format of the info file. */
  fprintf(fOutfile, "# The data format for the individual tracks is as follows:\n");
  fprintf(fOutfile, "#\n");
  fprintf(fOutfile, "#   Field 1 ... Track number.\n");
  fprintf(fOutfile, "#   Field 2 ... Filename, or would-be filename.\n");
  fprintf(fOutfile, "#   Field 3 ... Frame offset. (One frame = 1/75 second)\n");
  fprintf(fOutfile, "#   Field 4 ... Play time in seconds.\n");
  fprintf(fOutfile, "#   Field 5 ... Title of the track enclosed in braces -- {}.\n");
  fprintf(fOutfile, "#\n");
  fprintf(fOutfile, "# Field 5 is followed by a newline.\n");
  fprintf(fOutfile, "#\n\n");

  /* Dump data from the CDDB info struct. */
  for (iCount=0; iCount < pstDiscInformation->pstTOCheader->ending_track; iCount++) {
    fprintf(fOutfile, "%02d %s %d %d {%s}\n", iCount + 1,
            pstDiscInformation->pstTrackData[iCount].szTrackFilename,
            pstDiscInformation->pstCDDBinformation->iTrackFrameOffset[iCount],
            pstDiscInformation->pstCDDBinformation->iTrackSeconds[iCount],
            pstDiscInformation->pstCDDBinformation->szTrackTitle[iCount]);
  }

#ifdef CDDB_DEBUG
  fprintf(fOutfile, "\nQuery string:\n-------------\n%s\n", pstDiscInformation->pstCDDBinformation->szCDDBquery);
#endif

  /* Print the end-of-text marker. */
  fprintf(fOutfile, "\n# End of CDDB information.\n");

  /* Flush the stream and close it. */
  fflush(fOutfile);
  fclose(fOutfile);

  fprintf(stderr, "DAEX: Finished dumping CDDB data.\n\n");

  return 0;
}


/*========================================================================*/
int
fnCDDB_DoCDDBQuery(void *pvDiscInformation, char *szCDDB_RemoteHost, int iCDDB_RemotePort,
                   int iCDDB_SkipFilenames)
/*
 * Handle the connection to the CDDB server, the retrieval of disc info,
 * and the conversion of the disc and track titles.
 *
 *   Input:  pvDiscInformation   - Disc information structure.
 *           szCDDB_RemoteHost   - The remote hostname or IP of the CDDB server.
 *           szCDDB_RemotePort   - The port the CDDB server is listening on.
 *           iCDDB_SkipFilenames - Flag which determines whether or not to use
 *                                 CDDB filenames in the extraction.
 *
 *  Returns:  -1 on error, 0 otherwise.
 */
/*========================================================================*/
{
  int iSocketFD, iReturnValue;
  struct DiscInformation_t *pstDiscInformation;
  struct ioc_toc_header    *pstTOCheader;
  int iTerminationType = 1; /* 1 == send quit */


  pstDiscInformation = (struct DiscInformation_t *) pvDiscInformation;
  pstTOCheader = pstDiscInformation->pstTOCheader;

  fprintf(stderr, "DAEX: Establishing connection with CDDB server.\n");

  /* Connect to the remote CDDB server. Return with error on error. */
  if ((iSocketFD = fnCDDB_ConnectToServer(szCDDB_RemoteHost, iCDDB_RemotePort)) < 0) {
    fprintf(stderr, "DAEX: Connection failed.\n\n");
    return -1;
  }

  fprintf(stderr, "DAEX: Connection established.\n");

  /* Handle the lookup and retrieval of the disc's title and track information. */
  iReturnValue = fnCDDB_SessionWrapper(iSocketFD, pstDiscInformation, &iTerminationType);
  /* Terminate the session. */
  fnCDDB_SessionTerminate(iSocketFD, iTerminationType);

  /* If there was an error, exit. */
  if (iReturnValue < 0) {
    fprintf(stderr, "\n");
    return -1;
  }

  /* If we're not skipping CDDB derived filenames, store them in the disc 
   * information struct.
   */
  if (!iCDDB_SkipFilenames)
   if (fnCDDB_StoreFilenames(pstDiscInformation) < 0) {
     fprintf(stderr, "\n");
     return -1;
   }

  fprintf(stderr, "\n");
  return 0;
}
