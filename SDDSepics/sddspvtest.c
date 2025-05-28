/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *
 *  $Log: not supported by cvs2svn $
 *  Revision 1.12  2008/03/07 22:11:06  shang
 *  removed the long type variable interval in runControlPingWhileSleeop() because its value
 *  is set to 0 if the loop interval or runcontrol ping interval is less than 1 seconds and this will cause
 *  the runcontrol ping without sleeping. Remove it because it is not required, Or change its tyoe
 *   to double will solve the problem also.
 *
 *  Revision 1.11  2007/09/27 15:08:23  shang
 *  added logdaemon
 *
 *  Revision 1.10  2005/11/08 22:34:39  soliday
 *  Updated to remove compiler warnings on Linux.
 *
 *  Revision 1.9  2005/05/31 13:06:18  shang
 *  the runcontrol ping interval now is at least 1.0 seconds to reduce the CPU
 *  usage.
 *
 *  Revision 1.8  2005/04/21 19:28:24  soliday
 *  Added the ability to use CA monitoring instead of the CA gets every iteration.
 *  This can be done for all PVs using the -monitor option. Or it can be setup
 *  to only monitor selected PVs by creating a Monitor column with Y or N for values.
 *  This should reduce network traffic and CPU time.
 *
 *  Revision 1.7  2004/09/10 14:37:56  soliday
 *  Changed the flag for oag_ca_pend_event to a volatile variable
 *
 *  Revision 1.6  2004/07/22 22:05:19  soliday
 *  Improved signal support when using Epics/Base 3.14.6
 *
 *  Revision 1.5  2004/07/19 17:39:38  soliday
 *  Updated the usage message to include the epics version string.
 *
 *  Revision 1.4  2004/07/16 21:24:40  soliday
 *  Replaced sleep commands with ca_pend_event commands because Epics/Base
 *  3.14.x has an inactivity timer that was causing disconnects from PVs
 *  when the log interval time was too large.
 *
 *  Revision 1.3  2004/06/15 22:23:20  soliday
 *  Catch problems with calls to ca_pend_io now.
 *
 *  Revision 1.2  2004/06/11 18:26:39  soliday
 *  Added missing break statement.
 *
 *  Revision 1.1  2003/08/27 19:51:19  soliday
 *  Moved into subdirectory
 *
 *  Revision 1.15  2003/03/04 19:13:11  soliday
 *  Changed the default pend time to 10 seconds.
 *
 *  Revision 1.14  2002/11/27 16:22:06  soliday
 *  Fixed issues on Windows when built without runcontrol
 *
 *  Revision 1.13  2002/11/19 22:33:05  soliday
 *  Altered to work with the newer version of runcontrol.
 *
 *  Revision 1.12  2002/11/05 15:49:19  soliday
 *  Fixed a problem with runcontrol
 *
 *  Revision 1.11  2002/11/04 20:48:25  soliday
 *  There were issues running runcontrol because it is still using ezca calls.
 *
 *  Revision 1.10  2002/08/14 20:00:35  soliday
 *  Added Open License
 *
 *  Revision 1.9  2002/04/19 19:02:44  shang
 *  added printout of time message
 *
 *  Revision 1.8  2002/04/19 15:15:37  soliday
 *  Removed memory leaks.
 *
 *  Revision 1.7  2002/04/18 22:25:33  soliday
 *  Added support for vxWorks. I still need to check for memory leaks.
 *
 *  Revision 1.6  2001/10/30 21:20:54  shang
 *  modified printout message when tests fail
 *
 *  Revision 1.5  2001/10/28 05:13:31  shang
 *  added -verbose option to print out the test-failed pv names.
 *
 *  Revision 1.4  2001/10/27 00:42:42  shang
 *  added -pendIOtime option
 *
 *  Revision 1.3  2001/09/10 19:35:07  soliday
 *  Fixed Linux compiler warnings.
 *
 *
 */

/*sddspvtest.c */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include <stdlib.h>
#include <time.h>
#include <cadef.h>
#include "SDDSepics.h"
#include <alarm.h>
#include <signal.h>
#if defined(vxWorks)
#  include <taskLib.h>
#  include <taskVarLib.h>
#  include <taskHookLib.h>
#endif

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#ifdef USE_LOGDAEMON
#  include <logDaemonLib.h>
#endif

void sddspvtest_interrupt_handler(int sig);
void sddspvtest_sigint_interrupt_handler(int sig);

#ifdef _WIN32
#  include <windows.h>
#  include <process.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

#define NTimeUnitNames 4

#define CLO_PVOUTPUT 0
#define CLO_TIME 1
#define CLO_INTERVAL 2
#define CLO_RUNCONTROLPV 3
#define CLO_RUNCONTROLDESC 4
#define CLO_PENDIOTIME 5
#define CLO_VERBOSE 6
#define CLO_MONITOR 7
#define CLO_EXIT_CONDITION 8
#define COMMANDLINE_OPTIONS 9

#ifdef USE_RUNCONTROL
typedef struct
{
  char *PVparam, *DescParam;
  char *PV, *Desc;
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status;
#  if !defined(vxWorks)
  RUNCONTROL_INFO rcInfo;
#  endif
} RUNCONTROL_PARAM;
#endif
int runControlPingWhileSleep(double sleepTime);

long getInputInfo(char ***ControlName, double **MaximumValue, double **MinimumValue, 
                  char **Monitor, double **ChangeLimit, long *pvNumber, char *inputFile);
void EventHandler(struct event_handler_args event);

#if defined(vxWorks)
void sddsPVtestDeleteHook(WIND_TCB *pTcb);
#else
void sddsPVtestDeleteHook();
#endif

typedef struct
{
#ifdef USE_RUNCONTROL
  RUNCONTROL_PARAM rcParam;
#endif
#ifdef USE_LOGDAEMON
  LOGHANDLE logHandle;
  long useLogDaemon;
#endif
  char **ControlName;
  double *MaximumValue;
  double *MinimumValue;
  double *ChangeLimit;
  double *CurrentValue;
  double *ReferenceValue;
  int *MonitorFlag;
  short *Updated;
  char *Monitor;
  chid *channelID;
  long pvNumber;
  char *inputFile, *pvOutput;
  int argc;
  volatile int sigint;
  char **argv;
  long *usrValue;
} SDDSPVTEST_GLOBAL;
SDDSPVTEST_GLOBAL *sddspvtestGlobal;

#define EXIT_IF_ALL_PASS  0x01UL
#define EXIT_IF_ANY_FAIL  0x02UL
#define EXIT_IF_CONVERGED 0x04UL
#define EXIT_IF_CHANGE    0x08UL

#if defined(vxWorks)
int sddsIocPVtest(char *name, char *options)
#else
int main(int argc, char **argv)
#endif
{
#if defined(vxWorks)
  int argc;
  char **argv = NULL;
  char *buffer = NULL;
#endif
  SCANNED_ARG *s_arg;
  double totalTime = -1, interval = 5, quitingTime, pingTime;
  long result, previousResult, i_arg, optionCode, TimeUnits, i, j, verbose = 0, monitor = 0;
  long somethingChanged;
  unsigned long exitConditionFlags = 0; 
  short setOutput = 0, outputValuesGiven = 0;
  chid outputID;
  long outputPassValue = 1, outputFailValue = 0, outputValue = 0;
  double pendIOtime = 10.0;
  int allMonitoring = 1;
  uint64_t cycleCount, nValid = 0;
  uint32_t noChangeCount = 0, convergenceCycles = 3;
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
  char errorMsg[2048];
#  endif
#endif

#ifdef USE_RUNCONTROL
  unsigned long dummyFlags;
#endif
  char *TimeUnitNames[NTimeUnitNames] = {
    "seconds",
    "minutes",
    "hours",
    "days",
  };
  double TimeUnitFactor[NTimeUnitNames] = {
    1, 60, 3600, 86400};
  char *commandline_option[COMMANDLINE_OPTIONS] = {
    "pvOutput",
    "time",
    "interval",
    "runControlPV",
    "runControlDescription",
    "pendiotime",
    "verbose",
    "monitor",
    "exitcondition",
  };
#if !defined(vxWorks)
  char *USAGE = "sddspvtest <inputFile> [-pvOutput=<pvName>[,passValue=<integer>,failValue=<integer>]] \n\
  [-time=<timeToRun>,<timeUnits>] [-interval=<timeInterval>,<timeUnits>] \n\
  [-monitor] \n\
  [-exitcondition={allPass|anyFail|converged=<cycles>|changed}]\n\
  [-runControlPV={string=<string>|parameter=<string>},pingTimeout=<value>,\n\
  pingInterval=<value>] \n\
  [-runControlDescription={string=<string>|parameter=<string>}] \n\
  [-pendIOtime=<value>] [-verbose]\n\
<inputFile>        the name of the input file, which contains ControlName,\n\
                   MaximumValue and MinimumValue columns. The Monitor and ChangeLimit columns\n\
                   are optional. If the Monitor values are Y or y it will use CA monitoring\n\
                   instead of getting the values every iteration; the values\n\
                   will still only be checked every iteration. ChangeLimit values are\n\
                   used with the -exit=change and -exit=nochange options.\n\
pvOutput           the output pv name for storing the testing results of PVs \n\
                   in the input. By default, the output value is the number of failed tests.\n\
time               Total time for testing process variable.\n\
                   Valid time units are seconds,minutes,hours, or days. \n\
pendIOtime         sets the maximum time to wait for return of each value.\n\
interval           desired time interval for testing, the units are seconds,\n\
                   minutes,hours, or days. \n\
exit               Program will exit if all tests pass, any test fails, values converged (stop\n\
                   changing), or values change (from first value). The <cycles> value for converged is\n\
                   the number of cycles over which no change is seen.\n\
monitor            All PVs will be monitored regardless of the values in the\n\
                   Monitor column.\n\
verbose            print out messages. \n\
runControlPV       specifies the runControl PV record.\n\
runControlDescription\n\
                   specifies a string parameter whose value is a runControl PV \n\
                   description record.\n\n\
Program by Hairong Shang, ANL\n\
Link date: "__DATE__
                " "__TIME__
                ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";
#  ifndef USE_RUNCONTROL
  char *USAGE_WARNING = "** note ** Program is not compiled with run control.\n";
#  else
  char *USAGE_WARNING = "";
#  endif
#endif

  signal(SIGINT, sddspvtest_sigint_interrupt_handler);
  signal(SIGTERM, sddspvtest_sigint_interrupt_handler);
  signal(SIGILL, sddspvtest_interrupt_handler);
  signal(SIGABRT, sddspvtest_interrupt_handler);
  signal(SIGFPE, sddspvtest_interrupt_handler);
  signal(SIGSEGV, sddspvtest_interrupt_handler);
#ifndef _WIN32
  signal(SIGHUP, sddspvtest_interrupt_handler);
  signal(SIGQUIT, sddspvtest_sigint_interrupt_handler);
  signal(SIGTRAP, sddspvtest_interrupt_handler);
  signal(SIGBUS, sddspvtest_interrupt_handler);
#endif

#ifdef EPICS313
  ca_task_initialize();
#else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
#endif
  ca_add_exception_event(oag_ca_exception_handler, NULL);

#if !defined(vxWorks)
  SDDS_RegisterProgramName("sddspvtest");
#endif

#if defined(vxWorks)
  buffer = malloc(sizeof(char) * (strlen(options) + 1 + 11));
  sprintf(buffer, "sddspvtest %s", options);
  argc = parse_string(&argv, buffer);
  if (buffer)
    free(buffer);
  buffer = NULL;

  if (taskVarAdd(0, (int *)&sddspvtestGlobal) != OK) {
    fprintf(stderr, "can't taskVarAdd sddspvtestGlobal\n");
    for (i = 0; i < argc; i++)
      free(argv[i]);
    free(argv);
    return (1);
  }
  if ((sddspvtestGlobal = (SDDSPVTEST_GLOBAL *)calloc(1, sizeof(SDDSPVTEST_GLOBAL))) == NULL) {
    fprintf(stderr, "can't malloc sddspvtestGlobal\n");
    for (i = 0; i < argc; i++)
      free(argv[i]);
    free(argv);
    return (1);
  }
#else
  if ((sddspvtestGlobal = (SDDSPVTEST_GLOBAL *)calloc(1, sizeof(SDDSPVTEST_GLOBAL))) == NULL) {
    fprintf(stderr, "can't malloc sddspvtestGlobal\n");
    return (1);
  }
#endif
#ifdef USE_RUNCONTROL
  sddspvtestGlobal->rcParam.PV = NULL;
  sddspvtestGlobal->rcParam.Desc = NULL;
  sddspvtestGlobal->rcParam.PVparam = NULL;
  sddspvtestGlobal->rcParam.DescParam = NULL;
  sddspvtestGlobal->rcParam.pingInterval = 2;
  sddspvtestGlobal->rcParam.pingTimeout = 10;
  sddspvtestGlobal->rcParam.alarmSeverity = NO_ALARM;
#endif
  sddspvtestGlobal->pvNumber = 0;
#if defined(vxWorks)
  sddspvtestGlobal->argc = argc;
  sddspvtestGlobal->argv = argv;
  taskDeleteHookAdd((FUNCPTR)sddsPVtestDeleteHook);
#else
  atexit(sddsPVtestDeleteHook);
#endif

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
#if defined(vxWorks)
    fprintf(stderr, "invalid sddspvtest command line\n");
#else
    fprintf(stderr, "%s\n", USAGE);
    fprintf(stderr, "%s", USAGE_WARNING);
#endif
    free_scanargs(&s_arg, argc);
    return (1);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (optionCode = match_string(s_arg[i_arg].list[0],
                                        commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PVOUTPUT:
        if (s_arg[i_arg].n_items < 2 ||
            !SDDS_CopyString(&sddspvtestGlobal->pvOutput, s_arg[i_arg].list[1])) {
          fprintf(stderr, "no value given for option -pvOutput\n");
          free_scanargs(&s_arg, argc);
          return (1);
        }
        if (s_arg[i_arg].n_items > 2) {
          if (s_arg[i_arg].n_items!=4) {
            fprintf(stderr, "incorrect -pvOutput syntax\n");
            return 1;
          }
          s_arg[i_arg].n_items -= 2;
          outputValuesGiven = 1;
          if (!scanItemList(&dummyFlags, s_arg[i_arg].list+2, &s_arg[i_arg].n_items, 0,
                            "passvalue", SDDS_LONG, &outputPassValue, 1, 1,
                            "failvalue", SDDS_LONG, &outputFailValue, 1, 2,
                            NULL) || dummyFlags!=3) {
            fprintf(stderr, "incorrect -pvOutput syntax\n%s", USAGE);
            return 1;
          }
        }
        setOutput = 1;
        break;
      case CLO_TIME:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&totalTime, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -time\n");
          free_scanargs(&s_arg, argc);
          return (1);
        }
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) != 0) {
            totalTime *= TimeUnitFactor[TimeUnits];
          }
        }
        break;
      case CLO_INTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&interval, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -interval\n");
          free_scanargs(&s_arg, argc);
          return (1);
        }
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) != 0) {
            interval *= TimeUnitFactor[TimeUnits];
          }
        }
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_MONITOR:
        monitor = 1;
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0) {
          fprintf(stderr, "invalid -pendIOtime syntax\n");
          free_scanargs(&s_arg, argc);
          return (1);
        }
        break;
      case CLO_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &sddspvtestGlobal->rcParam.PVparam, 1, 0,
                          "string", SDDS_STRING, &sddspvtestGlobal->rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &sddspvtestGlobal->rcParam.pingTimeout, 1, 0,
                          "pingInterval", SDDS_DOUBLE, &sddspvtestGlobal->rcParam.pingInterval, 1, 0,
                          NULL) ||
            (!sddspvtestGlobal->rcParam.PVparam && !sddspvtestGlobal->rcParam.PV) ||
            (sddspvtestGlobal->rcParam.PVparam && sddspvtestGlobal->rcParam.PV)) {
          fprintf(stderr, "bad -runControlPV syntax\n");
          fprintf(stderr, "-runControlPV={string=<string>|parameter=<string>}\n");
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, argc);
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        sddspvtestGlobal->rcParam.pingTimeout *= 1000;
        if (sddspvtestGlobal->rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.\n");
          free_scanargs(&s_arg, argc);
          return (1);
        }
#else
        fprintf(stderr, "runControl is not available. Option -runControlPV ignored.\n");
#endif
        break;
      case CLO_RUNCONTROLDESC:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &sddspvtestGlobal->rcParam.DescParam, 1, 0,
                          "string", SDDS_STRING, &sddspvtestGlobal->rcParam.Desc, 1, 0,
                          NULL) ||
            (!sddspvtestGlobal->rcParam.DescParam && !sddspvtestGlobal->rcParam.Desc) ||
            (sddspvtestGlobal->rcParam.DescParam && sddspvtestGlobal->rcParam.Desc)) {
          fprintf(stderr, "bad -runControlDesc syntax\n");
          fprintf(stderr, "-runControlDescription={string=<string>|parameter=<string>}\n");
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, argc);
          return (1);
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlDescription ignored.\n");
#endif
        break;
      case CLO_EXIT_CONDITION:
        exitConditionFlags = 0;
        if ((s_arg[i_arg].n_items -= 1) <= 0 ||
            !scanItemList(&exitConditionFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "anyfail", -1, NULL, 0, EXIT_IF_ANY_FAIL,
                          "allpass", -1, NULL, 0, EXIT_IF_ALL_PASS,
                          "converged", SDDS_USHORT, &convergenceCycles, 1, EXIT_IF_CONVERGED,
                          "changed", -1, NULL, 0, EXIT_IF_CHANGE,
                          NULL) || convergenceCycles<1) {
          fprintf(stderr, "bad -exitCondition syntax\n");
          return (1);
        }
        if (bitsSet(exitConditionFlags)!=1) {
          fprintf(stderr, "Given one and only one of anyFail, allPass, change, or converged qualifier for -exitCondition\n");
          return 1;
        }
        break;
      default:
        printf("unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        free_scanargs(&s_arg, argc);
        return (1);
      }
    } else {
      if (!sddspvtestGlobal->inputFile) {
        if (!SDDS_CopyString(&sddspvtestGlobal->inputFile, s_arg[i_arg].list[0])) {
          fprintf(stderr, "Error reading inputfile\n");
          free_scanargs(&s_arg, argc);
          return (1);
        }
      } else {
        fprintf(stderr, "too many filenames given\n");
        free_scanargs(&s_arg, argc);
        return (1);
      }
    }
  }
  free_scanargs(&s_arg, argc);
  if (!sddspvtestGlobal->inputFile) {
    fprintf(stderr, "input file is not given!\n");
    return (1);
  }
  if (totalTime<0) {
    fprintf(stderr, "time is not given\n");
    return (1);
  }
#ifdef USE_RUNCONTROL
#  ifdef USE_LOGDAEMON
  sddspvtestGlobal->useLogDaemon = 0;
  if (sddspvtestGlobal->rcParam.PV) {
    switch (logOpen(&sddspvtestGlobal->logHandle, NULL, "pvtestAudit", "Instance Action")) {
    case LOG_OK:
      sddspvtestGlobal->useLogDaemon = 1;
      break;
    case LOG_INVALID:
      fprintf(stderr, "warning: logDaemon rejected given sourceId and tagList. logDaemon messages not logged.\n");
      break;
    case LOG_ERROR:
      fprintf(stderr, "warning: unable to open connection to logDaemon. logDaemon messages not logged.\n");
      break;
    case LOG_TOOBIG:
      fprintf(stderr, "warning: one of the string supplied is too long. logDaemon messages not logged.\n");
      break;
    default:
      fprintf(stderr, "warning: unrecognized return code from logOpen.\n");
      break;
    }
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Start", NULL);
  }
#  endif
  if (sddspvtestGlobal->rcParam.PV && !sddspvtestGlobal->rcParam.Desc) {
#  ifdef USE_LOGDAEMON
    if (sddspvtestGlobal->useLogDaemon)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "The controlDescription should not be null", NULL);
#  endif
    fprintf(stderr, "the runControlDescription should not be null, application stopped!\n");
    return (1);
  }
#endif
  if (setOutput) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Connecting to output pv", NULL);

#  endif
#endif
    if (ca_search(sddspvtestGlobal->pvOutput, &outputID) != ECA_NORMAL) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
        sprintf(errorMsg, "error (sddspvtest): problem doing search for %s\n, application stopped.", sddspvtestGlobal->pvOutput);
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
      }
#  endif
#endif
      fprintf(stderr, "error (sddspvtest): problem doing search for %s\n", sddspvtestGlobal->pvOutput);
      return (1);
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
        sprintf(errorMsg, "error (sddspvtest): problem doing search for %s\n, application stopped.", sddspvtestGlobal->pvOutput);
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
      }
#  endif
#endif
      fprintf(stderr, "error (sddspvtest): problem doing search for %s\n", sddspvtestGlobal->pvOutput);
      return (1);
    }
    if (ca_state(outputID) != cs_conn) {
      fprintf(stderr, "error (sddspvtest): no connection for %s\n", sddspvtestGlobal->pvOutput);
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
        sprintf(errorMsg, "error (sddspvtest): no connection for %s\n, application stopped.", sddspvtestGlobal->pvOutput);
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
      }
#  endif
#endif
      return (1);
    }
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Connecting to output pv done", NULL);
#  endif
#endif
  }
  if (!getInputInfo(&sddspvtestGlobal->ControlName,
                    &sddspvtestGlobal->MaximumValue,
                    &sddspvtestGlobal->MinimumValue,
                    &sddspvtestGlobal->Monitor,
                    &sddspvtestGlobal->ChangeLimit,
                    &sddspvtestGlobal->pvNumber,
                    sddspvtestGlobal->inputFile)) {
    fprintf(stderr, "Error in reading input file!\n");
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Error in reading input file.", NULL);
#  endif
#endif
    return (1);
  }
  if (exitConditionFlags&(EXIT_IF_CHANGE|EXIT_IF_CONVERGED) && !(sddspvtestGlobal->ChangeLimit)) {
    fprintf(stderr, "Error: -exit=change or -exit=converged given, but file lacks the ChangeLimit column\n");
    exit(1);
  }
  /*Allocate memory*/
  if (!(sddspvtestGlobal->channelID = (chid *)malloc(sizeof(*sddspvtestGlobal->channelID) * sddspvtestGlobal->pvNumber))) {
    fprintf(stderr, "memory allocation failure\n");
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Memory allocation failure of channeldID.", NULL);
#  endif
#endif
    return (1);
  }
  if (!(sddspvtestGlobal->MonitorFlag = (int *)malloc(sizeof(*sddspvtestGlobal->MonitorFlag) * sddspvtestGlobal->pvNumber))) {
    fprintf(stderr, "memory allocation failure\n");
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Memory allocation failure of MonitorFlag.", NULL);
#  endif
#endif
    return (1);
  }
  if (!(sddspvtestGlobal->usrValue = (long *)malloc(sizeof(*sddspvtestGlobal->usrValue) * sddspvtestGlobal->pvNumber))) {
    fprintf(stderr, "memory allocation failure\n");
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Memory allocation failure of usrValue.", NULL);
#  endif
#endif
    return (1);
  }
  if (!(sddspvtestGlobal->CurrentValue = (double *)malloc(sizeof(*sddspvtestGlobal->CurrentValue) * sddspvtestGlobal->pvNumber))) {
    fprintf(stderr, "memory allocation failure\n");
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Memory allocation failure of CurrentValue.", NULL);
#  endif
#endif
    return (1);
  }
  if (!(sddspvtestGlobal->ReferenceValue = (double *)malloc(sizeof(*sddspvtestGlobal->ReferenceValue) * sddspvtestGlobal->pvNumber))) {
    fprintf(stderr, "memory allocation failure\n");
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Memory allocation failure of ReferenceValue.", NULL);
#  endif
#endif
    return (1);
  }
  if (!(sddspvtestGlobal->Updated = (short*)malloc(sizeof(*sddspvtestGlobal->Updated) * sddspvtestGlobal->pvNumber))) {
    fprintf(stderr, "memory allocation failure\n");
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Memory allocation failure of Updates.", NULL);
#  endif
#endif
    return (1);
  }
  /*Initialize current values to huge numbers*/
  for (i = 0; i < sddspvtestGlobal->pvNumber; i++) {
    sddspvtestGlobal->CurrentValue[i] = sddspvtestGlobal->ReferenceValue[i] = DBL_MAX;
    sddspvtestGlobal->Updated[i] = 0;
  }
  /*Check which PVs will be monitored and which will be checked each interval*/
  if (sddspvtestGlobal->Monitor != NULL) {
    for (j = 0; j < sddspvtestGlobal->pvNumber; j++) {
      if ('y' == sddspvtestGlobal->Monitor[j] || 'Y' == sddspvtestGlobal->Monitor[j]) {
        sddspvtestGlobal->MonitorFlag[j] = 1;
      } else {
        allMonitoring = 0;
        sddspvtestGlobal->MonitorFlag[j] = 0;
      }
    }
  } else {
    allMonitoring = 0;
    for (j = 0; j < sddspvtestGlobal->pvNumber; j++) {
      sddspvtestGlobal->MonitorFlag[j] = 0;
    }
  }
  if (monitor) {
    for (j = 0; j < sddspvtestGlobal->pvNumber; j++) {
      sddspvtestGlobal->MonitorFlag[j] = 1;
    }
    allMonitoring = 1;
  }
  /*Search for each PV*/
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
  if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
    logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Connecting test pvs", NULL);
#  endif
#endif
  for (j = 0; j < sddspvtestGlobal->pvNumber; j++) {
    if (ca_search(sddspvtestGlobal->ControlName[j], sddspvtestGlobal->channelID + j) != ECA_NORMAL) {
      fprintf(stderr, "error (sddspvtest): problem doing search for %s\n", sddspvtestGlobal->ControlName[j]);
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
        sprintf(errorMsg, "error (sddspvtest): problem doing search for %s\n", sddspvtestGlobal->ControlName[j]);
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
      }
#  endif
#endif
      return (1);
    }
    sddspvtestGlobal->usrValue[j] = j;
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "problem doing search for test pvs", NULL);
#  endif
#endif
    fprintf(stderr, "error (sddspvtest): problem doing search for some PVs\n");
    return (1);
  }
  /*Double check to ensure PV connection exists*/
  result = 0;
  for (j = 0; j < sddspvtestGlobal->pvNumber; j++) {
    if (ca_state(sddspvtestGlobal->channelID[j]) != cs_conn) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
        sprintf(errorMsg, "no connection for %s", sddspvtestGlobal->ControlName[j]);
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
      }
#  endif
#endif
      fprintf(stderr, "error (sddspvtest): no connection for %s\n", sddspvtestGlobal->ControlName[j]);
      result++;
    }
  }
  if (result)
    return (1);
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
  if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
    logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "initialize run control", NULL);
#  endif
#endif
    /*Initiate run control*/
#ifdef USE_RUNCONTROL
  if (sddspvtestGlobal->rcParam.PV) {
    sddspvtestGlobal->rcParam.handle[0] = (char)0;
    sddspvtestGlobal->rcParam.Desc = (char *)realloc(sddspvtestGlobal->rcParam.Desc, 41 * sizeof(char));
    sddspvtestGlobal->rcParam.PV = (char *)realloc(sddspvtestGlobal->rcParam.PV, 41 * sizeof(char));
    sddspvtestGlobal->rcParam.status = runControlInit(sddspvtestGlobal->rcParam.PV,
                                                      sddspvtestGlobal->rcParam.Desc,
                                                      sddspvtestGlobal->rcParam.pingTimeout,
                                                      sddspvtestGlobal->rcParam.handle,
                                                      &(sddspvtestGlobal->rcParam.rcInfo),
                                                      pendIOtime);
    if (sddspvtestGlobal->rcParam.status != RUNCONTROL_OK) {
#  ifdef USE_LOGDAEMON
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Error initializing run control", NULL);
#  endif
      fprintf(stderr, "Error initializing run control.\n");
      if (sddspvtestGlobal->rcParam.status == RUNCONTROL_DENIED) {
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      }
      return (1);
    }
#  ifdef USE_LOGDAEMON
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "ping runcontrol at the beginning", NULL);
#  endif
    /* ping once at the beginning */
    if (runControlPingWhileSleep(0.0) != RUNCONTROL_OK) {
#  ifdef USE_LOGDAEMON
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Problem pinging the run control record", NULL);
#  endif
      fprintf(stderr, "Problem pinging the run control record.\n");
      return (1);
    }
    strcpy(sddspvtestGlobal->rcParam.message, "pv test started");
    sddspvtestGlobal->rcParam.status = runControlLogMessage(sddspvtestGlobal->rcParam.handle, sddspvtestGlobal->rcParam.message, NO_ALARM, &(sddspvtestGlobal->rcParam.rcInfo));
    if (sddspvtestGlobal->rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable tp write status message and alarm severity\n");
      return (1);
    }
#  ifdef USE_LOGDAEMON
    if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "pv test started", NULL);
#  endif
  }
#endif

  cycleCount = 0;
  previousResult = -1;
  quitingTime = getTimeInSecs() + totalTime; /*TotalTime is the input time duration */
  pingTime = getTimeInSecs();
  /*ping again before starting test.*/
#ifdef USE_RUNCONTROL
  if (sddspvtestGlobal->rcParam.PV) {
    if (runControlPingWhileSleep(0.0) != RUNCONTROL_OK) {
#  ifdef USE_LOGDAEMON
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "problem pinging the run control record before monitoring", NULL);
#  endif
      fprintf(stderr, "Problem pinging the run control record.\n");
      return (1);
    }
  }
#endif
  noChangeCount = 0;
  if (verbose && setOutput) {
    if (outputValuesGiven)
      fprintf(stderr, "Will write results to PV %s.   Pass value: %ld    Fail value: %ld\n", sddspvtestGlobal->pvOutput, outputPassValue, outputFailValue);
    else
      fprintf(stderr, "Will write results to PV %s.   Pass value: 0      Fail value: number of failed conditions\n", sddspvtestGlobal->pvOutput);
  }
  while (getTimeInSecs() < quitingTime || totalTime==0) {
    result = somethingChanged = 0;
    if (cycleCount) {
      /*Call ca_pend_event every time except first time through the loop*/
#ifdef USE_RUNCONTROL
      if (sddspvtestGlobal->rcParam.PV && (getTimeInSecs() - pingTime) >= 1.0) {
        if (runControlPingWhileSleep(interval) != RUNCONTROL_OK) {
#  ifdef USE_LOGDAEMON
          if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
            logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "problem pinging the run control record during the loop", NULL);
#  endif
          fprintf(stderr, "Problem pinging the run control record.\n");
          return (1);
        }
        pingTime = getTimeInSecs();
      } else {
        oag_ca_pend_event(interval, &(sddspvtestGlobal->sigint));
      }
#else
      oag_ca_pend_event(interval, &(sddspvtestGlobal->sigint));
#endif
    }
    if (sddspvtestGlobal->sigint) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
        logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Exit by signal handler", NULL);
#  endif
#endif
      if (verbose)
        fprintf(stderr, "Exiting due to SIGINT\n");
      return (1);
    }
    for (j = 0; j < sddspvtestGlobal->pvNumber; j++) {
      /*Double check that PVs are still connected*/
      if (ca_state(sddspvtestGlobal->channelID[j]) != cs_conn) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
        if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
          sprintf(errorMsg, "error (sddspvtest): no connection for %s", sddspvtestGlobal->ControlName[j]);
          logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
        }
#  endif
#endif
        fprintf(stderr, "error (sddspvtest): no connection to %s\n", sddspvtestGlobal->ControlName[j]);
        return (1);
      }
      /*Get all PV values first time through, after that only get PV values that are not monitored*/
      if ((cycleCount == 0) || (sddspvtestGlobal->MonitorFlag[j] == 0)) {
        if (ca_get(DBR_DOUBLE, sddspvtestGlobal->channelID[j], sddspvtestGlobal->CurrentValue + j) != ECA_NORMAL) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
          if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
            sprintf(errorMsg, "error (sddspvtest): problem reading %s", sddspvtestGlobal->ControlName[j]);
            logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
          }
#  endif
#endif
          fprintf(stderr, "error (sddspvtest): problem doing get for %s\n", sddspvtestGlobal->ControlName[j]);
          return (1);
        }
        if (sddspvtestGlobal->ReferenceValue[j] == DBL_MAX && sddspvtestGlobal->CurrentValue[j] != DBL_MAX &&
            !sddspvtestGlobal->Updated[j]) {
          sddspvtestGlobal->ReferenceValue[j] = sddspvtestGlobal->CurrentValue[j];
          sddspvtestGlobal->Updated[j] = 1;
          nValid++;
        }
      }
    }
    if ((cycleCount == 0) || (!allMonitoring)) {
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
        if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
          logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "error (sddspvtest): problem reading the values of some test pvs.", NULL);
#  endif
#endif
        fprintf(stderr, "error (sddspvtest): problem doing getting the values for some PVs\n");
        return (1);
      }
    }
    /*Check if all PVs pass test*/
    for (j = 0; j < sddspvtestGlobal->pvNumber; j++) {
      if (sddspvtestGlobal->CurrentValue[j] == DBL_MAX) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
        if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
          sprintf(errorMsg, "error (sddspvtest): no value returned in time for %s", sddspvtestGlobal->ControlName[j]);
          logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
        }
#  endif
#endif
        fprintf(stderr, "error (sddspvtest): no value returned in time for %s\n", sddspvtestGlobal->ControlName[j]);
        return (1);
      }
      if (sddspvtestGlobal->CurrentValue[j] > sddspvtestGlobal->MaximumValue[j] ||
          sddspvtestGlobal->CurrentValue[j] < sddspvtestGlobal->MinimumValue[j]) {
        if (verbose)
          fprintf(stderr, "%s: Out of range:  %s  %e\n", getHourMinuteSecond(), sddspvtestGlobal->ControlName[j], sddspvtestGlobal->CurrentValue[j]);
        result++;
      }
      if (cycleCount>2 && (exitConditionFlags&(EXIT_IF_CHANGE|EXIT_IF_CONVERGED))) {
        if (sddspvtestGlobal->Updated[j]) {
          if (fabs(sddspvtestGlobal->CurrentValue[j]-sddspvtestGlobal->ReferenceValue[j]) > sddspvtestGlobal->ChangeLimit[j]) {
            if (verbose)
              fprintf(stderr, "%s: %s changed from %e to %e\n", getHourMinuteSecond(), sddspvtestGlobal->ControlName[j],
                      sddspvtestGlobal->ReferenceValue[j],
                      sddspvtestGlobal->CurrentValue[j]);
            somethingChanged = 1;
          }
          if (exitConditionFlags&EXIT_IF_CONVERGED)
            /* Update the reference */
            sddspvtestGlobal->ReferenceValue[j] = sddspvtestGlobal->CurrentValue[j];
        }
      }
    }
    if (nValid==sddspvtestGlobal->pvNumber && !somethingChanged)
      noChangeCount++; /* How many cycles with no change seen */
    else
      noChangeCount = 0;
    
    /*Post result*/
    if (!setOutput) {
      fprintf(stderr, "%ld\n", result);
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
      if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
        if (result)
          logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "out of range", NULL);
        else
          logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "test passed", NULL);
      }
#  endif
#endif
    } else {
      if (verbose) {
        if ((!result) && (previousResult))
          fprintf(stderr, "%s: test passed!\n", getHourMinuteSecond());
      }
      if (result != previousResult) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
        if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
          if (result)
            logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "out of range", NULL);
          else
            logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "test passed", NULL);
          if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV)
            logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "write to output pv", NULL);
        }
#  endif
#endif
      }
      outputValue = outputValuesGiven ? (result ? outputFailValue : outputPassValue) : result;
      if (ca_put(DBR_LONG, outputID, &outputValue) != ECA_NORMAL) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
        if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
          sprintf(errorMsg, "error (sddspvtest): problem writing to %s", sddspvtestGlobal->pvOutput);
          logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
        }
#  endif
#endif
        fprintf(stderr, "error (sddspvtest): problem doing put for %s\n", sddspvtestGlobal->pvOutput);
        return (1);
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
        if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
          sprintf(errorMsg, "error (sddspvtest): problem doing put for %s", sddspvtestGlobal->pvOutput);
          logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
        }
#  endif
#endif
        fprintf(stderr, "error (sddspvtest): problem doing put for %s\n", sddspvtestGlobal->pvOutput);
          return (1);
      }
    }
    previousResult = result;
    if (!cycleCount) {
      /*If this is the first time through the loop, setup monitoring event handler*/
      for (j = 0; j < sddspvtestGlobal->pvNumber; j++) {
        if (sddspvtestGlobal->MonitorFlag[j] == 1) {
          if (ca_add_masked_array_event(DBR_TIME_DOUBLE, 1, sddspvtestGlobal->channelID[j], EventHandler,
                                        (void *)&(sddspvtestGlobal->usrValue[j]), (ca_real)0, (ca_real)0, (ca_real)0,
                                        NULL, DBE_VALUE | DBE_ALARM) != ECA_NORMAL) {
#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
            if (sddspvtestGlobal->useLogDaemon && sddspvtestGlobal->rcParam.PV) {
              sprintf(errorMsg, "error: unable to setup alarm callback for %s", sddspvtestGlobal->ControlName[j]);
              logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, errorMsg, NULL);
            }
#  endif
#endif
            fprintf(stderr, "error: unable to setup alarm callback for control name %s\n", sddspvtestGlobal->ControlName[j]);
            return (1);
          }
        }
      }
    }
    if (totalTime==0) {
      if (verbose)
        fprintf(stderr, "Terminating due to expiration of allotted time.\n");
      break;
    }
    if (exitConditionFlags) {
      switch (exitConditionFlags) {
      case EXIT_IF_ANY_FAIL:
        if (result) {
          if (verbose)
            fprintf(stderr, "Exiting due to failed condition(s).\n");
          return 1;
        }
        break;
      case EXIT_IF_ALL_PASS:
        if (!result && nValid==sddspvtestGlobal->pvNumber) {
          if (verbose)
            fprintf(stderr, "Exiting due to satisifed conditions.\n");
          return 1;
        }
        break;
      case EXIT_IF_CHANGE:
        if (somethingChanged) {
          if (verbose)
            fprintf(stderr, "Exiting due to change(s).\n");
          return 1;
        }
        break;
      case EXIT_IF_CONVERGED:
        if (!somethingChanged && nValid==sddspvtestGlobal->pvNumber && noChangeCount>=convergenceCycles) {
          if (verbose)
            fprintf(stderr, "Exiting due to convergence.\n");
          return 1;
        }
      }
    }
    cycleCount++;
  }
  return (0);
}

long getInputInfo(char ***ControlName, double **MaximumValue, double **MinimumValue,
                  char **Monitor, double **ChangeLimit, long *pvNumber, char *inputFile) {
  SDDS_DATASET inSet;
  int monitorColumnFound = 0, changeLimitFound = 0;

  *ControlName = NULL;
  *MaximumValue = *MinimumValue = *ChangeLimit = NULL;
  *Monitor = NULL;
  *pvNumber = 0;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;
  switch (SDDS_CheckColumn(&inSet, "ControlName", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "\"ControlName\" column does not exist\n");
    SDDS_Terminate(&inSet);
    return (0);
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "Something wrong with column \"ControlName\".\n");
    SDDS_Terminate(&inSet);
    return (0);
  }
  switch (SDDS_CheckColumn(&inSet, "MaximumValue", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "\"MaximumValue\" column does not exist\n");
    SDDS_Terminate(&inSet);
    return (0);
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "Something wrong with column \"MaximumValue\".\n");
    SDDS_Terminate(&inSet);
    return (0);
  }
  switch (SDDS_CheckColumn(&inSet, "MinimumValue", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "\"MinimumValue\" column does not exist\n");
    SDDS_Terminate(&inSet);
    return (0);
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "Something wrong with column \"MinimumValue\".\n");
    SDDS_Terminate(&inSet);
    return (0);
  }
  switch (SDDS_CheckColumn(&inSet, "Monitor", NULL, SDDS_CHARACTER, NULL)) {
  case SDDS_CHECK_OKAY:
    monitorColumnFound = 1;
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "Something wrong with column \"Monitor\".\n");
    SDDS_Terminate(&inSet);
    return (0);
  }
  switch (SDDS_CheckColumn(&inSet, "ChangeLimit", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    changeLimitFound = 1;
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "Something wrong with column \"ChangeLimit\".\n");
    SDDS_Terminate(&inSet);
    return (0);
  }

  if (0 >= SDDS_ReadPage(&inSet)) {
    SDDS_Terminate(&inSet);
    return (0);
  }

  if (!(*pvNumber = SDDS_RowCount(&inSet))) {
    fprintf(stderr, "Zero rows defined in input file.\n");
    SDDS_Terminate(&inSet);
    return (0);
  }
  *ControlName = (char **)SDDS_GetColumn(&inSet, "ControlName");
  if (!(*MaximumValue = SDDS_GetColumnInDoubles(&inSet, "MaximumValue")) ||
      !(*MinimumValue = SDDS_GetColumnInDoubles(&inSet, "MinimumValue"))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&inSet);
    return (0);
  }
  if (monitorColumnFound) {
    *Monitor = (char *)SDDS_GetColumn(&inSet, "Monitor");
  }
  if (changeLimitFound) {
    if (!(*ChangeLimit = SDDS_GetColumnInDoubles(&inSet, "ChangeLimit"))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Terminate(&inSet);
      return (0);
    }
  }    
  SDDS_Terminate(&inSet);
  return (1);
}

void sddspvtest_interrupt_handler(int sig) {
  exit(1);
}

void sddspvtest_sigint_interrupt_handler(int sig) {
  signal(SIGINT, sddspvtest_interrupt_handler);
  signal(SIGTERM, sddspvtest_interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, sddspvtest_interrupt_handler);
#endif
  sddspvtestGlobal->sigint = 1;
  return;
}

#ifdef USE_RUNCONTROL

/* returns value from same list of statuses as other runcontrol calls */
int runControlPingWhileSleep(double sleepTime) {
  double timeLeft;

  timeLeft = sleepTime;
  do {
    sddspvtestGlobal->rcParam.status = runControlPing(sddspvtestGlobal->rcParam.handle, &(sddspvtestGlobal->rcParam.rcInfo));
    switch (sddspvtestGlobal->rcParam.status) {
    case RUNCONTROL_ABORT:
      fprintf(stderr, "Run control application aborted.\n");
#  ifdef USE_LOGDAEMON
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Run control application aborted", NULL);
#  endif
      return (RUNCONTROL_ABORT);
    case RUNCONTROL_TIMEOUT:
      fprintf(stderr, "Run control application timed out.\n");
      strcpy(sddspvtestGlobal->rcParam.message, "Application timed-out");
#  ifdef USE_LOGDAEMON
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Run control timed out", NULL);
#  endif
      runControlLogMessage(sddspvtestGlobal->rcParam.handle, sddspvtestGlobal->rcParam.message, MAJOR_ALARM, &(sddspvtestGlobal->rcParam.rcInfo));
      return (RUNCONTROL_TIMEOUT);
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Communications error with runcontrol record.\n");
#  ifdef USE_LOGDAEMON
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Communications error with runcontrol record", NULL);
#  endif
      return (RUNCONTROL_ERROR);
    default:
#  ifdef USE_LOGDAEMON
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Unknown run conrol error code.", NULL);
#  endif
      fprintf(stderr, "Unknown run control error code.\n");
      break;
    }
    oag_ca_pend_event(MIN(timeLeft, sddspvtestGlobal->rcParam.pingInterval), &(sddspvtestGlobal->sigint));
    if (sddspvtestGlobal->sigint) {
#  ifdef USE_LOGDAEMON
      logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Exit by signal handler.", NULL);
#  endif
      exit(1);
    }
    timeLeft -= sddspvtestGlobal->rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}
#endif

#if defined(vxWorks)
void sddsPVtestDeleteHook(WIND_TCB *pTcb)
#else
void sddsPVtestDeleteHook()
#endif

{
  SDDSPVTEST_GLOBAL *sg;
  int ca = 1, i;

#if defined(vxWorks)
  int tid;
  if (pTcb->entry == (FUNCPTR)sddsIocPVtest) {
    tid = taskNameToId(pTcb->name);
    sg = (SDDSPVTEST_GLOBAL *)taskVarGet(tid, (int *)&sddspvtestGlobal);
    taskDeleteHookDelete((FUNCPTR)sddsPVtestDeleteHook);
    if ((sg != NULL) && ((int)sg != ERROR)) {
#else
  sg = sddspvtestGlobal;
#endif

#ifndef EPICS313
      if (ca_current_context() == NULL)
        ca = 0;
      if (ca)
        ca_attach_context(ca_current_context());
#endif

      if (ca) {
#ifdef USE_RUNCONTROL
#  ifdef USE_LOGDAEMON
        if (sg->useLogDaemon && sg->rcParam.PV) {
          logArguments(sddspvtestGlobal->logHandle, sddspvtestGlobal->rcParam.PV, "Stop", NULL);
          logClose(sg->logHandle);
        }
#  endif
        if ((sg->rcParam.PV) && (sg->rcParam.status != RUNCONTROL_DENIED)) {
          strcpy(sg->rcParam.message, "Application exited.");
          runControlLogMessage(sg->rcParam.handle, sg->rcParam.message, MAJOR_ALARM, &(sg->rcParam.rcInfo));
          switch (runControlExit(sg->rcParam.handle, &(sg->rcParam.rcInfo))) {
          case RUNCONTROL_OK:
            break;
          case RUNCONTROL_ERROR:
            fprintf(stderr, "Error exiting run control.\n");
            break;
          default:
            fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
          }
        }
#endif
        ca_task_exit();
      }

      if (sg->ControlName) {
        for (i = 0; i < sg->pvNumber; i++) {
          if (sg->ControlName[i]) {
            free(sg->ControlName[i]);
            sg->ControlName[i] = NULL;
          }
        }
        free(sg->ControlName);
        sg->ControlName = NULL;
      }
      if (sg->MaximumValue) {
        free(sg->MaximumValue);
        sg->MaximumValue = NULL;
      }
      if (sg->MinimumValue) {
        free(sg->MinimumValue);
        sg->MinimumValue = NULL;
      }
      if (sg->CurrentValue) {
        free(sg->CurrentValue);
        sg->CurrentValue = NULL;
      }
      if (sg->channelID) {
        free(sg->channelID);
        sg->channelID = NULL;
      }
      if (sg->inputFile) {
        free(sg->inputFile);
        sg->inputFile = NULL;
      }
      if (sg->pvOutput) {
        free(sg->pvOutput);
        sg->pvOutput = NULL;
      }

#if defined(vxWorks)
      if (sg->argv) {
        for (i = 0; i < sg->argc; i++) {
          if ((sg->argv)[i]) {
            free((sg->argv)[i]);
          }
        }
        free(sg->argv);
      }
#endif
      free(sg);
      sg = NULL;
#if defined(vxWorks)
      taskVarDelete(tid, (int *)&sddspvtestGlobal);
    }
  }
#endif
}

void EventHandler(struct event_handler_args event) {
  struct dbr_time_double *dbrValue;
  long index;
  index = *((long *)event.usr);
  dbrValue = (struct dbr_time_double *)event.dbr;
  sddspvtestGlobal->CurrentValue[index] = (double)dbrValue->value;
  if (!sddspvtestGlobal->Updated[index]) {
    sddspvtestGlobal->ReferenceValue[index] = sddspvtestGlobal->CurrentValue[index];
    sddspvtestGlobal->Updated[index] = 1;
  }
}
