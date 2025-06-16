/**
 * @file standardize.c
 * @brief Standardize magnets using an SDDS specification file.
 *
 * Reads an SDDS file describing devices and standardization parameters,
 * configures the appropriate subroutine records, and optionally starts,
 * stops, or queries the standardization process.
 *
 * @section Usage
 * ```
 * standardize [-query|-configure|-stop]
 *             [-pendIoTime <seconds>] [--] <spec-file>
 * ```
 *
 * @section Options
 * | Option        | Description |
 * |---------------|-------------|
 * | `-query`      | Print the standardization state of devices in the specification file. |
 * | `-configure`  | Configure records for standardization but do not initiate. |
 * | `-stop`       | Halt standardization of devices in the specification file. |
 * | `-pendIoTime` | Maximum time to wait for Channel Access I/O. |
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
 * C. Saunders, J. Anderson, R. Soliday, H. Shang
 */
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

struct sub_rec_pvs {
  char sub_proc[255]; /* for initiating standardization */
  chid sub_proc_chid;
  char sub_snam[255]; /* is proper subroutine attached ? */
  chid sub_snam_chid;
  char sub_b[255]; /* arguments to standardization in fields b-h */
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
  char **name;          /* device name or process variable */
  char **suffix = NULL; /* suffix name, not all standardize supplies ended with StandardizeSUB */
  char **type;          /* dev or pv */
  double *from;         /* start point of conditioning cycle */
  double *to;           /* end point of conditioning cycle */
  double *rampRate, *rampTransitionPercentage, *dwellTime;
  int32_t *min, *sec; /* time for transition from..to   */
  int32_t *ncycles;   /* # of cycles in standardization */
  double *finish;     /* final setpoint to stop at */
  char **approach;    /* approach finish from "above" or "below" */
  double *dblapp;     /* same as approach but 0.0 for below 1.0 for above */
  char **pvname;      /* name of subroutine record process variable */
  char **outb = NULL, **outc = NULL, **outd = NULL;

  struct sub_rec_pvs *sub;
  int query, configure, stop;
  double pendIoTime;
  char specfilename[255]; /* specification file name */
  // char *contents;      /* text and contents fields of SDDS descrip. */
  long nrows;
  int status;
  long i, k, l, nsupplies;
  double *state; /* gets proc field value from subroutine record */
  int *ndevs;    /* number of atomic devices in device i */
  char **subname;
  char **dname; /* names of individual atomic devices */
  short start = 1;
  short halt = 0, dwellMode = 1;
  char buffer[255];

  fprintf(stdout, "Magnet Standardization Utility\n");
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

  /*
    contents = NULL;
    if (!SDDS_GetDescription(&table, NULL, &contents)) {
    fprintf(stderr,"%s: SDDS error: unable to get description from file %s\n",
    argv[0], specfilename);
    exit(1);
    }
    if (contents==NULL || strcmp(contents,"standardization specification")) {
    fprintf(stderr,"%s: SDDS error: file %s is not a standardization specification\n",argv[0],specfilename);
    exit(1);
    }
  */

  if ((nrows = SDDS_CountRowsOfInterest(&table)) == -1) {
    fprintf(stderr, "%s: SDDS error: unable to get row count\n", argv[0]);
    exit(1);
  }

  if ((name = SDDS_GetColumn(&table, "ControlName")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read ControlName column\n", argv[0]);
    exit(1);
  }
  if ((type = SDDS_GetColumn(&table, "ControlType")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read ControlType column\n", argv[0]);
    exit(1);
  }
  if ((from = SDDS_GetColumn(&table, "From")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read From column\n", argv[0]);
    exit(1);
  }
  if ((to = SDDS_GetColumn(&table, "To")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read To column\n", argv[0]);
    exit(1);
  }
  min = SDDS_GetColumn(&table, "Minutes");
  sec = SDDS_GetColumn(&table, "Seconds");
  rampRate = SDDS_GetColumn(&table, "RampRate");
  rampTransitionPercentage = SDDS_GetColumn(&table, "RampTransitionPercentage");
  dwellTime = SDDS_GetColumn(&table, "DwellTime");
  outb = SDDS_GetColumn(&table, "OUTB");
  outc = SDDS_GetColumn(&table, "OUTC");
  outd = SDDS_GetColumn(&table, "OUTD");
  if ((rampRate == NULL) || (rampTransitionPercentage == NULL) || (dwellTime == NULL)) {
    dwellMode = 0;
    if ((min == NULL) || (sec == NULL)) {
      fprintf(stderr, "%s: SDDS error: missing either RampRate, RampTransitionPercentage or DwellTime columns or Minutes and Seconds columns\n", argv[0]);
      exit(1);
    }
  }
  // fprintf(stderr, "DEBUG1 %lf %lf\n", dwellTime[0], to[0]);

  if ((ncycles = SDDS_GetColumn(&table, "NumCycles")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read ncycles column\n", argv[0]);
    exit(1);
  }
  if ((finish = SDDS_GetColumn(&table, "Finish")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read finish column\n", argv[0]);
    exit(1);
  }
  if ((approach = SDDS_GetColumn(&table, "Approach")) == NULL) {
    fprintf(stderr, "%s: SDDS error: unable to read approach column\n", argv[0]);
    exit(1);
  }
  if (SDDS_CheckColumn(&table, "SuffixName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    if ((suffix = (char **)SDDS_GetColumn(&table, "SuffixName")) == NULL) {
      fprintf(stderr, "%s: SDDS error: unable to read SuffixName column\n", argv[0]);
      exit(1);
    }
  }

  /*
    if (outb) {
    for (i=0 ; i < nrows ; i++) {
    fprintf(stderr, "%ld (%s)\n", i, outb[i]);
    }
    }
  */

  if (!dwellMode) {
    for (i = 0; i < nrows; i++)
      if (validate_time(min[i], sec[i])) {
        fprintf(stderr, "%s: %s has invalid time specified\n", argv[0], name[i]);
        exit(1);
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
      fprintf(stderr, "%s: exiting, no standardization initiated\n", argv[0]);
      exit(1);
    }
  }

  sub = (struct sub_rec_pvs *)malloc(sizeof(struct sub_rec_pvs) * nsupplies);
  dblapp = (double *)malloc(sizeof(double) * nsupplies);
  state = (double *)malloc(sizeof(double) * nsupplies);
  pvname = (char **)malloc(sizeof(char *) * nsupplies);
  subname = (char **)malloc(sizeof(char *) * nsupplies);
  dname = (char **)malloc(sizeof(char *) * nsupplies);
  for (i = 0; i < nsupplies; i++) {
    pvname[i] = (char *)malloc(sizeof(char) * 255);
    subname[i] = (char *)malloc(sizeof(char) * 255);
    dname[i] = (char *)malloc(sizeof(char) * 255);
  }

  for (i = 0; i < nrows; i++)
    dblapp[i] = (strcmp(approach[i], "above")) ? 0.0 : 1.0;

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
      strcpy(sub[k].sub_g, pvname[k]);
      strcat(sub[k].sub_g, ".G");
      strcpy(sub[k].sub_h, pvname[k]);
      strcat(sub[k].sub_h, ".H");
      if (dwellMode) {
        strcpy(sub[k].sub_i, pvname[k]);
        strcat(sub[k].sub_i, ".I");
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
      status = ca_search(sub[k].sub_b, &(sub[k].sub_b_chid));
      status |= ca_search(sub[k].sub_c, &(sub[k].sub_c_chid));
      status |= ca_search(sub[k].sub_d, &(sub[k].sub_d_chid));
      status |= ca_search(sub[k].sub_e, &(sub[k].sub_e_chid));
      status |= ca_search(sub[k].sub_f, &(sub[k].sub_f_chid));
      status |= ca_search(sub[k].sub_g, &(sub[k].sub_g_chid));
      status |= ca_search(sub[k].sub_h, &(sub[k].sub_h_chid));
      if (dwellMode) {
        status |= ca_search(sub[k].sub_i, &(sub[k].sub_i_chid));
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

  /* Check to make sure subroutine is standardize, not degauss */
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
      if (dwellMode) {
        if (strcmp(subname[k], "standardizeDwellProc")) {
          fprintf(stderr, "%25s : device has wrong subroutine specified for standardization: %s\n", dname[k], subname[k]);
          fprintf(stderr, "%s: exiting, no standardization performed\n", argv[0]);
          exit(1);
        }
      } else {
        if (strcmp(subname[k], "standardizeProc")) {
          fprintf(stderr, "%25s : device has wrong subroutine specified for standardization: %s\n", dname[k], subname[k]);
          fprintf(stderr, "%s: exiting, no standardization performed\n", argv[0]);
          exit(1);
        }
      }
    }
  }

  if (stop) {
    fprintf(stdout, "Halting standardization...");
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
          fprintf(stdout, "%25s : standardizing active\n", dname[k]);
        } else {
          fprintf(stdout, "%25s : standardizing inactive\n", dname[k]);
        }
      }
    }
  }

  if (!query && !stop) {
    fprintf(stdout, "Configure records for standardization...");
    fflush(stdout);
    for (i = 0, k = 0; i < nrows; i++) {
      for (l = 0; l < ndevs[i]; l++, k++) {
        if (state[k] == 0.0) {
          status = ca_put(DBR_DOUBLE, sub[k].sub_b_chid, &(from[i]));
          status |= ca_put(DBR_DOUBLE, sub[k].sub_c_chid, &(to[i]));
          if (dwellMode) {
            status |= ca_put(DBR_DOUBLE, sub[k].sub_d_chid, &(rampRate[i]));
            status |= ca_put(DBR_DOUBLE, sub[k].sub_e_chid, &(rampTransitionPercentage[i]));
            status |= ca_put(DBR_DOUBLE, sub[k].sub_i_chid, &(dwellTime[i]));
            if (outb)
              status |= ca_put(DBR_STRING, sub[k].sub_outb_chid, outb[i]);
            if (outc)
              status |= ca_put(DBR_STRING, sub[k].sub_outc_chid, outc[i]);
            if (outd)
              status |= ca_put(DBR_STRING, sub[k].sub_outd_chid, outd[i]);
          } else {
            status |= ca_put(DBR_LONG, sub[k].sub_d_chid, &(min[i]));
            status |= ca_put(DBR_LONG, sub[k].sub_e_chid, &(sec[i]));
          }
          status |= ca_put(DBR_LONG, sub[k].sub_f_chid, &(ncycles[i]));
          status |= ca_put(DBR_DOUBLE, sub[k].sub_g_chid, &(finish[i]));
          status |= ca_put(DBR_DOUBLE, sub[k].sub_h_chid, &(dblapp[i]));
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
    fprintf(stdout, "Initiating standardization...");
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
  /*  exit(0);*/
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
  /*  void usage();*/

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
          fprintf(stderr, "invalid -pendIoTime value (standardize)\n");
          exit(1);
        }
        if ((sscanf(argv[i], "%lf", pendIoTime) != 1) ||
            (*pendIoTime <= 0.0)) {
          fprintf(stderr, "invalid -pendIoTime value (standardize)\n");
          exit(1);
        }
      } else {
        if ((!dd) && (!strncmp("-", argv[i], 1))) {
          fprintf(stderr, "invalid option %s (standardize)\n", argv[i]);
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
  fprintf(stderr, "purpose:\tStandardizes the set of devices specified in\n");
  fprintf(stderr, "        \tspec-file. The spec-file must be in SDDS format.\n");
  fprintf(stderr, "        \tEach row in spec-file contains the device name,\n");
  fprintf(stderr, "        \trange over which to standardize, time over which\n");
  fprintf(stderr, "        \tto standardize, number of cycles, finishing setpoint,\n");
  fprintf(stderr, "        \tand whether to approach finish from above or below.\n");
  fprintf(stderr, "        \tEntering \"standardize myfile\" begins standardization.\n");
  fprintf(stderr, "options:\n");
  fprintf(stderr, "\t-query \n");
  fprintf(stderr, "\t\tPrint standardization state of devices in\n");
  fprintf(stderr, "\t\tspec-file (active/inactive).\n");
  fprintf(stderr, "\t-configure \n");
  fprintf(stderr, "\t\tConfigure subroutine records for\n");
  fprintf(stderr, "\t\tstandardization. Do not initiate.\n");
  fprintf(stderr, "\t-stop \n");
  fprintf(stderr, "\t\tHalt standardization of devices in spec-file.\n");
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
