/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddssnapshot.c
 * purpose: take a snapshot of EPICS PVs for scalar values only
 *
 * Michael Borland, 1995
 $Log: not supported by cvs2svn $
 Revision 1.6  2005/11/08 22:05:04  soliday
 Added support for 64 bit compilers.

 Revision 1.5  2004/09/27 16:19:00  soliday
 Added missing ca_task_exit call.

 Revision 1.4  2004/07/19 17:39:38  soliday
 Updated the usage message to include the epics version string.

 Revision 1.3  2004/07/16 21:24:40  soliday
 Replaced sleep commands with ca_pend_event commands because Epics/Base
 3.14.x has an inactivity timer that was causing disconnects from PVs
 when the log interval time was too large.

 Revision 1.2  2003/11/11 16:40:07  borland
 Fixed some memory leaks.  Also, no longer set the column ControlName twice.

 Revision 1.1  2003/08/27 19:51:20  soliday
 Moved into subdirectory

 Revision 1.17  2002/10/31 15:47:25  soliday
 It now converts the old ezca option into a pendtimeout value.

 Revision 1.16  2002/10/21 16:55:16  soliday
 sddssnapshot no longer uses the ezca library.

 Revision 1.15  2002/08/14 20:00:36  soliday
 Added Open License

 Revision 1.14  2002/07/01 21:12:11  soliday
 Fsync is no longer the default so it is now set after the file is opened.

 Revision 1.13  2001/05/03 19:53:46  soliday
 Standardized the Usage message.

 Revision 1.12  2001/03/12 17:45:03  soliday
 Changed the method that the output file is created so that it does not call
 SDDS_DeleteColumn.

 Revision 1.11  2000/10/16 21:48:04  soliday
 Removed tsDefs.h include statement.

 Revision 1.10  2000/04/20 15:59:08  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.9  2000/04/19 15:52:25  soliday
 Removed some solaris compiler warnings.

 Revision 1.8  2000/03/08 17:14:35  soliday
 Removed compiler warnings under Linux.

 Revision 1.7  1999/09/17 22:13:02  soliday
 This version now works with WIN32

 Revision 1.6  1999/03/04 16:34:02  borland
 Added output of CA error status in CAError column.

 Revision 1.5  1996/11/22 21:00:53  borland
 Removed improper use of sprintf() (code assumed sprintf returned a value
 of char *).

 * Revision 1.4  1995/11/15  21:58:59  borland
 * Changed interrupt handlers to have proper arguments for non-Solaris machines.
 *
 * Revision 1.3  1995/11/15  00:05:29  borland
 * Fixed non-ANSI use of char * return from sprintf().  Added conditional
 * compilation for SOLARIS signal-handling functions.
 *
 * Revision 1.2  1995/11/09  03:22:39  borland
 * Added copyright message to source files.
 *
 * Revision 1.1  1995/09/25  20:15:43  saunders
 * First release of SDDSepics applications.
 *
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include <stdlib.h>
#ifndef _WIN32
#  include <sys/time.h>
#endif
#include <sys/types.h>
#include <time.h>
#include <signal.h>
/*#include <tsDefs.h>*/
#include <cadef.h>

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

#define CLO_PIPE 0
#define CLO_EZCATIMING 1
#define CLO_UNITS 2
#define CLO_NAME 3
#define CLO_SERVERMODE 4
#define CLO_AVERAGE 5
#define CLO_PENDIOTIME 6
#define COMMANDLINE_OPTIONS 7
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "pipe", "ezcatiming", "unitsofdata", "nameofdata", "servermode",
  "average", "pendiotime"};

static char *USAGE = "sddssnapshot [-pipe[=input][,output]] [<input>] [<output>]\n\
[-pendIOtime=<seconds>] [-unitsOfData=<string>] [-nameOfData=<string>]\n\
[-serverMode=<pidFile>] [-average=<number>,<intervalInSec>]\n\n\
Takes a snapshot of EPICS scalar process variables.\n\
Requires the column \"ControlName\" with the process variable names.\n\
For server mode, writes a new file to the given filename whenever\n\
SIGUSR1 is received.  Exits when SIGUSR2 is received.\n\
Program by Michael Borland.\n\
Link date: "__DATE__
                     " "__TIME__
                     ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#ifdef SUNOS4
void snapshotServer();
void serverExit();
#else
void snapshotServer(int sig);
void serverExit(int sig);
#endif

void makeSnapshot(unsigned long mode, double pendIOtime);
void exitIfServerRunning(char *pidFile);

static char **controlName, *inputfile, *outputfile, *nameColumn;
static char *outputName, *outputUnits;
static char *pidFile;
static long averages;
static double interval;

#define MAKESNAP_DIRECT 0x0001UL
#define MAKESNAP_CONNECT 0x0002UL
#define MAKESNAP_HIDE 0x0004UL

int main(int argc, char **argv) {
  long i_arg, pid;
  SCANNED_ARG *s_arg;
  unsigned long pipeFlags;
  long serverMode;
  double pendIOtime = 10.0;
  double timeout;
  long retries;

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  inputfile = outputfile = NULL;
  pipeFlags = 0;
  outputName = "Value";
  outputUnits = NULL;
  serverMode = 0;
  pidFile = NULL;
  averages = 1;
  interval = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          bomb("invalid -pipe syntax", NULL);
        break;
      case CLO_EZCATIMING:
        /* This option is obsolete */
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb("wrong number of items for -ezcaTiming");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &timeout) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%ld", &retries) != 1 ||
            timeout <= 0 || retries < 0)
          bomb("invalid -ezca values", NULL);
        pendIOtime = timeout * (retries + 1);
        break;
      case CLO_NAME:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -nameOfColumn", NULL);
        SDDS_CopyString(&outputName, s_arg[i_arg].list[1]);
        break;
      case CLO_UNITS:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -unitsOfColumn", NULL);
        SDDS_CopyString(&outputUnits, s_arg[i_arg].list[1]);
        break;
      case CLO_SERVERMODE:
        serverMode = 1;
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -serverMode", NULL);
        SDDS_CopyString(&pidFile, s_arg[i_arg].list[1]);
        break;
      case CLO_AVERAGE:
        if (s_arg[i_arg].n_items != 3 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &averages) != 1 || averages < 1 ||
            sscanf(s_arg[i_arg].list[2], "%le", &interval) != 1 || interval < 0)
          bomb("invalid syntax/values for -average", NULL);
        break;
      default:
        bomb("unrecognized option given", NULL);
        break;
      }
    } else {
      if (!inputfile)
        SDDS_CopyString(&inputfile, s_arg[i_arg].list[0]);
      else if (!outputfile)
        SDDS_CopyString(&outputfile, s_arg[i_arg].list[0]);
      else
        bomb("too many filenames given", NULL);
    }
  }
#ifdef _WIN32
  if (serverMode)
    bomb("-serverMode is not available on WIN32", NULL);
#endif
  processFilenames("sddssnapshot", &inputfile, &outputfile, pipeFlags, 0, NULL);

  if (serverMode && (!inputfile || !outputfile))
    bomb("-serverMode is incompatible with -pipe option", NULL);
#ifndef _WIN32
  if (serverMode)
    exitIfServerRunning(pidFile);
#endif
  ca_task_initialize();
  if (serverMode) {
#ifndef _WIN32
    FILE *fp;
    makeSnapshot(MAKESNAP_CONNECT, pendIOtime);
    pid = getpid();
    if (!(fp = fopen(pidFile, "w")))
      bomb("unable to open PID file", NULL);
    fprintf(fp, "%ld\n", pid);
    fclose(fp);
    signal(SIGUSR2, serverExit);
    while (1) {
      signal(SIGUSR1, snapshotServer);
      {
        sigset_t suspend_mask;
        sigemptyset(&suspend_mask);
        sigaddset(&suspend_mask, SIGUSR1);
        sigsuspend(&suspend_mask);
      }
    }
#endif
  } else
    makeSnapshot(MAKESNAP_DIRECT, pendIOtime);
  free_scanargs(&s_arg, argc);
  ca_task_exit();
  return 0;
}

#ifdef SUNOS4
void serverExit()
#else
void serverExit(int sig)
#endif
{
  char s[1024];
  sprintf(s, "rm %s", pidFile);
  system(s);
  exit(0);
}

#ifdef SUNOS4
void snapshotServer()
#else
void snapshotServer(int sig)
#endif
{
  makeSnapshot(MAKESNAP_HIDE, 10.0);
}

void makeSnapshot(unsigned long mode, double pendIOtime) {
  SDDS_DATASET inSet, outSet;
  char *timeStamp;
  time_t timeVal;
  long i, rows, iaverage;
  double *value, *sum, *sum2;
  chid *PVchid = NULL;
  char *tmpfile, s[SDDS_MAXLINE];
  char *caError;
  long pend = 0;
  char **orig_column_name, **orig_parameter_name, **orig_array_name;
  int32_t orig_column_names, orig_parameter_names, orig_array_names;

  value = sum = sum2 = NULL;
  caError = NULL;

  if (mode & MAKESNAP_HIDE) {
    tmpfile = tmalloc(sizeof(*tmpfile) * (strlen(outputfile) + 2));
    sprintf(tmpfile, "%sX", outputfile);
  } else
    tmpfile = NULL;

  if (!SDDS_InitializeInput(&inSet, inputfile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(nameColumn = SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                                     "ControlName", "Device", NULL)))
    bomb("Didn't find ControlName or Device string columns", NULL);

  if (!(mode & MAKESNAP_CONNECT)) {
    if (!SDDS_InitializeOutput(&outSet, inSet.layout.data_mode.mode,
                               inSet.layout.data_mode.lines_per_row, NULL, NULL,
                               tmpfile ? tmpfile : outputfile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    SDDS_EnableFSync(&outSet);
    if (!(orig_column_name = SDDS_GetColumnNames(&inSet, &orig_column_names))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (!(orig_parameter_name = SDDS_GetParameterNames(&inSet, &orig_parameter_names))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (!(orig_array_name = SDDS_GetArrayNames(&inSet, &orig_array_names))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    for (i = 0; i < orig_parameter_names; i++) {
      if (strcmp(orig_parameter_name[i], "TimeStamp") != 0) {
        if (!SDDS_TransferParameterDefinition(&outSet, &inSet, orig_parameter_name[i],
                                              orig_parameter_name[i])) {
          fprintf(stderr, "unable to transfer parameter %s to %s\n",
                  orig_parameter_name[i], orig_parameter_name[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      }
      free(orig_parameter_name[i]);
    }
    free(orig_parameter_name);

    for (i = 0; i < orig_array_names; i++) {
      if (!SDDS_TransferArrayDefinition(&outSet, &inSet, orig_array_name[i], orig_array_name[i])) {
        fprintf(stderr, "unable to transfer array %s to %s\n",
                orig_array_name[i], orig_array_name[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      free(orig_array_name[i]);
    }
    free(orig_array_name);

    for (i = 0; i < orig_column_names; i++) {
      if ((strcmp(orig_column_name[i], outputName) != 0) &&
          (strcmp(orig_column_name[i], "CAError") != 0)) {
        if (!SDDS_TransferColumnDefinition(&outSet, &inSet, orig_column_name[i],
                                           orig_column_name[i])) {
          fprintf(stderr, "unable to transfer column %s to %s\n",
                  orig_column_name[i], orig_column_name[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      }
      free(orig_column_name[i]);
    }
    free(orig_column_name);

    timeVal = time(NULL);
    timeStamp = ctime(&timeVal);
    timeStamp[strlen(timeStamp) - 1] = 0;

    if ((SDDS_DefineColumn(&outSet, outputName, NULL, outputUnits, NULL, NULL,
                           SDDS_DOUBLE, 0) < 0) ||
        (!SDDS_ChangeColumnInformation(&outSet, "name", "ControlName",
                                       SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME,
                                       nameColumn)) ||
        (SDDS_DefineColumn(&outSet, "CAError", NULL, NULL, NULL, NULL, SDDS_CHARACTER, 0) < 0) ||
        (SDDS_DefineParameter(&outSet, "TimeStamp", NULL, NULL, NULL, NULL,
                              SDDS_STRING, timeStamp) < 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (averages > 1) {
      sprintf(s, "%sStDev", outputName);
      if (SDDS_DefineColumn(&outSet, s, NULL,
                            outputUnits, NULL, NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_WriteLayout(&outSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  while (SDDS_ReadPage(&inSet) > 0) {
    rows = SDDS_CountRowsOfInterest(&inSet);
    if (!(mode & MAKESNAP_CONNECT) && !SDDS_CopyPage(&outSet, &inSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (rows) {
      if (!(controlName = SDDS_GetColumn(&inSet, nameColumn)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(sum = (double *)calloc(rows, sizeof(*sum))) ||
          !(value = (double *)calloc(rows, sizeof(*value))) ||
          !(sum2 = (double *)calloc(rows, sizeof(*sum2))) ||
          !(PVchid = (chid *)calloc(rows, sizeof(*PVchid))) ||
          !(caError = (char *)calloc(rows, sizeof(*caError))))
        bomb("allocation failure", NULL);
      for (i = 0; i < rows; i++) {
        sum[i] = sum2[i] = 0;
        caError[i] = 'n';
        PVchid[i] = NULL;
      }
      for (iaverage = 0; iaverage < averages; iaverage++) {
        for (i = 0; i < rows; i++) {
          if (!PVchid[i]) {
            pend = 1;
            if (ca_search(controlName[i], &(PVchid[i])) != ECA_NORMAL) {
              fprintf(stderr, "error: problem doing search for %s\n", controlName[i]);
              exit(1);
            }
          }
        }
        if (pend) {
          if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
            fprintf(stderr, "error: problem doing search for some channels\n");
          }
          pend = 0;
        }
        for (i = 0; i < rows; i++) {
          if (ca_state(PVchid[i]) == cs_conn) {
            if (ca_get(DBR_DOUBLE, PVchid[i], value + i) != ECA_NORMAL) {
              fprintf(stderr, "error: problem reading %s\n", controlName[i]);
              value[i] = 0;
              caError[i] = 'y';
            }
          } else {
            fprintf(stderr, "PV %s is not connected\n", controlName[i]);
            value[i] = 0;
            caError[i] = 'y';
          }
        }
        if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for some channels\n");
        }
        for (i = 0; i < rows; i++) {
          sum[i] += value[i];
          sum2[i] += value[i] * value[i];
        }
        if (mode & MAKESNAP_CONNECT)
          break;
        if ((averages > 1) && (interval > 0)) {
          ca_pend_event(interval);
        }
      }
      if (!(mode & MAKESNAP_CONNECT)) {
        for (i = 0; i < rows; i++) {
          value[i] = sum[i] / averages;
          if (averages > 1) {
            sum[i] = sum2[i] / averages - value[i] * value[i];
            if (sum[i] < 0)
              sum[i] = 0;
            sum[i] = sqrt(sum[i]) * ((double)averages) / ((double)averages - 1);
          }
        }
        sprintf(s, "%sStDev", outputName);
        if (!SDDS_SetColumn(&outSet, SDDS_BY_NAME, value, rows, outputName) ||
            !SDDS_SetColumn(&outSet, SDDS_BY_NAME, caError, rows, "CAError") ||
            (averages > 1 &&
             !SDDS_SetColumn(&outSet, SDDS_BY_NAME, sum, rows, s)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      for (i = 0; i < rows; i++)
        free(controlName[i]);
      free(controlName);
      free(value);
      free(sum);
      free(sum2);
      free(PVchid);
      free(caError);
    }
    if (!(mode & MAKESNAP_CONNECT) && !SDDS_WritePage(&outSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&inSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!(mode & MAKESNAP_CONNECT)) {
    if (!SDDS_Terminate(&outSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (tmpfile) {
      char s[1024];
      if (fexists(outputfile))
        sprintf(s, "rm %s ; mv %s %s", outputfile, tmpfile, outputfile);
      else
        sprintf(s, "mv %s %s", tmpfile, outputfile);
      system(s);
    }
  }
}

void exitIfServerRunning(char *pidFile) {
  FILE *fp;
  int pid;
  char s[1024];

  if (!(fp = fopen(pidFile, "r"))) {
    fprintf(stderr, "No server file found--starting new server\n");
    return;
  }
  pid = -1;
#ifdef _WIN32
  if (fscanf(fp, "%d", &pid) != 1) {
#else
  if (fscanf(fp, "%d", &pid) != 1 || getpgid(pid) == -1) {
#endif
#ifdef DEBUG
    fprintf(stderr, "Old server pid was %ld\n", pid);
    fprintf(stderr, "Removing old server PID file\n");
#endif
    sprintf(s, "rm %s", pidFile);
    system(s);
#ifdef DEBUG
    fprintf(stderr, "Starting new server\n");
#endif
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Server %s already running\n", pidFile);
#endif
  exit(0);
}
