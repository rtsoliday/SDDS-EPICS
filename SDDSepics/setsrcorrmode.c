/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: setcorrmode.c
 * purpose: to improve the speed of set corrector mode, 
 * 2019.6   H. Shang
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include "SDDSsrcorr.h"
#include <time.h>
#if !defined(vxWorks)
#  include <sys/timeb.h>
#endif
/*#include <chandata.h>*/
#include <signal.h>
#ifdef _WIN32
#  include <windows.h>
#  include <process.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif
#include <alarm.h>
#if defined(vxWorks)
#  include <taskLib.h>
#  include <taskVarLib.h>
#  include <taskHookLib.h>
#  include <sysLib.h>
#endif

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#if defined(DBAccess)
#  include "dbDefs.h"
#  include "dbAccess.h"
#  include "errlog.h"
#  include "link.h"
#else
#  include <cadef.h>
#endif
#include <epicsVersion.h>

#define CLO_PENDIOTIME 0
#define CLO_PLANE 1
#define CLO_CORRECTOR_TYPE 2
#define CLO_DRY_RUN 3
#define CLO_VERBOSE 4
#define CLO_SETSOURCE 5
#define CLO_UNIFIED 6
#define COMMANDLINE_OPTIONS 7
char *commandline_option[COMMANDLINE_OPTIONS] = {"pendIOtime", "plane", "corrType", "dryRun", "verbose", "setsource", "unified"};

static char *USAGE1 = "setsrcorrectormode \n\
    [-pendIOtime=<seconds>] -plane=<h|v> -corrType=<DP|vector|scalar|dynamic|plain> \n\
    [-dryRun] [-verbose]  [-setsource] [-unified] \n\n\
pendIOTime         optional, wait time for channel access \n\
verbose            print out the message \n\
dryRun             do not change pvs.\n\
palne              required, h or v plane must provided.\n\
corrType           required, DP or vector or scalar or dynamic or plain type must be provided\n\
setsource          if provided, the corrector source mode will be set.\n\
unified            unified corrector or not.\n\
Program by H. Shang.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

int main(int argc, char **argv) {
  SCANNED_ARG *scArg;
  long i_arg, verbose = 0, dryRun = 0, setSourceMode = 0, corrTypeIndex = -1, unified = 0;
  double pendIOTime = 30, startTime;
  char *plane = NULL, *corrTypeInput = NULL, *sourceModeInput = NULL;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scArg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE1);
    exit(1);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scArg[i_arg].arg_type == OPTION) {
      delete_chars(scArg[i_arg].list[0], "_");
      switch (match_string(scArg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (scArg[i_arg].n_items != 2 || sscanf(scArg[i_arg].list[1], "%lf", &pendIOTime) != 1 ||
            pendIOTime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_DRY_RUN:
        dryRun = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_PLANE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -plane option!");
        plane = scArg[i_arg].list[1];
        if (strcmp(plane, "h") != 0 && strcmp(plane, "v") != 0) {
          fprintf(stderr, "invalid plane value %s provided, has to be h or v!\n", plane);
          exit(1);
        }
        break;
      case CLO_CORRECTOR_TYPE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -corrType option!");
        corrTypeInput = scArg[i_arg].list[1];
        if ((corrTypeIndex = match_string(corrTypeInput, corrType, corrTypes, EXACT_MATCH)) < 0) {
          fprintf(stderr, "invalid corrector type value %s provided!\n", corrTypeInput);
          exit(1);
        }
        if (corrTypeIndex != 4)
          sourceModeInput = sourceMode[0];
        else
          sourceModeInput = sourceMode[1];
        break;
      case CLO_SETSOURCE:
        setSourceMode = 1;
        break;
      case CLO_UNIFIED:
        unified = 1;
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", scArg[i_arg].list[0]);
        exit(1);
      }
    } else {
      fprintf(stderr, "invalid file option %s provided\n", scArg[i_arg].list[0]);
      exit(1);
    }
  }
  startTime = getTimeInSecs();

  ca_task_initialize();
  if (!plane || !corrTypeInput)
    SDDS_Bomb("plane and corrector type are required");
  if (setSourceMode) {
    set_source_mode(plane, sourceModeInput, dryRun, pendIOTime);
  }
  set_corr_mode(plane, corrTypeInput, dryRun, pendIOTime, unified);
  ca_task_exit();
  free_scanargs(&scArg, argc);

  if (verbose)
    fprintf(stdout, "at %f seconds done.\n", getTimeInSecs() - startTime);
  return 0;
}
