/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  $Log: not supported by cvs2svn $
 *  Revision 1.1  2006/10/30 22:11:04  shang
 *  first version, for generating booster ramp table with bumping correctors.
 *
 *  for generating booster ramp table per provided parameters, H. Shang, Oct. 2006
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include "match_string.h"
#include <stdlib.h>
#include <time.h>
#include <cadef.h>
#include <epicsVersion.h>

#define SET_PIPE 0
#define SET_CORRECTOR 1
#define SET_BUMP_TIME 2
#define SET_MAX_CURRENT 3
#define SET_ORBIT_CHANGE 4
#define N_OPTIONS 5

char *option[N_OPTIONS] = {
  "pipe",
  "corrector",
  "bumpTime",
  "maximumCurrent",
  "orbitChange",
};

typedef struct
{
  char *correctorName; /*in format of B1C1H, B1C1V, etc. */
  char rampPV[256];
  chid nordID, biasID, timeID, valID;
  double bumpValue, bias; /*bumpValue in units of Amp at extraction point */
  long rampPoints, elements, nord;
  double *rampTime, *rampSetpoint, *time, *ramp;
} CORRECTOR_RAMPTABLE;

static char *USAGE = "sddsboosterbump <outputFile>|-pipe=out \n\
      -corrector=name=<string>,bumpValue=<value> [-bumpTime=<time is ms>] \n\
      [-maximumCurrent=<value in amp>] [-orbitChange=<value>] \n\
outputFile               file for writing the booster ramptable.\n\
                         either output file or -pipe=out has to be provided.\n\
corrector                specifies the corrector inside the bump, and the bump value in amps.\n\
                         multiple correctors can be provided, depends the number of correctors \n\
                         for generating the bump (usually 3).\n\
bumpTime                 specifies the time (during the ramp) of bump starting. \n\
                         The final ramp table is: at and after this bump time, all ramp setpoint\n\
                         is changed to give the same orbit bump. Before this bump point, the table\n\
                         is combination of original table and new table with maximum allowable slope\n\
                         to reach this bump current.\n\
maximumCurrent           specifies the maximum current of booster correctors, default is 19.0 amps.\n\
orbitChange              provides the change of orbit in mm starting at the bump point.\n\
created a new ramptable for given correctors based the bump value (deltaI) and bump time.\n\
Link date: "__DATE__
                     " "__TIME__
                     ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

void SetupOutput(char *outputFile, SDDS_DATASET *SDDSout);

int main(int argc, char **argv) {
  char *outputFile;
  SDDS_DATASET SDDSout;
  int i, j, i_arg;
  unsigned long dummyFlags, pipeFlags;
  SCANNED_ARG *s_arg;
  CORRECTOR_RAMPTABLE *corrector_ramp;
  long correctors, caErrors = 0, code, count, insert_bump, insert_cross;
  double bumptime, on_time, ext_time, maxcurrent, orbit_change = 0.0;
  char buff[256], desc[2046];
  double pendIOtime = 30.0, I_b0, I_b, delta, I0, IE, t_cross, delta_Ib, t0;

  ca_task_initialize();

  bumptime = on_time = 0;
  ext_time = 236.439589;
  maxcurrent = 19.0;

  outputFile = NULL;
  corrector_ramp = NULL;
  correctors = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s\n", USAGE);
    exit(1);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        if (!(pipeFlags & USE_STDOUT))
          SDDS_Bomb("Invalid pipe flags given, has to be -pipe=out or -pipe.");
        break;
      case SET_ORBIT_CHANGE:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &orbit_change) != 1)
          SDDS_Bomb("invalid -orbitChange syntax");
        break;
      case SET_BUMP_TIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &bumptime) != 1 ||
            bumptime < 0)
          SDDS_Bomb("invalid -bumpTime syntax");
        break;
      case SET_MAX_CURRENT:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &maxcurrent) != 1 ||
            maxcurrent < 0)
          SDDS_Bomb("invalid -maxcurrent syntax");
        break;
      case SET_CORRECTOR:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -poisson syntax");
        s_arg[i_arg].n_items -= 1;
        if (!(corrector_ramp = SDDS_Realloc(corrector_ramp, sizeof(*corrector_ramp) * (correctors + 1))))
          SDDS_Bomb("memory allocation failure.");
        memset(corrector_ramp + correctors, 0, sizeof(*corrector_ramp));
        corrector_ramp[correctors].bumpValue = 0;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &corrector_ramp[correctors].correctorName, 1, 0,
                          "bumpValue", SDDS_DOUBLE, &corrector_ramp[correctors].bumpValue, 1, 0,
                          NULL) ||
            !(corrector_ramp[correctors].correctorName))
          SDDS_Bomb("invalid -corrector syntax.");
        s_arg[i_arg].n_items += 1;
        correctors++;
        break;
      default:
        fprintf(stderr, "unknown option - %s given.\n", s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      if (!outputFile)
        outputFile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many files given.");
    }
  }
  if (!outputFile && !(pipeFlags & USE_STDOUT))
    SDDS_Bomb("Output file is not provided.");
  if (!correctors)
    SDDS_Bomb("No correctors provided.");
  buff[0] = 0;
  for (i = 0; i < correctors; i++) {
    corrector_ramp[i].rampPV[0] = 0;
    sprintf(corrector_ramp[i].rampPV, "%s:rampRAMPTABLE", corrector_ramp[i].correctorName);
    sprintf(buff, "%s.NORD", corrector_ramp[i].rampPV);
    if (ca_search(buff, &corrector_ramp[i].nordID) != ECA_NORMAL) {
      fprintf(stderr, "Error: problem doing search for %s\n", buff);
      exit(1);
    }
    sprintf(buff, "%s.BIAS", corrector_ramp[i].rampPV);
    if (ca_search(buff, &corrector_ramp[i].biasID) != ECA_NORMAL) {
      fprintf(stderr, "Error: problem doing search for %s\n", buff);
      exit(1);
    }
    sprintf(buff, "%s.TIMA", corrector_ramp[i].rampPV);
    if (ca_search(buff, &corrector_ramp[i].timeID) != ECA_NORMAL) {
      fprintf(stderr, "Error: problem doing search for %s\n", buff);
      exit(1);
    }
    sprintf(buff, "%s.VAL", corrector_ramp[i].rampPV);
    if (ca_search(buff, &corrector_ramp[i].valID) != ECA_NORMAL) {
      fprintf(stderr, "Error: problem doing search for %s\n", buff);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < correctors; i++) {
      if (ca_state(corrector_ramp[i].nordID) != cs_conn ||
          ca_state(corrector_ramp[i].biasID) != cs_conn ||
          ca_state(corrector_ramp[i].timeID) != cs_conn ||
          ca_state(corrector_ramp[i].valID) != cs_conn)
        fprintf(stderr, "%s not connected\n", corrector_ramp[i].rampPV);
    }
    exit(1);
  }
  for (i = 0; i < correctors; i++) {
    if (ca_get(DBR_LONG, corrector_ramp[i].nordID, &corrector_ramp[i].nord) != ECA_NORMAL) {
      fprintf(stderr, "Error in reading value of %s.NORD\n", corrector_ramp[i].rampPV);
      caErrors++;
    }
    if (ca_get(DBR_DOUBLE, corrector_ramp[i].biasID, &corrector_ramp[i].bias) != ECA_NORMAL) {
      fprintf(stderr, "Error in reading value of %s.BIAS\n", corrector_ramp[i].rampPV);
      caErrors++;
    }
    corrector_ramp[i].elements = ca_element_count(corrector_ramp[i].timeID);
    if (!(corrector_ramp[i].time = malloc(sizeof(*(corrector_ramp[i].time)) * corrector_ramp[i].elements)) ||
        !(corrector_ramp[i].ramp = malloc(sizeof(*(corrector_ramp[i].ramp)) * corrector_ramp[i].elements)))
      SDDS_Bomb("memory allocation failure.");
    if (ca_array_get(DBR_DOUBLE, corrector_ramp[i].elements, corrector_ramp[i].timeID, corrector_ramp[i].time) != ECA_NORMAL) {
      fprintf(stderr, "Error in reading value of %s.TIMA\n", corrector_ramp[i].rampPV);
      caErrors++;
    }
    if (ca_array_get(DBR_DOUBLE, corrector_ramp[i].elements, corrector_ramp[i].valID, corrector_ramp[i].ramp) != ECA_NORMAL) {
      fprintf(stderr, "Error in reading value of %s.VAL\n", corrector_ramp[i].rampPV);
      caErrors++;
    }
  }
  ca_pend_io(pendIOtime);
  ca_task_exit();
  if (caErrors)
    exit(1);
  SetupOutput(outputFile, &SDDSout);
  sprintf(desc, "bump start at %.3f ms", bumptime);
  for (i = 0; i < correctors; i++) {
    if (corrector_ramp[i].nord < 3) {
      fprintf(stderr, "%s ramptable can not be modified.\n", corrector_ramp[i].correctorName);
      exit(1);
    }
    on_time = corrector_ramp[i].time[0];
    ext_time = corrector_ramp[i].time[corrector_ramp[i].nord - 2];
    if (bumptime < on_time || bumptime > ext_time) {
      fprintf(stderr, "bumptime is outside the start and extraction time range for corrector %s.\n",
              corrector_ramp[i].correctorName);
      exit(1);
    }
    I0 = corrector_ramp[i].ramp[0];                          /*start current (normalized) */
    IE = corrector_ramp[i].ramp[corrector_ramp[i].nord - 2]; /* extraction current, normalized.*/

    /* I_bump is normalized to maxcurrent, while bumpValue is not */
    I_b0 = interp(corrector_ramp[i].ramp, corrector_ramp[i].time, corrector_ramp[i].nord, bumptime,
                  0, 1, &code);
    delta_Ib = corrector_ramp[i].bumpValue / maxcurrent * bumptime / ext_time;
    I_b = I_b0 + delta_Ib;
    if (I_b > 1.0) {
      fprintf(stderr, "The normalized current for %s at start bump time %.3f ms is greater than 1.0, it is illegal.\n",
              corrector_ramp[i].correctorName, bumptime);
      exit(1);
    }
    if (delta_Ib > 0)
      t0 = bumptime - I_b * ext_time;
    else
      t0 = bumptime + I_b * ext_time;

    /* t0 the is the time while I=0 with 1/ramp_time slope and I=I_b at bumptime */
    if (t0 < 0) {
      fprintf(stderr, "Can not make bump for %s at %.3f ms, the slope from bump point to start point exceeds the maximum allowance.\n",
              corrector_ramp[i].correctorName, bumptime);
      exit(1);
    }
    count = insert_bump = insert_cross = 0;
    if (delta_Ib > 0)
      t_cross = t0 / (1.0 - IE);
    else
      t_cross = t0 / (1.0 + IE);
    for (j = 0; j < corrector_ramp[i].nord; j++) {
      delta = 0;
      if (corrector_ramp[i].time[j] <= t_cross) {
        /*keep original point */
        delta = 0;
        /*check if need insert cross point*/
        if (corrector_ramp[i].time[j + 1] > t_cross) {
          if (t_cross - corrector_ramp[i].time[j] > 0.1 && corrector_ramp[i].time[j + 1] - t_cross > 0.1)
            /*need insert cross point now, since the cross point is between this point and next point, not at one of them*/
            insert_cross = 1;
        }
        if (abs(corrector_ramp[i].time[j] - bumptime) <= 0.1 || abs(corrector_ramp[i].time[j + 1] - bumptime) <= 0.1) {
          /*the bumptime point is included in this or next point, no need insert bumptime point */
          delta = corrector_ramp[i].bumpValue / maxcurrent * corrector_ramp[i].time[j] / ext_time;
        } else if (corrector_ramp[i].time[j + 1] > bumptime) {
          /*need insert now, since bumptime point is between this point and next point*/
          insert_bump = 1;
        }
        /* fprintf(stderr,"%s, time=%f, next_t=%f, cross=%f, insert_cross=%d, insert_bump=%d\n", corrector_ramp[i].correctorName,
                 corrector_ramp[i].time[j], corrector_ramp[i].time[j+1], t_cross, insert_cross, insert_bump); */
      } else if (corrector_ramp[i].time[j] <= bumptime) {
        /*increase current with maximum allowed slope, 
                with form of I = 1/ext*(t-t0) */
        delta = 1 / ext_time * (corrector_ramp[i].time[j] - t0) - corrector_ramp[i].ramp[j];
        /* if delta direction is different from delta_Ib, keep original value */
        if (delta_Ib * delta < 0)
          delta = 0;
        /*check if need insert bumptime point */
        if (abs(corrector_ramp[i].time[j] - bumptime) <= 0.1 || abs(corrector_ramp[i].time[j + 1] - bumptime) <= 0.1) {
          /*the bumptime point is included in this or next point, no need insert bumptime point */
          delta = corrector_ramp[i].bumpValue / maxcurrent * corrector_ramp[i].time[j] / ext_time;
        } else if (corrector_ramp[i].time[j + 1] > bumptime) {
          /*need insert now, since bumptime point is between this point and next point*/
          insert_bump = 1;
        }
      } else {
        if (corrector_ramp[i].time[j] < ext_time)
          delta = corrector_ramp[i].bumpValue / maxcurrent * corrector_ramp[i].time[j] / ext_time;
        else
          delta = corrector_ramp[i].bumpValue / maxcurrent;
      }

      corrector_ramp[i].rampTime = SDDS_Realloc(corrector_ramp[i].rampTime, sizeof(double) * (count + 1));
      corrector_ramp[i].rampSetpoint = SDDS_Realloc(corrector_ramp[i].rampSetpoint, sizeof(double) * (count + 1));
      corrector_ramp[i].rampTime[count] = corrector_ramp[i].time[j];
      corrector_ramp[i].rampSetpoint[count] = corrector_ramp[i].ramp[j] + delta;

      count++;
      if (t_cross < bumptime) {
        if (insert_cross) {
          corrector_ramp[i].rampTime = SDDS_Realloc(corrector_ramp[i].rampTime, sizeof(double) * (count + 1));
          corrector_ramp[i].rampSetpoint = SDDS_Realloc(corrector_ramp[i].rampSetpoint, sizeof(double) * (count + 1));
          corrector_ramp[i].rampTime[count] = t_cross;
          corrector_ramp[i].rampSetpoint[count] = interp(corrector_ramp[i].ramp, corrector_ramp[i].time,
                                                         corrector_ramp[i].nord, t_cross, 0, 1, &code);
          count++;
          insert_cross = 0;
        }
      }
      if (insert_bump) {
        corrector_ramp[i].rampTime = SDDS_Realloc(corrector_ramp[i].rampTime, sizeof(double) * (count + 1));
        corrector_ramp[i].rampSetpoint = SDDS_Realloc(corrector_ramp[i].rampSetpoint, sizeof(double) * (count + 1));
        corrector_ramp[i].rampTime[count] = bumptime;
        corrector_ramp[i].rampSetpoint[count] = I_b;
        count++;
        insert_bump = 0;
      }
    }
    sprintf(buff, "%s.CurentAO", corrector_ramp[i].correctorName);
    if (!SDDS_StartPage(&SDDSout, count) ||
        !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "ControlName", corrector_ramp[i].rampPV,
                            "ControlType", "pv",
                            "BumpStartTime", bumptime,
                            "RampBias", corrector_ramp[i].bias,
                            "CorrName", corrector_ramp[i].correctorName,
                            "CorrCurrentAO", buff,
                            "Description", desc,
                            "OrbitChange", orbit_change, NULL) ||
        !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                        corrector_ramp[i].rampTime, count, "RampTime") ||
        !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                        corrector_ramp[i].rampSetpoint, count, "RampSetpoint") ||
        !SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    free(corrector_ramp[i].correctorName);
    free(corrector_ramp[i].time);
    free(corrector_ramp[i].ramp);
    free(corrector_ramp[i].rampTime);
    free(corrector_ramp[i].rampSetpoint);
  }
  if (!SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(corrector_ramp);
  free_scanargs(&s_arg, argc);
  return 0;
}

void SetupOutput(char *outputFile, SDDS_DATASET *SDDSout) {
  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, "Booster Ramp Table", "booster ramp", outputFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_DefineParameter(SDDSout, "ControlName", "ControlName", NULL, NULL, NULL, SDDS_STRING, NULL) < 0 ||
      SDDS_DefineParameter(SDDSout, "ControlType", "ControlType", NULL, NULL, NULL, SDDS_STRING, NULL) < 0 ||
      SDDS_DefineParameter(SDDSout, "RampBias", "RampBias", NULL, NULL, NULL, SDDS_DOUBLE, NULL) < 0 ||
      SDDS_DefineParameter(SDDSout, "CorrName", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0 ||
      SDDS_DefineParameter(SDDSout, "CorrCurrentAO", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0 ||
      SDDS_DefineParameter(SDDSout, "BumpStartTime", NULL, "ms", NULL, NULL, SDDS_DOUBLE, NULL) < 0 ||
      SDDS_DefineParameter(SDDSout, "Description", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0 ||
      SDDS_DefineParameter(SDDSout, "OrbitChange", NULL, "mm", NULL, NULL, SDDS_DOUBLE, NULL) < 0 ||
      SDDS_DefineColumn(SDDSout, "RampTime", "RampTime", NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "RampSetpoint", "RampSetpoint", NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      !SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}
