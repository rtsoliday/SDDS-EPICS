/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program toggle.c
 *
 $Log: not supported by cvs2svn $
 Revision 1.9  2011/02/15 17:42:33  lemery
 Added information on -prompt in usage message.

 Revision 1.8  2010/03/11 15:51:26  shang
 added -singleshot option, and made the -singleshot and -prompt options to work in the same way as sddswmonitor.

 Revision 1.7  2006/04/04 21:44:18  soliday
 Xuesong Jiao modified the error messages if one or more PVs don't connect.
 It will now print out all the non-connecting PV names.

 Revision 1.6  2005/11/08 22:05:05  soliday
 Added support for 64 bit compilers.

 Revision 1.5  2004/09/10 14:37:57  soliday
 Changed the flag for oag_ca_pend_event to a volatile variable

 Revision 1.4  2004/07/23 16:27:42  soliday
 Improved signal support when using Epics/Base 3.14.6

 Revision 1.3  2004/07/19 17:39:39  soliday
 Updated the usage message to include the epics version string.

 Revision 1.2  2004/07/16 21:24:41  soliday
 Replaced sleep commands with ca_pend_event commands because Epics/Base
 3.14.x has an inactivity timer that was causing disconnects from PVs
 when the log interval time was too large.

 Revision 1.1  2003/08/27 19:51:30  soliday
 Moved into subdirectory

 Revision 1.14  2002/11/26 22:07:10  soliday
 ezca has been 100% removed from all SDDS Epics programs.

 Revision 1.13  2002/08/14 20:00:38  soliday
 Added Open License

 Revision 1.12  2001/05/03 19:53:49  soliday
 Standardized the Usage message.

 Revision 1.11  2000/04/20 16:00:12  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.10  2000/04/19 15:53:47  soliday
 Removed some solaris compiler warnings.

 Revision 1.9  2000/03/08 17:15:37  soliday
 Removed compiler warnings under Linux.

 Revision 1.8  1999/09/17 22:14:06  soliday
 This version now works with WIN32

 Revision 1.7  1999/08/26 21:02:29  borland
 Added -offset option to sddsfeedforward.
 Modified sddsstatmon.c and toggle.c to accommodate placement of
 ../oagca/pvMultiList.h in SDDSepics.h in early revisions.

 Revision 1.6  1999/04/15 13:27:54  borland
 Removed use of the device library.

 Revision 1.5  1998/04/15 23:22:44  emery
 Fixed formatting of time variables in fprintf statements.

 * Revision 1.4  1996/05/26  22:34:57  emery
 * Changed ftime calls to SDDSepics.c time routines.
 * Fixed location of the endGroup command to fix bug.
 *
 * Revision 1.3  1996/05/23  04:13:34  emery
 * Added missing break statement in the finalset parsing option.
 * Put some print statements inside a if (verbose) block.
 *
 * Revision 1.2  1996/05/08  01:00:10  emery
 * Changed old ca library calls to ezca calls. Changed the finalSet option
 * syntax.
 *
 * Revision 1.1  1996/05/05  14:06:12  borland
 * First version (by L. Emery).
 *
 *
 * toggles between two sets of PV values.
 * The time intervals are specified in the command line.
 * The sets are read from burt sdds file (snapshot file).
 * If one set is specified, then the other set
 * is constructed from current values.
 * One may specify PV 's explicitly on the command line.
 *
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"
#include "SDDSepics.h"
/*#include "SDDSepics.h"*/
#include <time.h>
#include <sys/timeb.h>
#include <cadef.h>
#include <epicsVersion.h>
/*#include <chandata.h>*/
#include <signal.h>

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

void interrupt_handler(int sig);
void dummy_interrupt_handler(int);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

void getTimeBreakdown(double *Time, double *Day, double *Hour, double *JulianDay,
                      double *Year, double *Month, char **TimeStamp);
double getTimeInSecs(void);

#define CLO_INTERVAL 0
#define CLO_CYCLES 1
#define CLO_CONTROL_NAME 2
#define CLO_VERBOSE 3
#define CLO_WARNING 4
#define CLO_FINAL_SET 5
#define CLO_PROMPT_FOR_TOGGLE 6
#define CLO_EZCATIMING 7
#define CLO_PENDIOTIME 8
#define CLO_SINGLE_SHOT 9
#define COMMANDLINE_OPTIONS 10
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "interval",
  "cycles",
  "controlname",
  "verbose",
  "warning",
  "finalSet",
  "prompt",
  "ezcaTiming",
  "pendiotime",
  "singleShot",
};

#define FINALSET_ORIGINAL 0
#define FINALSET_FIRST 1
#define FINALSET_SECOND 2
#define FINALSET_OPTIONS 3
static char *finalSetOption[FINALSET_OPTIONS] = {
  "original",
  "first",
  "second"};

static char *USAGE = "toggle [<snapshotfile1> [<snapshotfile2>]\n\
   [-controlName=<PVname>,<value1>[,<value2>]...]\n\
   [-interval=<interval1>[,<interval2>]] [-cycles=<number>]\n\
   [-finalSet={original|first|second}]\n\
   [-pendIOtime=<seconds>]\n\
   [-prompt] \n\
   [-singleShot{=noprompt|stdout}][,resend]] [-verbose] [-warning]\n\n\
Alternate between two sets of EPICS PV (process variable) values stored in \n\
snapshot files. Set 1 is in effect for a duration of \"<interval1>\" seconds,\n\
and set 2 is in effect for \"<interval2>\" seconds. Additional process variables and\n\
their values may be specified explicitly on the command line.\n\
<snapshotfile1>   snapshot SDDS file for PV values of set 1.\n\
<snapshotfile2>   snapshot SDDS file for PV values of set 2.\n\
                  Unexpected results occur if the PV's in the\n\
                  files don't match.\n\
                  If only one file is specified on the command line,\n\
                  The original values make up set 2.\n\
controlname       defines a PV name and the values to be alternated.\n\
                  If only one value is given, then the second value\n\
                  is given by the current value.\n\
interval          interval1 and interval2 are the durations of\n\
                  PV value sets 1 and 2, respectively.\n\
                  If interval2 isn't present, then interval2=interval1.\n\
                  If option isn't present, then the default is 1 seconds each.\n\
cycles            number of cycles. Default is 1.\n\
finalSet          specifies which PV value set to apply at normal termination.\n\
                  During abnormal termination, the PVs are\n\
                  returned to their original values.\n\
prompt            toggle values only on a <CR> key press. Same as singleShot with no suboptions for backward compatibility.\n\
singleShot        single shot set initiated by a <cr> key press; time_interval is disabled.\n\
                  By default, a prompt is issued to stderr.  The qualifiers may be used to\n\
                  remove the prompt or have it go to stdout. If \"resend\" is given, the value will be sent twice with 1 second interval.\n\
verbose           prints extra information.\n\
warning           prints warning messages.\n\
Program by Louis Emery, ANL\n\
Concept augmentation by M. Borland, ANL\n\
Link date: "__DATE__
                     " "__TIME__
                     ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#define DEFAULT_CYCLES 1

typedef union {
  double numerical;
  char *string;
} PV_VALUE_UNION;

typedef struct
{
#define PV_VALUE_STRING 0
#define PV_VALUE_NUMERICAL 1
  int type;
  PV_VALUE_UNION value;
} PV_STUFF;

typedef struct
{
  char *PV_name;
  PV_STUFF *set1;
  PV_STUFF *set2;
} CONTROL_NAME_DEFINITION;

#define SS_PROMPT 1
#define SS_NOPROMPT 2
#define SS_STDOUTPROMPT 4
#define ANSWER_LENGTH 256

/* these variable are taken out of the main routine so that the
   interrupt handler can reset the PV's to their
   initial values.
*/
char **Snap1ControlName, **Snap2ControlName;
chid *Snap1CHID, *Snap2CHID;
PV_STUFF *InitialValue;
long Snap1Rows, Snap2Rows;
long controlNames;
volatile int sigint = 0;

void LinkDevices(char **device_name, chid **PVchid, long number, double pendIOtime);
long ReadDevices(char **device_name, chid **PVchid,
                 PV_STUFF *value, long number, double pendIOtime);
long SetDevices(char **device_name, chid **PVchid,
                PV_STUFF *value, long number, double pendIOtime, long resend);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE Snapshot1Table, Snapshot2Table;

  char *SnapshotFile1, *SnapshotFile2;
  /*char **Snap1ColumnNames, **Snap2ColumnNames; */
  char **Snap1ValueString, **Snap2ValueString;
  PV_STUFF *Snap1Value, *Snap2Value;

  /*int32_t columns; */
  CONTROL_NAME_DEFINITION *controlNameDefs;
  double Interval1, Interval2;
  long cycles;
  double pendIOtime = 10.0;
  double timeout;
  long retries;

  long i, i_arg;
  double startTime, elapsedTime, epochTime;
  long verbose, warning;
  long finalSet;
  char *finalSetString;
  long singleShot = 0, resend = 0;
  char ans[ANSWER_LENGTH];

  Snap1Value = Snap2Value = NULL;
  controlNameDefs = NULL;
  finalSetString = NULL;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

#ifdef EPICS313
  ca_task_initialize();
#else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
#endif
  signal(SIGINT, sigint_interrupt_handler);
  signal(SIGTERM, sigint_interrupt_handler);
  signal(SIGILL, interrupt_handler);
  signal(SIGABRT, interrupt_handler);
  signal(SIGFPE, interrupt_handler);
  signal(SIGSEGV, interrupt_handler);
#ifndef _WIN32
  signal(SIGHUP, interrupt_handler);
  signal(SIGQUIT, sigint_interrupt_handler);
  signal(SIGTRAP, interrupt_handler);
  signal(SIGBUS, interrupt_handler);
#endif
  atexit(rc_interrupt_handler);

  SnapshotFile1 = SnapshotFile2 = NULL;
  Interval1 = Interval2 = 1;
  cycles = DEFAULT_CYCLES;
  verbose = 0;
  warning = 0;
  controlNames = 0;
  finalSet = FINALSET_ORIGINAL;
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, UNIQUE_MATCH)) {
      case CLO_CONTROL_NAME:
        if (s_arg[i_arg].n_items < 3 || s_arg[i_arg].n_items > 4)
          bomb("Invalid syntax for option -value", USAGE);
        if (!controlNames) {
          controlNameDefs = (CONTROL_NAME_DEFINITION *)malloc(sizeof(CONTROL_NAME_DEFINITION));
        } else {
          controlNameDefs = (CONTROL_NAME_DEFINITION *)
            realloc(controlNameDefs, (controlNames + 1) * sizeof(CONTROL_NAME_DEFINITION));
        }
        controlNameDefs[controlNames].set1 = NULL;
        controlNameDefs[controlNames].set2 = NULL;
        controlNameDefs[controlNames].PV_name = s_arg[i_arg].list[1];
        if (s_arg[i_arg].n_items == 3) {
          controlNameDefs[controlNames].set1 = (PV_STUFF *)malloc(sizeof(PV_STUFF));
          if (get_double(&controlNameDefs[controlNames].set1->value.numerical, s_arg[i_arg].list[2])) {
            controlNameDefs[controlNames].set1->type = PV_VALUE_NUMERICAL;
          } else {
            controlNameDefs[controlNames].set1->type = PV_VALUE_STRING;
            controlNameDefs[controlNames].set1->value.string = s_arg[i_arg].list[2];
          }
        } else if (s_arg[i_arg].n_items == 4) {
          controlNameDefs[controlNames].set1 = (PV_STUFF *)malloc(sizeof(PV_STUFF));
          controlNameDefs[controlNames].set2 = (PV_STUFF *)malloc(sizeof(PV_STUFF));
          if (get_double(&controlNameDefs[controlNames].set1->value.numerical, s_arg[i_arg].list[2])) {
            controlNameDefs[controlNames].set1->type = PV_VALUE_NUMERICAL;
          } else {
            controlNameDefs[controlNames].set1->type = PV_VALUE_STRING;
            controlNameDefs[controlNames].set1->value.string = s_arg[i_arg].list[2];
          }
          if (get_double(&controlNameDefs[controlNames].set2->value.numerical, s_arg[i_arg].list[3])) {
            controlNameDefs[controlNames].set2->type = PV_VALUE_NUMERICAL;
          } else {
            controlNameDefs[controlNames].set2->type = PV_VALUE_STRING;
            controlNameDefs[controlNames].set2->value.string = s_arg[i_arg].list[3];
          }
        }
        controlNames++;
        break;
      case CLO_INTERVAL:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 3)
          bomb("Invalid syntax for option -value", USAGE);
        if (!(get_double(&Interval1, s_arg[i_arg].list[1])))
          bomb("First item in option -interval not a number", USAGE);
        if (s_arg[i_arg].n_items == 3) {
          if (!(get_double(&Interval2, s_arg[i_arg].list[2])))
            bomb("Second item in option -interval not a number", USAGE);
        } else
          Interval2 = Interval1;
        break;
      case CLO_CYCLES:
        if (!(get_long(&cycles, s_arg[i_arg].list[1])))
          bomb("no number given for option -cycles", USAGE);
        break;
      case CLO_FINAL_SET:
        if (s_arg[i_arg].n_items != 2 ||
            !(finalSetString = s_arg[i_arg].list[1]))
          bomb("no value given for option -finalSet", NULL);
        switch (finalSet = match_string(finalSetString, finalSetOption,
                                        FINALSET_OPTIONS, 0)) {
        case FINALSET_ORIGINAL:
        case FINALSET_FIRST:
        case FINALSET_SECOND:
          break;
        default:
          printf("unrecognized value given for option %s\n", s_arg[i_arg].list[0]);
          exit(1);
        }
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_PROMPT_FOR_TOGGLE:
        singleShot = SS_PROMPT;
        break;
      case CLO_SINGLE_SHOT:
        singleShot = SS_PROMPT;
        if (s_arg[i_arg].n_items >= 2) {
          if (strncmp(s_arg[i_arg].list[1], "noprompt", strlen(s_arg[i_arg].list[1])) == 0)
            singleShot = SS_NOPROMPT;
          else if (strncmp(s_arg[i_arg].list[1], "stdout", strlen(s_arg[i_arg].list[1])) == 0)
            singleShot = SS_STDOUTPROMPT;
          else if (strncmp(s_arg[i_arg].list[1], "resend", strlen(s_arg[i_arg].list[1])) == 0)
            resend = 1;
          else
            SDDS_Bomb("invalid -singleShot qualifier");
          if (s_arg[i_arg].n_items > 2) {
            if (strncmp(s_arg[i_arg].list[2], "resend", strlen(s_arg[i_arg].list[2])) == 0)
              resend = 1;
          }
        }
        break;
      case CLO_WARNING:
        warning = 1;
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
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      default:
        bomb("unrecognized option given", USAGE);
      }
    } else {
      if (!SnapshotFile1)
        SnapshotFile1 = s_arg[i_arg].list[0];
      else if (!SnapshotFile2)
        SnapshotFile2 = s_arg[i_arg].list[0];
      else
        bomb("too many filenames given", USAGE);
    }
  }
  if (verbose)
    fprintf(stderr, "Durations of set 1 and 2, respectively: %f %f.\n", Interval1, Interval2);
  /* If any snapshot files are defined */
  if (SnapshotFile1) {
    if (!SDDS_InitializeInput(&Snapshot1Table, SnapshotFile1))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    /*Snap1ColumnNames=(char**)SDDS_GetColumnNames(&Snapshot1Table, &columns); */
    switch (SDDS_CheckColumn(&Snapshot1Table, "ControlName", NULL, SDDS_STRING, stdout)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", "ControlName", SnapshotFile1);
      exit(1);
    }
    switch (SDDS_CheckColumn(&Snapshot1Table, "ValueString", NULL, SDDS_STRING, stdout)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", "ValueString", SnapshotFile1);
      exit(1);
    }
  }

  if (SnapshotFile2) {
    if (!SDDS_InitializeInput(&Snapshot2Table, SnapshotFile2))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    /* Snap2ColumnNames=(char**)SDDS_GetColumnNames(&Snapshot2Table, &columns); */
    /* check for columns ControlName, RestoreMsg, BackupMsg, ValueString */
    switch (SDDS_CheckColumn(&Snapshot2Table, "ControlName", NULL, SDDS_STRING, stdout)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", "ControlName", SnapshotFile2);
      exit(1);
    }
    switch (SDDS_CheckColumn(&Snapshot2Table, "ValueString", NULL, SDDS_STRING, stdout)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", "ValueString", SnapshotFile2);
      exit(1);
    }
  }

  if (SnapshotFile1) {
    if (SDDS_ReadTable(&Snapshot1Table) < 0)
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    Snap1Rows = SDDS_CountRowsOfInterest(&Snapshot1Table);
    if (Snap2Rows == Snap1Rows && warning)
      fprintf(stderr, "Number of rows in snapshot files aren't equal.\n");
    Snap1ControlName = (char **)SDDS_GetColumn(&Snapshot1Table, "ControlName");
    Snap1CHID = (chid *)malloc(sizeof(chid) * Snap1Rows);
    for (i = 0; i < Snap1Rows; i++)
      Snap1CHID[i] = NULL;
    Snap1ValueString = (char **)SDDS_GetColumn(&Snapshot1Table, "ValueString");
    SDDS_Terminate(&Snapshot1Table);
    Snap1Value = (PV_STUFF *)malloc(Snap1Rows * sizeof(PV_STUFF));
    for (i = 0; i < Snap1Rows; i++) {
      if (get_double(&Snap1Value[i].value.numerical, Snap1ValueString[i])) {
        Snap1Value[i].type = PV_VALUE_NUMERICAL;
      } else {
        Snap1Value[i].value.string = Snap1ValueString[i];
        Snap1Value[i].type = PV_VALUE_STRING;
      }
    }
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (SnapshotFile2) {
    if (SDDS_ReadTable(&Snapshot2Table) < 0)
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    Snap2Rows = SDDS_CountRowsOfInterest(&Snapshot2Table);
    Snap2ControlName = (char **)SDDS_GetColumn(&Snapshot2Table, "ControlName");
    Snap2CHID = (chid *)malloc(sizeof(chid) * Snap2Rows);
    for (i = 0; i < Snap2Rows; i++)
      Snap2CHID[i] = NULL;
    Snap2ValueString = (char **)SDDS_GetColumn(&Snapshot2Table, "ValueString");
    SDDS_Terminate(&Snapshot2Table);
    Snap2Value = (PV_STUFF *)malloc(Snap2Rows * sizeof(PV_STUFF));
    for (i = 0; i < Snap2Rows; i++) {
      if (get_double(&Snap2Value[i].value.numerical, Snap2ValueString[i])) {
        Snap2Value[i].type = PV_VALUE_NUMERICAL;
      } else {
        Snap2Value[i].value.string = Snap2ValueString[i];
        Snap2Value[i].type = PV_VALUE_STRING;
      }
    }
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (SnapshotFile1 && !SnapshotFile2) {
    Snap2Rows = Snap1Rows;
    Snap2ControlName = Snap1ControlName;
    Snap2CHID = Snap1CHID;
  }

  /* add on command-line-specified PV values at the end of snap file definitions, if any */
  if (controlNames) {
    if (!SnapshotFile2 && !SnapshotFile1) {
      /* if all PV's come from command line, then character arrays for sets 1 and 2 
             are the same */
      Snap1ControlName = (char **)malloc(controlNames * sizeof(char *));
      Snap1CHID = (chid *)malloc(sizeof(chid) * controlNames);
      Snap2ControlName = Snap1ControlName;
      Snap2CHID = Snap1CHID;
      for (i = 0; i < controlNames; i++) {
        Snap1ControlName[i] = controlNameDefs[i].PV_name;
        Snap1CHID[i] = NULL;
      }
    } else if (SnapshotFile1 && !SnapshotFile2) {
      /* When only one file is specified on the command line, then the characters array
             for sets 1 and 2 are also identical */
      Snap1ControlName = (char **)realloc(Snap1ControlName, (Snap1Rows + controlNames) * sizeof(char *));
      Snap1CHID = (chid *)realloc(Snap1CHID, sizeof(chid) * (Snap1Rows + controlNames));
      Snap2ControlName = Snap1ControlName;
      Snap2CHID = Snap1CHID;
      for (i = 0; i < controlNames; i++) {
        Snap1ControlName[Snap1Rows + i] = controlNameDefs[i].PV_name;
        Snap1CHID[Snap1Rows + i] = NULL;
      }
    } else {
      /* two file are specified in the command line, and the character arrays for set 1 and 2
             aren't guaranteed to be the same */
      Snap1ControlName = (char **)realloc(Snap1ControlName, (Snap1Rows + controlNames) * sizeof(char *));
      Snap1CHID = (chid *)realloc(Snap1CHID, sizeof(chid) * (Snap1Rows + controlNames));
      Snap2ControlName = (char **)realloc(Snap2ControlName, (Snap2Rows + controlNames) * sizeof(char *));
      Snap2CHID = (chid *)realloc(Snap2CHID, sizeof(chid) * (Snap2Rows + controlNames));
      for (i = 0; i < controlNames; i++) {
        Snap1ControlName[Snap1Rows + i] = controlNameDefs[i].PV_name;
        Snap1CHID[Snap1Rows + i] = NULL;
        Snap2ControlName[Snap2Rows + i] = controlNameDefs[i].PV_name;
        Snap2CHID[Snap2Rows + i] = NULL;
      }
    }
  }

  LinkDevices(Snap1ControlName, &Snap1CHID, (Snap1Rows + controlNames), pendIOtime);
  LinkDevices(Snap2ControlName, &Snap2CHID, (Snap2Rows + controlNames), pendIOtime);

  /* initial values are read for PV's corresponding to snapshot file 1 plus PV's in the command line */
  InitialValue = (PV_STUFF *)malloc((Snap1Rows + controlNames) * sizeof(PV_STUFF));
  ReadDevices(Snap1ControlName, &Snap1CHID,
              InitialValue, (Snap1Rows + controlNames), pendIOtime);
  if (verbose) {
    for (i = 0; i < (Snap1Rows + controlNames); i++) {
      fprintf(stderr, "%s: InitialValue[i].type: %d  ", Snap1ControlName[i], InitialValue[i].type);
      if (InitialValue[i].type == PV_VALUE_NUMERICAL)
        fprintf(stderr, "InitialValue[i].value.numerical: %f\n", InitialValue[i].value.numerical);
      else if (InitialValue[i].type == PV_VALUE_STRING)
        fprintf(stderr, "InitialValue[i].value.string: %s\n", InitialValue[i].value.string);
    }
  }

  /* prepare arrays of PV_STUFF for set 1 and 2 */
  if (controlNames && !SnapshotFile1 && !SnapshotFile2) {
    Snap1Value = (PV_STUFF *)malloc(controlNames * sizeof(PV_STUFF));
    Snap2Value = (PV_STUFF *)malloc(controlNames * sizeof(PV_STUFF));
  } else if (controlNames) {
    Snap1Value = (PV_STUFF *)realloc(Snap1Value, (Snap1Rows + controlNames) * sizeof(PV_STUFF));
    Snap2Value = (PV_STUFF *)realloc(Snap2Value, (Snap2Rows + controlNames) * sizeof(PV_STUFF));
  } else if (!SnapshotFile2) {
    Snap2Value = (PV_STUFF *)realloc(Snap2Value, Snap2Rows * sizeof(PV_STUFF));
  }
  if (SnapshotFile1 && !SnapshotFile2)
    for (i = 0; i < Snap1Rows; i++)
      Snap2Value[i] = InitialValue[i];

  if (controlNames) {
    for (i = 0; i < controlNames; i++) {
      if (!controlNameDefs[i].set1) {
        bomb("Problem with allocation of structure PV_STUFF. Contact author.\n", NULL);
      }
      Snap1Value[Snap1Rows + i] = *controlNameDefs[i].set1;
      if (controlNameDefs[i].set2)
        Snap2Value[Snap2Rows + i] = *controlNameDefs[i].set2;
      else
        Snap2Value[Snap2Rows + i] = InitialValue[Snap2Rows + i];
    }
  }

  if (verbose) {
    for (i = 0; i < (Snap1Rows + controlNames); i++) {
      fprintf(stderr, "%s: Snap1Value[i].type: %d  ", Snap1ControlName[i], Snap1Value[i].type);
      if (Snap1Value[i].type == PV_VALUE_NUMERICAL)
        fprintf(stderr, "Snap1Value[i].value.numerical: %f\n", Snap1Value[i].value.numerical);
      else if (Snap1Value[i].type == PV_VALUE_STRING)
        fprintf(stderr, "Snap1Value[i].value.string: %s\n", Snap1Value[i].value.string);
    }

    for (i = 0; i < (Snap2Rows + controlNames); i++) {
      fprintf(stderr, "%s: Snap2Value[i].type: %d  ", Snap2ControlName[i], Snap2Value[i].type);
      if (Snap2Value[i].type == PV_VALUE_NUMERICAL)
        fprintf(stderr, "Snap2Value[i].value.numerical: %f\n", Snap2Value[i].value.numerical);
      else if (Snap2Value[i].type == PV_VALUE_STRING)
        fprintf(stderr, "Snap2Value[i].value.string: %s\n", Snap2Value[i].value.string);
    }
  }

  getTimeBreakdown(&startTime, NULL, NULL, NULL, NULL, NULL, NULL);

  for (i = 1; i <= cycles; i++) {
    if (sigint)
      return (1);
    if (singleShot) {
      if (singleShot != SS_NOPROMPT) {
        fputs("Type <cr> to set set #1 pv, q to quit:\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
        fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
      }
      fgets(ans, ANSWER_LENGTH, stdin);
      if (ans[0] == 'q' || ans[0] == 'Q') {
        return (1);
      }
    }
    oag_ca_pend_event(0, &sigint);
    if (sigint)
      return (1);
    if (verbose) {
      elapsedTime = (epochTime = getTimeInSecs()) - startTime;
      fprintf(stderr, "Setting device(s) to first set of values at %f seconds.\n", elapsedTime);
    }
    SetDevices(Snap1ControlName, &Snap1CHID, Snap1Value, (Snap1Rows + controlNames), pendIOtime, resend);
    if (!singleShot) {
      oag_ca_pend_event(Interval1, &sigint);
    }

    if (sigint)
      return (1);
    if (singleShot) {
      if (singleShot != SS_NOPROMPT) {
        fputs("Type <cr> to set set #2 pv, q to quit:\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
        fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
      }
      fgets(ans, ANSWER_LENGTH, stdin);
      if (ans[0] == 'q' || ans[0] == 'Q') {
        return (1);
      }
    }
    oag_ca_pend_event(0, &sigint);
    if (sigint)
      return (1);
    if (verbose) {
      elapsedTime = (getTimeInSecs()) - startTime;
      fprintf(stderr, "Setting device(s) to second set of values at %f seconds.\n", elapsedTime);
    }
    SetDevices(Snap2ControlName, &Snap2CHID, Snap2Value, (Snap2Rows + controlNames), pendIOtime, resend);
    if (!singleShot) {
      oag_ca_pend_event(Interval2, &sigint);
    }
  }

  if (verbose) {
    elapsedTime = getTimeInSecs() - startTime;
    fprintf(stderr, "Setting device(s) to set of final values at %f seconds.\n", elapsedTime);
  }
  switch (finalSet) {
  case FINALSET_ORIGINAL:
    SetDevices(Snap1ControlName, &Snap1CHID, InitialValue, (Snap1Rows + controlNames), pendIOtime, 0);
    break;
  case FINALSET_FIRST:
    SetDevices(Snap1ControlName, &Snap1CHID, Snap1Value, (Snap1Rows + controlNames), pendIOtime, 0);
    break;
  case FINALSET_SECOND:
    SetDevices(Snap2ControlName, &Snap2CHID, Snap2Value, (Snap2Rows + controlNames), pendIOtime, 0);
    break;
  }

  free_scanargs(&s_arg, argc);
  return 0;
}

void LinkDevices(char **deviceName, chid **PVchid, long n, double pendIOtime) {
  int i, pend = 0;
#if defined(NO_EPICS)
  return;
#endif
  for (i = 0; i < n; i++) {
    if (!(*PVchid)[i]) {
      pend = 1;
      if (ca_search(deviceName[i], &((*PVchid)[i])) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", deviceName[i]);
        exit(1);
      }
    }
  }
  if (pend) {
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      for (i = 0; i < n; i++) {
        if (ca_state((*PVchid)[i]) != cs_conn)
          fprintf(stderr, "%s not connected\n", deviceName[i]);
      }
      exit(1);
    }
  }
}

long ReadDevices(char **deviceName, chid **PVchid, PV_STUFF *value, long n, double pendIOtime) {
  long i, caErrors, groupStarted;

  caErrors = groupStarted = 0;
  if (!deviceName || !n)
    return 0;
  if (!value) {
    fprintf(stderr, "Error: array \"value\" in function ReadDevices unallocated.\n");
    exit(1);
  }

  for (i = 0; i < n; i++) {
#if !defined(NO_EPICS)
    value[i].type = PV_VALUE_NUMERICAL;
    if (ca_state((*PVchid)[i]) == cs_conn) {
      if (ca_get(DBR_DOUBLE, (*PVchid)[i], &value[i].value.numerical) != ECA_NORMAL) {
        value[i].value.numerical = 0;
        caErrors++;
      }
    } else {
      value[i].value.numerical = 0;
      caErrors++;
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL)
    return n;
#endif
  return caErrors;
}

long SetDevices(char **deviceName, chid **PVchid, PV_STUFF *value, long n, double pendIOtime, long resend) {
  long i, caErrors, groupStarted, loops = 1, j;

  caErrors = groupStarted = 0;
  if (!deviceName || !n)
    return 0;
  if (!value) {
    fprintf(stderr, "Error: array \"value\" in function SetDevices unallocated.\n");
    exit(1);
  }
  if (resend)
    loops = 2;
  for (j = 0; j < loops; j++) {
    for (i = 0; i < n; i++) {
#if !defined(NO_EPICS)
      if (ca_state((*PVchid)[i]) == cs_conn) {
        if (value[i].type == PV_VALUE_NUMERICAL) {
          if (ca_put(DBR_DOUBLE, (*PVchid)[i], &value[i].value.numerical) != ECA_NORMAL) {
            fprintf(stderr, "Error: unable to put value for %s.\n", deviceName[i]);
            caErrors++;
          }
        } else if (value[i].type == PV_VALUE_STRING) {
          if (ca_put(DBR_STRING, (*PVchid)[i], &value[i].value.string) != ECA_NORMAL) {
            fprintf(stderr, "Error: unable to put value for %s.\n", deviceName[i]);
            caErrors++;
          }
        }
      } else {
        fprintf(stderr, "Error: unable to put value for %s.\n", deviceName[i]);
        caErrors++;
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL)
      return n;
    if (j) {
      /*sleep 1 second, resend the values */
      usleep(1.0e6);
    }
  }
#endif
  return caErrors;
}

void interrupt_handler(int sig) {
  exit(1);
}

void dummy_interrupt_handler(int sig) {
}

void sigint_interrupt_handler(int sig) {
  sigint = 1;
  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, interrupt_handler);
#endif
}

void rc_interrupt_handler() {
  int ca = 1;
  signal(SIGINT, dummy_interrupt_handler);
  signal(SIGTERM, dummy_interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, dummy_interrupt_handler);
#endif
#ifndef EPICS313
  if (ca_current_context() == NULL)
    ca = 0;
  if (ca)
    ca_attach_context(ca_current_context());
#endif
  if (ca) {
    SetDevices(Snap1ControlName, &Snap1CHID,
               InitialValue, (Snap1Rows + controlNames), 10.0, 0);
    ca_task_exit();
  }
}
