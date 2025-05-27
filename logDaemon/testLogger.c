/*
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.1  2003/08/27 19:45:49  soliday
 * Moved to a subdirectory of SDDSepics.
 *
 * Revision 1.3  2000/05/17 15:33:33  soliday
 * Removed Linux compiler warnings.
 *
 * Revision 1.2  2000/05/17 14:53:03  soliday
 * Removed Solaris compiler warnings.
 *
 * Revision 1.1  1996/10/23 19:46:56  saunders
 * Improved error checking in both client library and server. Added
 * testLogger utility to automatically test basic functionality of client
 * library and server. Greatly reduced stack usage by client library
 * and removed redundant string copying.
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  fprintf(stderr, "%s [-serviceId=<str>]\n", appName);
  fprintf(stderr, "    -serviceId selects the desired logDaemon (defaults if not given)\n");
}

int main(int argc, char *argv[]) {
  int i;
  int status;
  LOGHANDLE h;
  char *tagStringp;
  char tagString[MAX_MESSAGE_SIZE + 256];
  char *tagArray[10];
  char *serviceId = NULL;
  char *sourceIdp;
  char sourceId[MAX_MESSAGE_SIZE + 256];

  /* TEST1: overly long sourceId */
  fprintf(stdout, "TEST1:overly long sourceId\n");
  for (i = 0; i < STDBUF - 4; i++)
    sourceId[i] = 'a';
  sourceId[STDBUF - 4] = '\0';
  tagStringp = "this that other";
  if ((status = logOpen(&h, serviceId, sourceId, tagStringp)) != LOG_TOOBIG) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  /* TEST2: overly long tag list */
  fprintf(stdout, "TEST2:overly long tag list\n");
  sourceIdp = "test2";
  for (i = 0; i < MAX_MESSAGE_SIZE; i++)
    tagString[i] = 'a';
  tagString[MAX_MESSAGE_SIZE] = '\0';
  if ((status = logOpen(&h, serviceId, sourceIdp, tagString)) != LOG_TOOBIG) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  /* TEST3: rejection of invalid sourceId/tagString combination */
  /* First assert a sourceId/tag combination */
  fprintf(stdout, "TEST3:rejection of invalid sourceId/tagString combination\n");
  sourceIdp = "test3";
  tagStringp = "this that other";
  if ((status = logOpen(&h, serviceId, sourceIdp, tagStringp)) != LOG_OK) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  logClose(h);
  tagStringp = "this that";
  if ((status = logOpen(&h, serviceId, sourceIdp, tagStringp)) != LOG_INVALID) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  /* TEST4: overly long value list to logString */
  fprintf(stdout, "TEST4: overly long value list to logString\n");
  sourceIdp = "test4";
  tagStringp = "tag";
  if ((status = logOpen(&h, serviceId, sourceIdp, tagStringp)) != LOG_OK) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  for (i = 0; i < MAX_MESSAGE_SIZE; i++)
    tagString[i] = 'a';
  tagString[MAX_MESSAGE_SIZE] = '\0';
  if ((status = logString(h, tagString)) != LOG_TOOBIG) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  /* TEST5: valid call to logString */
  fprintf(stdout, "TEST5: valid call to logString\n");
  tagString[MAX_MESSAGE_SIZE - 30] = '\0';
  if ((status = logString(h, tagString)) != LOG_OK) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  /* TEST6: overly long value to logArguments */
  fprintf(stdout, "TEST6: overly long value to logArguments\n");
  for (i = 0; i < MAX_MESSAGE_SIZE; i++)
    tagString[i] = 'a';
  tagString[MAX_MESSAGE_SIZE] = '\0';
  if ((status = logArguments(h, tagString, NULL)) != LOG_TOOBIG) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  /* TEST7: valid call to logArguments */
  fprintf(stdout, "TEST7: valid call to logArguments\n");
  tagString[MAX_MESSAGE_SIZE - 30] = '\0';
  if ((status = logArguments(h, tagString, NULL)) != LOG_OK) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  /* TEST8: overly long value to logArray */
  fprintf(stdout, "TEST8: overly long value to logArrray\n");
  tagArray[0] = malloc((MAX_MESSAGE_SIZE + 1) * sizeof(char));
  tagArray[1] = NULL;

  for (i = 0; i < MAX_MESSAGE_SIZE; i++)
    *(tagArray[0] + i) = 'a';
  *(tagArray[0] + MAX_MESSAGE_SIZE) = '\0';
  if ((status = logArray(h, tagArray)) != LOG_TOOBIG) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  /* TEST9: valid call to logArray */
  fprintf(stdout, "TEST9: valid call to logArray\n");
  *(tagArray[0] + MAX_MESSAGE_SIZE - 30) = '\0';
  if ((status = logArray(h, tagArray)) != LOG_OK) {
    fprintf(stderr, "failed\n");
    exit(1);
  }
  fprintf(stdout, "passed\n");

  logClose(h);

  return (0);
}
