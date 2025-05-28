#include <complex>
#include <iostream>
#include <cerrno>
#include <csignal>

#include <cadef.h>
#include <epicsVersion.h>

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include "pvaSDDS.h"
#include "libruncontrol.h"

#define CLO_VERBOSE 0
#define CLO_PENDIOTIME 1
#define CLO_RUNCONTROLPV 2
#define CLO_RUNCONTROLDESC 3
#define CLO_EXECUTIONTIME 4
#define COMMANDLINE_OPTIONS 5

char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"verbose",
  (char *)"pendIOTime",
  (char *)"runControlPV",
  (char *)"runControlDescription",
  (char *)"executionTime",
};

char *USAGE = (char *)"sddspermissive <inputfile>\n\
 -executionTime=<seconds>\n\
 -runControlPV=string=<string>,pingTimeout=<value>,pingInterval=<value>\n\
 -runControlDescription=string=<string>\n\
[-verbose] [-pendIOTime=<seconds>]\n\
\n\
Required SDDS columns: ControlName (string)\n\
                       Description (string)\n\
                       MinimumValue (numeric)\n\
                       MaximumValue (numeric)\n\
Optional SDDS columns: Provider (string)\n\
Optional SDDS parameters: MaskControlName (string)\n\
                          MaskMinimumValue (numeric)\n\
                          MaskMaximumValue (numeric)\n\
                          MaskProvider (string)\n\
When a PV is out of range, the MSG field in the runcontrol record will be set to the\n\
associated Description value. The run control record will also be put into minor alarm.\n\
The PVs on a page with a mask PV value within the min-max range will be ignored.\n\n\
Program by Robert Soliday, ANL\n\
Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

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

typedef struct
{
  int64_t pvCount, pvMaskCount;
  char **ControlName, **Description;
  double *MinimumValue, *MaximumValue;
  char **Provider;
  char *MaskControlName, *MaskProvider;
  double MaskMinimumValue, MaskMaximumValue;
} MONITOR_DATA_PAGE;

typedef struct
{
  bool verbose;
  char *inputFile;
  double pendIOTime;
  MONITOR_DATA_PAGE *page;
  int32_t pages;
  int64_t total_pvCount;
  double executionTime;
} MONITOR_DATA;

int ReadArguments(int argc, char **argv, MONITOR_DATA *monitor);
int ReadInput(MONITOR_DATA *monitor);
int ConnectRC(MONITOR_DATA *monitor);
long pvaThreadSleep(long double seconds);
int ConnectPVs(PVA_OVERALL *pva, MONITOR_DATA *monitor);
long VerifyNumericScalarPVTypes(PVA_OVERALL *pva);
int EventLoop(PVA_OVERALL *pva, MONITOR_DATA *monitor);
void SetupAtExit();
void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();
volatile int sigint = 0;
char firstPV[100];
class permissiveMonitorRequester;
typedef std::tr1::shared_ptr<permissiveMonitorRequester> permissiveMonitorRequesterPtr;

int main(int argc, char **argv) {
  PVA_OVERALL pva;
  MONITOR_DATA monitor;
  int64_t i, p;
  if (ReadArguments(argc, argv, &monitor) == 1) {
    return (1);
  }
  if (ReadInput(&monitor) == 1) {
    return (1);
  }
  if (ConnectRC(&monitor) == 1) {
    return (1);
  }
  SetupAtExit();
  if (ConnectPVs(&pva, &monitor) == 1) {
    return (1);
  }
  if (EventLoop(&pva, &monitor) == 1) {
    return (1);
  }
  PausePVAMonitoring(&pva);
  for (p = 0; p < monitor.pages; p++) {
    for (i = 0; i < monitor.page[p].pvCount; i++) {
      free(monitor.page[p].ControlName[i]);
      free(monitor.page[p].Description[i]);
      free(monitor.page[p].Provider[i]);
    }
    free(monitor.page[p].ControlName);
    free(monitor.page[p].Description);
    free(monitor.page[p].MinimumValue);
    free(monitor.page[p].MaximumValue);
    free(monitor.page[p].Provider);
  }
  if (monitor.pages)
    free(monitor.page);
  if (monitor.inputFile)
    free(monitor.inputFile);
  freePVA(&pva);
  return (0);
}

int ReadArguments(int argc, char **argv, MONITOR_DATA *monitor) {
  SCANNED_ARG *s_arg;
  unsigned long dummyFlags;
  long i_arg;

  monitor->verbose = false;
  monitor->inputFile = NULL;
  rcParam.PV = rcParam.Desc = NULL;
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10.0;
  rcParam.alarmSeverity = NO_ALARM;
  monitor->pendIOTime = 10.0;
  monitor->executionTime = DBL_MAX;

  rcParam.Desc = (char *)realloc(rcParam.Desc, 41 * sizeof(char));
  sprintf(rcParam.Desc, "sddspermissive session");

  SDDS_RegisterProgramName("sddspermissive");

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s\n", USAGE);
    return (1);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_double(&monitor->pendIOTime, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -pendIOTime\n");
          return (1);
        }
        break;
      case CLO_VERBOSE:
        monitor->verbose = true;
        break;
      case CLO_EXECUTIONTIME:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "invalid -executionTime syntax\n");
          return (1);
        }
        if (sscanf(s_arg[i_arg].list[1], "%lf", &(monitor->executionTime)) != 1) {
          fprintf(stderr, "invalid -executionTime syntax or value");
          return (1);
        }
        if (monitor->executionTime < .5) {
          monitor->executionTime = DBL_MAX;
        }
        break;
      case CLO_RUNCONTROLPV:
        s_arg[i_arg].n_items -= 1;
        if ((s_arg[i_arg].n_items < 0) ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          "pingInterval", SDDS_DOUBLE, &rcParam.pingInterval, 1, 0,
                          NULL) ||
            (!rcParam.PV)) {
          fprintf(stderr, "bad -runControlPV syntax\n");
          return (1);
        }
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_RUNCONTROLDESC:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            (!rcParam.Desc)) {
          fprintf(stderr, "bad -runControlDesc syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      }
    } else {
      if (!monitor->inputFile)
        SDDS_CopyString(&monitor->inputFile, s_arg[i_arg].list[0]);
      else {
        fprintf(stderr, "too many filenames given\n");
        return (1);
      }
    }
  }
  if (!monitor->inputFile) {
    fprintf(stderr, "no inputfile given\n");
    return (1);
  }
  if (!rcParam.PV) {
    fprintf(stderr, "no runcontrol PV given\n");
    return (1);
  }
  free_scanargs(&s_arg, argc);
  return (0);
}

int ReadInput(MONITOR_DATA *monitor) {
  SDDS_DATASET input;
  monitor->pages = 0;
  monitor->total_pvCount = 0;
  bool caOnly = false;
  int64_t i;
  int32_t page=0, useMask=0;

  if (!SDDS_InitializeInput(&input, monitor->inputFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (SDDS_CheckParameter(&input, (char *)"MaskControlName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    if (SDDS_CheckParameter(&input, (char *)"MaskMinimumValue", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) != SDDS_CHECK_OK) {
      fprintf(stderr, "Parameter \"MaskMinimumValue\" does not exist or something wrong with it in input file %s\n", monitor->inputFile);
      return (1);
    }
    if (SDDS_CheckParameter(&input, (char *)"MaskMaximumValue", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) != SDDS_CHECK_OK) {
      fprintf(stderr, "Parameter \"MaskMaximumValue\" does not exist or something wrong with it in input file %s\n", monitor->inputFile);
      return (1);
    }
    useMask = 1;
  }
  if (SDDS_CheckColumn(&input, (char *)"ControlName", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    fprintf(stderr, "Column \"ControlName\" does not exist or something wrong with it in input file %s\n", monitor->inputFile);
    return (1);
  }
  if (SDDS_CheckColumn(&input, (char *)"Description", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    fprintf(stderr, "Column \"Description\" does not exist or something wrong with it in input file %s\n", monitor->inputFile);
    return (1);
  }
  if (SDDS_CheckColumn(&input, (char *)"MinimumValue", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) != SDDS_CHECK_OK) {
    fprintf(stderr, "Column \"MinimumValue\" does not exist or something wrong with it in input file %s\n", monitor->inputFile);
    return (1);
  }
  if (SDDS_CheckColumn(&input, (char *)"MaximumValue", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) != SDDS_CHECK_OK) {
    fprintf(stderr, "Column \"MaximumValue\" does not exist or something wrong with it in input file %s\n", monitor->inputFile);
    return (1);
  }
  if (SDDS_CheckColumn(&input, (char *)"Provider", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    caOnly = true;
  }
  while (SDDS_ReadPage(&input) > 0) {
    page++;
    if (page == 1) {
      monitor->page = (MONITOR_DATA_PAGE *)malloc(sizeof(MONITOR_DATA_PAGE));
    } else {
      monitor->page = (MONITOR_DATA_PAGE *)realloc(monitor->page, sizeof(MONITOR_DATA_PAGE) * page);
    }
    monitor->pages = page;
    monitor->page[page-1].pvCount = input.n_rows;
    monitor->total_pvCount += input.n_rows;
    monitor->page[page-1].ControlName = monitor->page[page-1].Description = NULL;
    monitor->page[page-1].MinimumValue = monitor->page[page-1].MaximumValue = NULL;
    
    monitor->page[page-1].pvMaskCount = 0;
    if (useMask) {
      if (!(SDDS_GetParameterAsString(&input, (char *)"MaskControlName", &(monitor->page[page-1].MaskControlName))) ||
          !(SDDS_GetParameterAsDouble(&input, (char *)"MaskMinimumValue", &(monitor->page[page-1].MaskMinimumValue))) ||
          !(SDDS_GetParameterAsDouble(&input, (char *)"MaskMaximumValue", &(monitor->page[page-1].MaskMaximumValue)))) {
        fprintf(stderr, (char *)"No data provided in the input file.\n");
        return (1);
      }
      if (SDDS_CheckParameter(&input, (char *)"MaskProvider", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
        if (!(SDDS_GetParameterAsString(&input, (char *)"MaskProvider", &(monitor->page[page-1].MaskProvider)))) {
          fprintf(stderr, (char *)"No data provided in the input file.\n");
          return (1);
        }
      } else {
        monitor->page[page-1].MaskProvider = (char*)malloc(sizeof(char) * 5);
        sprintf(monitor->page[page-1].MaskProvider, "ca");
      }
      if (strlen(monitor->page[page-1].MaskControlName) > 0) {
        //monitor->page[page-1].pvCount++;
        monitor->total_pvCount++;
        monitor->page[page-1].pvMaskCount = 1;
      }
    } else {
      monitor->page[page-1].MaskControlName = NULL;
    }
    if (!(monitor->page[page-1].ControlName = (char **)SDDS_GetColumn(&input, (char *)"ControlName")) ||
        !(monitor->page[page-1].Description = (char **)SDDS_GetColumn(&input, (char *)"Description")) ||
        !(monitor->page[page-1].MinimumValue = (double *)SDDS_GetColumnInDoubles(&input, (char *)"MinimumValue")) ||
        !(monitor->page[page-1].MaximumValue = (double *)SDDS_GetColumnInDoubles(&input, (char *)"MaximumValue"))) {
      fprintf(stderr, (char *)"No data provided in the input file.\n");
      return (1);
    }
    if (caOnly) {
      monitor->page[page-1].Provider = (char **)malloc(sizeof(char *) * input.n_rows);
      for (i = 0; i < input.n_rows; i++) {
        monitor->page[page-1].Provider[i] = (char *)malloc(sizeof(char) * 3);
        sprintf(monitor->page[page-1].Provider[i], "ca");
      }
    } else {
      if (!(monitor->page[page-1].Provider = (char **)SDDS_GetColumn(&input, (char *)"Provider"))) {
        fprintf(stderr, (char *)"No data provided in the input file.\n");
        return (1);
      }
    }
  }
  if (!SDDS_Terminate(&input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  return (0);
}

int ConnectRC(MONITOR_DATA *monitor) {
  if (monitor->pendIOTime * 1000 > rcParam.pingTimeout) {
    /* runcontrol will time out if CA connection
         takes longer than its timeout, so the runcontrol ping timeout should be longer
         than the pendIOTime*/
    rcParam.pingTimeout = (monitor->pendIOTime + 10) * 1000;
  }
  rcParam.handle[0] = (char)0;
  rcParam.PV = (char *)realloc(rcParam.PV, 41 * sizeof(char));
  rcParam.status = runControlInit(rcParam.PV,
                                  rcParam.Desc,
                                  rcParam.pingTimeout,
                                  rcParam.handle,
                                  &(rcParam.rcInfo),
                                  30.0);
  if (rcParam.status != RUNCONTROL_OK) {
    fprintf(stderr, "Error initializing run control.\n");
    if (rcParam.status == RUNCONTROL_DENIED) {
      fprintf(stderr, "Another application instance is using the same runcontrol record,\n");
      fprintf(stderr, "\tor has the same description string,\n");
      fprintf(stderr, "\tor the runcontrol record has not been cleared from previous use.\n");
    }
    return (1);
  }
  rcParam.status = runControlLogMessage(rcParam.handle, (char *)"Initializing", MINOR_ALARM, &(rcParam.rcInfo));
  if (rcParam.status != RUNCONTROL_OK) {
    fprintf(stderr, "Error initializing run control.\n");
    return (1);
  }

  return (0);
}

void SetupAtExit() {
  atexit(rc_interrupt_handler);
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
}

void interrupt_handler(int sig) {
  exit(1);
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
  if (rcParam.PV) {
    rcParam.status = runControlExit(rcParam.handle, &rcParam.rcInfo);
  }
}

class permissiveMonitorRequester : public epics::pvaClient::PvaClientMonitorRequester,
                                   public std::tr1::enable_shared_from_this<permissiveMonitorRequester> {
public:
  POINTER_DEFINITIONS(permissiveMonitorRequester);
  permissiveMonitorRequester() {
  }

  static permissiveMonitorRequesterPtr create() {
    permissiveMonitorRequesterPtr client(permissiveMonitorRequesterPtr(new permissiveMonitorRequester()));
    return client;
  }
  virtual void event(epics::pvaClient::PvaClientMonitorPtr const &monitor) {
    if (firstPV[0] == 0) {
      //This is the first monitored PV to change in the .01 second interval. It may or may not be out of range.
      sprintf(firstPV, "%s", monitor->getPvaClientChannel()->getChannelName().c_str());
    }
  }
};

#if defined(_WIN32)
#  include <thread>
#  include <chrono>
int nanosleep(const struct timespec *req, struct timespec *rem) {
  long double ldSeconds;
  long mSeconds;
  ldSeconds = req->tv_sec + req->tv_nsec;
  mSeconds = ldSeconds / 1000000;
  std::this_thread::sleep_for(std::chrono::microseconds(mSeconds));
  return (0);
}
#endif

long pvaThreadSleep(long double seconds) {
  struct timespec delayTime;
  struct timespec remainingTime;

  if (seconds > 0) {
    delayTime.tv_sec = seconds;
    delayTime.tv_nsec = (seconds - delayTime.tv_sec) * 1e9;
    while (nanosleep(&delayTime, &remainingTime) == -1 &&
           errno == EINTR)
      delayTime = remainingTime;
  }
  return (0);
}

int ConnectPVs(PVA_OVERALL *pva, MONITOR_DATA *monitor) {
  long i, j, p;
  //Allocate memory for pva structures
  allocPVA(pva, monitor->total_pvCount);
  //List PV names
  epics::pvData::shared_vector<std::string> names(pva->numPVs);
  epics::pvData::shared_vector<std::string> providerNames(pva->numPVs);
  j = 0;
  for (p = 0; p < monitor->pages; p++) {
    for (i = 0; i < monitor->page[p].pvCount; i++) {
      names[j] = monitor->page[p].ControlName[i];
      providerNames[j] = monitor->page[p].Provider[i];
      j++;
    }
  }
  for (p = 0; p < monitor->pages; p++) {
    for (i = 0; i < monitor->page[p].pvMaskCount; i++) {
      names[j] = monitor->page[p].MaskControlName;
      providerNames[j] = monitor->page[p].MaskProvider;
      j++;
    }
  }
  pva->pvaChannelNames = freeze(names);
  pva->pvaProvider = freeze(providerNames);
  ConnectPVA(pva, monitor->pendIOTime);
  if (monitor->verbose) {
    if (pva->numNotConnected > 0) {
      fprintf(stdout, "%ld of %ld PVs did not connect\n", pva->numNotConnected, pva->numPVs);
    } else {
      fprintf(stdout, "Connected to all %ld PVs\n", pva->numPVs);
    }
  }
  //pva->limitGetReadings = true;
  j = 0;
  for (p = 0; p < monitor->pages; p++) {
    for (i = 0; i < monitor->page[p].pvCount; i++) {
      if (!pva->isConnected[j]) {
        fprintf(stderr, "Error (sddspermissive): problem doing search for %s\n", monitor->page[p].ControlName[i]);
        return (1);
      }
      j++;
    }
  }
  for (p = 0; p < monitor->pages; p++) {
    for (i = 0; i < monitor->page[p].pvMaskCount; i++) {
      if (!pva->isConnected[j]) {
        fprintf(stderr, "Error (sddspermissive): problem doing search for %s\n", monitor->page[p].MaskControlName);
        return (1);
      }
      j++;
    }
  }
  //Get initial values
  if (GetPVAValues(pva) == 1) {
    return (1);
  }
  if (VerifyNumericScalarPVTypes(pva) == 1) {
    return (1);
  }
  pva->useMonitorCallbacks = true;
  pva->monitorReqPtr = permissiveMonitorRequester::create();
  firstPV[0] = 0;
  if (MonitorPVAValues(pva) == 1) {
    return (1);
  }
  if (pvaThreadSleep(.1) == 1) {
    return (1);
  }
  if (PollMonitoredPVA(pva) == -1) {
    return (1);
  }
  rcParam.status = runControlLogMessage(rcParam.handle, (char *)"Running", MINOR_ALARM, &(rcParam.rcInfo));
  if (rcParam.status != RUNCONTROL_OK) {
    fprintf(stderr, "Error initializing run control.\n");
    return (1);
  }
  return (0);
}

int EventLoop(PVA_OVERALL *pva, MONITOR_DATA *monitor) {
  epicsTime begin(epicsTime::getCurrent());
  long count, pingInterval, pingStep = 0;
  int64_t i, j, p, first = 0, prevFirst = 0, mask_start;
  bool pass = true, prevState = true, init = true;
  time_t t;
  struct tm *ptm;
  char cur_time[128];
  char *desc=NULL, *name=NULL;
  bool *mask_alerts;
  pingInterval = (long)(rcParam.pingInterval / .01);
  if (pingInterval == 0) {
    pingInterval = 1;
  }

  mask_alerts = (bool*)malloc(sizeof(bool) * monitor->pages);
  j = 0;
  for (p = 0; p < monitor->pages; p++) {
    mask_alerts[p] = false;
    for (i = 0; i < monitor->page[p].pvCount; i++) {
      j++;
    }
  }
  mask_start = j;

  while (true) {
    if (pvaThreadSleep(.01) == 1) {
      return (1);
    }
    count = PollMonitoredPVA(pva);
    if (count == -1) {
      fprintf(stderr, "Exiting due to PV monitoring error\n");
      return (1);
    }
    if (monitor->executionTime < (epicsTime::getCurrent() - begin)) {
      fprintf(stderr, "Exiting due to executionTime limit\n");
      return (1);
    }
    if ((count > 0) || (init == true)) {
      init = false;
      pass = true;
      first = -1;
      if (monitor->verbose) {
        t = time(NULL);
        ptm = localtime(&t);
        strftime(cur_time, 128, "%H:%M:%S", ptm);
      }
      j = mask_start;
      for (p = 0; p < monitor->pages; p++) {
        for (i = 0; i < monitor->page[p].pvMaskCount; i++) {
          if (pva->isConnected[j] && (pva->pvaData[j].numMonitorReadings > 0)) {
            if ((pva->pvaData[j].monitorData[0].values[0] < monitor->page[p].MaskMinimumValue) ||
                (pva->pvaData[j].monitorData[0].values[0] > monitor->page[p].MaskMaximumValue)) {
              if ((monitor->verbose) && (mask_alerts[p] != false)) {
                fprintf(stdout, "%s -- No longer masking due to %s.\n", cur_time, monitor->page[p].MaskControlName);
                fflush(stdout);
              }
              mask_alerts[p] = false;
            } else {
              if ((monitor->verbose) && (mask_alerts[p] != true)) {
                fprintf(stdout, "%s -- Masking due to %s.\n", cur_time, monitor->page[p].MaskControlName);
                fflush(stdout);
              }
              mask_alerts[p] = true;
            }
          } else {
            //Not connected. Ignore mask
            if ((monitor->verbose) && (mask_alerts[p] != false)) {
              fprintf(stdout, "%s -- Masking due to %s.\n", cur_time, monitor->page[p].MaskControlName);
              fflush(stdout);
            }
            mask_alerts[p] = false;
          }
          j++;
        }
      }

      j = 0;
      for (p = 0; p < monitor->pages; p++) {
        if (mask_alerts[p]) {
          j += monitor->page[p].pvCount;
          continue;
        }
        for (i = 0; i < monitor->page[p].pvCount; i++) {
          if (pva->isConnected[j] && (pva->pvaData[j].numMonitorReadings > 0)) {
            if ((pva->pvaData[j].monitorData[0].values[0] < monitor->page[p].MinimumValue[i]) ||
                (pva->pvaData[j].monitorData[0].values[0] > monitor->page[p].MaximumValue[i])) {
              pass = false;
              if ((strcmp(firstPV, monitor->page[p].ControlName[i]) == 0) || (first == -1)) {
                first = j;
                desc = monitor->page[p].Description[i];
                name = monitor->page[p].ControlName[i];
              }
            }
          } else {
            //Not connected. Raise alarm
            pass = false;
            if ((strcmp(firstPV, monitor->page[p].ControlName[i]) == 0) || (first == -1)) {
              first = j;
              desc = monitor->page[p].Description[i];
              name = monitor->page[p].ControlName[i];
            }
          }
          j++;
        }
      }

      if (pass) {
        firstPV[0] = 0;
      }
      if ((pass != prevState) || (first != prevFirst)) {
        if (pass) {
          rcParam.status = runControlLogMessage(rcParam.handle, (char *)"Running", NO_ALARM, &(rcParam.rcInfo));
          if (rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr, "Error initializing run control.\n");
            return (1);
          }
          prevFirst = -1;
          if (monitor->verbose) {
            fprintf(stdout, "%s -- All PVs in range.\n", cur_time);
            fflush(stdout);
          }
        } else {
          rcParam.status = runControlLogMessage(rcParam.handle, desc, MINOR_ALARM, &(rcParam.rcInfo));
          if (rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr, "Error initializing run control.\n");
            return (1);
          }
          prevFirst = first;
          if (monitor->verbose) {
            fprintf(stdout, "%s -- Warning: %s is out of range. (%s)\n", cur_time, name, desc);
            fflush(stdout);
          }
        }
        prevState = pass;
      }
    }
    pingStep++;
    if (pingStep == pingInterval) {
      rcParam.status = runControlPing(rcParam.handle, &(rcParam.rcInfo));
      switch (rcParam.status) {
      case RUNCONTROL_ABORT:
        fprintf(stderr, "Run control application aborted.\n");
        return (1);
        break;
      case RUNCONTROL_TIMEOUT:
        fprintf(stderr, "Run control application timed out.\n");
        strcpy(rcParam.message, "Application timed-out");
        runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
        return (1);
        break;
      case RUNCONTROL_OK:
        break;
      case RUNCONTROL_ERROR:
        fprintf(stderr, "Communications error with runcontrol record.\n");
        return (1);
      default:
        fprintf(stderr, "Unknown run control error code.\n");
        return (1);
        break;
      }
      pingStep = 0;
    }
  }
  return (0);
}

long VerifyNumericScalarPVTypes(PVA_OVERALL *pva) {
  long j;

  if (pva == NULL) {
    return (0);
  }
  for (j = 0; j < pva->numPVs; j++) {
    if (pva->isConnected[j] == true) {
      if (pva->pvaData[j].numeric == false) {
        fprintf(stderr, "Error (sddspermissive): %s is not numeric\n", pva->pvaChannelNames[j].c_str());
        return (1);
      }
      if ((pva->pvaData[j].fieldType != epics::pvData::scalar) && (pva->pvaData[j].pvEnumeratedStructure == false)) {
        fprintf(stderr, "Error (sddspermissive): %s is not scalar\n", pva->pvaChannelNames[j].c_str());
        return (1);
      }
    } else {
      //Assume the PVs that didn't connect are of the proper type
    }
  }
  return (0);
}
