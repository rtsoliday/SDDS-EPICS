/**
 * @file cawait.cc
 * @brief Wait for EPICS process variables to reach specified conditions.
 *
 * cawait monitors one or more process variables and waits until user-defined
 * criteria are satisfied. Logical combinations of conditions and event
 * notifications are supported for flexible automation.
 *
 * @section Usage
 * ```
 * cawait [-interval=<seconds>] 
 *        [-timelimit=<seconds>] 
 *        [-totaltimelimit=<seconds>]
 *         -waitFor=<PVname>[,lowerLimit=<value>,upperLimit=<value>][,equalTo=<value>][,sameAs=<string>][,above=<value>][,below=<value>][,changed]
 *        [-and | -or | -not] 
 *        [-repeat[=<number>]] 
 *        [-emit=event=<string>[,timeout=<string>][,end=<string>]]
 *        [-preevent=<command>] 
 *        [-onevent=<command>] 
 *        [-postevent=<command>] 
 *        [-onend=<command>]
 *        [-subprocess=<command>[,event=<string>][,timeout=<string>][,end=<string>]]
 *        [-pendiotime=<seconds>] [-nowarnings] 
 *        [-provider={ca|pva}]
 * ```
 *
 * @section Options
 * | Required                | Description                                                             |
 * |-------------------------|-------------------------------------------------------------------------|
 * | `-waitFor`              | Define PV and completion criterion.                                      |
 *
 * | Optional                | Description                                                             |
 * |-------------------------|-------------------------------------------------------------------------|
 * | `-interval`             | Interval between checks (default 0.1 s).                                |
 * | `-ezcatiming`           | Enable EZCA timing mode.                                                |
 * | `-timelimit`            | Maximum time to wait for the condition.                                  |
 * | `-totaltimelimit`       | Maximum total runtime for cawait.                                        |
 * | `-repeat`               | Number of events to wait for (default 1; infinite if no number given).   |
 * | `-emit`                 | Emit strings on event, timeout, or end.                                  |
 * | `-usemonitor`           | Use CA monitors instead of polling.                                      |
 * | `-not`, `-and`, `-or`   | Logical NOT, AND, OR on conditions.                                      |
 * | `-preevent`             | Command to execute before each event.                                    |
 * | `-onevent`              | Command to execute when an event occurs.                                 |
 * | `-postevent`            | Command to execute after an event clears.                                |
 * | `-onend`                | Command to execute after final event.                                    |
 * | `-subprocess`           | Spawn subprocess and send notifications.                                  |
 * | `-pendiotime`           | Time allowed for CA operations (default 10 s).                           |
 * | `-nowarnings`           | Suppress timeout warnings.                                               |
 * | `-provider`             | Choose Channel Access or PVAccess provider.                              |
 *
 * @subsection Incompatibilities
 * - `-not` requires a preceding `-waitFor` option.
 * - `-and` and `-or` require at least two `-waitFor` options.
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * M. Borland, R. Soliday
 */

#include <complex>
#include <iostream>
#include <queue>
#include <cstdlib>
#include <ctime>
#include <cstddef>

#if defined(_WIN32)
#  include <winsock.h>
#else
#  include <unistd.h>
#endif

#include <cadef.h>
#include <epicsVersion.h>
#if (EPICS_VERSION > 3)
#  include "pvaSDDS.h"
#  include "pv/thread.h"
#endif

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"

#define CLO_INTERVAL 0
#define CLO_WAITFOR 1
#define CLO_EZCATIMING 2
#define CLO_TIMELIMIT 3
#define CLO_USEMONITOR 4
#define CLO_NOT 5
#define CLO_OR 6
#define CLO_AND 7
#define CLO_REPEAT 8
#define CLO_EMIT 9
#define CLO_PREEVENT 10
#define CLO_POSTEVENT 11
#define CLO_SUBPROCESS 12
#define CLO_ONEVENT 13
#define CLO_ONEND 14
#define CLO_TOTALTIMELIMIT 15
#define CLO_PENDIOTIME 16
#define CLO_NOWARN 17
#define CLO_PROVIDER 18
#define COMMANDLINE_OPTIONS 19
char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"interval", (char *)"waitfor", (char *)"ezcatiming",
  (char *)"timelimit", (char *)"usemonitor", (char *)"not",
  (char *)"or", (char *)"and", (char *)"repeat", (char *)"emit",
  (char *)"preevent", (char *)"postevent", (char *)"subprocess",
  (char *)"onevent", (char *)"onend", (char *)"totaltimelimit",
  (char *)"pendiotime", (char *)"nowarnings", (char *)"provider"};

#define PROVIDER_CA 0
#define PROVIDER_PVA 1
#define PROVIDER_COUNT 2
char *providerOption[PROVIDER_COUNT] = {
  (char *)"ca",
  (char *)"pva",
};

static char *USAGE =
  (char *)"cawait [-interval=<seconds>] [-timeLimit=<seconds>] [-totalTimeLimit=<seconds>]\n\
-waitFor=<PVname>[,lowerLimit=<value>,upperLimit=<value>][,equalTo=<value>][,sameAs=<string>][,above=<value>][,below=<value>][,changed] \n\
 [{-and | -or | -not}]\n\
 [-repeat[=<number>]]\n\
 [-emit=event=<string>[,timeout=<string>][,end=<string>]]\n\
 [-preEvent=<command>]\n\
 [-onEvent=<command>]\n\
 [-postEvent=<command>]\n\
 [-onEnd=<command>]\n\
 [-subprocess=<command>[,event=<string>][,timeout=<string>][,end=<string>]]\n\
 [-pendIOTime=<seconds>]\n\
 [-noWarnings]\n\
 [-provider={ca|pva}]\n\
-interval    specifies interval between checking PVs. Default is 0.1 s.\n\
             Since monitors are used, this doesn't load the IOCs.\n\
-timeLimit   specifies maximum time to wait. Default is infinite.\n\
-totalTimeLimit \n\
             specifies maximum time cawait will run.  Default is infinite.\n\
-waitFor     specifies PV name, plus criterion for ending wait.\n\
             Each occurrence pushes a result on the logic stack.\n\
-and, -or, -not\n\
             Performs operations on the logic stack.\n\
-repeat      specifies number of times to wait for the event. Default is\n\
             infinite if -repeat is given with no qualifiers, otherwise the\n\
             default is 1. The event condition must become false to start a new\n\
             event.\n\
-emit        specifies string sto emit to the standard output when the event\n\
             condition occurs, when a timeout occurs, or when the specified\n\
             number of events has been detected.\n\
-preEvent    specifies a command to execute before waiting for the event.\n\
-onEvent     specifies a command to execute when the event occurs.\n\
-postEvent   specifies a command to execute after waiting for the event.\n\
-onEnd       specifies a command to execute after the final event has been detected.\n\
-subprocess  specifies a command to execute on a pipe and optionally the messages to\n\
             pass to the subprocess for events, timeouts, and end-of-run.\n\
-pendIOTime  set time (in seconds) allowed for CA operations.\n\
-provider    defaults to CA (channel access) calls.\n\
Program by Michael Borland, ANL/APS (" EPICS_VERSION_STRING ", " __DATE__ ")\n";

#define LOWERLIMIT_GIVEN 0x00001U
#define UPPERLIMIT_GIVEN 0x00002U
#define EQUALTO_GIVEN 0x00004U
#define ABOVE_GIVEN 0x00008U
#define BELOW_GIVEN 0x00010U
#define SAME_AS_GIVEN 0x00020U
#define CHANGED_GIVEN 0x00040U

#define OP_NOT CLO_NOT
#define OP_OR CLO_OR
#define OP_AND CLO_AND

typedef struct
{
  char *PVname;
  chid channelID;
  long channelType;
  double lowerLimit, upperLimit, equalTo;
  double above, below, changeReference;
  long operations;
  short *operation;
  char *sameAs;
  unsigned long flags;
  double value;
  char stringValue[256];
  long usrValue;
  short changeReferenceLoaded;
} WAITFOR;

typedef struct
{
  char *command;
  char *emitOnEvent, *emitOnTimeout, *emitOnEnd;
  FILE *fp;
} SUBPROCESS;

#define EMIT_EVENT_GIVEN 0x0001U
#define EMIT_TIMEOUT_GIVEN 0x0002U
#define EMIT_END_GIVEN 0x0004U
#define EVENT 0
#define TIMEOUT 1
#define LOGIC_ERROR 2

long waitForEvent(double timeLimit, double interval,
                  long invert);
long rpn_stack_test(long stack_ptr, long numneeded, char *stackname, char *caller);
long rpn_pop_log(long *logical);
long rpn_push_log(long logical);
void rpn_log_and(void);
void rpn_log_or(void);
void rpn_log_not(void);
void rpn_clear_stack();
void EventHandler(struct event_handler_args event);
void StringEventHandler(struct event_handler_args event);
void oag_ca_exception_handler(struct exception_handler_args args);
long hasStarted = 0;

double pendIOTime;
WAITFOR *waitFor;
long waitFors;

#if (EPICS_VERSION > 3)
long waitForEventPVA(PVA_OVERALL *pva, double timeLimit, double interval, long invert);
#endif

int main(int argc, char **argv) {
  long i_arg, j, terminateLoop;
  unsigned long flags;
  SCANNED_ARG *s_arg;
  double interval, timeLimit, totalTimeLimit, startTime;
  //long useMonitor;
  long code = 0, repeats, eventEndPending;
  char *emitOnEvent, *emitOnTimeout, *emitOnEnd;
  char **preEvent, **postEvent, **onEvent, **onEnd;
  SUBPROCESS *subprocess;
  long preEvents, postEvents, onEvents, onEnds, subprocesses;
  long noWarnings = 0;
  long providerMode = 0;
#if (EPICS_VERSION > 3)
  PVA_OVERALL pva;
#endif

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  waitFor = NULL;
  waitFors = 0;
  interval = 0.1;
  timeLimit = totalTimeLimit = -1;
  //useMonitor = 0;
  repeats = 1;
  emitOnEvent = emitOnTimeout = emitOnEnd = NULL;
  preEvent = postEvent = onEvent = onEnd = NULL;
  subprocess = NULL;
  preEvents = postEvents = onEvents = onEnds = subprocesses = 0;
  pendIOTime = 10;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], (char *)"_");
      switch (code = match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_INTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &interval) != 1 ||
            interval <= 0)
          SDDS_Bomb((char *)"invalid -interval syntax/value");
        break;
      case CLO_TIMELIMIT:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &timeLimit) != 1 ||
            timeLimit <= 0)
          SDDS_Bomb((char *)"invalid -timeLimit syntax/value");
        break;
      case CLO_WAITFOR:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb((char *)"invalid -waitFor syntax");
        waitFor = (WAITFOR *)trealloc(waitFor, sizeof(*waitFor) * (waitFors + 1));
        waitFor[waitFors].PVname = s_arg[i_arg].list[1];
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&waitFor[waitFors].flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "lowerlimit", SDDS_DOUBLE, &waitFor[waitFors].lowerLimit, 1, LOWERLIMIT_GIVEN,
                          "upperlimit", SDDS_DOUBLE, &waitFor[waitFors].upperLimit, 1, UPPERLIMIT_GIVEN,
                          "above", SDDS_DOUBLE, &waitFor[waitFors].above, 1, ABOVE_GIVEN,
                          "below", SDDS_DOUBLE, &waitFor[waitFors].below, 1, BELOW_GIVEN,
                          "equal", SDDS_DOUBLE, &waitFor[waitFors].equalTo, 1, EQUALTO_GIVEN,
                          "sameas", SDDS_STRING, &waitFor[waitFors].sameAs, 1, SAME_AS_GIVEN,
                          "changed", -1, NULL, 0, CHANGED_GIVEN,
                          NULL) ||
            !waitFor[waitFors].flags ||
            (waitFor[waitFors].flags & LOWERLIMIT_GIVEN && !(waitFor[waitFors].flags & UPPERLIMIT_GIVEN)) ||
            (!(waitFor[waitFors].flags & LOWERLIMIT_GIVEN) && waitFor[waitFors].flags & UPPERLIMIT_GIVEN) ||
            (waitFor[waitFors].flags & LOWERLIMIT_GIVEN &&
             waitFor[waitFors].lowerLimit >= waitFor[waitFors].upperLimit) ||
            (waitFor[waitFors].flags & SAME_AS_GIVEN && waitFor[waitFors].flags & (~SAME_AS_GIVEN)))
          SDDS_Bomb((char *)"invalid -waitFor syntax/values");
        waitFor[waitFors].operations = 0;
        waitFor[waitFors].operation = NULL;
        waitFor[waitFors].changeReferenceLoaded = 0;
        waitFors++;
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_EZCATIMING:
        break;
      case CLO_USEMONITOR:
        //useMonitor = 1;
        break;
      case CLO_NOWARN:
        noWarnings = 1;
        break;
      case CLO_NOT:
      case CLO_OR:
      case CLO_AND:
        if (waitFors < 1 && code == CLO_NOT)
          SDDS_Bomb((char *)"invalid syntax---give a -waitFor option before -not");
        if (waitFors < 2 && code != CLO_NOT)
          SDDS_Bomb((char *)"invalid syntax---give at least two -waitFor options before -or or -and");
        waitFor[waitFors - 1].operation = (short *)SDDS_Realloc(waitFor[waitFors - 1].operation,
                                                                sizeof(*waitFor[waitFors - 1].operation) * (waitFor[waitFors - 1].operations + 1));
        waitFor[waitFors - 1].operation[waitFor[waitFors - 1].operations] = code;
        waitFor[waitFors - 1].operations++;
        break;
      case CLO_REPEAT:
        if (s_arg[i_arg].n_items == 1)
          repeats = 0; /* indefinite number of repeats */
        else if (s_arg[i_arg].n_items != 2 ||
                 sscanf(s_arg[i_arg].list[1], "%ld", &repeats) != 1 ||
                 repeats < 1)
          SDDS_Bomb((char *)"invalid -repeats syntax/value");
        break;
      case CLO_EMIT:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "event", SDDS_STRING, &emitOnEvent, 1, EMIT_EVENT_GIVEN,
                          "timeout", SDDS_STRING, &emitOnTimeout, 1, EMIT_TIMEOUT_GIVEN,
                          "end", SDDS_STRING, &emitOnEnd, 1, EMIT_END_GIVEN,
                          NULL) ||
            !(flags & EMIT_EVENT_GIVEN))
          SDDS_Bomb((char *)"invalid -emit syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_PREEVENT:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"invalid -preEvent syntax");
        if (!(preEvent = (char **)SDDS_Realloc(preEvent, sizeof(*preEvent) * (preEvents + 1))))
          SDDS_Bomb((char *)"allocation failure");
        preEvent[preEvents] = s_arg[i_arg].list[1];
        preEvents++;
        break;
      case CLO_POSTEVENT:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"invalid -postEvent syntax");
        if (!(postEvent = (char **)SDDS_Realloc(postEvent, sizeof(*postEvent) * (postEvents + 1))))
          SDDS_Bomb((char *)"allocation failure");
        postEvent[postEvents] = s_arg[i_arg].list[1];
        postEvents++;
        break;
      case CLO_SUBPROCESS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -subprocess syntax");
        if (!(subprocess = (SUBPROCESS *)SDDS_Realloc(subprocess, sizeof(*subprocess) * (subprocesses + 1))))
          SDDS_Bomb((char *)"allocation failure");
        subprocess[subprocesses].command = s_arg[i_arg].list[1];
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "event", SDDS_STRING, &subprocess[subprocesses].emitOnEvent, 1, EMIT_EVENT_GIVEN,
                          "timeout", SDDS_STRING, &subprocess[subprocesses].emitOnTimeout, 1, EMIT_TIMEOUT_GIVEN,
                          "end", SDDS_STRING, &subprocess[subprocesses].emitOnEnd, 1, EMIT_END_GIVEN,
                          NULL) ||
            !(flags & EMIT_EVENT_GIVEN))
          SDDS_Bomb((char *)"invalid -subprocess syntax/values");
        subprocess[subprocesses].fp = NULL;
        subprocesses++;
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_ONEVENT:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"invalid -onEvent syntax");
        if (!(onEvent = (char **)SDDS_Realloc(onEvent, sizeof(*onEvent) * (onEvents + 1))))
          SDDS_Bomb((char *)"allocation failure");
        onEvent[onEvents] = s_arg[i_arg].list[1];
        onEvents++;
        break;
      case CLO_ONEND:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"invalid -onEnd syntax");
        if (!(onEnd = (char **)SDDS_Realloc(onEnd, sizeof(*onEnd) * (onEnds + 1))))
          SDDS_Bomb((char *)"allocation failure");
        onEnd[onEnds] = s_arg[i_arg].list[1];
        onEnds++;
        break;
      case CLO_TOTALTIMELIMIT:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &totalTimeLimit) != 1 ||
            totalTimeLimit <= 0)
          SDDS_Bomb((char *)"invalid -totalTimeLimit syntax/value");
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2)
          bomb((char *)"wrong number of items for -pendIoTime", NULL);
        if (sscanf(s_arg[i_arg].list[1], "%lf", &pendIOTime) != 1 ||
            pendIOTime <= 0)
          bomb((char *)"invalid -pendIoTime value (cawait)", NULL);
        break;
      case CLO_PROVIDER:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"no value given for option -provider");
        if ((providerMode = match_string(s_arg[i_arg].list[1], providerOption,
                                         PROVIDER_COUNT, 0)) < 0)
          SDDS_Bomb((char *)"invalid -provider");
        break;
      default:
        SDDS_Bomb((char *)"unknown option");
        break;
      }
    } else
      SDDS_Bomb((char *)"unknown option");
  }

  if (!waitFors)
    SDDS_Bomb((char *)"-waitFor option must be given at least once");
  if (providerMode == PROVIDER_PVA) {
#if (EPICS_VERSION > 3)
    //Allocate memory for pva structure
    allocPVA(&pva, waitFors, 0);
    //List PV names
    epics::pvData::shared_vector<std::string> names(pva.numPVs);
    epics::pvData::shared_vector<std::string> provider(pva.numPVs);
    for (j = 0; j < pva.numPVs; j++) {
      if (strncmp(waitFor[j].PVname, "pva://", 6) == 0) {
        waitFor[j].PVname += 6;
        names[j] = waitFor[j].PVname;
        waitFor[j].PVname -= 6;
        provider[j] = "pva";
      } else if (strncmp(waitFor[j].PVname, "ca://", 5) == 0) {
        waitFor[j].PVname += 5;
        names[j] = waitFor[j].PVname;
        waitFor[j].PVname -= 5;
        provider[j] = "ca";
      } else {
        names[j] = waitFor[j].PVname;
        provider[j] = "pva";
      }
    }
    pva.pvaChannelNames = freeze(names);
    pva.pvaProvider = freeze(provider);
    //Connect to PVs
    ConnectPVA(&pva, pendIOTime);

    for (j = 0; j < pva.numPVs; j++) {
      if (!pva.isConnected[j]) {
        fprintf(stderr, "Error (cawait): problem doing search for %s\n", waitFor[j].PVname);
        return (1);
      }
    }
    //Get initial values
    if (GetPVAValues(&pva) == 1) {
      return (1);
    }
    if (MonitorPVAValues(&pva) == 1) {
      return (1);
    }
    epicsThreadSleep(.1);
    if (PollMonitoredPVA(&pva)) {
      //fprintf(stderr, "something changed\n");
    }
#else
    fprintf(stderr, "-provider=pva is only available with newer versions of EPICS\n");
    return (1);
#endif
  } else {
    if (ca_task_initialize() != ECA_NORMAL) {
      fprintf(stderr, "Error (cawait): problem initializing channel access\n");
      return (1);
    }
    ca_add_exception_event(oag_ca_exception_handler, NULL);

    /* connect to all PVs */
    for (j = 0; j < waitFors; j++) {
      if (ca_search(waitFor[j].PVname, &waitFor[j].channelID) != ECA_NORMAL) {
        fprintf(stderr, "Error (cawait): problem doing search for %s\n",
                waitFor[j].PVname);
        ca_task_exit();
        return (1);
      }
      waitFor[j].usrValue = j;
    }

    ca_pend_io(pendIOTime);

#ifdef DEBUG
    fprintf(stderr, "Returned from ca_pend_io after ca_search calls\n");
#endif

    for (j = 0; j < waitFors; j++) {
      if (ca_state(waitFor[j].channelID) != cs_conn) {
        fprintf(stderr, "Error (cawait): no connection in time for %s\n",
                waitFor[j].PVname);
        ca_task_exit();
        return (1);
      }
      if (!(waitFor[j].flags & SAME_AS_GIVEN)) {
        waitFor[j].channelType = DBF_DOUBLE;
        if (ca_add_masked_array_event(DBR_TIME_DOUBLE, 1, waitFor[j].channelID,
                                      EventHandler, (void *)&waitFor[j].usrValue,
                                      (ca_real)0, (ca_real)0, (ca_real)0,
                                      NULL, DBE_VALUE) != ECA_NORMAL) {
          fprintf(stderr, "error: unable to setup callback for PV %s\n", waitFor[j].PVname);
          ca_task_exit();
          return (1);
        }
      } else {
        waitFor[j].channelType = DBF_STRING;
        if (ca_add_masked_array_event(DBR_TIME_STRING, 1, waitFor[j].channelID,
                                      StringEventHandler, (void *)&waitFor[j].usrValue,
                                      (ca_real)0, (ca_real)0, (ca_real)0,
                                      NULL, DBE_VALUE) != ECA_NORMAL) {
          fprintf(stderr, "error: unable to setup callback for PV %s\n", waitFor[j].PVname);
          ca_task_exit();
          return (1);
        }
      }
    }
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "Error (cawait): unable to setup callbacks for PVs\n");
      ca_task_exit();
      return (1);
    }
#ifdef DEBUG
    fprintf(stderr, "ca_pend_io called after ca_add_masked_array_event calls\n");
#endif
  }

  /* initialize the subprocesses */
  for (j = 0; j < subprocesses; j++) {
    if (!(subprocess[j].fp = popen(subprocess[j].command, "w"))) {
      fprintf(stderr, "Error (cawait): problem starting subprocess with command %s\n", subprocess[j].command);
      if (providerMode != PROVIDER_PVA)
        ca_task_exit();
      return (1);
    }
  }

  startTime = getTimeInSecs();
  if (totalTimeLimit > 0 && totalTimeLimit < timeLimit) {
    timeLimit = totalTimeLimit;
  }
  eventEndPending = 0; /* indicates that the end of the last event is pending. */
  do {
#ifdef DEBUG
    fprintf(stderr, "At top of main loop\n");
#endif
    terminateLoop = 0;
    if (totalTimeLimit > 0 && ((getTimeInSecs() - startTime) > totalTimeLimit)) {
      terminateLoop = 1;
    }

    if (!eventEndPending) {
      if (!terminateLoop) {
        /* execute pre-event commands */
        for (j = 0; j < preEvents; j++) {
          system(preEvent[j]);
        }

        if (providerMode == PROVIDER_PVA) {
#if (EPICS_VERSION > 3)
          /* wait for event to become true */
          code = waitForEventPVA(&pva, timeLimit, interval, 0);
#endif
        } else {
          code = waitForEvent(timeLimit, interval, 0);
        }

        if (code == LOGIC_ERROR) {
          fputs("logic error---probably too few operators (cawait)\n", stderr);
          if (providerMode != PROVIDER_PVA)
            ca_task_exit();
          return (1);
        }

        /* emit messages to standard output, as requested */
        if (emitOnEvent) {
          switch (code) {
          case EVENT:
            puts(emitOnEvent);
            fflush(stdout);
            break;
          case TIMEOUT:
            if (emitOnTimeout) {
              puts(emitOnTimeout);
              fflush(stdout);
            }
            break;
          default:
            break;
          }
        }

        /* emit messages to subprocesses, as requested */
        for (j = 0; j < subprocesses; j++) {
          switch (code) {
          case EVENT:
            fputs(subprocess[j].emitOnEvent, subprocess[j].fp);
            fputc('\n', subprocess[j].fp);
            fflush(subprocess[j].fp);
            break;
          case TIMEOUT:
            if (subprocess[j].emitOnTimeout) {
              fputs(subprocess[j].emitOnTimeout, subprocess[j].fp);
              fputc('\n', subprocess[j].fp);
              fflush(subprocess[j].fp);
            }
            break;
          }
        }

        /* execute on-event commands */
        if (code == EVENT) {
          for (j = 0; j < onEvents; j++) {
            system(onEvent[j]);
          }
        }
      }
    }

    if (repeats == 1 || terminateLoop) {
      if (!eventEndPending) {
        /* last repeat */
        if (emitOnEnd) {
          /* emit end-of-run message to stdout */
          puts(emitOnEnd);
          fflush(stdout);
        }
        /* execute end-of-run commands */
        for (j = 0; j < onEnds; j++) {
          system(onEnd[j]);
        }
        /* emit end-of-run messages to subprocesses */
        for (j = 0; j < subprocesses; j++) {
          if (subprocess[j].emitOnEnd) {
            fputs(subprocess[j].emitOnEnd, subprocess[j].fp);
            fputc('\n', subprocess[j].fp);
            fflush(subprocess[j].fp);
          }
        }
#if (EPICS_VERSION > 3)
        if (providerMode == PROVIDER_PVA)
          freePVA(&pva);
#endif
        free_scanargs(&s_arg, argc);
        if (terminateLoop || eventEndPending) {
          if (providerMode != PROVIDER_PVA)
            ca_task_exit();
          return (0);
        }
        /* exit with error if non-event end */
        return (code == EVENT ? 0 : 1);
      }
    }

#ifdef DEBUG
    fprintf(stderr, "Waiting for event\n");
#endif
    if (providerMode == PROVIDER_PVA) {
#if (EPICS_VERSION > 3)
      /* wait for event to become false */
      code = waitForEventPVA(&pva, timeLimit, interval, 1);
#endif
    } else {
      code = waitForEvent(timeLimit, interval, 1);
    }
    if (code == TIMEOUT) {
      eventEndPending = 1;
      if (!noWarnings) {
        fprintf(stderr, "warning: timeout error waiting for event to clear (cawait)\n");
      }
    } else {
      eventEndPending = 0;
      /* execute post-event commands */
      for (j = 0; j < postEvents; j++) {
        system(postEvent[j]);
      }
    }
    for (j = 0; j < waitFors; j++) {
      waitFor[j].changeReferenceLoaded = 0;
    }
  } while (--repeats);

  if (providerMode != PROVIDER_PVA)
    ca_task_exit();

  SDDS_Bomb((char *)"Abnormal exit from repeats loop--this shouldn't happen");
  return (1);
}

void EventHandler(struct event_handler_args event) {
  struct dbr_time_double *dbrValue;
  long index;
  index = *((long *)event.usr);
  dbrValue = (struct dbr_time_double *)event.dbr;
  waitFor[index].value = (double)dbrValue->value;
}

void StringEventHandler(struct event_handler_args event) {
  struct dbr_time_string *dbrValue;
  long index;
  index = *((long *)event.usr);
  dbrValue = (struct dbr_time_string *)event.dbr;
  snprintf(waitFor[index].stringValue, sizeof(waitFor[index].stringValue), "%s", (char *)dbrValue->value);
  ;
}

void sleep_us(unsigned int nusecs) {
  struct timeval tval;

  tval.tv_sec = nusecs / 1000000;
  tval.tv_usec = nusecs % 1000000;
  select(0, NULL, NULL, NULL, &tval);
}

#define RPN_LOGICSTACKSIZE 500

/* stack for logical operations */
short rpn_logicstack[RPN_LOGICSTACKSIZE];
long rpn_lstackptr = 0;

long waitForEvent(double timeLimit, double interval,
                  long invert) {
  double time0, timeNow;
  long j, k, result, term;

  time0 = getTimeInSecs();
  if (timeLimit > 0)
    timeLimit += time0;
  timeNow = getTimeInSecs();
  while (timeLimit < 0 || timeNow < timeLimit) {
#ifdef DEBUG
    fprintf(stderr, "At top of while loop in waitForEvent()\n");
#endif
    if (!hasStarted) {
      for (j = 0; j < waitFors; j++) {
        /* request data for all PVs */
        if (ca_get(waitFor[j].channelType, waitFor[j].channelID,
                   (waitFor[j].channelType == DBF_DOUBLE ? (void *)&waitFor[j].value : (void *)&waitFor[j].stringValue)) != ECA_NORMAL) {
          fprintf(stderr, "Error (cawait): problem doing ca_get for %s\n",
                  waitFor[j].PVname);
          ca_task_exit();
          exit(1);
        }
      }
      if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
        fprintf(stderr, "Error (cawait): problem doing ca_get for PVs\n");
        ca_task_exit();
        exit(1);
      }
      hasStarted = 1;
#ifdef DEBUG
      for (j = 0; j < waitFors; j++) {
        switch (waitFor[j].channelType) {
        case DBF_DOUBLE:
          fprintf(stderr, "Initial %s = %e\n", waitFor[j].PVname, waitFor[j].value);
          break;
        default:
          fprintf(stderr, "Initial %s = %s\n", waitFor[j].PVname, waitFor[j].stringValue);
          break;
        }
      }
#endif
    }
    for (j = 0; j < waitFors; j++) {
      if (waitFor[j].flags & CHANGED_GIVEN && !waitFor[j].changeReferenceLoaded) {
        waitFor[j].changeReference = waitFor[j].value;
        waitFor[j].changeReferenceLoaded = 1;
      }
    }
    rpn_clear_stack();
    for (j = 0; j < waitFors; j++) {
      term = 1;
      if (waitFor[j].flags & SAME_AS_GIVEN) {
        term = !strcmp(waitFor[j].stringValue, waitFor[j].sameAs);
      } else if (waitFor[j].flags & EQUALTO_GIVEN) {
        if (waitFor[j].value != waitFor[j].equalTo)
          term = 0;
      } else if (waitFor[j].flags & (LOWERLIMIT_GIVEN + UPPERLIMIT_GIVEN)) {
        if (waitFor[j].value < waitFor[j].lowerLimit ||
            waitFor[j].value > waitFor[j].upperLimit)
          term = 0;
      } else if (waitFor[j].flags & ABOVE_GIVEN) {
        if (waitFor[j].value <= waitFor[j].above)
          term = 0;
      } else if (waitFor[j].flags & BELOW_GIVEN) {
        if (waitFor[j].value >= waitFor[j].below)
          term = 0;
      } else if (waitFor[j].flags & CHANGED_GIVEN) {
        if (waitFor[j].value == waitFor[j].changeReference)
          term = 0;
        else
          waitFor[j].changeReference = waitFor[j].value;
      } else {
        fprintf(stderr, "no flags set for PV %s--this shouldn't happen\n",
                waitFor[j].PVname);
        ca_task_exit();
        exit(1);
      }
      rpn_push_log(term);
      for (k = 0; k < waitFor[j].operations; k++) {
        switch (waitFor[j].operation[k]) {
        case OP_NOT:
          rpn_log_not();
          break;
        case OP_OR:
          rpn_log_or();
          break;
        case OP_AND:
          rpn_log_and();
          break;
        default:
          ca_task_exit();
          SDDS_Bomb((char *)"invalid operation seen---this shouldn't happen");
          break;
        }
      }
    }
    rpn_pop_log(&result);
    if (rpn_lstackptr != 0)
      return LOGIC_ERROR;
    if (invert)
      result = !result;
    if (result)
      return EVENT;
#ifdef DEBUG
    fprintf(stderr, "At bottom of while loop in waitForEvent().  Calling ca_pend_event().\n");
#endif
    ca_pend_event(interval);
    timeNow = getTimeInSecs();
  }
  return TIMEOUT;
}

#if (EPICS_VERSION > 3)
long waitForEventPVA(PVA_OVERALL *pva, double timeLimit, double interval, long invert) {
  double time0, timeNow;
  long j, k, result, term;
  time0 = getTimeInSecs();
  if (timeLimit > 0)
    timeLimit += time0;
  timeNow = getTimeInSecs();
  while (timeLimit < 0 || timeNow < timeLimit) {
    if (!hasStarted) {
      hasStarted = 1;
    }
    for (j = 0; j < waitFors; j++) {
      if ((pva->isConnected[j]) && (pva->pvaData[j].fieldType == epics::pvData::scalar)) {
        if (waitFor[j].flags & CHANGED_GIVEN && !waitFor[j].changeReferenceLoaded) {
          waitFor[j].changeReference = pva->pvaData[j].getData[0].values[0];
          waitFor[j].changeReferenceLoaded = 1;
        }
      }
    }
    rpn_clear_stack();
    term = -1;
    for (j = 0; j < waitFors; j++) {
      if ((pva->isConnected[j]) && (pva->pvaData[j].numMonitorReadings > 0)) {
        if (pva->pvaData[j].fieldType == epics::pvData::scalar) {
          term = 1;
          if (waitFor[j].flags & SAME_AS_GIVEN) {
            term = !strcmp(pva->pvaData[j].monitorData[0].stringValues[0], waitFor[j].sameAs);
          } else if (waitFor[j].flags & EQUALTO_GIVEN) {
            if (pva->pvaData[j].monitorData[0].values[0] != waitFor[j].equalTo)
              term = 0;
          } else if (waitFor[j].flags & (LOWERLIMIT_GIVEN + UPPERLIMIT_GIVEN)) {
            if (pva->pvaData[j].monitorData[0].values[0] < waitFor[j].lowerLimit ||
                pva->pvaData[j].monitorData[0].values[0] > waitFor[j].upperLimit)
              term = 0;
          } else if (waitFor[j].flags & ABOVE_GIVEN) {
            if (pva->pvaData[j].monitorData[0].values[0] <= waitFor[j].above)
              term = 0;
          } else if (waitFor[j].flags & BELOW_GIVEN) {
            if (pva->pvaData[j].monitorData[0].values[0] >= waitFor[j].below)
              term = 0;
          } else if (waitFor[j].flags & CHANGED_GIVEN) {
            if (pva->pvaData[j].monitorData[0].values[0] == waitFor[j].changeReference)
              term = 0;
            else
              waitFor[j].changeReference = pva->pvaData[j].monitorData[0].values[0];
          } else {
            fprintf(stderr, "no flags set for PV %s--this shouldn't happen\n", waitFor[j].PVname);
            exit(1);
          }
        } else { //For scalarArray PVs, it may be useful to wait on the .timeStamp.secondsPast
          fprintf(stderr, "error: cawait only works with scalar values.\n");
          exit(1);
        }
        rpn_push_log(term);
        for (k = 0; k < waitFor[j].operations; k++) {
          switch (waitFor[j].operation[k]) {
          case OP_NOT:
            rpn_log_not();
            break;
          case OP_OR:
            rpn_log_or();
            break;
          case OP_AND:
            rpn_log_and();
            break;
          default:
            SDDS_Bomb((char *)"invalid operation seen---this shouldn't happen");
            break;
          }
        }
      }
    }
    if (term != -1) {
      rpn_pop_log(&result);
      if (rpn_lstackptr != 0)
        return LOGIC_ERROR;
      if (invert)
        result = !result;
      if (result)
        return EVENT;
    }
    epicsThreadSleep(interval);
    if (PollMonitoredPVA(pva)) {
      //fprintf(stderr, "something changed\n");
    }
    timeNow = getTimeInSecs();
  }
  return TIMEOUT;
}
#endif

void rpn_clear_stack() {
  rpn_lstackptr = 0;
}

long rpn_stack_test(long stack_ptr, long numneeded, char *stackname, char *caller) {
  if (stack_ptr < numneeded) {
    fprintf(stderr, "too few items on %s stack (%s)\n", stackname, caller);
    return 0;
  }
  return (1);
}

long rpn_pop_log(long *logical) {
  if (rpn_lstackptr < 1) {
    fputs("too few items on logical stack (rpn_pop_log)\n", stderr);
    ca_task_exit();
    exit(1);
  }
  *logical = rpn_logicstack[--rpn_lstackptr];
  return (1);
}

long rpn_push_log(long logical) {
  if (rpn_lstackptr == RPN_LOGICSTACKSIZE) {
    fputs("stack overflow--logical stack size exceeded (rpn_push_log)\n", stderr);
    ca_task_exit();
    exit(1);
  }
  rpn_logicstack[rpn_lstackptr++] = logical;
  return (1);
}

void rpn_log_and(void) {
  if (!rpn_stack_test(rpn_lstackptr, 2, (char *)"logical", (char *)"rpn_log_and")) {
    ca_task_exit();
    exit(1);
  }
  rpn_logicstack[rpn_lstackptr - 2] = (rpn_logicstack[rpn_lstackptr - 1] && rpn_logicstack[rpn_lstackptr - 2]);
  rpn_lstackptr--;
}

void rpn_log_or(void) {
  if (!rpn_stack_test(rpn_lstackptr, 2, (char *)"logical", (char *)"rpn_log_or")) {
    ca_task_exit();
    exit(1);
  }
  rpn_logicstack[rpn_lstackptr - 2] = (rpn_logicstack[rpn_lstackptr - 1] || rpn_logicstack[rpn_lstackptr - 2]);
  rpn_lstackptr--;
}

void rpn_log_not(void) {
  if (!rpn_stack_test(rpn_lstackptr, 1, (char *)"logical", (char *)"rpn_log_not")) {
    ca_task_exit();
    exit(1);
  }
  rpn_logicstack[rpn_lstackptr - 1] = !rpn_logicstack[rpn_lstackptr - 1];
}

void oag_ca_exception_handler(struct exception_handler_args args) {
  char *pName;
  int severityInt;
  static const char *severity[] =
    {
      "Warning",
      "Success",
      "Error",
      "Info",
      "Fatal",
      "Fatal",
      "Fatal",
      "Fatal"};

  severityInt = CA_EXTRACT_SEVERITY(args.stat);

  if ((severityInt != 1) && (severityInt != 3)) {
    fprintf(stderr, "CA.Client.Exception................\n");
    fprintf(stderr, "    %s: \"%s\"\n",
            severity[severityInt],
            ca_message(args.stat));

    if (args.ctx) {
      fprintf(stderr, "    Context: \"%s\"\n", args.ctx);
    }
    if (args.chid) {
      pName = (char *)ca_name(args.chid);
      fprintf(stderr, "    Channel: \"%s\"\n", pName);
      fprintf(stderr, "    Type: \"%s\"\n", dbr_type_to_text(args.type));
    }
    fprintf(stderr, "This sometimes indicates an IOC that is hung up.\n");
    _exit(1);
  } else {
    fprintf(stdout, "CA.Client.Exception................\n");
    fprintf(stdout, "    %s: \"%s\"\n",
            severity[severityInt],
            ca_message(args.stat));

    if (args.ctx) {
      fprintf(stdout, "    Context: \"%s\"\n", args.ctx);
    }
    if (args.chid) {
      pName = (char *)ca_name(args.chid);
      fprintf(stdout, "    Channel: \"%s\"\n", pName);
      fprintf(stdout, "    Type: \"%s\"\n", dbr_type_to_text(args.type));
    }
  }
}
