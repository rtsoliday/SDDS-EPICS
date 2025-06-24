/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* SCCS ID: @(#)rampload.c	1.5 5/5/94 */
/*************************************************************************
 * FILE:      rampload.c
 * Author:    Claude Saunders
 * 
 * Purpose:   Program for loading ramp tables to ramptable records on
 *            an IOC. The IOC record and device support software then
 *            transfers the table to the PSCU.
 *
 * Mods:      login mm/dd/yy: description
 *************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include "cadef.h"
#include "SDDS.h"
#include "mdb.h"

#define  VERSION   "V1.0"
#define  MAX_RAMP_POINTS 4096

char *usage="ramploadnew <rampfile> \n\
<rampfile> must have CorrName (eg. B2C0V) parameter \n\
           and RampTime, RampSetpoint columns, the number of rows can not exceed 4096.\n\
ramploadnew is a new and simplified rampload to load the ramps into new AFGs.\n\n";

int main(int argc, char *argv[]) {
  SDDS_TABLE table;
  char *corrname=NULL, *filename=NULL;
  double *ramptime;               /* ramp time values supplied in spec file */
  double *rampsetpoint;           /* setpoints supplied in spec file */
  double  maxsetpoint, minsetpoint, zerovalue=0.0, onevalue=1.0, fixedValue=55;
  chid ramp_procID, ramp_setID, ramp_timeID, eventProcID;
  char ramp_set[256], ramp_time[256], ramp_proc[256];
  
  long nrows, tablenum=0;
  int exitstatus = 0;
  long i;
  fprintf(stdout,"Booster Ramp Table Load New Utility %s\n",VERSION);
  fprintf(stdout,"------------------------------------\n");

  if (argc==1) {
    fprintf(stderr, "%s\n", usage);
    return 1;
  }
  filename = argv[1];
  
  if (!SDDS_InitializeInput(&table, filename)) {
    fprintf(stderr,"%s: SDDS error: unable to initialize with file %s\n",
	    argv[0], filename);
    return(1);
  }
  
  ca_task_initialize();
  if (ca_search("It:EG1:vme_eventLO", &eventProcID)!=ECA_NORMAL) {
    fprintf(stderr,"%s: problem doing ca_search for gbl pvs.\n",argv[0]);
    return(1);
  }
  if (ca_pend_io(30)!=ECA_NORMAL) {
    fprintf(stderr, "%s: problem doing ca_search1 for It:EG1:vme_eventLO\n",argv[0]);
    return(1);
  }
  while ((tablenum = SDDS_ReadTable(&table)) != -1) {
    if (tablenum == 0) {
      fprintf(stderr,"%s: SDDS error: unable to read data table from %s\n",
	      argv[0], filename);
      return(1);
    }
        
    if ((corrname = SDDS_GetParameterAsString(&table, "CorrName", NULL)) == NULL) {
      fprintf(stderr,"%s: SDDS error: unable to read ControlName parameter\n",argv[0]);
      return(1);
    }
    if ((nrows = SDDS_RowCount(&table)) == -1) {
      fprintf(stderr,"%s: SDDS error: unable to get row count\n",argv[0]);
      return(1);
    }

    if (nrows < 2 || nrows > MAX_RAMP_POINTS) {
      fprintf(stderr,"%s: device %s: you must specify at least 2 ramp points and at most %d\n",argv[0], corrname,MAX_RAMP_POINTS);
      return(1);
    }

    if ((ramptime = SDDS_GetColumn(&table, "RampTime"))==NULL) {
      fprintf(stderr,"%s: SDDS error: unable to read RampTime column\n",argv[0]);
      return(1);
    }
    if ((rampsetpoint = SDDS_GetColumn(&table, "RampSetpoint"))==NULL) {
      fprintf(stderr,"%s: SDDS error: unable to read RampSetpoint column\n",argv[0]);
      return(1);
    }
    
    /*check monotonic increasing  of ramp time and find the max setpoint */
    maxsetpoint = minsetpoint = rampsetpoint[0];
    for (i=1; i<nrows; i++) {
      if (rampTime[i]<0) {
        fprintf(stderr, "%s, negative ramp time found at %ld point.\n", argv[0], i);
        exitstatus = 1;
      }
      if (ramptime[i]<ramptime[i-1]) {
        fprintf(stderr, "%s, ramp time descreases at %ld point.\n", argv[0], i);
        exitstatus = 1;
      }
      if (rampsetpoint[i]>maxsetpoint)
        maxsetpoint = rampsetpoint[i];
      if (rampsetpoint[i]<minsetpoint)
        minsetpoint = rampsetpoint[i];
    }
    if (minsetpoint<-1 || maxsetpoint>1) {
      fprintf(stderr, "%s, ramp setpoint of %s less than -1 or greater than 1.\n", argv[0], corrname);
      exitstatus = 1;
    }
    
    if (exitstatus) {
      fprintf(stderr,"%s: exiting, no rampload initiated\n",argv[0]);
      return(exitstatus);
    }
    
    sprintf(ramp_set, "B:%s:source", corrname);
    sprintf(ramp_time, "B:%s:time", corrname);
    sprintf(ramp_proc, "B:%s:sub.PROC", corrname);
    if ((ca_search(ramp_time, &ramp_timeID)!=ECA_NORMAL) ||
        (ca_search(ramp_set, &ramp_setID)!=ECA_NORMAL) ||
        (ca_search(ramp_proc, &ramp_procID)!=ECA_NORMAL)) {
      fprintf(stderr,"%s: problem doing ca_search for %s\n",argv[0], corrname);
      return(1);
    } 
    if (ca_pend_io(30)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_search1 for %s %s %s\n",argv[0], ramp_time, ramp_set, ramp_proc);
      return(1);
    }
    /* Write ramp table out to PV */
    if ((ca_state(ramp_timeID)!=cs_conn) ||
        (ca_state(ramp_setID)!=cs_conn)) {
      fprintf(stderr, "%s: unable to connect to %s\n",argv[0], corrname);
      return(1);
    }
    if ((ca_array_put(DBR_DOUBLE, nrows, ramp_timeID, ramptime)!=ECA_NORMAL) ||
        (ca_array_put(DBR_DOUBLE, nrows, ramp_setID, rampsetpoint)!=ECA_NORMAL)) {
      fprintf(stderr,"%s: problem doing ca_array_put for %s\n",argv[0], corrname);
      return(1);
    }
    if (ca_pend_io(15)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_array_put for %s\n",argv[0], corrname);
      return(1);
    }
    if (ca_put(DBR_DOUBLE, ramp_procID, &onevalue)!=ECA_NORMAL) {
      fprintf(stderr,"%s: problem doing ca_array_put for %s proc\n",argv[0], corrname);
      return(1);
    }
    if (ca_pend_io(15)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_array_put for %s\n",argv[0], corrname);
      return(1);
    }
    free(ramptime); ramptime=NULL;
    free(rampsetpoint); rampsetpoint=NULL;
    free(corrname); corrname=NULL;
    /*ca_pend_event(1);*/
  }
  if (ca_put(DBR_DOUBLE, eventProcID, &fixedValue)!=ECA_NORMAL) {
    fprintf(stderr, "%s: problem doing ca_put for gbl pvs\n", argv[0]);
     ca_task_exit();
     return(1);
  }
  if (ca_pend_io(15)!=ECA_NORMAL) {
    fprintf(stderr, "%s: problem doing ca_put for gbl pvs\n",argv[0]);
    ca_task_exit();
    return(1);
  }
  ca_task_exit();
  if (!SDDS_Terminate(&table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  return(0);
}
