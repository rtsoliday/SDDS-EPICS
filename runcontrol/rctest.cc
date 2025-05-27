/*************************************************************************
 * FILE:      rctest.c
 * Author:    Claude Saunders
 * 
 * Purpose:   Application to test out function of runControl library
 *            and runcontrol record support.
 *************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cadef.h>
#if defined(_WIN32)
#  include <windows.h>
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif
#include <signal.h>

#include <epicsVersion.h>
#include <libruncontrol.h>

void exitHandler(int sig);

char handle[255];
RUNCONTROL_INFO rcInfo;

int main(int argc, char *argv[]) {
  char pv[255];
  int status;
  strcpy(pv, "APS:RunControlFixed0RC");
  if (argc < 2) {
    fprintf(stderr, "usage: %s <description-string>\n\n", argv[0]);
    exit(1);
  }

  status = runControlInit(pv, argv[1], 8000.0, handle, &rcInfo, 10.0);
  if (status != RUNCONTROL_OK) {
    fprintf(stderr, "ERROR initializing run control\n");
    exit(1);
  }
  signal(SIGINT, exitHandler);

  while (1) {
    status = runControlPing(handle, &rcInfo);
    switch (status) {
    case RUNCONTROL_ABORT:
      fprintf(stderr, "Application aborted\n");
      runControlExit(handle, &rcInfo);
      return (1);
    case RUNCONTROL_TIMEOUT:
      fprintf(stderr, "Application timed out\n");
      return (1);
    case RUNCONTROL_OK:
      sleep(5);
      status = runControlLogMessage(handle, (char *)"I'm up", NO_ALARM, &rcInfo);
      if (status != RUNCONTROL_OK) {
        fprintf(stderr, "Unable to log message, exiting\n");
        return (1);
      }
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Error during ping operation\n");
      return (1);
    default:
      fprintf(stderr, "Unknown error code\n");
      return (1);
    }
  }
}

void exitHandler(int sig) {
  runControlExit(handle, &rcInfo);
  exit(1);
}
