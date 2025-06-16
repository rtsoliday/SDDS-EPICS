/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\*************************************************************************/
/* SCCS ID: @(#)UnixDegauss.c	1.7 2/16/94 */
/*************************************************************************
 * FILE:      UnixDegauss.c
 * Author:    Claude Saunders
 *
 * Purpose:   Program for degaussing magnets. An SDDS data file is
 *            read which specifies which devices (or PV's) are to be
 *            degaussed and how. Subroutine records on IOC's are
 *            configured appropriately and processed to begin degaussing
 *            Various options allow the user to start/stop/query
 *            the degaussing process.
 *
 * Mods:      login mm/dd/yy: description
 *************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stddef.h>
#include <cadef.h>
#include <SDDS.h> /* SDDS library header */
#include <mdb.h>

#define CA_TIMEOUT 10.0

/* Function Prototypes */
void sleep_us(unsigned int);
int validate_time(int32_t min, int32_t sec);
int process_cmdline_args(int argc, char *argv[], char *spec, int *query,
                         int *configure, int *stop, double *pendIoTime);
void usage(char *name);
void oag_ca_exception_handler(struct exception_handler_args args);

/****************************
 Degauss PV values:                             SDDS Column
 .B   Minutes for one decay cycle               DecayMinutes
 .C   Seconds for one decay cycle               DecaySeconds
 .D   Seconds for one sine wave period          PeriodSeconds
 .E   Max current                               MaxCurrent
 .F   Cycles                                    NumDecay
 .I   Status

 Tri-Degauss PV values
 .E   Peak current for first cycle              MaxCurrent
 .F   Cycles                                    NumDecay
 .G   Peak current for last cycle               LastPeakCurrent
 .H   Time between output steps                 OutputInterval
 .I   Maximum step size between two setpoints   -------------

 APSU Tri-Degauss PV values
 .B   Peak current for first cycle              FirstPeakCurrent
 .C   Cycles                                    NumCycles
 .D   Peak current for last cycle               LastPeakCurrent
 .E   Time between output steps                 OutputInterval
 .F   Maximum step size between two setpoints   MaxStepSize

***************************/

struct sub_rec_pvs {
  char sub_proc[255];
  chid sub_proc_chid;
  char sub_snam[255]; /* is proper subroutine attached ? */
  chid sub_snam_chid;
  char sub_b[255];
  chid sub_b_chid;
  char sub_c[255];
  chid sub_c_chid;
  char sub_d[255];
  chid sub_d_chid;
  char sub_e[255];
  chid sub_e_chid;
  char sub_f[255];
  chid sub_f_chid;
  char sub_g[255];
  chid sub_g_chid;
  char sub_h[255];
  chid sub_h_chid;
  char sub_i[255];
  chid sub_i_chid;
  char sub_outb[255];
  chid sub_outb_chid;
  char sub_outc[255];
  chid sub_outc_chid;
  char sub_outd[255];
  chid sub_outd_chid;
};

int main(int argc, char *argv[]) {
  SDDS_TABLE table;
  char **name;                   /* device name or process variable */
  char **suffix = NULL;          /* suffix name, some pv maybe named as degaussSU */
  char **type;                   /* dev or pv */
  int32_t *decaymin = NULL;      /* .B .B -- */
  int32_t *decaysec = NULL;      /* .C .C -- */
  int32_t *periodsec = NULL;     /* .D .D -- */
  double *imax = NULL;           /* .E .E .B */
  int32_t *numdecay = NULL;      /* .F .F .C */
  double *imin = NULL;           /* -- .G .D */
  double *outputInterval = NULL; /* -- .F .E */
  double *maxstepsize = NULL;    /* -- -- .F */

  char **pvname; /* name of subroutine record process variable */
  char **outb = NULL, **outc = NULL, **outd = NULL;

  struct sub_rec_pvs *sub;
  /*  devstructarr info;*/
  /*  devnamearr namearr; */
  int query, configure, stop;
  double pendIoTime;
  char specfilename[255]; /* specification file name */
  char *contents;         /* contents fields of SDDS descrip. */

  long nrows;
  int status;
  long i, k, l, nsupplies;
  double *state; /* gets proc field value from subroutine record */
  int *ndevs;    /* number of atomic devices in device i */
  char **subname;
  char **dname;
  short start = 1;
  short halt = 0;
  char buffer[255];
  short trimode = 1;
  short apsuTriMode = 0;

  fprintf(stdout, "Magnet Degaussing Utility\n");
  fprintf(stdout, "------------------------------\n");

  process_cmdline_args(argc, argv, specfilename, &query, &configure, &stop, &pendIoTime);

  if (!SDDS_InitializeInput(&table, specfilename)) {
    fprintf(stderr, "%s: SDDS error: unable to initialize with file %s\n",
            argv[0], specfilename);
    exit(1);
  }
  if (SDDS_ReadTable(&table) <= 0) {
    fprintf(stderr, "%s: SDDS error: unable to read data table from %s\n",
            argv[0], specfilename);
    exit(1);
  }
  SDDS_SetColumnFlags(&table, 1);
  SDDS_SetRowFlags(&table, 1);

  contents = NULL;
  if (!SDDS_GetDescription(&table, NULL, &contents)) {
    fprintf(stderr, "%s: SDDS error: unable to get description from file %s\n",
            argv[0], specfilename);
    exit(1);
  }
  if (contents == NULL) {
    fprintf(stderr, "%s: SDDS error: file %s is not a degaussing specification\n", argv[0], specfilename);
    exit(1);
  }
  if (strcmp(contents, "tri-degaussing specification") == 0) {
    apsuTriMode = 1;
    trimode = 0;
  } else if (strcmp(contents, "degaussing specification") != 0) {
    fprintf(stderr, "%s: SDDS error: file %s is not a degaussing specification\n", argv[0], specfilename);
    exit(1);
  }

  if ((nrows = SDDS_CountRowsOfInterest(&table)) == -1) {
    fprintf(stderr, "%s: SDDS error: unable to get row count\n", argv[0]);
    exit(1);
  }

  if ((name = SDDS_GetColumn(&table, "ControlName")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read controlname column\n", argv[0]);
    exit(1);
  }
  if ((type = SDDS_GetColumn(&table, "ControlType")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read controltype column\n", argv[0]);
    exit(1);
  }
  if (SDDS_CheckColumn(&table, "SuffixName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    if ((suffix = (char **)SDDS_GetColumn(&table, "SuffixName")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read SuffixName column\n", argv[0]);
      exit(1);
    }
  }
  if (apsuTriMode) {
    if ((imax = SDDS_GetColumn(&table, "FirstPeakCurrent")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read FirstPeakCurrent column\n", argv[0]);
      exit(1);
    }
    if ((numdecay = SDDS_GetColumn(&table, "NumCycles")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read NumCycles column\n", argv[0]);
      exit(1);
    }
    if ((imin = SDDS_GetColumn(&table, "LastPeakCurrent")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read LastPeakCurrent column\n", argv[0]);
      exit(1);
    }
    if ((outputInterval = SDDS_GetColumn(&table, "OutputInterval")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read OutputInterval column\n", argv[0]);
      exit(1);
    }
    if ((maxstepsize = SDDS_GetColumn(&table, "MaxStepSize")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read MaxStepSize column\n", argv[0]);
      exit(1);
    }
    outb = SDDS_GetColumn(&table, "OUTB");
    outc = SDDS_GetColumn(&table, "OUTC");
    outd = SDDS_GetColumn(&table, "OUTD");
  } else {
    if ((decaymin = SDDS_GetColumn(&table, "DecayMinutes")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read DecayMinutes column\n", argv[0]);
      exit(1);
    }
    if ((decaysec = SDDS_GetColumn(&table, "DecaySeconds")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read DecaySeconds column\n", argv[0]);
      exit(1);
    }
    if ((periodsec = SDDS_GetColumn(&table, "PeriodSeconds")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read PeriodSeconds column\n", argv[0]);
      exit(1);
    }
    if ((imax = SDDS_GetColumn(&table, "MaxCurrent")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read MaxCurrent column\n", argv[0]);
      exit(1);
    }
    if ((numdecay = SDDS_GetColumn(&table, "NumDecay")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read NumDecay column\n", argv[0]);
      exit(1);
    }
    if (SDDS_GetColumnIndex(&table, "LastPeakCurrent") == -1) {
      trimode = 0;
    } else {
      if ((imin = SDDS_GetColumn(&table, "LastPeakCurrent")) == NULL) {
        fprintf(stderr, "%s: SDDS error: unable to read LastPeakCurrent column\n", argv[0]);
        exit(1);
      }
      if ((outputInterval = SDDS_GetColumn(&table, "OutputInterval")) == NULL) {
        fprintf(stderr, "%s: SDDS error: unable to read OutputInterval column\n", argv[0]);
        exit(1);
      }
    }
    for (i = 0; i < nrows; i++) {
      if (validate_time(decaymin[i], decaysec[i])) {
        fprintf(stderr, "%s: %s invalid decay time specified\n", argv[0], name[i]);
        exit(1);
      }
      if (validate_time(0, periodsec[i])) {
        fprintf(stderr, "%s: %s invalid period time specified\n", argv[0], name[i]);
        exit(1);
      }
    }
  }
  ndevs = (int *)malloc(sizeof(int) * nrows);

  for (i = 0, nsupplies = 0; i < nrows; i++) {
    if ((!strcmp(type[i], "dev")) || (!strcmp(type[i], "pv"))) {
      nsupplies += 1;
      ndevs[i] = 1;
    } else {
      fprintf(stderr, "%s: illegal ControlType field for %s\n",
              argv[0], name[i]);
      fprintf(stderr, "%s: exiting, no degaussing initiated\n", argv[0]);
      exit(1);
    }
  }

  sub = (struct sub_rec_pvs *)malloc(sizeof(struct sub_rec_pvs) * nsupplies);
  state = (double *)malloc(sizeof(double) * nsupplies);
  pvname = (char **)malloc(sizeof(char *) * nsupplies);
  subname = (char **)malloc(sizeof(char *) * nsupplies);
  dname = (char **)malloc(sizeof(char *) * nsupplies);
  for (i = 0; i < nsupplies; i++) {
    pvname[i] = (char *)malloc(sizeof(char) * 255);
    subname[i] = (char *)malloc(sizeof(char) * 255);
    dname[i] = (char *)malloc(sizeof(char) * 255);
  }

  for (i = 0; i < nrows; i++) {
    strcpy(dname[i], name[i]);
  }

  /* Build pv names for needed sub record fields based on given pv name */
  for (i = 0; i < nrows; i++) {
    if (!strcmp(type[i], "dev")) { /* device name specified in input */
      strcpy(buffer, name[i]);
      if (!suffix || (suffix[i] && is_blank(suffix[i])))
        strcat(buffer, ":StandardizeSUB");
      else {
        strcat(buffer, ":");
        strcat(buffer, suffix[i]);
      }
      strcpy(pvname[i], buffer);
    } else {
      strcpy(pvname[i], name[i]);
    }
  }
  for (i = 0, k = 0; i < nrows; i++) {    /* for each row in sdds file */
    for (l = 0; l < ndevs[i]; l++, k++) { /* for each atomic in comp */
      strcpy(sub[k].sub_proc, pvname[k]);
      strcat(sub[k].sub_proc, ".PROC");
      strcpy(sub[k].sub_snam, pvname[k]);
      strcat(sub[k].sub_snam, ".SNAM");
      strcpy(sub[k].sub_b, pvname[k]);
      strcat(sub[k].sub_b, ".B");
      strcpy(sub[k].sub_c, pvname[k]);
      strcat(sub[k].sub_c, ".C");
      strcpy(sub[k].sub_d, pvname[k]);
      strcat(sub[k].sub_d, ".D");
      strcpy(sub[k].sub_e, pvname[k]);
      strcat(sub[k].sub_e, ".E");
      strcpy(sub[k].sub_f, pvname[k]);
      strcat(sub[k].sub_f, ".F");
      if (trimode) {
        strcpy(sub[k].sub_g, pvname[k]);
        strcat(sub[k].sub_g, ".G");
        strcpy(sub[k].sub_h, pvname[k]);
        strcat(sub[k].sub_h, ".H");
      }
      if (apsuTriMode) {
        strcpy(sub[k].sub_outb, pvname[k]);
        strcat(sub[k].sub_outb, ".OUTB");
        strcpy(sub[k].sub_outc, pvname[k]);
        strcat(sub[k].sub_outc, ".OUTC");
        strcpy(sub[k].sub_outd, pvname[k]);
        strcat(sub[k].sub_outd, ".OUTD");
      }
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
  ca_add_exception_event(oag_ca_exception_handler, NULL);

  fprintf(stdout, "Searching for devices...");
  fflush(stdout);
  for (i = 0, k = 0; i < nrows; i++) {
    for (l = 0; l < ndevs[i]; l++, k++) {
      status = ca_search(sub[k].sub_proc, &(sub[k].sub_proc_chid));
      if (status != ECA_NORMAL) {
        fprintf(stderr, "%s: unable to connect with %s\n", argv[0], dname[k]);
        exit(1);
      }
      status = ca_search(sub[k].sub_snam, &(sub[k].sub_snam_chid));
      status |= ca_search(sub[k].sub_b, &(sub[k].sub_b_chid));
      status |= ca_search(sub[k].sub_c, &(sub[k].sub_c_chid));
      status |= ca_search(sub[k].sub_d, &(sub[k].sub_d_chid));
      status |= ca_search(sub[k].sub_e, &(sub[k].sub_e_chid));
      status |= ca_search(sub[k].sub_f, &(sub[k].sub_f_chid));
      if (trimode) {
        status |= ca_search(sub[k].sub_g, &(sub[k].sub_g_chid));
        status |= ca_search(sub[k].sub_h, &(sub[k].sub_h_chid));
      }
      if (apsuTriMode) {
        status |= ca_search(sub[k].sub_outb, &(sub[k].sub_outb_chid));
        status |= ca_search(sub[k].sub_outc, &(sub[k].sub_outc_chid));
        status |= ca_search(sub[k].sub_outd, &(sub[k].sub_outd_chid));
      }
      if (status != ECA_NORMAL) {
        fprintf(stderr, "%s: unable to connect with %s\n", argv[0], dname[k]);
        exit(1);
      }
    }
  }
  status = ca_pend_io(pendIoTime);
  if (status != ECA_NORMAL) {
    fprintf(stderr, "%s: unable to complete connections\n", argv[0]);
    exit(1);
  }
  fprintf(stdout, "done\n");

  /* Check to make sure subroutine is degauss, not standardize */
  for (i = 0, k = 0; i < nrows; i++) {
    for (l = 0; l < ndevs[i]; l++, k++) {
      status = ca_get(DBR_STRING, sub[k].sub_snam_chid, subname[k]);
      if (status != ECA_NORMAL) {
        fprintf(stderr, "%s: unable to ca_get from %s\n", argv[0], dname[k]);
        exit(1);
      }
      status = ca_pend_io(pendIoTime);
      if (status != ECA_NORMAL) {
        fprintf(stderr, "%s: unable to flush ca_get\n", argv[0]);
        exit(1);
      }
      if (strcmp(subname[k], "degaussProc") && strcmp(subname[k], "triDegaussProc")) {
        fprintf(stderr, "%25s : device has wrong subroutine specified (%s) for degaussing\n", dname[k], subname[k]);
        fprintf(stderr, "%s: exiting, no degaussing performed\n", argv[0]);
        exit(1);
      }
    }
  }

  if (stop) {
    fprintf(stdout, "Halting degaussing...");
    fflush(stdout);
    for (i = 0, k = 0; i < nrows; i++) {
      for (l = 0; l < ndevs[i]; l++, k++) {
        status = ca_put(DBR_SHORT, sub[k].sub_proc_chid, &halt);
        if (status != ECA_NORMAL) {
          fprintf(stderr, "%s: unable to ca_put to PROC field\n", argv[0]);
        }
        status = ca_pend_io(pendIoTime);
        if (status != ECA_NORMAL) {
          fprintf(stderr, "%s: unable to complete ca_put to PROC field\n",
                  argv[0]);
        }
      }
    }
    fprintf(stdout, "done\n");
  }

  if (!stop) {
    for (i = 0, k = 0; i < nrows; i++) {
      for (l = 0; l < ndevs[i]; l++, k++) {
        status = ca_get(DBR_DOUBLE, sub[k].sub_proc_chid, &(state[k]));
        if (status != ECA_NORMAL) {
          fprintf(stderr, "%s: unable to ca_get from %s\n", argv[0], dname[k]);
          exit(1);
        }
        status = ca_pend_io(pendIoTime);
        if (status != ECA_NORMAL) {
          fprintf(stderr, "%s: unable to flush ca_get\n", argv[0]);
          exit(1);
        }
        if (state[k] != 0.0) {
          fprintf(stdout, "%25s : degaussing active\n", dname[k]);
        } else {
          fprintf(stdout, "%25s : degaussing inactive\n", dname[k]);
        }
      }
    }
  }

  if (!query && !stop) {
    fprintf(stdout, "Configure records for degaussing...");
    fflush(stdout);
    for (i = 0, k = 0; i < nrows; i++) {
      for (l = 0; l < ndevs[i]; l++, k++) {
        if (state[k] == 0.0) {
          if (apsuTriMode) {
            status = ca_put(DBR_DOUBLE, sub[k].sub_b_chid, &(imax[i]));
            status |= ca_put(DBR_LONG, sub[k].sub_c_chid, &(numdecay[i]));
            status |= ca_put(DBR_DOUBLE, sub[k].sub_d_chid, &(imin[i]));
            status |= ca_put(DBR_DOUBLE, sub[k].sub_e_chid, &(outputInterval[i]));
            status |= ca_put(DBR_DOUBLE, sub[k].sub_f_chid, &(maxstepsize[i]));
            if (outb)
              status |= ca_put(DBR_STRING, sub[k].sub_outb_chid, outb[i]);
            if (outc)
              status |= ca_put(DBR_STRING, sub[k].sub_outc_chid, outc[i]);
            if (outd)
              status |= ca_put(DBR_STRING, sub[k].sub_outd_chid, outd[i]);
          } else {
            status = ca_put(DBR_LONG, sub[k].sub_b_chid, &(decaymin[i]));
            status |= ca_put(DBR_LONG, sub[k].sub_c_chid, &(decaysec[i]));
            status |= ca_put(DBR_LONG, sub[k].sub_d_chid, &(periodsec[i]));
            status |= ca_put(DBR_DOUBLE, sub[k].sub_e_chid, &(imax[i]));
            status |= ca_put(DBR_LONG, sub[k].sub_f_chid, &(numdecay[i]));
            if (trimode) {
              status |= ca_put(DBR_DOUBLE, sub[k].sub_g_chid, &(imin[i]));
              status |= ca_put(DBR_DOUBLE, sub[k].sub_h_chid, &(outputInterval[i]));
            }
          }
          if (status != ECA_NORMAL) {
            fprintf(stderr, "%s: unable to ca_put to %s\n", argv[0], dname[k]);
            exit(1);
          }
        }
      }
      status = ca_pend_io(pendIoTime);
      if (status != ECA_NORMAL) {
        fprintf(stderr, "%s: unable to complete ca_puts\n", argv[0]);
        exit(1);
      }
    }
    fprintf(stdout, "done\n");
  }

  if (!query && !stop && !configure) {
    fprintf(stdout, "Initiating degaussing...");
    fflush(stdout);
    sleep(1);
    for (i = 0, k = 0; i < nrows; i++) {
      for (l = 0; l < ndevs[i]; l++, k++) {
        if (state[k] == 0.0) {
          status = ca_put(DBR_SHORT, sub[k].sub_proc_chid, &start);
          if (status != ECA_NORMAL) {
            fprintf(stderr, "%s: unable to ca_put to PROC field\n", argv[0]);
            exit(1);
          }
          status = ca_pend_io(pendIoTime);
          if (status != ECA_NORMAL) {
            fprintf(stderr, "%s: unable to complete ca_put to PROC field\n", argv[0]);
            exit(1);
          }
        }
        sleep_us(100000);
      }
    }
    fprintf(stdout, "done\n");
  }
  /*  exit(0); */
  ca_task_exit();
  return (0);
}

/*************************************************************************
 * FUNCTION : validate_time()
 * PURPOSE  : Makes sure that time specs are valid. Ie. minutes and
 *            seconds are between 0 and 60.
 * ARGS in  : min:   minutes value for device i
 *            sec:   seconds value for device i
 * ARGS out : none
 * GLOBAL   : nothing
 * RETURNS  : 0 - time values valid
 *            -1 - at least one time value is invalid
 ************************************************************************/
int validate_time(int32_t min, int32_t sec) {
  if (min < 0 || min > 60 || sec < 0 || sec > 60)
    return (-1);
  else
    return (0);
}

/*************************************************************************
 * FUNCTION : process_cmdline_args()
 * PURPOSE  : Get command line arguments and set flags appropriately.
 * ARGS in  : argc, argv - standard unix
 * ARGS out : spec, query, configure, stop
 * GLOBAL   : nothing
 * RETURNS  : 0
 ************************************************************************/
int process_cmdline_args(int argc, char *argv[], char *spec, int *query,
                         int *configure, int *stop, double *pendIoTime) {
  int i, dd;
  int cmderror = 0;
  /* void usage();*/

  /* Process command line options */
  if (argc < 2 || argc > 5)
    cmderror++;
  else {
    *query = 0;
    *configure = 0;
    *stop = 0;
    *spec = '\0';
    *pendIoTime = CA_TIMEOUT;
    dd = 0;
    for (i = 1; i < argc; i++) {
      if (!strncmp("--", argv[i], 2))
        dd = 1;
      else if ((!dd) && (!strcmp(argv[i], "-q") || !strcmp(argv[i], "-query")))
        *query = 1;
      else if ((!dd) && (!strcmp(argv[i], "-c") || !strcmp(argv[i], "-configure")))
        *configure = 1;
      else if ((!dd) && (!strcmp(argv[i], "-s") || !strcmp(argv[i], "-stop")))
        *stop = 1;
      else if ((!dd) && (!strcmp(argv[i], "-p") ||
                         !strcmp(argv[i], "-pend") ||
                         !strcmp(argv[i], "-pendIoTime"))) {
        i++;
        if (i >= argc) {
          fprintf(stderr, "invalid -pendIoTime value (degauss)\n");
          exit(1);
        }
        if ((sscanf(argv[i], "%lf", pendIoTime) != 1) ||
            (*pendIoTime <= 0.0)) {
          fprintf(stderr, "invalid -pendIoTime value (degauss)\n");
          exit(1);
        }
      } else {
        if ((!dd) && (!strncmp("-", argv[i], 1))) {
          fprintf(stderr, "invalid option %s (degauss)\n", argv[i]);
          cmderror++;
        } else {
          strcpy(spec, argv[i]);
        }
      }
    }
  }

  if ((*query + *configure + *stop) > 1)
    cmderror++;
  if (strlen(spec) == 0)
    cmderror++;

  if (cmderror) {
    usage(argv[0]);
    exit(1);
  }
  return (0);
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
  fprintf(stderr, "usage: %s [-query -configure -stop]\n", name);
  fprintf(stderr, "       [-pendIoTime <seconds>] [--] spec-file\n");
  fprintf(stderr, "purpose:\tDegausses the set of devices specified in\n");
  fprintf(stderr, "        \tspec-file. The spec-file must be in SDDS format.\n");
  fprintf(stderr, "        \tEach row in spec-file contains the device name,\n");
  fprintf(stderr, "        \tdecay time, sin wave period, max current, and\n");
  fprintf(stderr, "        \tnumber of decay times desired.\n");
  fprintf(stderr, "        \tEntering \"degauss myfile\" begins degaussing.\n");
  fprintf(stderr, "options:\n");
  fprintf(stderr, "\t-query \n");
  fprintf(stderr, "\t\tPrint degaussing state of devices in\n");
  fprintf(stderr, "\t\tspec-file (active/inactive).\n");
  fprintf(stderr, "\t-configure \n");
  fprintf(stderr, "\t\tConfigure subroutine records for\n");
  fprintf(stderr, "\t\tdegaussing. Do not initiate.\n");
  fprintf(stderr, "\t-stop \n");
  fprintf(stderr, "\t\tHalt degaussing of devices in spec-file.\n");
}

void sleep_us(unsigned int nusecs) {
  struct timeval tval;

  tval.tv_sec = nusecs / 1000000;
  tval.tv_usec = nusecs % 1000000;
  select(0, NULL, NULL, NULL, &tval);
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
