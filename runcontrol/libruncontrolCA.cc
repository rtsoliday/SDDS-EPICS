/*
*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
*/

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.6  2008/04/28 14:41:11  soliday
 * Added Eric Norum's change.
 *
 * Revision 1.5  2004/07/16 18:26:42  soliday
 * Changed sleep command to ca_pend_event command.
 *
 * Revision 1.4  2004/03/15 16:50:31  soliday
 * Fixed problem with connecting to runcontrol record with Base 3.14.5
 *
 * Revision 1.3  2003/09/02 21:16:16  soliday
 * Cleaned up for Borland C++
 *
 * Revision 1.2  2003/08/28 15:43:14  soliday
 * Removed some unused variables.
 *
 * Revision 1.1  2003/08/27 19:44:17  soliday
 * Moved to a subdirectory of SDDSepics.
 *
 * Revision 1.13  2003/04/24 16:00:48  soliday
 * Modified to work with OSX
 *
 * Revision 1.12  2003/02/12 20:42:00  soliday
 * Removed debugging statement
 *
 * Revision 1.11  2003/02/12 20:36:31  soliday
 * Fixed a major bug that could cause multiple connections to the same runcontrol.
 *
 * Revision 1.10  2002/11/27 18:43:33  soliday
 * Fixed a minor bug with last change.
 *
 * Revision 1.9  2002/11/27 16:47:20  soliday
 * Added support for WIN32
 *
 * Revision 1.8  2002/11/19 21:45:30  soliday
 * ezca no longer used
 *
 * Revision 1.7  2000/05/17 19:39:26  soliday
 * Removed Solaris and Linux warnings.
 *
 * Revision 1.6  1996/07/17 18:05:26  saunders
 * Changed getlogin() call to cuserid() in hopes of preventing blocking
 * when no terminal is attached to process.
 *
 * Revision 1.5  1995/11/07  18:33:12  saunders
 * Added runControlLogMessage() function.
 *
 * Revision 1.4  1995/10/27  18:48:42  saunders
 * Abort will now stop an application suspended in runControlPing().
 *
 * Revision 1.3  1995/10/25  15:37:16  saunders
 * First release of runControl library.
 *
 * Revision 1.2  1995/10/23  21:48:37  saunders
 * Close to final version. Not releasable yet.
 *
 * Revision 1.1.1.1  1995/10/20  22:40:54  saunders
 * Imported Files
 * */

/*************************************************************************
 * FILE:      libruncontrol.c
 * Author:    Claude Saunders
 * 
 * Purpose:   C library of routines for communicating with an EPICS
 *            runcontrol record. Allows a workstation application to 
 *            "register" with an EPICS record, thereby preventing
 *            additional instances of the same application from being
 *            run. Also provides means for said runcontrol record to
 *            suspend or abort the workstation application. Application
 *            must "ping" the record with a call to runControlPing()
 *            regularly. If the application does not "ping" the record
 *            within the timeout specified in runControlInit(), the
 *            runControlPing() will return an error, and the application
 *            should exit.
 *************************************************************************/

#ifndef L_cuserid
#  define L_cuserid 9
#endif

#if defined(linux)
extern char *cuserid __P((char *__s));
#endif

static void sem_put_callback(struct event_handler_args);
static int sem_check(int *sem, double pendIOtime);

/* Local function */
static int runControlFindSlot(char *desc, char *handle, double pendIOtime);

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
  int status;        /* return status of ca calls        */
  short semTake = 0; /* value to write SEM field of record */
  char username[L_cuserid];
  char hostname[255];
  char pid[255];
  time_t calt;               /* system calendar time               */
  char localtimestring[255]; /* converted to local time            */
  int sem = -1000;

  if (rcInfo == NULL)
    return (RUNCONTROL_ERROR);

  (*rcInfo).pendIOtime = pendIOtime;
  if (pv == NULL) {
    /* User will let library find next available runcontrol record */
    status = runControlFindSlot(desc, handle, (*rcInfo).pendIOtime);
    if (status != RUNCONTROL_OK)
      return (status);
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

  /* Collect host, pid, username, and time information */
#if defined(_WIN32) || defined(__APPLE__)
  sprintf(username, "unknown");
#else
  cuserid(username);
#endif
  gethostname(hostname, 255);

  (*rcInfo).myPid = getpid();
  sprintf(pid, "%d", getpid());

  time(&calt);
  strcpy(localtimestring, ctime(&calt));
  localtimestring[strlen(localtimestring) - 1] = '\0';

  /* First set the heartbeat timeout interval */

  if ((ca_search((*rcInfo).pv_hbt, &((*rcInfo).pv_hbt_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_sem, &((*rcInfo).pv_sem_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_user, &((*rcInfo).pv_user_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_host, &((*rcInfo).pv_host_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_pid, &((*rcInfo).pv_pid_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_strt, &((*rcInfo).pv_strt_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_desc, &((*rcInfo).pv_desc_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_abrt, &((*rcInfo).pv_abrt_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_susp, &((*rcInfo).pv_susp_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_hbto, &((*rcInfo).pv_hbto_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_val, &((*rcInfo).pv_val_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_msg, &((*rcInfo).pv_msg_chid)) != ECA_NORMAL) ||
      (ca_search((*rcInfo).pv_alrm, &((*rcInfo).pv_alrm_chid)) != ECA_NORMAL))
    return (RUNCONTROL_ERROR);
  if (ca_pend_io((*rcInfo).pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);

  if (ca_put(DBR_FLOAT, (*rcInfo).pv_hbt_chid, &timeout) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  if (ca_pend_io((*rcInfo).pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);

  /* Grab semaphore from record */

  if (ca_put_callback(DBR_SHORT, (*rcInfo).pv_sem_chid, &semTake, sem_put_callback, &sem) != ECA_NORMAL)
    return (RUNCONTROL_DENIED);
  if (sem_check(&sem, (*rcInfo).pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_DENIED);

  /* Load record with user, host, pid, start time, and description */
  if ((ca_put(DBR_STRING, (*rcInfo).pv_user_chid, username) != ECA_NORMAL) ||
      (ca_put(DBR_STRING, (*rcInfo).pv_host_chid, hostname) != ECA_NORMAL) ||
      (ca_put(DBR_STRING, (*rcInfo).pv_pid_chid, pid) != ECA_NORMAL) ||
      (ca_put(DBR_STRING, (*rcInfo).pv_strt_chid, localtimestring) != ECA_NORMAL) ||
      (ca_put(DBR_STRING, (*rcInfo).pv_desc_chid, desc) != ECA_NORMAL))
    return (RUNCONTROL_ERROR);
  if (ca_pend_io((*rcInfo).pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
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
  short abrt, susp, hbto; /* ABRT, SUSP, and HBTO field values */
  unsigned int pid;
  double ping = 1.0;
  /* See if record is requesting abort, suspend, or has timed out. */
  if ((ca_get(DBR_SHORT, (*rcInfo).pv_abrt_chid, &abrt) != ECA_NORMAL) ||
      (ca_get(DBR_SHORT, (*rcInfo).pv_susp_chid, &susp) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, (*rcInfo).pv_pid_chid, &pid) != ECA_NORMAL) ||
      (ca_get(DBR_SHORT, (*rcInfo).pv_hbto_chid, &hbto) != ECA_NORMAL))
    return (RUNCONTROL_ERROR);
  if (ca_pend_io((*rcInfo).pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  /* If the PID of the connected process is not the same as our PID, then something
     is wrong and we return an error.  The process will then exit.
     Presumably a second process started on this run control record and the present
     process was too tied up to realize that it should exit.
     */

  if (pid != (*rcInfo).myPid) {
    return (RUNCONTROL_ABORT);
  }

  if (abrt) {
    return (RUNCONTROL_ABORT);
  } else if (hbto)
    return (RUNCONTROL_TIMEOUT);
  else if (susp) { /* Enter into sleep loop with 1 second period. */
    while (susp) {
      ca_pend_event(1);
      if ((ca_get(DBR_SHORT, (*rcInfo).pv_abrt_chid, &abrt) != ECA_NORMAL) ||
          (ca_get(DBR_SHORT, (*rcInfo).pv_susp_chid, &susp) != ECA_NORMAL))
        return (RUNCONTROL_ERROR);
      if (ca_pend_io((*rcInfo).pendIOtime) != ECA_NORMAL)
        return (RUNCONTROL_ERROR);

      if (abrt) {
        return (RUNCONTROL_ABORT);
      }
    }
  }

  /* No suspend, abort, or timeout. So ping the record. */
  if ((ca_put(DBR_DOUBLE, (*rcInfo).pv_val_chid, &ping) != ECA_NORMAL))
    return (RUNCONTROL_ERROR);
  if (ca_pend_io((*rcInfo).pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
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
  short semGive = 1;
  unsigned int pid;

  if ((ca_get(DBR_LONG, (*rcInfo).pv_pid_chid, &pid) != ECA_NORMAL))
    return (RUNCONTROL_ERROR);
  if (ca_pend_io((*rcInfo).pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  if (pid != (*rcInfo).myPid)
    return (RUNCONTROL_ERROR);
  if ((ca_put(DBR_SHORT, (*rcInfo).pv_sem_chid, &semGive) != ECA_NORMAL))
    return (RUNCONTROL_ERROR);
  return (RUNCONTROL_OK);
}

/*************************************************************************
 * FUNCTION : runControlFindSlot()
 * PURPOSE  : Given application description (desc), check to make sure
 *            that a runcontrol record with the same description doesn't
 *            exist already. If not, find the next unused runcontrol
 *            record from a hardcoded set. Note that the records searched
 *            for all begin with RUNCONTROL_PV_PREFIX, and are numbered
 *            sequentially up to the number given in the soft pv,
 *            RUNCONTROL_COUNT_PV.
 * ARGS in  : desc   - description string (39 chars max)
 *            handle - string from call to runControlInit()
 * RETURNS  : RUNCONTROL_OK  - all is well
 *            RUNCONTROL_ERROR - unable to communicate with records
 *            RUNCONTROL_DENIED - matching description found in a slot
 ************************************************************************/
static int runControlFindSlot(char *desc, char *handle, double pendIOtime) {
  double maxSlots; /* max number of runcontrol slot records */
  int imaxSlots;
  char **slotPv;      /* array of runcontrol slot record names */
  char **slotPv_desc; /* above, plus .DESC */
  char buf[255];
  char **slotDesc; /* contents of description field */
  int i;
  chid countPVchid;
  chid *slotPV_chid;

  if (ca_search(RUNCONTROL_COUNT_PV, &countPVchid) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  if (ca_pend_io(pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  if (ca_get(DBR_DOUBLE, countPVchid, &maxSlots) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  if (ca_pend_io(pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);

  imaxSlots = (int)maxSlots;
  slotPv = (char **)malloc(sizeof(char *) * imaxSlots);
  slotPv_desc = (char **)malloc(sizeof(char *) * imaxSlots);
  slotDesc = (char **)malloc(sizeof(char *) * imaxSlots);
  slotPV_chid = (chid *)malloc(sizeof(chid) * imaxSlots);

  /* Construct all needed pv names and string storage */
  for (i = 0; i < imaxSlots; i++) {
    /* Construct pv name */
    *(slotPv + i) = (char *)malloc(sizeof(char) * 255);
    *(*(slotPv + i)) = '\0';
    strcpy(*(slotPv + i), RUNCONTROL_PV_PREFIX);
    sprintf(buf, "%d", i);
    strcat(*(slotPv + i), buf);
    strcat(*(slotPv + i), "RC");

    /* Construct pv name plus .DESC */
    *(slotPv_desc + i) = (char *)malloc(sizeof(char) * 255);
    *(*(slotPv_desc + i)) = '\0';
    strcpy(*(slotPv_desc + i), *(slotPv + i));
    strcat(*(slotPv_desc + i), ".DESC");

    /* Allocate array to store description field */
    *(slotDesc + i) = (char *)malloc(sizeof(char) * 255);
    *(*(slotDesc + i)) = '\0';
  }

  /* Get description strings from runcontrolSlot records */
  for (i = 0; i < imaxSlots; i++)
    if (ca_search(*(slotPv_desc + i), &(slotPV_chid[i])) != ECA_NORMAL)
      return (RUNCONTROL_ERROR);
  if (ca_pend_io(pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  for (i = 0; i < imaxSlots; i++) {
    *(slotDesc + i) = (char *)malloc(sizeof(char) * 256);
    if (ca_get(DBR_STRING, slotPV_chid[i], *(slotDesc + i)) != ECA_NORMAL)
      return (RUNCONTROL_ERROR);
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  /* Scan slots to make sure nobody has one with same description. */
  for (i = 0; i < imaxSlots; i++) {
    if (!strcmp(desc, *(slotDesc + i))) { /* matching desc found */
      return (RUNCONTROL_DENIED);
    }
  }

  for (i = 0; i < imaxSlots; i++) {
    if (strlen(*(slotDesc + i)) == 0) { /* free slot found */
      strcpy(handle, *(slotPv + i));
      return (RUNCONTROL_OK);
    }
  }
  return (RUNCONTROL_DENIED);
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
  short sev = severity;

  if ((ca_put(DBR_STRING, (*rcInfo).pv_msg_chid, message) != ECA_NORMAL) ||
      (ca_put(DBR_SHORT, (*rcInfo).pv_alrm_chid, &sev) != ECA_NORMAL))
    return (RUNCONTROL_ERROR);
  if (ca_pend_io((*rcInfo).pendIOtime) != ECA_NORMAL)
    return (RUNCONTROL_ERROR);
  return (RUNCONTROL_OK);
}

static void sem_put_callback(struct event_handler_args arg) {
  *(int *)arg.usr = arg.status;
  return;
}

static int sem_check(int *sem, double pendIOtime) {
  double max;
  long i = 0;
  max = pendIOtime / .0001;

  while ((*sem == -1000) && (i < max)) {
    i++;
    ca_pend_event(.0001);
  }
  if (*sem != ECA_NORMAL)
    return (RUNCONTROL_DENIED);
  return (ECA_NORMAL);
}
