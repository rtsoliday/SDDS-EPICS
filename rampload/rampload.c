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

#define  VERSION   "V1.5.4"
#define  MAX_RAMP_POINTS 4096

/* Function Prototypes */
int process_cmdline_args(int argc, char *argv[], char *spec);
void usage(char *name);

/* PV name fields of interest in the ramptable record */
static struct ramp_rec_pvs {
  char ramp_set[255];       /* VAL field (array of setpoints) */
  char ramp_time[255];      /* TIMA field (array of time values) */
  char ramp_bias[255];      /* BIAS field */
  char ramp_nelm[255];      /* NELM field */
  char ramp_timf[255];      /* TIMF field */
  char ramp_timl[255];      /* TIML field */
  char ramp_drvh[255];     /* DRVH field */
  char ramp_drvl[255];     /* DRVL field */
  char ramp_gain[255];     /* afg gain*/
  chid chid_set;
  chid chid_time;
  chid chid_bias;
  chid chid_nelm;
  chid chid_timf;
  chid chid_timl;
  chid chid_drvh;
  chid chid_drvl;
  chid chid_gain;
};
  
int main(int argc, char *argv[]) {
  SDDS_TABLE table;
  char **name;                         /* device name or process variable */
  char **type;                         /* dev or pv */
  double *ramptime;               /* ramp time values supplied in spec file */
  double padramptime[MAX_RAMP_POINTS]; /* padded to make MAX_RAMP_POINTS */
  double *rampsetpoint;           /* setpoints supplied in spec file */
  double padrampsetpoint[MAX_RAMP_POINTS]; /* padded to make MAX_RAMP_POINTS */
  double *rampbias;
  double rampnelm, ramptimf, ramptiml, drivehigh, drivelow, gain, maxsetpoint;

  char pvname[255];           /* name of ramptable record process variable */

  struct ramp_rec_pvs ramp;   /* struct containing PV names and chids */
  char specfilename[255];     /* specification file name */
  char *contents;             /* contents field of SDDS descrip. */
  long nrows;
  int exitstatus = 0;
  long i;
  int tablenum = 0;

  fprintf(stdout,"Booster Ramp Table Load Utility %s\n",VERSION);
  fprintf(stdout,"------------------------------------\n");

  if (process_cmdline_args(argc, argv, specfilename)) {
    return(1);
  }

  if (!SDDS_InitializeInput(&table, specfilename)) {
    fprintf(stderr,"%s: SDDS error: unable to initialize with file %s\n",
	    argv[0], specfilename);
    return(1);
  }
  
  contents = NULL;
  /* comment this part because it is not uncessary (HS)
    if (!SDDS_GetDescription(&table, NULL, &contents)) {
    fprintf(stderr,"%s: SDDS error: unable to get description from file %s\n",
    argv[0], specfilename);
    return(1);
    }
    if (contents==NULL || strcmp(contents,"booster ramp")) {
    fprintf(stderr,"%s: SDDS error: file %s is not a booster ramp specification\n",argv[0],specfilename);
    return(1);
    }
    */
  ca_task_initialize();

  while ((tablenum = SDDS_ReadTable(&table)) != -1) {
    if (tablenum == 0) {
      fprintf(stderr,"%s: SDDS error: unable to read data table from %s\n",
	      argv[0], specfilename);
      return(1);
    }
        
    if ((name = SDDS_GetParameter(&table, "ControlName", NULL)) == NULL) {
      fprintf(stderr,"%s: SDDS error: unable to read ControlName parameter\n",argv[0]);
      return(1);
    }
    
    if ((type = SDDS_GetParameter(&table, "ControlType", NULL)) == NULL) {
      fprintf(stderr,"%s: SDDS error: unable to read ControlType parameter\n",argv[0]);
      return(1);
    }

    if ((rampbias = SDDS_GetParameter(&table, "RampBias", NULL)) == NULL) {
      fprintf(stderr,"%s: SDDS error: unable to read RampBias parameter\n",argv[0]);
      return(1);
    }
    
    if ((nrows = SDDS_RowCount(&table)) == -1) {
      fprintf(stderr,"%s: SDDS error: unable to get row count\n",argv[0]);
      return(1);
    }

    if (nrows < 2 || nrows > MAX_RAMP_POINTS) {
      fprintf(stderr,"%s: device %s: you must specify at least 2 ramp points and at most %d\n",argv[0],*name,MAX_RAMP_POINTS);
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
    ramptiml = ramptime[0];
    ramptimf = ramptime[nrows-1];

    /*check monotonic increasing  of ramp time and find the max setpoint */
    maxsetpoint = rampsetpoint[0];
    for (i=1; i<nrows; i++) {
      if (ramptime[i]<0) {
        fprintf(stderr, "%s, negative ramp time at %ld point.\n", argv[0], i);
        exitstatus = 1;
      }
      if (ramptime[i]<ramptime[i-1]) {
        fprintf(stderr, "%s, ramp time descreases at %ld point.\n", argv[0], i);
        exitstatus = 1;
      }
      if (rampsetpoint[i]>maxsetpoint)
        maxsetpoint = rampsetpoint[i];
    }
    /* Pad out remaining ramp points if fewer than MAX_RAMP_POINTS given */
    for (i=0 ; i < nrows ; i++) {
      padramptime[i] = ramptime[i];
      padrampsetpoint[i] = rampsetpoint[i];
    }
    for (i=nrows ; i < MAX_RAMP_POINTS ; i++) {
      padramptime[i] = 0.0;
      padrampsetpoint[i] = 0.0;
    }

    /* Take ControlName (with ControlType) and extract out needed PV names */
    if (!strcmp(*type,"dev")) {        /* device name specified in input */
      fprintf(stderr,"%s: dev ControlType no longer supported\n", argv[0]);
      exitstatus = 1;
    } else {                           /* pv name specified in input */
      strcpy_ss(pvname, *name);
    }
    if (exitstatus) {
      fprintf(stderr,"%s: exiting, no rampload initiated\n",argv[0]);
      return(exitstatus);
    }
    /* strcpy_ss(ramp.ramp_time, pvname);
    strcat(ramp.ramp_time, ".TIMA");
    strcpy_ss(ramp.ramp_bias, pvname);
    strcat(ramp.ramp_bias, ".BIAS");
    strcpy_ss(ramp.ramp_nelm, pvname);
    strcat(ramp.ramp_nelm, ".NELM");
    strcpy_ss(ramp.ramp_set, pvname); */
    sprintf(ramp.ramp_set, "%s", pvname);
    sprintf(ramp.ramp_time, "%s.TIMA", pvname);
    sprintf(ramp.ramp_bias, "%s.BIAS", pvname);
    sprintf(ramp.ramp_nelm, "%s.NELM", pvname);
    sprintf(ramp.ramp_timf, "%s.TIMF", pvname);
    sprintf(ramp.ramp_timl, "%s.TIML", pvname);
    sprintf(ramp.ramp_gain, "%s.GAIN", pvname);
    sprintf(ramp.ramp_drvh, "%s.DRVH", pvname);
    sprintf(ramp.ramp_drvl, "%s.DRVL", pvname);
    
    if ((ca_search(ramp.ramp_nelm, &ramp.chid_nelm)!=ECA_NORMAL) ||
        (ca_search(ramp.ramp_time, &ramp.chid_time)!=ECA_NORMAL) ||
        (ca_search(ramp.ramp_set, &ramp.chid_set)!=ECA_NORMAL) ||
        (ca_search(ramp.ramp_bias, &ramp.chid_bias)!=ECA_NORMAL) ||
        (ca_search(ramp.ramp_timf, &ramp.chid_timf)!=ECA_NORMAL) ||
        (ca_search(ramp.ramp_timl, &ramp.chid_timl)!=ECA_NORMAL) ||
        (ca_search(ramp.ramp_gain, &ramp.chid_gain)!=ECA_NORMAL) ||
        (ca_search(ramp.ramp_drvh, &ramp.chid_drvh)!=ECA_NORMAL) ||
        (ca_search(ramp.ramp_drvl, &ramp.chid_drvl)!=ECA_NORMAL)) {
      fprintf(stderr,"%s: problem doing ca_search for %s\n",argv[0], ramp.ramp_set);
      return(1);
    }
    if (ca_pend_io(15)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_search for %s\n",argv[0], *name);
      return(1);
    }
    if (ca_state(ramp.chid_nelm)!=cs_conn) {
      fprintf(stderr, "%s: unable to connect to %s\n",argv[0],ramp.ramp_nelm);
      return(1);
    }
    if (ca_get(DBR_DOUBLE, ramp.chid_nelm, &rampnelm)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_get for %s\n",argv[0],ramp.ramp_nelm);
      return(1);
    }
    if (ca_get(DBR_DOUBLE, ramp.chid_gain, &gain)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_get for %s\n",argv[0],ramp.ramp_gain);
      return(1);
    }
    if (ca_get(DBR_DOUBLE, ramp.chid_drvh, &drivehigh)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_get for %s\n",argv[0],ramp.ramp_drvh);
      return(1);
    }
    if (ca_get(DBR_DOUBLE, ramp.chid_drvl, &drivelow)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_get for %s\n",argv[0],ramp.ramp_drvl);
      return(1);
    }

    if (ca_pend_io(5)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_get for %s\n",argv[0],ramp.ramp_nelm);
      return(1);
    }
    if (nrows > (int)rampnelm) {
      fprintf(stderr,"%s: too many ramp points specified for %s\n", argv[0], *name);
      fprintf(stderr,"%s: ramp table record limited to %d\n", argv[0], (int)rampnelm);
      return(1);
    }
    if (ca_put(DBR_DOUBLE, ramp.chid_timf, &ramptimf)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_put for %s\n",argv[0],ramp.ramp_timf);
      return(1);
    }
    if (ca_put(DBR_DOUBLE, ramp.chid_timl, &ramptiml)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_put for %s\n",argv[0],ramp.ramp_timl);
      return(1);
    }
    if (ca_pend_io(15)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_get/put for %s\n",argv[0], *name);
      return(1);
    }
    /*check setpoint within valid range*/
    maxsetpoint = maxsetpoint * gain * drivehigh + *rampbias * drivehigh;
    if (maxsetpoint > drivehigh || maxsetpoint < drivelow) {
      fprintf(stderr,  "%s: maximum setpoint output of range.\n", argv[0]);
      return(1);
    }
    /* Write ramp table out to PV */
    if ((ca_state(ramp.chid_time)!=cs_conn) ||
        (ca_state(ramp.chid_set)!=cs_conn) ||
        (ca_state(ramp.chid_bias)!=cs_conn)) {
      fprintf(stderr, "%s: unable to connect to %s\n",argv[0], *name);
      return(1);
    }
    if ((ca_array_put(DBR_DOUBLE, nrows, ramp.chid_time, padramptime)!=ECA_NORMAL) ||
        (ca_array_put(DBR_DOUBLE, nrows, ramp.chid_set, padrampsetpoint)!=ECA_NORMAL) ||
        (ca_put(DBR_DOUBLE, ramp.chid_bias, rampbias)!=ECA_NORMAL)) {
      fprintf(stderr,"%s: problem doing ca_array_put for %s\n",argv[0], *name);
      return(1);
    }
    if (ca_pend_io(15)!=ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_array_put for %s\n",argv[0], *name);
      return(1);
    }
    free(ramptime);
    free(rampsetpoint);
    /*ca_pend_event(1);*/
  }
  ca_task_exit();
  if (!SDDS_Terminate(&table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return(1);
  }
  return(0);

}

/*************************************************************************
 * FUNCTION : process_cmdline_args()
 * PURPOSE  : Get command line arguments and set flags appropriately.
 * ARGS in  : argc, argv - standard unix
 * ARGS out : spec - name of ramp specification file
 * GLOBAL   : nothing
 * RETURNS  : 0
 ************************************************************************/  
int process_cmdline_args(int argc, char *argv[], char *spec) {
  int cmderror = 0;

  *spec = '\0';

  /* Process command line options */
  if (argc != 2)
    cmderror++;
  else {
    strcpy_ss(spec,argv[1]);
  }

  if (strlen(spec)==0)
    cmderror++;

  if (cmderror) {
    usage(argv[0]);
    return(1);
  }
  return(0);
}

/*************************************************************************
 * FUNCTION : usage()
 * PURPOSE  : Describes command line options available to client.
 * ARGS in  : char *name - name of executable program
 * ARGS out : none
 * GLOBAL   : nothing
 * RETURNS  : none
 ************************************************************************/  
void usage(char *name) {
  fprintf(stderr,"usage: %s spec-file\n",name);
  fprintf(stderr,"purpose:\tLoads the specified ramp tables to the IOC.\n");
  fprintf(stderr,"\t\tNote: Setpoints are normalized (0 to 1 or -1 to 1)\n");
  fprintf(stderr,"\t\tRamp Control screen must be used to activate the\n");
  fprintf(stderr,"\t\tnew tables. Status also provided by screen.\n");
  fprintf(stderr,"sample ramp spec-file:\n");
  fprintf(stderr,"SDDS1\n");
  fprintf(stderr,"&description\n");
  fprintf(stderr,"        text=\"Booster Ramp Tables\"\n");
  fprintf(stderr,"        contents=\"booster ramp\"\n");
  fprintf(stderr,"&end\n");
  fprintf(stderr,"&parameter name=ControlName\n");
  fprintf(stderr,"           type=string\n");
  fprintf(stderr,"&end\n");
  fprintf(stderr,"&parameter name=ControlType\n");
  fprintf(stderr,"           type=string\n");
  fprintf(stderr,"&end\n");
  fprintf(stderr,"&parameter name=RampBias\n");
  fprintf(stderr,"           type=double\n");
  fprintf(stderr,"&end\n");
  fprintf(stderr,"&column name=RampTime\n");
  fprintf(stderr,"        type=double\n");
  fprintf(stderr,"&end\n");
  fprintf(stderr,"&column name=RampSetpoint\n");
  fprintf(stderr,"        type=double\n");
  fprintf(stderr,"&end\n");
  fprintf(stderr,"&data   mode=ascii\n");
  fprintf(stderr,"        no_row_counts=1\n");
  fprintf(stderr,"&end\n");
  fprintf(stderr,"B:BM:VoltageRT\n");
  fprintf(stderr,"pv\n");
  fprintf(stderr,"0.1\n");
  fprintf(stderr,"0.0  0.0\n");
  fprintf(stderr,"230.0 0.8\n");
  fprintf(stderr,"250.0 0.8\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"B:QF:VoltageRT\n");
  fprintf(stderr,"pv\n");
  fprintf(stderr,"0.13\n");
  fprintf(stderr,"0.0  0.0\n");
  fprintf(stderr,"100.0 0.75\n");
  fprintf(stderr,"250.0 0.8\n");
}
