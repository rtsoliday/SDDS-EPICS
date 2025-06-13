/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: setcorrmode.c
 * purpose: to improve the speed of set corrector mode, 
 * 2019.6   H. Shang
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include "SDDSsrcorr.h"
#include <time.h>
#if !defined(vxWorks)
#  include <sys/timeb.h>
#endif
/*#include <chandata.h>*/
#include <signal.h>
#ifdef _WIN32
#  include <windows.h>
#  include <process.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif
#include <alarm.h>
#if defined(vxWorks)
#  include <taskLib.h>
#  include <taskVarLib.h>
#  include <taskHookLib.h>
#  include <sysLib.h>
#endif

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#if defined(DBAccess)
#  include "dbDefs.h"
#  include "dbAccess.h"
#  include "errlog.h"
#  include "link.h"
#else
#  include <cadef.h>
#endif
#include <epicsVersion.h>

#define CLO_PENDIOTIME 0
#define CLO_PLANE 1
#define CLO_DRY_RUN 2
#define CLO_VERBOSE 3
#define CLO_SETPOINT_FILE 4
#define CLO_OFFSET_FILE 5
#define CLO_GAIN_FILE 6
#define CLO_BPM_TYPE 7
#define COMMANDLINE_OPTIONS 8
char *commandline_option[COMMANDLINE_OPTIONS] = {"pendIOtime", "plane", "dryRun", "verbose", "setpointFile",
                                                 "offsetFile", "gainFile", "bpmType"};

static char *USAGE1 = "setsrcorrectormode <inputFile> \n\
    [-pendIOtime=<seconds>] -plane=<h|v> \n\
    [-dryRun] [-verbose]  \n\
    [-setpointFile=<bpm setpoint ref file>] [-offsetFile=<bpm offset reference file>] [-gainFile=<bpm gain reference file>] \n\
    [-bpmType=<DP|plain>] \n\n\
<inputFile>        bpm waveform (vector) configuration files including bpm adjust vector and gain vectors. \n\
pendIOTime         optional, wait time for channel access \n\
verbose            print out the message \n\
dryRun             do not change pvs.\n\
plane              required, h or v plane or both must provided.\n\
setpointFile       bpm setpoint reference file.\n\
offsetFile         bpm offset reference file.\n\
gainFile           bpm gain File.\n\
bpmType            default is DP; if bpmType is DP, bpm adjust and gain waveform will be initialized. others, only scalar pvs will be initialized \n\
This program set restore bpm setpoint, offset and gain, and initialize corrector vector, bpm adjust vector and gain vector for DP.\n\
Program by H. Shang.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct
{
  double *waveformValue, *setpoint, *offset;
  char *waveformPV;
  int32_t waveformRows;
  chid vectorID, *scalarID, *setpointID, *offsetID;
  short *isValid, vectorType;
} BPM_VECTOR;
/*vectorType=0, adjust vector; =1 gain vector */

void read_bpm_vectors(char *filename, char *plane, BPM_VECTOR **bpmVector, long *bpmVectors, char *setpointFile, char *offsetFile, char *gainFile, double pendIOTime);
void readPVvaluesFromFile(char *filename, char ***pvName, char ***valueString, int32_t *pvs);
void freeBPMVectorMemory(BPM_VECTOR *bpmVector, long bpmVectors);
void init_bpm_vector(BPM_VECTOR *bpmVector, long bpmVectors, char *bpmType);

int main(int argc, char **argv) {
  SCANNED_ARG *scArg;
  long i_arg, verbose = 0, dryRun = 0, i, j;
  double pendIOTime = 30, startTime, tmpTime, tmpTime1;
  char *plane = NULL, *bpmType = NULL;
  char *setpointFile, *offsetFile, *gainFile, *inputFile;
  BPM_VECTOR *bpmVector = NULL;
  long bpmVectors = 0;

  setpointFile = offsetFile = gainFile = inputFile = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scArg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE1);
    exit(1);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scArg[i_arg].arg_type == OPTION) {
      delete_chars(scArg[i_arg].list[0], "_");
      switch (match_string(scArg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (scArg[i_arg].n_items != 2 || sscanf(scArg[i_arg].list[1], "%lf", &pendIOTime) != 1 ||
            pendIOTime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_DRY_RUN:
        dryRun = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_PLANE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -plane option!");
        plane = scArg[i_arg].list[1];
        if (strcmp(plane, "h") != 0 && strcmp(plane, "v") != 0) {
          fprintf(stderr, "invalid plane value %s provided, has to be h or v!\n", plane);
          exit(1);
        }
        break;
      case CLO_SETPOINT_FILE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -setpointFile option!");
        SDDS_CopyString(&setpointFile, scArg[i_arg].list[1]);
        break;
      case CLO_OFFSET_FILE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -offsetFile option!");
        SDDS_CopyString(&offsetFile, scArg[i_arg].list[1]);
        break;
      case CLO_GAIN_FILE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -gainFile option!");
        SDDS_CopyString(&gainFile, scArg[i_arg].list[1]);
        break;
      case CLO_BPM_TYPE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -bpmType option!");
        bpmType = scArg[i_arg].list[1];
        if (strcmp(bpmType, "DP") != 0 && strcmp(bpmType, "plain") != 0)
          SDDS_Bomb("Invalid bpmType provided, has to be DP or plain");
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", scArg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!inputFile)
        inputFile = scArg[i_arg].list[0];
      else {
        fprintf(stderr, "unrecognized option %s provided\n", scArg[i_arg].list[0]);
        exit(1);
      }
    }
  }

  startTime = getTimeInSecs();

  ca_task_initialize();
  if (!plane)
    SDDS_Bomb("plane is required");
  if (!bpmType)
    SDDS_CopyString(&bpmType, "DP");
  if (!inputFile)
    SDDS_Bomb("input waveform file is required");
  if (!offsetFile)
    SDDS_CopyString(&offsetFile, "/home/helios/oagData/SCR/snapshots/SR/SR-BPMOffsetReference.gz");
  if (!setpointFile)
    SDDS_CopyString(&setpointFile, "/home/helios/oagData/SCR/snapshots/SR/SR-UserBeamPreferred.gz");
  if (!gainFile)
    SDDS_CopyString(&gainFile, "/home/helios/oagData/SCR/snapshots/SBPMs/SBPMs-Preferred.gz");

  read_bpm_vectors(inputFile, plane, &bpmVector, &bpmVectors, setpointFile, offsetFile, gainFile, pendIOTime);
  tmpTime1 = getTimeInSecs();
  if (verbose)
    fprintf(stdout, "reading bpms spent %f seconds\n", tmpTime1 - tmpTime);
  /*reading RefAO pvs for FFwaveform */
  for (i = 0; i < bpmVectors; i++) {
    if (bpmVector[i].vectorType == 2) {
      for (j = 0; j < bpmVector[i].waveformRows; j++) {
        if (ca_get(DBR_DOUBLE, bpmVector[i].scalarID[j], &bpmVector[i].waveformValue[j]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem reading bpm RefAO values\n");
          exit(1);
        }
      }
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing read for some channels\n");
    exit(1);
  }
  tmpTime = getTimeInSecs();
  if (!dryRun) {
    init_bpm_vector(bpmVector, bpmVectors, bpmType);
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing put for some channels\n");
    exit(1);
  }
  tmpTime1 = getTimeInSecs();
  if (verbose)
    fprintf(stdout, "init bpm vectors spent %f seconds\n", tmpTime1 - tmpTime);
  tmpTime = getTimeInSecs();
  if (verbose)
    fprintf(stdout, "set corr mode to vector spent %f seconds\n", tmpTime - tmpTime1);
  ca_task_exit();
  freeBPMVectorMemory(bpmVector, bpmVectors);
  free(offsetFile);
  free(setpointFile);
  free(gainFile);
  free_scanargs(&scArg, argc);
  if (verbose)
    fprintf(stdout, "at %f seconds done.\n", getTimeInSecs() - startTime);
  return 0;
}

void readPVvaluesFromFile(char *filename, char ***pvName, char ***valueString, int32_t *pvs) {
  SDDS_DATASET SDDSin;
  int32_t rows = 0;
  if (!SDDS_InitializeInput(&SDDSin, filename))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_ReadPage(&SDDSin) <= 0) {
    fprintf(stderr, "Error reading file %s\n", filename);
    exit(1);
  }
  if (!(rows = SDDS_RowCount(&SDDSin))) {
    fprintf(stderr, "file %s is empty\n", filename);
    exit(1);
  }
  *pvs = rows;
  *pvName = *valueString = NULL;
  if (!(*pvName = (char **)SDDS_GetColumn(&SDDSin, "ControlName")) ||
      !(*valueString = (char **)SDDS_GetColumn(&SDDSin, "ValueString")) ||
      !SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void read_bpm_vectors(char *filename, char *plane, BPM_VECTOR **bpmVector, long *bpmVectors, char *setpointFile, char *offsetFile, char *gainFile, double pendIOTime) {
  SDDS_DATASET SDDSvector;
  int32_t vectorRows, setpointRows, offsetRows, gainRows;
  char **deviceName, **setpointPVName, **offsetPVName, **gainPVName, **setpointValue, **offsetValue, **gainValue;
  char pvName[256], coord[256];
  long i, matchj, vectors = 0;
  BPM_VECTOR *vector = NULL;

  deviceName = setpointPVName = offsetPVName = gainPVName = setpointValue = offsetValue = gainValue = NULL;
  vectorRows = setpointRows = offsetRows = gainRows = 0;

  readPVvaluesFromFile(setpointFile, &setpointPVName, &setpointValue, &setpointRows);
  readPVvaluesFromFile(offsetFile, &offsetPVName, &offsetValue, &offsetRows);
  readPVvaluesFromFile(gainFile, &gainPVName, &gainValue, &gainRows);
  if (strcmp(plane, "h") == 0)
    sprintf(coord, "x");
  else
    sprintf(coord, "y");

  if (!SDDS_InitializeInput(&SDDSvector, filename))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  vectors = 0;
  vector = malloc(sizeof(*vector) * 10);
  while (SDDS_ReadPage(&SDDSvector) > 0) {
    if ((vectorRows = SDDS_RowCount(&SDDSvector)) <= 0)
      continue;
    if (vectors > 9) {
      vector = SDDS_Realloc(vector, sizeof(vector) * (vectors + 1));
    }
    vector[vectors].waveformPV = NULL;
    vector[vectors].waveformValue = vector[vectors].setpoint = vector[vectors].offset = NULL;
    vector[vectors].scalarID = vector[vectors].setpointID = vector[vectors].offsetID = NULL;
    vector[vectors].isValid = NULL;
    vector[vectors].vectorType = -1;
    vector[vectors].waveformRows = vectorRows;
    if (!(SDDS_GetParameter(&SDDSvector, "WaveformPV", &vector[vectors].waveformPV)) ||
        !(deviceName = (char **)SDDS_GetColumn(&SDDSvector, "DeviceName")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    vector[vectors].waveformValue = malloc(sizeof(double) * vectorRows);
    vector[vectors].isValid = malloc(sizeof(short) * vectorRows);
    if (ca_search(vector[vectors].waveformPV, &vector[vectors].vectorID) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", vector[vectors].waveformPV);
      ca_task_exit();
      exit(1);
    }
    if (wild_match(vector[vectors].waveformPV, "*Adjust*")) {
      vector[vectors].vectorType = 0;
      vector[vectors].setpoint = malloc(sizeof(double) * vectorRows);
      vector[vectors].offset = malloc(sizeof(double) * vectorRows);
      vector[vectors].setpointID = malloc(sizeof(chid) * vectorRows);
      vector[vectors].offsetID = malloc(sizeof(chid) * vectorRows);
    } else if (wild_match(vector[vectors].waveformPV, "*Gain*")) {
      vector[vectors].vectorType = 1;
      vector[vectors].scalarID = malloc(sizeof(chid) * vectorRows);
    } else if (wild_match(vector[vectors].waveformPV, "*SetPt:WF")) {
      /*feedforward pv, need find out what the pv names are*/
      vector[vectors].vectorType = 2;
      vector[vectors].scalarID = malloc(sizeof(chid) * vectorRows);
    }
    if (vector[vectors].vectorType == 0) {
      /*adjust vector */
      for (i = 0; i < vectorRows; i++) {
        sprintf(pvName, "%s:ms:%s:SetpointAO", deviceName[i], coord);
        vector[vectors].isValid[i] = 1;
        if ((matchj = match_string(pvName, setpointPVName, setpointRows, EXACT_MATCH)) < 0) {
          vector[vectors].isValid[i] = 0;
          vector[vectors].waveformValue[i] = 0;
          continue;
        }
        if (ca_search(pvName, &vector[vectors].setpointID[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
        vector[vectors].setpoint[i] = atof(setpointValue[matchj]);
        sprintf(pvName, "%s:ms:%s:OffsetAO", deviceName[i], coord);
        vector[vectors].isValid[i] = 1;
        if ((matchj = match_string(pvName, offsetPVName, offsetRows, EXACT_MATCH)) < 0) {
          vector[vectors].isValid[i] = 0;
          vector[vectors].waveformValue[i] = 0;
          continue;
        }
        if (ca_search(pvName, &vector[vectors].offsetID[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
        vector[vectors].offset[i] = atof(offsetValue[matchj]);
        vector[vectors].waveformValue[i] = vector[vectors].setpoint[i] + vector[vectors].offset[i];
      }
    } else if (vector[vectors].vectorType == 1) {
      /*gain vector*/
      for (i = 0; i < vectorRows; i++) {
        sprintf(pvName, "%s:ms:%s:GainAO", deviceName[i], coord);
        vector[vectors].isValid[i] = 1;
        if ((matchj = match_string(pvName, gainPVName, gainRows, EXACT_MATCH)) < 0) {
          vector[vectors].isValid[i] = 0;
          vector[vectors].waveformValue[i] = 0;
          continue;
        }
        if (ca_search(pvName, &vector[vectors].scalarID[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
        vector[vectors].waveformValue[i] = atof(gainValue[matchj]);
      }
    } else if (vector[vectors].vectorType == 2) {
      /*FFwaveform */
      for (i = 0; i < vectorRows; i++) {
        sprintf(pvName, "%s:%s:RefAO", deviceName[i], coord);
        if (ca_search(pvName, &vector[vectors].scalarID[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
      }
    }
    SDDS_FreeStringArray(deviceName, vectorRows);
    free(deviceName);
    deviceName = NULL;
    vectors++;
  }
  if (!SDDS_Terminate(&SDDSvector))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    exit(1);
  }

  *bpmVector = vector;
  *bpmVectors = vectors;
  SDDS_FreeStringArray(setpointPVName, setpointRows);
  free(setpointPVName);
  SDDS_FreeStringArray(setpointValue, setpointRows);
  free(setpointValue);
  SDDS_FreeStringArray(offsetPVName, offsetRows);
  free(offsetPVName);
  SDDS_FreeStringArray(offsetValue, offsetRows);
  free(offsetValue);
  SDDS_FreeStringArray(gainPVName, gainRows);
  free(gainPVName);
  SDDS_FreeStringArray(gainValue, gainRows);
  free(gainValue);
}

void freeBPMVectorMemory(BPM_VECTOR *bpmVector, long bpmVectors) {
  long i;
  for (i = 0; i < bpmVectors; i++) {
    free(bpmVector[i].waveformValue);
    if (bpmVector[i].setpoint)
      free(bpmVector[i].setpoint);
    if (bpmVector[i].offset)
      free(bpmVector[i].offset);
    if (bpmVector[i].scalarID)
      free(bpmVector[i].scalarID);
    if (bpmVector[i].setpointID)
      free(bpmVector[i].setpointID);
    if (bpmVector[i].offsetID)
      free(bpmVector[i].offsetID);
    free(bpmVector[i].isValid);
    free(bpmVector[i].waveformPV);
  }
  free(bpmVector);
}

void init_bpm_vector(BPM_VECTOR *bpmVector, long bpmVectors, char *bpmType) {
  long i, j;
  /*note that vectorType=2 FFwaveform, use the setpoint value, the scalar setpoint has been taken care of by the adjust waveform */
  for (i = 0; i < bpmVectors; i++) {
    if (strcmp(bpmType, "DP") == 0) {
      if (ca_array_put(DBR_DOUBLE, bpmVector[i].waveformRows, bpmVector[i].vectorID, bpmVector[i].waveformValue) != ECA_NORMAL) {
        fprintf(stderr, "Error setting values for %s waveform pv\n", bpmVector[i].waveformPV);
        exit(1);
      }
    }
    if (bpmVector[i].vectorType == 0) {
      /*adjust waveform */
      for (j = 0; j < bpmVector[i].waveformRows; j++) {
        if (bpmVector[i].isValid[j]) {
          if (ca_put(DBR_DOUBLE, bpmVector[i].setpointID[j], &(bpmVector[i].setpoint[j])) != ECA_NORMAL ||
              ca_put(DBR_DOUBLE, bpmVector[i].offsetID[j], &(bpmVector[i].offset[j])) != ECA_NORMAL) {
            fprintf(stderr, "Error setting values for bpm offset and setpoint\n");
            exit(1);
          }
        }
      }
    } else if (bpmVector[i].vectorType == 1) {
      /*gain waveform */
      for (j = 0; j < bpmVector[i].waveformRows; j++) {
        if (bpmVector[i].isValid[j]) {
          if (ca_put(DBR_DOUBLE, bpmVector[i].scalarID[j], &(bpmVector[i].waveformValue[j])) != ECA_NORMAL) {
            fprintf(stderr, "Error setting values for bpm gain\n");
            exit(1);
          }
        }
      }
    }
  }
}
