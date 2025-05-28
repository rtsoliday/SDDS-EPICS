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
#define CLO_SETSOURCE 4
#define CLO_UNIFIED 5
#define CLO_AOAItolerance 6
#define CLO_DACAOtolerance 7
#define CLO_CORR_FILE 8
#define CLO_CORR_TYPE 9
#define COMMANDLINE_OPTIONS 10
char *commandline_option[COMMANDLINE_OPTIONS] = {"pendIOtime", "plane", "dryRun", "verbose", "setsource",
                                                 "unified", "AOAItolerance", "dacAOtolerance",
                                                 "corrFile", "corrType"};

static char *USAGE1 = "setsrcorrectormode  \n\
    [-pendIOtime=<seconds>] -plane=<h|v|both> \n\
    [-dryRun] [-verbose]  [-setsource] [-unified] [-AOAItolerance=<value>] [-dacAOtolerance=<value>] \n\
    [-corrType=<DP|dynmaic|plain|vector|scalar>] \n\n\
pendIOTime         optional, wait time for channel access \n\
verbose            print out the message \n\
dryRun             do not change pvs.\n\
plane              required, h or v plane or both must provided.\n\
setsource          if provided, the corrector source mode will be set.\n\
unified            unified corrector or not.\n\
AOAItolerance      tolerance for checking the corrector AO and AI difference (default 2), if bigger than the tolerance, nothing will be done.\n\
dacAOtolerance     tolerance for DAC and AO difference (default 0.1), if bigger than the tolerance, nothing will be done.\n\
corrFile           SCR file with scalar corretor setpoints; if provided, the corrector vector \n\
                   will be initialized through this file; if not, the corrector setpoints will be read from ioc \n\
                   The corrector reference will be transferred from this file.\n\
corrType           default is DP; if corrType is DP or vector, corrector vector will be initialized.\n\
This program initialize corr vector.\n\
Program by H. Shang.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct {
  double *currentAO, *currentAI, *dacAI, *FFcurrentAO;
  double *FFvalue, *setpointValue;
  char **DeviceName;
  short *exist;
  int32_t waveformRows;
  chid setpointID, FFID, *currentID, *currentAIID, *dacAIID, *FFcurrentAOID;
} CORR_VECTOR;

void read_corr_waveform(char *plane, CORR_VECTOR *vecor, double pendIOTime, long corrTypeIndex);
long correctorSanityCheck(char *plane, CORR_VECTOR *vector);
void freeCorrVectorMemory(CORR_VECTOR *vecor);
void init_corr_vector(CORR_VECTOR *corrVector, char *plane, char *sourceModeInput, long setSourceMode,
                      long dryRun, double pendIOTime, double AOAItolerance, double dacAOtolerance, long unified, char *corrTypeInput);

int main(int argc, char **argv) {
  SCANNED_ARG *scArg;
  long i_arg, verbose = 0, dryRun = 0, corrTypeIndex = -1, setSourceMode = 0, unified = 0, i, j;
  double pendIOTime = 30, startTime;
  char *plane = NULL, *corrTypeInput = NULL;
  double AOAItolerance = 2.0, dacAOtolerance = 0.1;
  char *corrFile, *sourceModeInput = NULL;
  CORR_VECTOR corrVector;

  corrFile = NULL;

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
          fprintf(stderr, "invalid plane vlaue %s provided, has to be h or v!\n", plane);
          exit(1);
        }
        break;
      case CLO_CORR_TYPE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -corrType option!");
        corrTypeInput = scArg[i_arg].list[1];
        if ((corrTypeIndex = match_string(corrTypeInput, corrType, corrTypes, EXACT_MATCH)) < 0) {
          fprintf(stderr, "invalid corrector type vlaue %s provided, has to be h or v!\n", corrTypeInput);
          exit(1);
        }
        if (corrTypeIndex != 4)
          sourceModeInput = sourceMode[0];
        else
          sourceModeInput = sourceMode[1];
      case CLO_SETSOURCE:
        setSourceMode = 1;
        break;
      case CLO_UNIFIED:
        unified = 1;
        break;
      case CLO_AOAItolerance:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -AOAItolerance option!");
        if (!get_double(&AOAItolerance, scArg[i_arg].list[1]))
          SDDS_Bomb("Invalid AOAItolerance value provided.");
        break;
      case CLO_DACAOtolerance:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -DACAOtolerance option!");
        if (!get_double(&dacAOtolerance, scArg[i_arg].list[1]))
          SDDS_Bomb("Invalid dacAOtolerance value provided.");
        break;
      case CLO_CORR_FILE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -corrFile option!");
        SDDS_CopyString(&corrFile, scArg[i_arg].list[1]);
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", scArg[i_arg].list[0]);
        exit(1);
      }
    } else {
      fprintf(stderr, "unrecognized option %s provided\n", scArg[i_arg].list[0]);
      exit(1);
    }
  }

  startTime = getTimeInSecs();

  ca_task_initialize();
  if (!plane || !corrTypeInput)
    SDDS_Bomb("plane and corrector type are required");
  if (!corrFile)
    SDDS_CopyString(&corrFile, "/home/helios/oagData/SCR/snapshots/SR/SR-UserBeamPreferred.gz");
  read_corr_waveform(plane, &corrVector, pendIOTime, corrTypeIndex);
  if (verbose)
    fprintf(stdout, "reading corrector spent %f seconds\n", getTimeInSecs() - startTime);
  /*read scalar pvs values */
  for (i = 0; i < corrVector.waveformRows; i++) {
    corrVector.FFvalue[i] = 0;
    corrVector.setpointValue[i] = 0;
    if (corrVector.exist[i]) {
      if (ca_get(DBR_DOUBLE, corrVector.currentID[i], &corrVector.currentAO[i]) != ECA_NORMAL ||
          ca_get(DBR_DOUBLE, corrVector.currentAIID[i], &corrVector.currentAI[i]) != ECA_NORMAL ||
          ca_get(DBR_DOUBLE, corrVector.dacAIID[i], &corrVector.dacAI[i]) != ECA_NORMAL ||
          ca_get(DBR_DOUBLE, corrVector.FFcurrentAOID[i], &corrVector.FFcurrentAO[i]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading corrector values\n");
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing read for some channels\n");
    exit(1);
  }
  for (i = 0; i < corrVector.waveformRows; i++) {
    corrVector.FFvalue[i] = 0;
    corrVector.setpointValue[i] = 0;
    if (corrVector.exist[i]) {
      corrVector.FFvalue[i] = corrVector.FFcurrentAO[i];
      corrVector.setpointValue[i] = corrVector.dacAI[i] - corrVector.FFcurrentAO[i];
    }
  }
  init_corr_vector(&corrVector, plane, sourceModeInput, setSourceMode,
                   dryRun, pendIOTime, AOAItolerance, dacAOtolerance, unified, corrTypeInput);
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing put for some channels\n");
    exit(1);
  }
  ca_task_exit();
  freeCorrVectorMemory(&corrVector);
  free(corrFile);
  free_scanargs(&scArg, argc);
  if (verbose)
    fprintf(stdout, "at %f seconds done.\n", getTimeInSecs() - startTime);
  return 0;
}

void read_corr_waveform(char *plane, CORR_VECTOR *vector, double pendIOTime, long corrTypeIndex) {
  SDDS_DATASET SDDSconfig, SDDSvector;
  int32_t rows = 0, configRows = 0;
  char configFile[256], vectorFile[256];
  char **deviceName = NULL, **configPVName = NULL;
  char pvName[256], *FFPV = NULL, *setpointPV = NULL, Plane[256];
  short *nonExist = NULL;
  long i, matchj;

  if (strcmp(plane, "h") == 0)
    sprintf(Plane, "H");
  else
    sprintf(Plane, "V");
  sprintf(vectorFile, "/home/helios/oagData/sr/orbitControllaw/waveforms/%scorrInfo.sdds", plane);
  sprintf(configFile, "/home/helios/oagData/sr/%sCorrectorStatus/config.sdds", Plane);

  if (!SDDS_InitializeInput(&SDDSconfig, configFile) ||
      !SDDS_InitializeInput(&SDDSvector, vectorFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_ReadPage(&SDDSconfig) <= 0) {
    fprintf(stderr, "Error reading config file %s\n", configFile);
    exit(1);
  }
  if (SDDS_ReadPage(&SDDSvector) <= 0) {
    fprintf(stderr, "Error reading vector file %s\n", vectorFile);
    exit(1);
  }
  if (!(configRows = SDDS_RowCount(&SDDSconfig))) {
    fprintf(stderr, "No data found config file %s\n", configFile);
    exit(1);
  }
  if (!(rows = SDDS_RowCount(&SDDSvector))) {
    fprintf(stderr, "No data found vector file %s\n", vectorFile);
    exit(1);
  }
  if (!(configPVName = (char **)SDDS_GetColumn(&SDDSconfig, "DeviceName")) ||
      !(nonExist = (short *)SDDS_GetColumnInShort(&SDDSconfig, "Nonexistent")) ||
      !SDDS_Terminate(&SDDSconfig))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(SDDS_GetParameter(&SDDSvector, "FeedforwardWaveformPV", &FFPV)) ||
      !(SDDS_GetParameter(&SDDSvector, "SetpointWaveformPV", &setpointPV)) ||
      !(deviceName = (char **)SDDS_GetColumn(&SDDSvector, "DeviceName")) ||
      !SDDS_Terminate(&SDDSvector))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  vector->waveformRows = rows;
  vector->exist = NULL;
  vector->currentAO = vector->currentAI = vector->dacAI = vector->FFcurrentAO = vector->FFvalue = vector->setpointValue = NULL;
  vector->currentID = vector->currentAIID = vector->FFcurrentAOID = NULL;
  vector->currentAO = malloc(sizeof(double) * rows);
  vector->FFcurrentAO = malloc(sizeof(double) * rows);
  vector->currentAI = malloc(sizeof(double) * rows);
  vector->dacAI = malloc(sizeof(double) * rows);
  vector->exist = malloc(sizeof(short) * rows);
  vector->currentID = malloc(sizeof(chid) * rows);
  vector->currentAIID = malloc(sizeof(chid) * rows);
  vector->dacAIID = malloc(sizeof(chid) * rows);
  vector->FFcurrentAOID = malloc(sizeof(chid) * rows);
  vector->FFvalue = malloc(sizeof(double) * rows);
  vector->setpointValue = malloc(sizeof(double) * rows);

  for (i = 0; i < rows; i++) {
    vector->exist[i] = 0;
    vector->FFvalue[i] = 0;
    vector->setpointValue[i] = 0;
    if ((matchj = match_string(deviceName[i], configPVName, configRows, EXACT_MATCH)) >= 0) {
      if (!nonExist[matchj]) {
        vector->exist[i] = 1;
        sprintf(pvName, "%s:DacAI", deviceName[i]);
        if (ca_search(pvName, &vector->dacAIID[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
        /*plain corrector use <corr>:CurrentAO */
        if (corrTypeIndex == 4)
          sprintf(pvName, "%s:CurrentAO", deviceName[i]);
        else
          sprintf(pvName, "SFB:%s:CurrentAO", deviceName[i]);
        if (ca_search(pvName, &vector->currentID[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
        sprintf(pvName, "%s:CurrentAI", deviceName[i]);
        if (ca_search(pvName, &vector->currentAIID[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
        sprintf(pvName, "SFB:%s:FFCurrentAO", deviceName[i]);
        if (ca_search(pvName, &vector->FFcurrentAOID[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
      }
    }
  }
  vector->DeviceName = deviceName;
  SDDS_FreeStringArray(configPVName, configRows);
  free(configPVName);
  free(nonExist);
  if (ca_search(setpointPV, &vector->setpointID) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for %s\n", setpointPV);
    ca_task_exit();
    exit(1);
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    exit(1);
  }

  free(FFPV);
  free(setpointPV);
  return;
}

void freeCorrVectorMemory(CORR_VECTOR *vector) {
  free(vector->currentAO);
  free(vector->currentAI);
  free(vector->dacAI);
  free(vector->FFcurrentAO);
  free(vector->FFvalue);
  free(vector->setpointValue);
  free(vector->currentID);
  free(vector->currentAIID);
  free(vector->dacAIID);
  free(vector->FFcurrentAOID);
  free(vector->exist);
  SDDS_FreeStringArray(vector->DeviceName, vector->waveformRows);
  free(vector->DeviceName);
}

void init_corr_vector(CORR_VECTOR *corrVector, char *plane, char *sourceModeInput, long setSourceMode,
                      long dryRun, double pendIOTime, double AOAItolerance, double dacAOtolerance, long unified, char *corrTypeInput) {
  long i, failed;
  double dacAI_AI_diff, dacAI_AO_diff;
  /*first corrector sanity check*/
  failed = 0;
  for (i = 0; i < corrVector->waveformRows; i++) {
    if (corrVector->exist[i]) {
      dacAI_AI_diff = fabs(corrVector->dacAI[i] - corrVector->currentAI[i]);
      dacAI_AO_diff = fabs(corrVector->dacAI[i] - corrVector->currentAO[i] - corrVector->FFcurrentAO[i]);

      if (dacAI_AI_diff > AOAItolerance || dacAI_AO_diff > dacAOtolerance) {
        fprintf(stderr, "Error %s sanity check failed, DacAI %f, CurrentAI %f, CurrentAO %f, FFcurrentAO %f\n", corrVector->DeviceName[i], corrVector->dacAI[i],
                corrVector->currentAI[i], corrVector->currentAO[i], corrVector->FFcurrentAO[i]);
        failed++;
      }
    }
  }
  if (failed > 0) {
    fprintf(stderr, "Corrector sanity check failed, quite initialize vector.\n");
    exit(1);
  }
  /*set corrector mode to scalar, then initialize corr vector */
  if (setSourceMode)
    set_source_mode(plane, sourceModeInput, dryRun, pendIOTime);
  set_corr_mode(plane, "scalar", dryRun, pendIOTime, unified);
  /*then set corr vector */
  if (!dryRun) {
    if (strcmp(corrTypeInput, "DP") == 0 || strcmp(corrTypeInput, "vector") == 0) {
      if (ca_array_put(DBR_DOUBLE, corrVector->waveformRows, corrVector->setpointID, corrVector->setpointValue) != ECA_NORMAL) {
        fprintf(stderr, "Error setting values for corrector vector\n");
        exit(1);
      }
    }
  }
}
