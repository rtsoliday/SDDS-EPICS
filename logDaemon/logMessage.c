/*
 * 
 * $Log: not supported by cvs2svn $
 * Revision 1.2  2003/08/28 15:34:01  soliday
 * Removed some needless include statements.
 *
 * Revision 1.1  2003/08/27 19:45:48  soliday
 * Moved to a subdirectory of SDDSepics.
 *
 * Revision 1.10  2003/07/19 01:46:41  borland
 * Builds with epics base R3.14.  Still builds with earlier versions, also.
 *
 * Revision 1.9  2000/05/17 15:32:59  soliday
 * Removed Linux compiler warnings.
 *
 * Revision 1.8  2000/05/17 14:54:24  soliday
 * Removed Solaris compiler warnings.
 *
 * Revision 1.7  1996/10/23 19:46:54  saunders
 * Improved error checking in both client library and server. Added
 * testLogger utility to automatically test basic functionality of client
 * library and server. Greatly reduced stack usage by client library
 * and removed redundant string copying.
 * */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <signal.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <Debug.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <ellLib.h>

#include "logDaemonLib.h"

typedef struct {
  ELLNODE nodePad;
  char value[STDBUF];
} STRNODE;

void usage(char *appName) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s [-serviceId=<str>] -sourceId=<str> -tag=<str> <str>\n", appName);
  fprintf(stderr, "    -serviceId selects the desired logDaemon (defaults if not given)\n");
  fprintf(stderr, "    -sourceId identifies source of logging\n");
  fprintf(stderr, "    -tag=<str> <str>  specified tag name and supplies the value\n");
  fprintf(stderr, "    Note: serviceId, sourceId, and tag names may not have spaces in them.\n");
  fprintf(stderr, "    Tag values may have spaces if quoted \"like this\".\n\n");
}

int main(int argc, char *argv[]) {
  int i;
  int status;
  LOGHANDLE h;
  ELLLIST tagList;
  char tagString[MAX_MESSAGE_SIZE];
  ELLLIST valList;
  char valString[MAX_MESSAGE_SIZE];
  STRNODE *strNode;
  char *p;
  char *serviceId = NULL;
  char *sourceId = NULL;

  if (argc < 4) {
    fprintf(stderr, "%s: insufficient arguments\n", argv[0]);
    usage(argv[0]);
    exit(1);
  }
  ellInit(&tagList);
  ellInit(&valList);

  for (i = 1; i < argc; i++) {
    if (*(argv[i]) != '-') {
      strNode = (STRNODE *)malloc(sizeof(STRNODE));
      strcpy(strNode->value, argv[i]);
      ellAdd(&(valList), (ELLNODE *)strNode);

    } else if (!strncmp(argv[i], "-serviceId=", 11)) {
      p = argv[i] + 11;
      if (strlen(p) == 0) {
        fprintf(stderr, "%s: serviceId string is empty\n", argv[0]);
        usage(argv[0]);
        exit(1);
      }
      serviceId = p;

    } else if (!strncmp(argv[i], "-sourceId=", 10)) {
      p = argv[i] + 10;
      if (strlen(p) == 0) {
        fprintf(stderr, "%s: sourceId string is empty\n", argv[0]);
        usage(argv[0]);
        exit(1);
      }
      sourceId = p;

    } else if (!strncmp(argv[i], "-tag=", 5)) {
      p = argv[i] + 5;
      if (strlen(p) == 0) {
        fprintf(stderr, "%s: tag name string is empty\n", argv[0]);
        usage(argv[0]);
        exit(1);
      }
      strNode = (STRNODE *)malloc(sizeof(STRNODE));
      strcpy(strNode->value, p);
      ellAdd(&(tagList), (ELLNODE *)strNode);
    }
  }

  if (sourceId == NULL) {
    fprintf(stderr, "%s: you must supply sourceId\n", argv[0]);
    usage(argv[0]);
    exit(1);
  }
  if (ellCount(&(tagList)) != ellCount(&(valList))) {
    fprintf(stderr, "you must supply one value for each -tag specified\n");
    usage(argv[0]);
    exit(1);
  }

#ifdef SOLARIS
#  ifdef _X86_
  /* This is ment to be temporary until logMessage is fixed on solaris-x86 */
  exit(0);
#  endif
#endif

  strNode = (STRNODE *)ellFirst(&(tagList));
  tagString[0] = '\0';
  while (strNode != NULL) {
    strcat(tagString, strNode->value);
    strcat(tagString, " ");
    strNode = (STRNODE *)ellNext((ELLNODE *)strNode);
  }

  if ((status = logOpen(&h, serviceId, sourceId, tagString)) != LOG_OK) {
    if (status == LOG_INVALID)
      fprintf(stderr, "%s: sourceId/tagList combination is invalid\n", argv[0]);
    else if (status == LOG_TOOBIG)
      fprintf(stderr, "%s: string too long\n", argv[0]);
    else
      fprintf(stderr, "%s: unable to open connection to logDaemon\n", argv[0]);
    exit(1);
  }

  strNode = (STRNODE *)ellFirst(&(valList));
  valString[0] = '\0';
  while (strNode != NULL) {
    strcat(valString, "\"");
    strcat(valString, strNode->value);
    strcat(valString, "\"");
    strcat(valString, " ");
    strNode = (STRNODE *)ellNext((ELLNODE *)strNode);
  }

  if ((status = logString(h, valString)) != LOG_OK) {
    if (status == LOG_INVALID)
      fprintf(stderr, "%s: tag values invalid\n", argv[0]);
    else if (status == LOG_TOOBIG)
      fprintf(stderr, "%s: string too long\n", argv[0]);
    else
      fprintf(stderr, "%s: unable to log message with logDaemon\n", argv[0]);
    exit(1);
  }

  logClose(h);

  ellFree(&(tagList));
  ellFree(&(valList));

  return (0);
}
