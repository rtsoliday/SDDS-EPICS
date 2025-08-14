/*************************************************************************\
 * Copyright (c) 2024 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

#include <stdlib.h>
#ifndef _WIN32
#  include <sys/time.h>
#endif
#include <sys/types.h>
#include <time.h>
#include <signal.h>
/*#include <tsDefs.h>*/
#include <cadef.h>
#include <errno.h>
#include <string.h>

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void user_interrupt_handler(int sig);
volatile int sigint = 0;

#define CA_INTERVAL 0
#define CA_REPETITIONS 1
#define CA_TRIGGERPV 2
#define CA_CONTROLNAME_PARAMETER 3
#define CA_DATA_COLUMN 4
#define CA_VERBOSE 5
#define CA_PENDIOTIME 6
#define CA_MODE 7
#define CA_RESTORE 8
#define CA_OFFSET 9
#define CA_FACTOR 10
#define CA_TIMEOFFSET 11
#define CLO_RUNCONTROLPV 12
#define CLO_RUNCONTROLDESC 13
#define N_CA_OPTIONS 14
char *commandlineOption[N_CA_OPTIONS] =
  {
   "interval", "repetitions", "triggerpv", "controlnameparameter", "datacolumn", "verbose",  "pendiotime", "mode",
   "restore", "offset", "factor", "timeoffset", "runcontrolpv", "runcontroldescription",
  };

typedef struct
{
  char *name;
  double startingValue, value;
  double *data;
  chid PVchid;
} PVDATA;

PVDATA *pvData = NULL;
long nPvs = 0;

typedef struct
{
  char *PV;
  chid channelID;
  double currentValue;
  short triggered, initialized;
  /* used with channel access routines to give index via callback: */
  long usrValue;
  long trigStep; /* trigStep: the Step where trigger occurs */
  long datastrobeMissed;
} DATASTROBE_TRIGGER;

DATASTROBE_TRIGGER datastrobeTrigger;
/* for datastrobe trigger, from sddslogger */
void datastrobeTriggerEventHandler(struct event_handler_args event);
long setupDatastrobeTriggerCallbacks(DATASTROBE_TRIGGER *datastrobeTrigger);

static char *USAGE = "sddscaplayback <inputFile> -interval=<seconds> [-repetitions=<number>]\n\
-mode={absolute|differential} [-triggerPV=<string>] [-controlNameParameter=<string>] [-dataColumn=<columnName>]\n\
[-offset=<integer>] [-timeOffset=<seconds>] [-factor=<value>] [-restore] [-verbose] [-pendIOtime=<seconds>]\n\
[-runControlPV=string=<string>,pingTimeout=<value>] [-runControlDescription=string=<string>]\n\n\
Takes an SDDS file containing several pages of data and \"plays back\" the data, interpreting each page\n\
as a waveform of numerical values for a different PV.\n\n\
<inputFile>   SDDS file with ControlName parameter and Value column.\n\
-interval     Interval in floating-point seconds between sending data from successive pages.\n\
-repetitions  Number of times to loop through the pages. Default is infinite.\n\
-mode         Specify whether to send the Value data as given, or as differences relative to the\n\
              starting values.\n\
-triggerPV    PV to monitor for changes. When it changes, the playback starts from the first page.\n\
-controlNameParameter\n\
              Name of column in input file giving the PV names. Defaults to \"ControlName\".\n\
-dataColumn   Name of column in input file giving the delta values. Defaults to \"Value\".\n\
-offset       Number of pages to offset by (circular buffer).\n\
-timeOffset   Fine offset of the timing. Must be less than the interval.\n\
-factor       Multiply waveforms by given factor prior to use.\n\
-restore      Set PVs back to starting values before exiting.\n\
-verbose      Output status information.\n\
-pendIoTime   Set the CA pend IO time.\n\
-runControlPV specifies a runControl PV name.\n\
-runControlDescription\n\
              specifies a runControl PV description record.\n\n\
Program by Michael Borland.\n\
Link date: "__DATE__
                     " "__TIME__
                     ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#ifdef USE_RUNCONTROL
static char *runControlPVUsage = "-runControlPV=string=<string>\n";
static char *runControlDescUsage = "-runControlDescription=string=<string>\n";
int runControlPing1();
typedef struct
{
  char *PV, *Desc;
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status;
  RUNCONTROL_INFO rcInfo;
} RUNCONTROL_PARAM;
RUNCONTROL_PARAM rcParam;
void rc_interrupt_handler();
#endif

#define MODE_ABSOLUTE 0
#define MODE_DIFFERENTIAL 1
#define N_MODE_OPTIONS 2
char *modeOption[N_MODE_OPTIONS] =
  {
   "absolute", "differential",
  };

void playBackData(long nRows, double interval, short differential, short verbose,
                  double pendIoTime, short datastrobeTriggerPresent, long offset, double factor, double timeOffset);
void restoreOriginalValues(double pendIoTime);

int main(int argc, char **argv)
{
  char *inputFile=NULL, *controlNameParameter="ControlName", *dataColumn="Value";
  long nRows, nRows0=0, repetitions=0, j;
  SDDS_DATASET SDDSin;
  SCANNED_ARG *s_arg;
  double pendIoTime = 10.0, interval=-1, factor = 1.0, timeOffset = 0;
  short modeSeen = 0, differential = 0, verbose = 0, datastrobeTriggerPresent = 0, restore = 0;
  long i_arg, code, offset=0;
  struct timespec sleepInterval, remainingInterval;
  unsigned long dummyFlags;

#ifdef USE_RUNCONTROL
  rcParam.PV = NULL;
  cp_str(&rcParam.Desc, "sddscaplayback");
  /* pingInterval should be short enough so that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;
#endif

  SDDS_RegisterProgramName(argv[0]);

  signal(SIGUSR1, user_interrupt_handler);   /* signals reading the input file again */
  signal(SIGINT, sigint_interrupt_handler);  /* trapped quit signal to allow cleanup */
  signal(SIGTERM, sigint_interrupt_handler); /* trapped quit signal to allow cleanup */
  /* We just exit for these signals */
  signal(SIGILL, interrupt_handler);
  signal(SIGABRT, interrupt_handler);
  signal(SIGFPE, interrupt_handler);
  signal(SIGSEGV, interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, sigint_interrupt_handler); /* trapped quit signal to allow cleanup */
  /* We just exit for these signals */
  signal(SIGHUP, interrupt_handler);
  signal(SIGTRAP, interrupt_handler);
  signal(SIGBUS, interrupt_handler);
#endif

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  datastrobeTrigger.PV = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandlineOption, N_CA_OPTIONS, 0)) {
      case CA_INTERVAL:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &interval) != 1 ||
            interval <= 0)
          SDDS_Bomb("invalid -interval syntax\n");
        break;
      case CA_REPETITIONS:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &repetitions) != 1 ||
            repetitions < 0)
          SDDS_Bomb("invalid -repetitions syntax\n");
        break;
      case CA_OFFSET:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &offset) != 1 ||
            offset < 0)
          SDDS_Bomb("invalid -offset syntax\n");
        break;
      case CA_FACTOR:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &factor) != 1)
          SDDS_Bomb("invalid -factor syntax\n");
        break;
      case CA_TIMEOFFSET:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &timeOffset) != 1 || timeOffset<0)
          SDDS_Bomb("invalid -timeOffset syntax\n");
        break;
      case CA_TRIGGERPV:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("wrong number of items for -triggerPV");
        datastrobeTrigger.PV = s_arg[i_arg].list[1];
        datastrobeTriggerPresent = 1;
        break;
      case CA_CONTROLNAME_PARAMETER:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("wrong number of items for -controlNameParameter");
        controlNameParameter = s_arg[i_arg].list[1];
        break;
      case CA_DATA_COLUMN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("wrong number of items for -dataColumn");
        dataColumn = s_arg[i_arg].list[1];
        break;
      case CA_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIoTime) != 1 ||
            pendIoTime <= 0)
          SDDS_Bomb("invalid -pendIoTime syntax\n");
        break;
      case CA_VERBOSE:
        verbose = 1;
        break;
      case CA_RESTORE:
        restore = 1;
        break;
      case CA_MODE:
        modeSeen = 1;
        if (s_arg[i_arg].n_items!=2)
          SDDS_Bomb("invalid -mode syntax");
        switch (match_string(s_arg[i_arg].list[1], modeOption, N_MODE_OPTIONS, 0)) {
        case MODE_ABSOLUTE:
          differential = 0;
          break;
        case MODE_DIFFERENTIAL:
          differential = 1;
          break;
        default:
          SDDS_Bomb("invalid -mode syntax");
          break;
        }
        break;
      case CLO_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          NULL) ||
            (!rcParam.PV))
          bomb("bad -runControlPV syntax", runControlPVUsage);
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlPV ignored.\n");
#endif
        break;
      case CLO_RUNCONTROLDESC:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            (!rcParam.Desc))
          bomb("bad -runControlDesc syntax", runControlDescUsage);
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlDescription ignored.\n");
#endif
      default:
        SDDS_Bomb("unrecognized option given");
        break;
      }
    } else {
      if (inputFile)
        SDDS_Bomb("too many filenames given");
      inputFile = s_arg[i_arg].list[0];
    }
  }

#ifdef EPICS313
  ca_task_initialize();
#else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
#endif

  if (!inputFile)
    SDDS_Bomb("no input file given");
  if (interval<=0)
    SDDS_Bomb("-interval not given");
  if (!modeSeen)
    SDDS_Bomb("-mode not given");
  if (timeOffset>=interval)
    SDDS_Bomb("time offset must be less than interval. Use -offset for large offsets.");

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    rcParam.handle[0] = (char)0;
    rcParam.Desc = (char *)realloc(rcParam.Desc, 41 * sizeof(char));
    rcParam.PV = (char *)realloc(rcParam.PV, 41 * sizeof(char));
    rcParam.status = runControlInit(rcParam.PV, rcParam.Desc, rcParam.pingTimeout, rcParam.handle,
                                    &(rcParam.rcInfo), 10.0);
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Error initializing run control.\n");
      if (rcParam.status == RUNCONTROL_DENIED) {
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      }
      return (1);
    }
  }
  atexit(rc_interrupt_handler);
#endif

  while (1) {
    nRows0 = 0;
    sigint = 0;
    /* this loop allows reading the input file again if SIGUSR1 is received */
    if (verbose)
      fprintf(stderr, "Reading input file\n");
    /* Read the input file */
    if (!SDDS_InitializeInput(&SDDSin, inputFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (SDDS_CheckParameter(&SDDSin, controlNameParameter, NULL, SDDS_STRING, stderr)!=SDDS_CHECK_OK) {
      fprintf(stderr, "Error: %s parameter missing or not string type\n", controlNameParameter);
      exit(1);
    }
    if (SDDS_CheckColumn(&SDDSin, dataColumn, NULL, SDDS_ANY_NUMERIC_TYPE, stderr)!=SDDS_CHECK_OK) {
      fprintf(stderr, "Error: %s column missing or not numerical type\n", dataColumn);
      exit(1);
    }
    nRows = 0;
    while ((code=SDDS_ReadPage(&SDDSin)) > 0) {
      pvData = SDDS_Realloc(pvData, sizeof(*pvData)*(nPvs+1));
      if ((nRows = SDDS_CountRowsOfInterest(&SDDSin)) < 0)
        SDDS_Bomb("Table has no rows.");
      if (verbose)
        fprintf(stderr, "Page %ld has %ld rows\n", code, nRows);
      if (nRows0==0) {
        /* first page */
        nRows0 = nRows;
      }
      if (nRows0!=nRows)
        SDDS_Bomb("Table has inconsistent number of rows.");
      if (!(pvData[nPvs].data = SDDS_GetColumnInDoubles(&SDDSin, dataColumn))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      if (!SDDS_GetParameter(&SDDSin, controlNameParameter, &(pvData[nPvs].name))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      nPvs ++;
    }
    if (code == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (!SDDS_Terminate(&SDDSin)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    
    if (verbose)
      fprintf(stderr, "Searching for PVs\n");
    
    /* Search for PVs */
    for (j = 0; j < nPvs; j++) {
      if (ca_search(pvData[j].name, &(pvData[j].PVchid)) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", pvData[j].name);
        exit(1);
      }
    }
    if (ca_pend_io(pendIoTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      for (j = 0; j < nPvs; j++) {
        if (ca_state(pvData[j].PVchid) != cs_conn)
          fprintf(stderr, "%s not connected\n", pvData[j].name);
      }
      exit(1);
    }
    
    /* Read present values of the PVs. */
    if (verbose)
      fprintf(stderr, "Reading present values for PVs\n");
    for (j = 0; j < nPvs; j++) {
      if (verbose)
        fprintf(stderr, "Reading present value for %s\n", pvData[j].name);
      if (ca_state(pvData[j].PVchid) == cs_conn) {
        if (ca_get(DBR_DOUBLE, pvData[j].PVchid, &(pvData[j].startingValue)) != ECA_NORMAL) {
          fprintf(stderr, "error: problem reading %s\n", pvData[j].name);
          exit(1);
        }
      }
      if (verbose)
        fprintf(stderr, "Present value for %s: %le\n", pvData[j].name, pvData[j].startingValue);
    }
    if (ca_pend_io(pendIoTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem reading some channels\n");
      exit(1);
    }
    
    if (datastrobeTriggerPresent) {
      if (verbose)
        fprintf(stderr, "Setting up trigger\n");
      setupDatastrobeTriggerCallbacks(&datastrobeTrigger);
    }
    
    if (datastrobeTriggerPresent) {
      double checkInterval = interval/10;
      sleepInterval.tv_sec = (time_t)checkInterval;
      sleepInterval.tv_nsec = 1e9*(checkInterval-(long)checkInterval);
    }
  
    do {
      if (datastrobeTriggerPresent) {
        if (verbose)
          fprintf(stderr, "Waiting for trigger\n");
        while (!datastrobeTrigger.triggered) {
          oag_ca_pend_event(0.0, &sigint);
          nanosleep(&sleepInterval, &remainingInterval);
          if (rcParam.PV)
            runControlPing1();
          if (sigint&1)
            break;
        }
        if (verbose && !sigint)
          fprintf(stderr, "Trigger seen\n");
        datastrobeTrigger.triggered = 0;
      }
      if (sigint&1)
        break;
      if (verbose)
        fprintf(stderr, "Playing back\n");
      playBackData(nRows, interval, differential, verbose, pendIoTime, datastrobeTriggerPresent, offset, factor, timeOffset);
      if (datastrobeTriggerPresent && datastrobeTrigger.triggered)
        fprintf(stderr, "Warning: trigger received during playback. Cycles will be misaligned.\n");
      if (rcParam.PV)
        runControlPing1();
    } while (--repetitions!=0 && !sigint);

    if (sigint!=2)
      break;

    restoreOriginalValues(pendIoTime);
    free(pvData);
    nPvs = 0;
  }

  if (restore) {
    if (verbose)
      fprintf(stderr, "Restoring original values\n");
    restoreOriginalValues(pendIoTime);
  }

  if (rcParam.PV)
    rc_interrupt_handler();
  else
    ca_task_exit();
  free_scanargs(&s_arg, argc);
  return (0);
}

void restoreOriginalValues(double pendIoTime)
{
  long j;
  for (j=0; j<nPvs; j++) {
    if (ca_state(pvData[j].PVchid) == cs_conn) {
      if (ca_put(DBR_DOUBLE, pvData[j].PVchid, &pvData[j].startingValue) != ECA_NORMAL) {
        fprintf(stderr, "error: problem setting %s\n", pvData[j].name);
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIoTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem setting for some channels\n");
    exit(1);
  }
}

void playBackData(long nRows, double interval, short differential, short verbose,
                  double pendIoTime, short datastrobeTriggerPresent, long offset, double factor, double timeOffset)
{
  long i, j, k;
  struct timespec sleepInterval, remainingInterval;

  if (timeOffset>0) {
    sleepInterval.tv_sec = (time_t)timeOffset;
    sleepInterval.tv_nsec = 1e9*(timeOffset-(long)timeOffset);
    nanosleep(&sleepInterval, &remainingInterval);
  }

  //fprintf(stderr, "Sleep: %le, %ld, %ld\n", interval, (long)sleepInterval.tv_sec, sleepInterval.tv_nsec);
  sleepInterval.tv_sec = (time_t)interval;
  sleepInterval.tv_nsec = 1e9*(interval-(long)interval);
  //fprintf(stderr, "Sleep: %le, %ld, %ld\n", interval, (long)sleepInterval.tv_sec, sleepInterval.tv_nsec);
  
  for (k=0; k<nRows; k++) {
    i = (k+offset)%nRows;
    if (differential) {
      for (j=0; j<nPvs; j++) {
        pvData[j].value = factor*pvData[j].data[i] + pvData[j].startingValue;
      }
    } else {
      for (j=0; j<nPvs; j++) {
        pvData[j].value = factor*pvData[j].data[i];
      }
    }
    for (j=0; j<nPvs; j++) {
      if (ca_state(pvData[j].PVchid) == cs_conn) {
        if (ca_put(DBR_DOUBLE, pvData[j].PVchid, &pvData[j].value) != ECA_NORMAL) {
          fprintf(stderr, "error: problem setting %s\n", pvData[j].name);
          exit(1);
        }
        if (verbose) {
          fprintf(stderr, "%s = %lf\n", pvData[j].name, pvData[j].value);
        }
      }
    }
    if (ca_pend_io(pendIoTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem setting for some channels\n");
      exit(1);
    }
    /* don't sleep after the last row if we have a trigger, since sleeping might cause us to miss the trigger */
    if ((!datastrobeTriggerPresent || i!=(nRows-1)) && nanosleep(&sleepInterval, &remainingInterval)==-1) 
      fprintf(stderr, "Warning: sleep time not reached.");
  }
}


/* from sddslogger */

void datastrobeTriggerEventHandler(struct event_handler_args event) {
  if (event.status != ECA_NORMAL) {
    fprintf(stderr, "Error received on data strobe PV\n");
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Received callback on data strobe PV\n");
#endif
  if (datastrobeTrigger.initialized == 0) {
    /* the first callback is just the connection event */
    datastrobeTrigger.initialized = 1;
    return;
  }
  datastrobeTrigger.currentValue = *((double *)event.dbr);
#ifdef DEBUG
  fprintf(stderr, "chid=%d, current=%f\n", (int)datastrobeTrigger.channelID, datastrobeTrigger.currentValue);
#endif
  datastrobeTrigger.triggered = 1;
  fprintf(stderr, "Triggered.\n");
  return;
}

long setupDatastrobeTriggerCallbacks(DATASTROBE_TRIGGER *datastrobeTrigger) {
  datastrobeTrigger->trigStep = -1;
  datastrobeTrigger->currentValue = 0;
  datastrobeTrigger->triggered = 0;
  datastrobeTrigger->datastrobeMissed = 0;
  datastrobeTrigger->usrValue = 1;
  datastrobeTrigger->initialized = 0;
  if (ca_search(datastrobeTrigger->PV, &datastrobeTrigger->channelID) != ECA_NORMAL) {
    fprintf(stderr, "error: search failed for control name %s\n", datastrobeTrigger->PV);
    return 0;
  }
  ca_poll();
  if (ca_add_masked_array_event(DBR_DOUBLE, 1, datastrobeTrigger->channelID,
                                datastrobeTriggerEventHandler,
                                (void *)&datastrobeTrigger, (ca_real)0, (ca_real)0,
                                (ca_real)0, NULL, DBE_VALUE) != ECA_NORMAL) {
    fprintf(stderr, "error: unable to setup datastrobe callback for control name %s\n",
            datastrobeTrigger->PV);
    exit(1);
  }
#ifdef DEBUG
  fprintf(stderr, "Set up callback on data strobe PV\n");
#endif
  ca_poll();
  return 1;
}

void interrupt_handler(int sig) {
  exit(1);
}

void user_interrupt_handler(int sig) {
  /* request to read the input file again */
  sigint |= 2;
  fprintf(stderr, "SIGUSR1 received: will read input file again\n");
}

void sigint_interrupt_handler(int sig) {
  sigint |= 1;
  fprintf(stderr, "Quit signal received\n");
  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, interrupt_handler);
#endif
}

/* RC routines from sddslogger.c */

void rc_interrupt_handler() {
  int ca = 1;
#ifndef EPICS313
  if (ca_current_context() == NULL)
    ca = 0;
  if (ca)
    ca_attach_context(ca_current_context());
#endif
  if (ca) {
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      rcParam.status = runControlExit(rcParam.handle, &(rcParam.rcInfo));
      if (rcParam.status != RUNCONTROL_OK) {
        fprintf(stderr, "Error during exiting run control.\n");
      }
    }
#endif
    ca_task_exit();
  }
}

#ifdef USE_RUNCONTROL
/* returns value from same list of statuses as other runcontrol calls */
int runControlPing1() {

  rcParam.status = runControlPing(rcParam.handle, &(rcParam.rcInfo));
  switch (rcParam.status) {
  case RUNCONTROL_ABORT:
    fprintf(stderr, "Run control application aborted.\n");
    sigint = 1; 
    break;
  case RUNCONTROL_TIMEOUT:
    fprintf(stderr, "Run control application timed out.\n");
    strcpy(rcParam.message, "Application timed-out");
    runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
    sigint = 1; 
    break;
  case RUNCONTROL_OK:
    break;
  case RUNCONTROL_ERROR:
    fprintf(stderr, "Communications error with runcontrol record.\n");
    return (RUNCONTROL_ERROR);
  default:
    fprintf(stderr, "Unknown run control error code.\n");
    break;
  }
  return (RUNCONTROL_OK);
}
#endif
