/*
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.12  2003/07/18 22:37:22  borland
 * Now works on Linux.  Simply ignored the new port number returned by
 * BSgetAddressPort, without understanding what was going on.
 *
 * Revision 1.11  2000/05/17 16:12:20  soliday
 * Changed how empty strings are handled.
 *
 * Revision 1.10  2000/05/17 15:41:48  soliday
 * Fixed cvs log.
 *
 * Revision 1.9  2000/05/17 15:40:57  soliday
 * Removed compiler warnings from Solaris and Linux. Also modified logString
 * to work with empty strings.
 *
 * Revision 1.8  1996/10/23 19:46:52  saunders
 * Improved error checking in both client library and server. Added
 * testLogger utility to automatically test basic functionality of client
 * library and server. Greatly reduced stack usage by client library
 * and removed redundant string copying.
 *
 * Revision 1.7  1996/10/22 21:35:02  saunders
 * A number of improvements resulting from design review.
 * 1. Don't stop config file matching on first match. Log the message
 *    to all valid matches.
 *
 * 2. In logDaemonValidateSourceId:
 *    Don't allow a subset of tagList to be considered valid.
 *
 * 3. Make LOG_ENT one big buffer of size MAX_UDP_SIZE, and make secs,
 *    usecs, and tag name/value pairs an array of ptrs. This allows
 *    tag values to use almost all of UDP packet if desired.
 *
 * 4. Add logArray() to library.
 *
 * Revision 1.6  1996/03/27 21:21:47  saunders
 * V2.0 of logDaemon ready and tested.
 *
 * Revision 1.5  1996/03/26 00:46:54  saunders
 * Upgrade to arbitrary tag fields complete except for cleaning up
 * comments, and format of email message.
 *
 * Revision 1.4  1996/03/25 20:09:59  saunders
 * Changed API to permit arbitrary tagged values.
 *
 * Revision 1.3  1996/03/24 21:48:00  saunders
 * Added logMessage acknowledgement. Changed time output to one double field.
 *
 * Revision 1.2  1996/03/21 22:55:38  saunders
 * Close to completion of logDaemon upgrade (tagged message fields).
 *
 * Revision 1.1.1.1  1995/12/13 22:05:47  saunders
 * Imported Files
 * */
/*************************************************************************
 * FILE:      logDaemonLib.c
 * Author:    Claude Saunders and Jim Kowalkowski
 * 
 * Purpose:   Client library for logDaemon. Calls provided for opening
 *            a connection with a log server, logging messages, and
 *            closing the connection.
 *
 *************************************************************************/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#ifdef vxWorks
#  include <vxWorks.h>
#  include <timers.h>
#else
#  include <memory.h>
#  include <sys/time.h>
#endif

#undef DEBUG
#include "Debug.h"
#include "logDaemonLib.h"

extern int errno;
/*extern char *sys_errlist[];*/

/*************************************************************************
 * FUNCTION : logOpen()
 * PURPOSE  : Opens a connection to the log server specified by serviceId,
 *            or to a default log server if serviceId is a NULL ptr. User
 *            must supply a pre-allocated LOGHANDLE, which logOpen will
 *            initialize. This handle is then used by the other library
 *            routines. The sourceId is an arbitrary id you wish to
 *            give to all your log activity. Typically this is used to
 *            identify a general class of user, such as IOC, or SDDS.
 *            The tagList is a space delimited set of field names, or
 *            "tags". On all subsequent calls, you supply a value for
 *            each tag. The resultant log file has a column for each tag.
 *
 * ARGS in  : handle - ptr to pre-allocated LOGHANDLE struct
 *            sourceId - arbitrary string up to 254 chars long (no spaces)
 *            serviceId - arbitrary server id string up to 254 chars long 
 *                        (no spaces)
 *            tagList - string of space delimited tag names, ie.
 *                      "System SubSystem Severity Message". Each
 *                      tag name may be no longer than 254 chars.
 *
 * ARGS out : handle - initialized by routine
 * GLOBAL   : nothing
 * RETURNS  : LOG_OK       - connection opened, sourceId and tagList valid
 *            LOG_TOOBIG   - one of the strings supplied is too long
 *            LOG_ERROR    - unable to open connection to logDaemon
 *            LOG_INVALID  - logDaemon rejected given sourceId and tagList
 ************************************************************************/
int logOpen(LOGHANDLE *handle, char *serviceId, char *sourceId,
            char *tagList) {
  char *logger_port, *logger_host;
  int log_port = DEF_LOGPORT;
  char log_service_id[STDBUF];
  char log_host[STDBUF];
  int bcastsockfd;
  BSDATA o_info, i_info;
  char omsg[MAX_MESSAGE_SIZE];
  char imsg[STDBUF];
  int i;

  Debug("entering logOpen\n", 0);

  /* Build up valid log service id. Use default if NULL */
  if (serviceId == NULL) {
    if (strlen(DEF_LOGSERVER_ID) >= (size_t)(STDBUF))
      return (LOG_TOOBIG);
    strcpy(log_service_id, DEF_LOGSERVER_ID);
  } else {
    if (strlen(serviceId) >= (size_t)(STDBUF))
      return (LOG_TOOBIG);
    strcpy(log_service_id, serviceId);
  }
  Debug("log service id is %s\n", log_service_id);
  strcpy(omsg, "*");
  strcat(omsg, log_service_id);

  /* Validate the sourceId. Use default if NULL */
  if (sourceId == NULL) {
    if (strlen(DEF_SOURCE_ID) >= (size_t)STDBUF - 4)
      return (LOG_TOOBIG);
    strcpy(handle->sourceId, DEF_SOURCE_ID);
  } else {
    if (strlen(sourceId) >= (size_t)STDBUF - 4)
      return (LOG_TOOBIG);
    strcpy(handle->sourceId, sourceId);
  }
  Debug("sourceId is %s\n", handle->sourceId);
  strcat(omsg, FIELD_DELIMITER);
  strcat(omsg, handle->sourceId);

  /* Get environment variables if present, otherwise broadcast for server */
  if ((logger_host = (char *)getenv(LOGGER_HOST)) != (char *)NULL) {
    if ((logger_port = (char *)getenv(LOGGER_PORT)) != (char *)NULL) {
      log_port = atoi(logger_port);
      Debug("log server port found from env var: %s\n", log_port);
    }
    strcpy(log_host, logger_host);
    Debug("log server host found from env var: %s\n", log_host);

  } else {
    if ((logger_port = (char *)getenv(LOGGER_PORT)) != (char *)NULL) {
      log_port = atoi(logger_port);
      Debug("log server bcast port found from env var: %d\n", log_port);
    }

    /* Add tag list to outgoing broadcast packet, changing ' ' to '~'  */
    i = 0;
    while (tagList[i] != '\0') {
      if (tagList[i] == ' ')
        tagList[i] = '~';
      i++;
    }
    strcat(omsg, FIELD_DELIMITER);
    if (((int)strlen(omsg) + (int)strlen(tagList) + 1) >= MAX_MESSAGE_SIZE)
      return (LOG_TOOBIG);
    strcat(omsg, tagList);

    Debug("broadcasting for log server, port: %d\n", log_port);
    Debug("broadcast msg: %s\n", omsg);
    if ((bcastsockfd = BSipBroadcastOpen(&o_info, log_port)) == -1)
      return (LOG_ERROR);

    if (BSbroadcastTrans(bcastsockfd, 3, &o_info, &i_info, omsg,
                         strlen(omsg), &imsg, STDBUF) != strlen(DEF_BCAST_ACK))
      return (LOG_ERROR);

    if (!strncmp(imsg, DEF_BCAST_ACK, strlen(DEF_BCAST_ACK))) {
      int log_port1;
      BSgetAddressPort(&i_info, log_host, &log_port1);
#if !defined(linux) && !defined(Linux) && !defined(LINUX)
      log_port = log_port1;
#endif
    } else
      return (LOG_INVALID);
    Debug("log server found by broadcast, port: %d\n", log_port);
    Debug("log server found by broadcast, host: %s\n", log_host);
  }

  if ((handle->sockfd = BSopenListenerUDP(0)) == -1)
    return (LOG_ERROR);

  if ((BSsetAddressPort(&(handle->bsData), log_host, log_port)) == -1)
    return (LOG_ERROR);

  return (LOG_OK);
}

/*************************************************************************
 * FUNCTION : logString()
 * PURPOSE  : Sends a log message to the log server.
 *            The message is time stamped for you by getting local time on 
 *            this system. The remaining data is supplied by valueList. This
 *            is a space delimited string of values, the order of which
 *            must correspond to the tagList given in the logOpen() call.
 *            If a given value has internal spaces, you must use double
 *            quotes to surround it. Ie. "val1 \"val with space\" val3"
 *
 * ARGS in  : handle - LOGHANDLE struct initialized by logOpen() call
 *            valueList - space delimited string of tag values
 *
 * ARGS out : none
 * GLOBAL   : sends out UDP packet
 * RETURNS  : LOG_OK       - message logged
 *            LOG_TOOBIG   - the valueList string supplied is too long
 *            LOG_ERROR    - unable to send message to logDaemon
 ************************************************************************/
int logString(LOGHANDLE handle, char *valueList) {
#ifdef vxWorks
  struct timespec t;
#else
  struct timeval t;
#endif
  char time_buf[STDBUF];
  int buflen, openDelimiter, i, j, k = -2;
  char len[10];
  char sock_buffer[MAX_UDP_SIZE]; /* header plus message */
  char imsg[STDBUF];
  BSDATA i_info;

  Debug("Entering logString\n", 0);
  Debug("Source id is %s\n", handle.sourceId);

#ifdef vxWorks
  clock_gettime(CLOCK_REALTIME, &t);
  snprintf(time_buf, sizeof(time_buf), "%ld~%ld", t.tv_sec, (long)(t.tv_nsec / 1000));
#else
  gettimeofday(&t, NULL);
  snprintf(time_buf, sizeof(time_buf), "%ld~%ld", t.tv_sec, (long)t.tv_usec);
#endif

  /* This byte differentiates log message from a broadcast for server */
  sock_buffer[0] = '\b';

  {
    size_t prefix_len = snprintf(sock_buffer + 5, sizeof(sock_buffer) - 5, "~%s~", time_buf);
    /* append sourceId and trailing '~' safely */
    snprintf(sock_buffer + 5 + prefix_len,
             sizeof(sock_buffer) - 5 - prefix_len,
             "%s~", handle.sourceId);
  }

  if ((strlen(&(sock_buffer[5])) + strlen(valueList) + 2) >= MAX_MESSAGE_SIZE)
    return (LOG_TOOBIG);

  /* Copy valueList to field_buffer, changing delimiter from ' ' to '~' */
  openDelimiter = 0;
  for (i = 0, j = strlen(&(sock_buffer[5])) + 5; valueList[i] != '\0'; i++) {
    if (valueList[i] == ' ' && !openDelimiter) {
      sock_buffer[j++] = '~';
    } else if (valueList[i] == '"' && !openDelimiter) {
      openDelimiter = 1;
      k = i;
    } else if (valueList[i] == '"' && openDelimiter) {
      openDelimiter = 0;
      if (k + 1 == i) {
        sock_buffer[j++] = ' ';
        sock_buffer[j++] = '\b';
      }
    } else {
      sock_buffer[j++] = valueList[i];
    }
  }
  sock_buffer[j] = '\0';

  /* Header byte plus 4 byte message length plus contents */
  buflen = 1 + 4 + strlen(&(sock_buffer[5]));
  Debug("length of message = %d\n", buflen);
  if (buflen >= MAX_UDP_SIZE)
    return (LOG_TOOBIG);

  snprintf(len, sizeof(len), "%4.04d", buflen);
  memcpy(sock_buffer + 1, len, 4);

  Debug("sock_buffer=(%s)\n", &(sock_buffer[1]));

  if (BSbroadcastTrans(handle.sockfd, 3, &(handle.bsData), &i_info,
                       sock_buffer, buflen, &imsg, STDBUF) != strlen(DEF_LOGMSG_ACK))
    return (LOG_ERROR);

  Debug("wrote %d bytes \n", buflen);
  return (LOG_OK);
}

/*************************************************************************
 * FUNCTION : logArguments()
 * PURPOSE  : Sends a log message to the log server.
 *            The message is time stamped for you by getting local time on 
 *            this system. The remaining data is supplied by a series of
 *            (char *) arguments. Each such argument supplies a value for
 *            one tag given in the tagList of the logOpen() call. The 
 *            order of arguments must correspond to the order of tags given
 *            in the logOpen() call. The last argument must be NULL.
 *            Ie. logArguments(h, "val1", "val with space", "val3", NULL);
 *
 * ARGS in  : handle - LOGHANDLE struct initialized by logOpen() call
 *            ...    - NULL terminated series of (char *) arguments.
 *
 * ARGS out : none
 * GLOBAL   : sends out UDP packet
 * RETURNS  : LOG_OK       - message logged
 *            LOG_TOOBIG   - one of the strings supplied is too long
 *            LOG_ERROR    - unable to log message
 ************************************************************************/
int logArguments(LOGHANDLE handle, ...) {
  va_list ap;
#ifdef vxWorks
  struct timespec t;
#else
  struct timeval t;
#endif
  char time_buf[STDBUF];
  int buflen;
  char len[10];
  char *tagValue;
  char sock_buffer[MAX_UDP_SIZE]; /* header plus message */
  char imsg[STDBUF];
  BSDATA i_info;
  size_t sockIndex, tagValueLen;

  Debug("Entering logArguments\n", 0);
  Debug("Source id is %s\n", handle.sourceId);

#ifdef vxWorks
  clock_gettime(CLOCK_REALTIME, &t);
  snprintf(time_buf, sizeof(time_buf), "%ld~%ld", t.tv_sec, (long)(t.tv_nsec / 1000));
#else
  gettimeofday(&t, NULL);
  snprintf(time_buf, sizeof(time_buf), "%ld~%ld", t.tv_sec, (long)t.tv_usec);
#endif

  /* This byte differentiates log message from a broadcast for server */
  sock_buffer[0] = '\b';

  {
    size_t prefix_len = snprintf(sock_buffer + 5, sizeof(sock_buffer) - 5, "~%s~", time_buf);
    /* append sourceId safely */
    snprintf(sock_buffer + 5 + prefix_len,
             sizeof(sock_buffer) - 5 - prefix_len,
             "%s", handle.sourceId);
  }

  sockIndex = strlen(&(sock_buffer[5])) + 5;

  /* Get tag values from arguments. */
  va_start(ap, handle);
  while ((tagValue = va_arg(ap, char *)) != NULL) {
    tagValueLen = strlen(tagValue) + 1;
    if ((sockIndex + tagValueLen - 5) >= MAX_MESSAGE_SIZE) {
      return (LOG_TOOBIG);
    }
    strcat(&(sock_buffer[sockIndex]), FIELD_DELIMITER);
    strcat(&(sock_buffer[sockIndex + 1]), tagValue);
    sockIndex += tagValueLen;
  }
  va_end(ap);

  /* Header byte plus 4 byte message length plus contents */
  buflen = 1 + 4 + strlen(&(sock_buffer[5]));
  Debug("length of message = %d\n", buflen);
  if (buflen >= MAX_UDP_SIZE)
    return (LOG_TOOBIG);

  snprintf(len, sizeof(len), "%4.04d", buflen);
  memcpy(sock_buffer + 1, len, 4);

  Debug("sock_buffer=(%s)\n", &(sock_buffer[1]));

  if (BSbroadcastTrans(handle.sockfd, 3, &(handle.bsData), &i_info,
                       sock_buffer, buflen, &imsg, STDBUF) != strlen(DEF_LOGMSG_ACK))
    return (LOG_ERROR);

  Debug("wrote %d bytes \n", buflen);
  return (LOG_OK);
}

/*************************************************************************
 * FUNCTION : logArray()
 * PURPOSE  : Sends a log message to the log server.
 *            The message is time stamped for you by getting local time on 
 *            this system. The remaining data is supplied by an array of
 *            (char *) values. Each element supplies a value for
 *            one tag given in the tagList of the logOpen() call. The 
 *            order of elements must correspond to the order of tags given
 *            in the logOpen() call. The last element must be NULL pointer.
 *            Ie. val[0] = "this"; val[1] = "that"; val[2] = NULL;
 *                logArray(h, val);
 *
 * ARGS in  : handle - LOGHANDLE struct initialized by logOpen() call
 *            ...    - array of (char *) with last pointer NULL
 *
 * ARGS out : none
 * GLOBAL   : sends out UDP packet
 * RETURNS  : LOG_OK       - message logged
 *            LOG_TOOBIG   - one of the strings supplied is too long
 *            LOG_ERROR    - unable to log message
 ************************************************************************/
int logArray(LOGHANDLE handle, char *valueArray[]) {
#ifdef vxWorks
  struct timespec t;
#else
  struct timeval t;
#endif
  char time_buf[STDBUF];
  int buflen, i;
  char len[10];
  char sock_buffer[MAX_UDP_SIZE]; /* header plus message */
  char imsg[STDBUF];
  BSDATA i_info;
  size_t sockIndex, tagValueLen;

  Debug("Entering logArray\n", 0);
  Debug("Source id is %s\n", handle.sourceId);

#ifdef vxWorks
  clock_gettime(CLOCK_REALTIME, &t);
  snprintf(time_buf, sizeof(time_buf), "%ld~%ld", t.tv_sec, (long)(t.tv_nsec / 1000));
#else
  gettimeofday(&t, NULL);
  snprintf(time_buf, sizeof(time_buf), "%ld~%ld", t.tv_sec, (long)t.tv_usec);
#endif

  /* This byte differentiates log message from a broadcast for server */
  sock_buffer[0] = '\b';

  {
    size_t prefix_len = snprintf(sock_buffer + 5, sizeof(sock_buffer) - 5, "~%s~", time_buf);
    /* append sourceId safely */
    snprintf(sock_buffer + 5 + prefix_len,
             sizeof(sock_buffer) - 5 - prefix_len,
             "%s", handle.sourceId);
  }

  sockIndex = strlen(&(sock_buffer[5])) + 5;
  i = 0;

  /* Get tag values from arguments. */
  while (valueArray[i] != NULL) {
    tagValueLen = strlen(valueArray[i]) + 1;
    if ((sockIndex + tagValueLen - 5) >= MAX_MESSAGE_SIZE) {
      return (LOG_TOOBIG);
    }
    strcat(&(sock_buffer[sockIndex]), FIELD_DELIMITER);
    strcat(&(sock_buffer[sockIndex + 1]), valueArray[i]);
    sockIndex += tagValueLen;
    i++;
  }

  /* Header byte plus 4 byte message length plus contents */
  buflen = 1 + 4 + strlen(&(sock_buffer[5]));
  Debug("length of message = %d\n", buflen);
  if (buflen >= MAX_UDP_SIZE)
    return (LOG_TOOBIG);

  snprintf(len, sizeof(len), "%4.04d", buflen);
  memcpy(sock_buffer + 1, len, 4);

  Debug("sock_buffer=(%s)\n", &(sock_buffer[1]));

  if (BSbroadcastTrans(handle.sockfd, 3, &(handle.bsData), &i_info,
                       sock_buffer, buflen, &imsg, STDBUF) != strlen(DEF_LOGMSG_ACK))
    return (LOG_ERROR);

  Debug("wrote %d bytes \n", buflen);
  return (LOG_OK);
}

/*************************************************************************
 * FUNCTION : logClose()
 * PURPOSE  : Close connection opened by logOpen(). You are free to
 *            re-use your LOGHANDLE after this.
 * ARGS in  : handle - LOGHANDLE struct initialized by logOpen() call
 * ARGS out : nothing
 * GLOBAL   : closes fd associated with connection
 * RETURNS  : LOG_OK
 ************************************************************************/
int logClose(LOGHANDLE handle) {
  close(handle.sockfd);
  return (LOG_OK);
}
