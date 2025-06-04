#ifndef L_cuserid
#  define L_cuserid 9
#endif

#if defined(linux)
extern char *cuserid __P((char *__s));
#endif

long hbtIndex = 0, userIndex = 1, hostIndex = 2, pidIndex = 3, semIndex = 4, descIndex = 5, strtIndex = 6;
long valIndex = 7, abrtIndex = 8, suspIndex = 9, hbtoIndex = 10, msgIndex = 11, alrmIndex = 12;

PVA_OVERALL pvaRC;
int runControlInitPVA(PVA_OVERALL *pva, char *pv, char *desc, float timeout, char *handle, RUNCONTROL_INFO *rcInfo, double pendIOtime);
int runControlPingPVA(PVA_OVERALL *pva, char *handle, RUNCONTROL_INFO *rcInfo);
int runControlExitPVA(PVA_OVERALL *pva, char *handle, RUNCONTROL_INFO *rcInfo);
int runControlLogMessagePVA(PVA_OVERALL *pva, char *handle, char *message, short severity, RUNCONTROL_INFO *rcInfo);

/*************************************************************************
 * FUNCTION : runControlInit()
 * PURPOSE  : Grab control of the specified EPICS runcontrol record, and load
 *            it with various application information (pid, host, etc.).
 *            Specific record may be requested by pv argument, or NULL
 *            may be given and the next free slot in a hardcoded set
 *            of records will be selected for you.
 *
 * ARGS in  : pv   - desired runcontrol record name or NULL (39 chars max)
 *            desc - brief, unique description of application (39 chars max)
 *            timeout - max time in ms between runControlPing() calls
 * ARGS out : handle - User must provide preallocated 255 char string.
 *                     The init routine will load it with a string to
 *                     be used in the other library calls, runControlPing(),
 *                     and runControlExit().
 *
 * RETURNS  : RUNCONTROL_OK       - application registered, proceed
 *            RUNCONTROL_DENIED   - your application is already running
 *            RUNCONTROL_ERROR    - unable to communicate with record
 *
 ************************************************************************/
int runControlInit(char *pv, char *desc, float timeout, char *handle, RUNCONTROL_INFO *rcInfo, double pendIOtime) {
  return (runControlInitPVA(&pvaRC, pv, desc, timeout, handle, rcInfo, pendIOtime));
}

int runControlInitPVA(PVA_OVERALL *pva, char *pv, char *desc, float timeout, char *handle, RUNCONTROL_INFO *rcInfo, double pendIOtime) {
  short semTake = 0; /* value to write SEM field of record */
  char username[L_cuserid];
  char hostname[255];
  char pid[255];
  time_t calt;               /* system calendar time               */
  char localtimestring[255]; /* converted to local time            */

  if (rcInfo == NULL)
    return (RUNCONTROL_ERROR);

  (*rcInfo).pendIOtime = pendIOtime;
  if (pv == NULL) {
    return (RUNCONTROL_ERROR);
  } else {
    /* User has explicitly supplied pv name of runcontrol record */
    strcpy(handle, pv);
  }

  /* Build up pv names */
  strcpy((*rcInfo).pv_hbt, handle);
  strcat((*rcInfo).pv_hbt, ".HBT");
  strcpy((*rcInfo).pv_user, handle);
  strcat((*rcInfo).pv_user, ".USER");
  strcpy((*rcInfo).pv_host, handle);
  strcat((*rcInfo).pv_host, ".HOST");
  strcpy((*rcInfo).pv_pid, handle);
  strcat((*rcInfo).pv_pid, ".PID");
  strcpy((*rcInfo).pv_sem, handle);
  strcat((*rcInfo).pv_sem, ".SEM");
  strcpy((*rcInfo).pv_desc, handle);
  strcat((*rcInfo).pv_desc, ".DESC");
  strcpy((*rcInfo).pv_strt, handle);
  strcat((*rcInfo).pv_strt, ".STRT");
  strcpy((*rcInfo).pv_val, handle);
  strcpy((*rcInfo).pv_abrt, handle);
  strcat((*rcInfo).pv_abrt, ".ABRT");
  strcpy((*rcInfo).pv_susp, handle);
  strcat((*rcInfo).pv_susp, ".SUSP");
  strcpy((*rcInfo).pv_hbto, handle);
  strcat((*rcInfo).pv_hbto, ".HBTO");
  strcpy((*rcInfo).pv_msg, handle);
  strcat((*rcInfo).pv_msg, ".MSG");
  strcpy((*rcInfo).pv_alrm, handle);
  strcat((*rcInfo).pv_alrm, ".ALRM");

  //Allocate memory for pva structures
  allocPVA(pva, 13);
  //List PV names
  epics::pvData::shared_vector<std::string> names(pva->numPVs);
  epics::pvData::shared_vector<std::string> providerNames(pva->numPVs);
  names[0] = (*rcInfo).pv_hbt;
  names[1] = (*rcInfo).pv_user;
  names[2] = (*rcInfo).pv_host;
  names[3] = (*rcInfo).pv_pid;
  names[4] = (*rcInfo).pv_sem;
  names[5] = (*rcInfo).pv_desc;
  names[6] = (*rcInfo).pv_strt;
  names[7] = (*rcInfo).pv_val;
  names[8] = (*rcInfo).pv_abrt;
  names[9] = (*rcInfo).pv_susp;
  names[10] = (*rcInfo).pv_hbto;
  names[11] = (*rcInfo).pv_msg;
  names[12] = (*rcInfo).pv_alrm;
  for (long j = 0; j < pva->numPVs; j++) {
    providerNames[j] = "ca";
  }
  pva->pvaChannelNames = freeze(names);
  pva->pvaProvider = freeze(providerNames);

  /* Collect host, pid, username, and time information */
#if defined(_WIN32) || defined(__APPLE__)
  snprintf(username, sizeof(username), "%s", "unknown");
#else
  cuserid(username);
#endif
  gethostname(hostname, 255);

  (*rcInfo).myPid = getpid();
  snprintf(pid, sizeof(pid), "%d", getpid());

  time(&calt);
  strcpy(localtimestring, ctime(&calt));
  localtimestring[strlen(localtimestring) - 1] = '\0';

  /* First set the heartbeat timeout interval */
  ConnectPVA(pva, (*rcInfo).pendIOtime);
  if (pva->numNotConnected > 0) {
    return (RUNCONTROL_ERROR);
  }
  if (GetPVAValues(pva) == 1) {
    return (RUNCONTROL_ERROR);
  }
  if ((-.1 < pva->pvaData[semIndex].getData[0].values[0]) &&
      (.1 > pva->pvaData[semIndex].getData[0].values[0])) {
    if (strlen(pva->pvaData[pidIndex].getData[0].stringValues[0]) > 0) {
      freePVAGetReadings(pva);
      return (RUNCONTROL_DENIED);
    }
  }
  freePVAGetReadings(pva);

  PrepPut(pva, hbtIndex, (double)timeout);
  PrepPut(pva, semIndex, (int64_t)semTake);
  if (PutPVAValues(pva) == 1) {
    return (RUNCONTROL_ERROR);
  }

  /* Load record with user, host, pid, start time, and description */
  PrepPut(pva, userIndex, username);
  PrepPut(pva, hostIndex, hostname);
  PrepPut(pva, pidIndex, pid);
  PrepPut(pva, strtIndex, localtimestring);
  PrepPut(pva, descIndex, desc);
  if (PutPVAValues(pva) == 1) {
    return (RUNCONTROL_ERROR);
  }

  return (RUNCONTROL_OK);
}

/*************************************************************************
 * FUNCTION : runControlPing()
 * PURPOSE  : Notifies runcontrol record that your application is still
 *            alive. Also provides means for runcontrol record to suspend
 *            you, or request that you abort. The return codes from this 
 *            call must be checked and acted on. 
 * ARGS in  : handle - string from call to runControlInit()
 *
 * RETURNS  : RUNCONTROL_OK      - all is well, continue
 *            RUNCONTROL_ABORT   - your application should clean up and exit
 *            RUNCONTROL_TIMEOUT - didn't ping in time, clean up and exit
 *            RUNCONTROL_ERROR   - unable to communicate with record,
 *                                 should attempt another Init() or exit.
 *            
 ************************************************************************/
int runControlPing(char *handle, RUNCONTROL_INFO *rcInfo) {
  return (runControlPingPVA(&pvaRC, handle, rcInfo));
}

int runControlPingPVA(PVA_OVERALL *pva, char *handle, RUNCONTROL_INFO *rcInfo) {
  short abrt, susp, hbto; /* ABRT, SUSP, and HBTO field values */
  int pid;
  double ping = 1.0;

  /* See if record is requesting abort, suspend, or has timed out. */
  if (GetPVAValues(pva) == 1) {
    return (RUNCONTROL_ERROR);
  }
  if (pva->numNotConnected > 0) {
    return (RUNCONTROL_ERROR);
  }
  abrt = pva->pvaData[abrtIndex].getData[0].values[0];
  susp = pva->pvaData[suspIndex].getData[0].values[0];
  hbto = pva->pvaData[hbtoIndex].getData[0].values[0];
  pid = atoi(pva->pvaData[pidIndex].getData[0].stringValues[0]);
  freePVAGetReadings(pva);

  /* If the PID of the connected process is not the same as our PID, then something
     is wrong and we return an error.  The process will then exit.
     Presumably a second process started on this run control record and the present
     process was too tied up to realize that it should exit.
  */
  if (pid != (int)((*rcInfo).myPid))
    return (RUNCONTROL_ABORT);

  if (abrt)
    return (RUNCONTROL_ABORT);
  else if (hbto)
    return (RUNCONTROL_TIMEOUT);
  else if (susp) { /* Enter into sleep loop with 1 second period. */
    while (susp) {
      epicsThreadSleep(1);
      if (GetPVAValues(pva) == 1) {
        return (RUNCONTROL_ERROR);
      }
      abrt = pva->pvaData[abrtIndex].getData[0].values[0];
      susp = pva->pvaData[suspIndex].getData[0].values[0];
      freePVAGetReadings(pva);
      if (abrt)
        return (RUNCONTROL_ABORT);
    }
  }

  /* No suspend, abort, or timeout. So ping the record. */
  PrepPut(pva, valIndex, ping);
  if (PutPVAValues(pva) == 1) {
    return (RUNCONTROL_ERROR);
  }
  return (RUNCONTROL_OK);
}

/*************************************************************************
 * FUNCTION : runControlExit()
 * PURPOSE  : Release control over the runcontrol record.
 * ARGS in  : handle - string from call to runControlInit()
 * RETURNS  : RUNCONTROL_OK    - all is well, proceed
 *            RUNCONTROL_ERROR - unable to communicate with record
 ************************************************************************/
int runControlExit(char *handle, RUNCONTROL_INFO *rcInfo) {
  return (runControlExitPVA(&pvaRC, handle, rcInfo));
}

int runControlExitPVA(PVA_OVERALL *pva, char *handle, RUNCONTROL_INFO *rcInfo) {
  short semGive = 1;
  int pid;

  if (GetPVAValues(pva) == 1) {
    return (RUNCONTROL_ERROR);
  }
  if (pva->numNotConnected > 0) {
    return (RUNCONTROL_ERROR);
  }
  pid = atoi(pva->pvaData[pidIndex].getData[0].stringValues[0]);
  freePVAGetReadings(pva);

  if (pid != (int)((*rcInfo).myPid))
    return (RUNCONTROL_ERROR);

  PrepPut(pva, semIndex, (int64_t)semGive);
  if (PutPVAValues(pva) == 1) {
    return (RUNCONTROL_ERROR);
  }

  freePVA(pva);
  return (RUNCONTROL_OK);
}

/*************************************************************************
 * FUNCTION : runControlLogMessage()
 * PURPOSE  : Logs a message and alarm severity to the associated
 *            runcontrol record.
 * ARGS in  : handle - string from call to runControlInit()
 *            message - message string (39 chars max)
 *            severity - NO_ALARM, MINOR_ALARM, MAJOR_ALARM, or INVALID_ALARM
 *
 * RETURNS  : RUNCONTROL_OK      - all is well, continue
 *            RUNCONTROL_ERROR   - unable to communicate with record,
 *                                 should attempt another Init() or exit.
 *            
 ************************************************************************/
int runControlLogMessage(char *handle, char *message, short severity, RUNCONTROL_INFO *rcInfo) {
  return (runControlLogMessagePVA(&pvaRC, handle, message, severity, rcInfo));
}

int runControlLogMessagePVA(PVA_OVERALL *pva, char *handle, char *message, short severity, RUNCONTROL_INFO *rcInfo) {
  short sev = severity;

  PrepPut(pva, alrmIndex, (int64_t)sev);
  PrepPut(pva, msgIndex, message);
  if (PutPVAValues(pva) == 1) {
    return (RUNCONTROL_ERROR);
  }

  return (RUNCONTROL_OK);
}
