
#include "SDDS.h"
#include <cadef.h>
#include <time.h>
#include <pthread.h>
#include "scan.h"
#include "mdb.h"

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#define CLO_XRAMFILE 0
#define CLO_YRAMFILE 1
#define CLO_DATAPOINTS 2
#define CLO_NUMBER_OF_TRAJECTORIES 3
#define CLO_PUT 4
#define CLO_SKIPIOC 5
#define CLO_PLANE 6
#define CLO_TEST 7
#define CLO_VERBOSE 8
#define COMMANDLINE_OPTIONS 9
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "xRamFile",
  "yRamFile",
  "datapoints",
  "numberoftrajactories",
  "put",
  "skipIOCs",
  "plane",
  "test",
  "verbose",
};

static char *USAGE1 = "getinjectiontrajectory <outputFile> -xRamFile=<filename> -yRamFile=<filename1> \n\
                       [-current=<value> [-datapoints=<integer>] [-numberofTrajectories=<integer>] \n\
<xRamFile>         required, control RAM file for x plane -- same for every bpm. \n\
<yRamFile>         required, control RAM file for y plane -- same for every bpm.\n\
datapoints         specify how many data points to be collected for orbit measurement. if not provided, it will be the waveform length.\n\
putcommand         if provided, it will load RAM, set arm pvs, do injection, and dump beam; otherwise, only collecting data for testing purpose.\n\
skipIOC            provide list of IOCs to skip checking the arming state before collecting data; this will avoid waiting too long for some slow responding\n\
                   sectorss; for example -skipSectors=S1A,S2A,S2B  S1A,S2A and S2B ioc will be skipped for checking.\n\
numberoftrajectories  number of trajectories to be collected. \n\n";

#define IOCS 80
char *ioc_name[IOCS] = {
  "S1A", "S1B", "S2A", "S2B", "S3A", "S3B", "S4A", "S4B", "S5A", "S5B", "S6A", "S6B", "S7A", "S7B", "S8A", "S8B", "S9A", "S9B", "S10A", "S10B",
  "S11A", "S11B", "S12A", "S12B", "S13A", "S13B", "S14A", "S14B", "S15A", "S15B", "S16A", "S16B", "S17A", "S17B", "S18A", "S18B", "S19A", "S19B", "S20A", "S20B",
  "S21A", "S21B", "S22A", "S22B", "S23A", "S23B", "S24A", "S24B", "S25A", "S25B", "S26A", "S26B", "S27A", "S27B", "S28A", "S28B", "S29A", "S29B", "S30A", "S30B",
  "S31A", "S31B", "S32A", "S32B", "S33A", "S33B", "S34A", "S34B", "S35A", "S35B", "S36A", "S36B", "S37A", "S37B", "S38A", "S38B", "S39A", "S39B", "S40A", "S40B"};

#define BPMTYPES 7
char *bpm_names[BPMTYPES] = {"A:P2", "A:P3", "A:P4", "B:P5", "B:P4", "B:P3", "B:P2"};
#define A_BPMs 3
char *A_BPM[A_BPMs] = {"A:P2", "A:P3", "A:P4"};
#define B_BPMs 4
char *B_BPM[B_BPMs] = {"B:P5", "B:P4", "B:P3", "B:P2"};

#define BPMs 280

SDDS_DATASET *outTable = NULL;

void ReadRAMdata(char *filename, int32_t **ramData, int32_t *points);
chid *armCHID, *ramCHID, *xCHID, *yCHID, *sumxCHID, *sumyCHID, *setArmCHID, injCHID, dumpCHID;
long step = 0, datapoints = 0;
char *outputfile = NULL;
float **bpmData, **sumData;
short armed[IOCS], skip[IOCS];
short plane;
int32_t **xRam = NULL, **yRam = NULL;
int32_t xRamLength, yRamLength;
long planes;
char *xRamFile, *yRamFile;

void *LoadRAM(void *threadid) {
  long tid;

  tid = (long)threadid;

  if (plane == 0) {
    if (ca_array_put(DBR_LONG, xRamLength, ramCHID[tid], xRam[tid]) != ECA_NORMAL) {
      fprintf(stderr, "Error loading ram waveform for %s\n", ioc_name[tid]);
    }
  } else {
    if (ca_array_put(DBR_LONG, yRamLength, ramCHID[tid], yRam[tid]) != ECA_NORMAL) {
      fprintf(stderr, "Error loading ram waveform for %s\n", ioc_name[tid]);
    }
  }
  return NULL;
}

void *SetArm(void *threadid) {
  long tid;
  short setArm = 1;
  tid = (long)threadid;

  if (ca_put(DBR_SHORT, setArmCHID[tid], &setArm) != ECA_NORMAL) {
    fprintf(stderr, "Error arming %s\n", ioc_name[tid]);
    exit(1);
  }
  return NULL;
}

void *ReadArmState(void *threadid) {
  long tid;
  tid = (long)threadid;

  if (ca_get(DBR_SHORT, armCHID[tid], &armed[tid]) != ECA_NORMAL) {
    fprintf(stderr, "Error reading ioc %s arm status\n", ioc_name[tid]);
    exit(1);
  }
  return NULL;
}

void *ReadBPMData(void *threadid) {
  long tid, space, sector, index, bpms, i;

  tid = (long)threadid;
  if (wild_match(ioc_name[tid], "*A")) {
    space = 0;
    bpms = 3;
  } else {
    space = 3; /*define A bpms first, then B bpms*/
    bpms = 4;
  }
  if (tid % 2 == 0)
    sector = tid / 2 + 1;
  else
    sector = (tid + 1) / 2;

  for (i = 0; i < bpms; i++) {
    index = (sector - 1) * BPMTYPES + space + i;
    if (plane == 0) {
      if (ca_array_get(DBR_FLOAT, datapoints, xCHID[index], bpmData[index]) != ECA_NORMAL ||
          ca_array_get(DBR_FLOAT, datapoints, sumxCHID[index], sumData[index]) != ECA_NORMAL) {
        fprintf(stderr, "Problem reading bpm data\n");
        exit(1);
      }
    } else {
      if (ca_array_get(DBR_FLOAT, datapoints, yCHID[index], bpmData[index]) != ECA_NORMAL ||
          ca_array_get(DBR_FLOAT, datapoints, sumyCHID[index], sumData[index]) != ECA_NORMAL) {
        fprintf(stderr, "Problem reading bpm data\n");
        exit(1);
      }
    }
  }
  return NULL;
}

void *connectPVs(void *threadid) {
  long tid, i, bpms, space, sector, index, pl;
  char pv_name[256];
  char **BPM_NAME;

  tid = (long)threadid;
  sprintf(pv_name, "%s:AcqControlSetWF", ioc_name[tid]);
  if (ca_search(pv_name, &ramCHID[tid]) != ECA_NORMAL) {
    fprintf(stderr, "Problem doing search for %s\n", pv_name);
    exit(1);
  }

  sprintf(pv_name, "%s:TurnHistoryIsArmed", ioc_name[tid]);
  if (ca_search(pv_name, &armCHID[tid]) != ECA_NORMAL) {
    fprintf(stderr, "Problem doing search for %s\n", pv_name);
    exit(1);
  }

  sprintf(pv_name, "%s:turn:gtr:arm", ioc_name[tid]);
  if (ca_search(pv_name, &setArmCHID[tid]) != ECA_NORMAL) {
    fprintf(stderr, "Problem doing search for %s\n", pv_name);
    exit(1);
  }
  if (wild_match(ioc_name[tid], "*A")) {
    space = 0;
    bpms = A_BPMs;
    BPM_NAME = A_BPM;
  } else {
    bpms = B_BPMs;
    space = 3; /*define A bpms first, then B bpms*/
    BPM_NAME = B_BPM;
  }
  if (tid % 2 == 0)
    sector = tid / 2 + 1;
  else
    sector = (tid + 1) / 2;

  for (i = 0; i < bpms; i++) {
    index = (sector - 1) * BPMTYPES + space + i;
    for (pl = 0; pl < 2; pl++) {
      if (pl == 0) {
        sprintf(pv_name, "S%ld%s:turnHistorySmall:Xposition", sector, BPM_NAME[i]);
        if (ca_search(pv_name, &xCHID[index]) != ECA_NORMAL) {
          fprintf(stderr, "Problem doing search for %s\n", pv_name);
          exit(1);
        }
        sprintf(pv_name, "S%ld%s:turnHistorySmall:Xsum", sector, BPM_NAME[i]);
        if (ca_search(pv_name, &sumxCHID[index]) != ECA_NORMAL) {
          fprintf(stderr, "Problem doing search for %s\n", pv_name);
          exit(1);
        }
      } else {
        sprintf(pv_name, "S%ld%s:turnHistorySmall:Yposition", sector, BPM_NAME[i]);
        if (ca_search(pv_name, &yCHID[index]) != ECA_NORMAL) {
          fprintf(stderr, "Problem doing search for %s\n", pv_name);
          exit(1);
        }
        sprintf(pv_name, "S%ld%s:turnHistorySmall:Ysum", sector, BPM_NAME[i]);
        if (ca_search(pv_name, &sumyCHID[index]) != ECA_NORMAL) {
          fprintf(stderr, "Problem doing search for %s\n", pv_name);
          exit(1);
        }
      }
    }
  }
  /*ca_pend_io(10.0); */
  return NULL;
}

void *defineOutTable(void *threadid) {
  long tid, j, sector;
  char pv_name[256], sum_name[256];

  tid = (long)threadid; /*tid from 0 to 39 */

  sector = tid + 1;

  for (j = 0; j < BPMTYPES; j++) {
    if (plane == 0) {
      sprintf(pv_name, "S%ld%s:turnHistorySmall:Xposition", sector, bpm_names[j]);
      sprintf(sum_name, "S%ld%s:turnHistorySmall:Xsum", sector, bpm_names[j]);
    } else {
      sprintf(pv_name, "S%ld%s:turnHistorySmall:Yposition", sector, bpm_names[j]);
      sprintf(sum_name, "S%ld%s:turnHistorySmall:Ysum", sector, bpm_names[j]);
    }
    if (!SDDS_DefineSimpleColumn(outTable, pv_name, NULL, SDDS_FLOAT) ||
        !SDDS_DefineSimpleColumn(outTable, sum_name, NULL, SDDS_FLOAT))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  return NULL;
}

void *setOutTable(void *threadid) {
  long tid, bpm, j;

  tid = (long)threadid; /*tid=0, to 40 */

  for (j = 0; j < BPMTYPES; j++) {
    bpm = tid * BPMTYPES + j;
    if (!SDDS_SetColumn(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, bpmData[bpm], datapoints, 2 * bpm + 1) ||
        !SDDS_SetColumn(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, sumData[bpm], datapoints, 2 * bpm + 2))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  return NULL;
}

void *ReadRAMdataP(void *threadid) {
  long i, tid;
  SDDS_DATASET inSet;
  char *par = NULL, *filename;

  tid = (long)threadid; /*tid=0, to 40 */
  if (tid == 0)
    filename = xRamFile;
  else
    filename = yRamFile;

  if (!SDDS_InitializeInput(&inSet, filename))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  i = 0;
  while (SDDS_ReadPage(&inSet) >= 0) {
    if (!SDDS_GetParameter(&inSet, "WaveformPV", &par))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (wild_match(par, "*SetWF")) {
      if (tid == 0) {
        xRamLength = SDDS_CountRowsOfInterest(&inSet);
        xRam[i] = (int32_t *)SDDS_GetColumn(&inSet, "Waveform");
      } else if (tid == 1) {
        yRamLength = SDDS_CountRowsOfInterest(&inSet);
        yRam[i] = (int32_t *)SDDS_GetColumn(&inSet, "Waveform");
      }
      i++;
    }
  }
  SDDS_Terminate(&inSet);
  return NULL;
}

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  char filename[256];
  chid currentCHID, injCurrentCHID;
  long i, i_arg, numTraj, sector, j;
  double injCurrent, srCurrent;
  double startTime, runTime, startTime0;
  int32_t *index = NULL;
  pthread_t threads[IOCS];
  short disarmed = 0, *disarmedA = NULL;
  short inj = 1, dump = 1;
  short putCommand = 0, test = 0, verbose = 0;
  long planes = 0;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE1);
    exit(1);
  }
  outTable = malloc(sizeof(*outTable));
  outputfile = xRamFile = yRamFile = NULL;
  armCHID = ramCHID = xCHID = yCHID = sumxCHID = sumyCHID = setArmCHID = NULL;
  xRam = yRam = NULL;
  xRamLength = yRamLength = 0;
  datapoints = 0;
  startTime = startTime0 = getTimeInSecs();
  numTraj = 1;
  bpmData = sumData = NULL;
  for (i = 0; i < IOCS; i++)
    skip[i] = 0;
  /*dataTypeColumnGiven = 0; */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_XRAMFILE:
        xRamFile = s_arg[i_arg].list[1];
        break;
      case CLO_YRAMFILE:
        yRamFile = s_arg[i_arg].list[1];
        break;
      case CLO_NUMBER_OF_TRAJECTORIES:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invaldi -numberoftrajectories syntax.");
        if (!get_long(&numTraj, s_arg[i_arg].list[1])) {
          SDDS_Bomb("Invalid -numberoftrajectories value provided");
        }
        break;
      case CLO_DATAPOINTS:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invaldi -datapoints syntax.");
        if (!get_long(&datapoints, s_arg[i_arg].list[1])) {
          SDDS_Bomb("Invalid -datapoints value provided");
        }
        break;
      case CLO_PUT:
        putCommand = 1;
        break;
      case CLO_SKIPIOC:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invaldi -skipIOC syntax.");
        for (i = 1; i < s_arg[i_arg].n_items; i++) {
          j = match_string(s_arg[i_arg].list[i], ioc_name, IOCS, EXACT_MATCH);
          if (j >= 0)
            skip[j] = 1;
        }
        break;
      case CLO_PLANE:
        if (s_arg[i_arg].n_items == 2) {
          if (strcmp("x", s_arg[i_arg].list[1]) == 0) {
            planes = 0;
          } else if (strcmp("y", s_arg[i_arg].list[1]) == 0) {
            planes = 1;
          } else if (strcmp("both", s_arg[i_arg].list[1]) == 0) {
            planes = 2;
          }
        }
        break;
      case CLO_TEST:
        test = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        bomb("too many filenames given", NULL);
    }
  }

  if (!(armCHID = malloc(sizeof(chid) * IOCS)) ||
      !(setArmCHID = malloc(sizeof(chid) * IOCS)) ||
      !(ramCHID = malloc(sizeof(chid) * IOCS)) ||
      !(disarmedA = malloc(sizeof(*disarmedA) * IOCS)) ||
      !(yRam = malloc(sizeof(*yRam) * IOCS)) ||
      !(xRam = malloc(sizeof(*xRam) * IOCS)) ||
      !(xCHID = malloc(sizeof(chid) * BPMs)) ||
      !(yCHID = malloc(sizeof(chid) * BPMs)) ||
      !(sumxCHID = malloc(sizeof(chid) * BPMs)) ||
      !(sumyCHID = malloc(sizeof(chid) * BPMs)) ||
      !(bpmData = malloc(sizeof(*bpmData) * BPMs)) ||
      !(sumData = malloc(sizeof(*sumData) * BPMs)))
    SDDS_Bomb("Memory allocation error");
  if (xRamFile && !yRamFile) {
    ReadRAMdata(xRamFile, xRam, &xRamLength);
  }
  if (!xRamFile && yRamFile) {
    ReadRAMdata(yRamFile, yRam, &yRamLength);
  }
  if (xRamFile && yRamFile) {
    i = 0;
    pthread_create(&threads[0], NULL, ReadRAMdataP, (void *)i);
    i = 1;
    pthread_create(&threads[1], NULL, ReadRAMdataP, (void *)i);
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
  }

  runTime = getTimeInSecs();
  if (verbose)
    fprintf(stdout, "parsing arguments and read RAM files took %f seconds\n", runTime - startTime);

  startTime = runTime;
  ca_task_initialize();
  if (ca_search("S35DCCT:currentCC", &currentCHID) != ECA_NORMAL) {
    fprintf(stderr, "Problem doing search for S35DCCT:currentCC");
    exit(1);
  }
  if (ca_search("Mt:SRinjectCurrentLimitAO.VAL", &injCurrentCHID) != ECA_NORMAL) {
    fprintf(stderr, "Problem doing search for Mt:SRinjectCurrentLimitAO.VAL");
    exit(1);
  }

  if (ca_search("Mt:SRinjectMultiBO.VAL", &injCHID) != ECA_NORMAL) {
    fprintf(stderr, "Problem doing search for Mt:SRinjectMultiBO.VAL");
    exit(1);
  }
  if (ca_search("Mt:Ddg3chan4.GATE", &dumpCHID) != ECA_NORMAL) {
    fprintf(stderr, "Problem doing search for Mt:SRinjectMultiBO.VAL");
    exit(1);
  }
  ca_pend_io(10);

  if (ca_get(DBR_DOUBLE, currentCHID, &srCurrent) != ECA_NORMAL ||
      ca_get(DBR_DOUBLE, injCurrentCHID, &injCurrent) != ECA_NORMAL) {
    fprintf(stderr, "Error reading sr and injection current\n");
    exit(1);
  }
  ca_pend_io(10);
  if (putCommand && srCurrent > injCurrent) {
    fprintf(stderr, "Error: Beam current exceeds beam limit, no injection possible.\n");
    exit(1);
  }
  runTime = getTimeInSecs();
  if (verbose)
    fprintf(stdout, "reading current took %f seconds\n", runTime - startTime);
  startTime = runTime;
  /*connect pvs */ /*do not know why can not use pthread for connecting pvs */
  for (i = 0; i < IOCS; i++) {
    connectPVs((void *)i);
  }
  if (ca_pend_io(60.0) != ECA_NORMAL) {
    fprintf(stderr, "Error searching for some pvs.\n");
    exit(1);
  }
  if (!datapoints) {
    datapoints = ca_element_count(xCHID[0]);
  }
  if (!(index = malloc(sizeof(*index) * datapoints)))
    SDDS_Bomb("Error allocating memory for index");
  for (i = 0; i < datapoints; i++)
    index[i] = i;
  for (i = 0; i < BPMs; i++) {
    bpmData[i] = sumData[i] = NULL;
    if (!(bpmData[i] = malloc(sizeof(**bpmData) * datapoints)) ||
        !(sumData[i] = malloc(sizeof(**sumData) * datapoints)))
      SDDS_Bomb("Error allocating memory for bpm data");
  }

  runTime = getTimeInSecs();
  if (verbose)
    fprintf(stdout, "connecting pvs took %f seconds\n", runTime - startTime);
  startTime = runTime;
  plane = planes;
  for (plane = 0; plane < 2; plane++) {
    if (planes == 1 && plane == 0)
      continue;
    if (planes == 0 && plane == 1)
      break;
    /*loading RAMP waveform */
    if (putCommand) {
      for (i = 0; i < IOCS; i++) {
        LoadRAM((void *)i);
      }
      if (ca_pend_io(60.0) != ECA_NORMAL) {
        fprintf(stderr, "Error loading ram waveforms\n");
        exit(1);
      }
      runTime = getTimeInSecs();
      if (verbose)
        fprintf(stdout, "load RAM took %f seconds\n", runTime - startTime);
      startTime = runTime;
    }
    if (ca_get(DBR_DOUBLE, currentCHID, &srCurrent) != ECA_NORMAL) {
      fprintf(stderr, "Error reading sr current\n");
      exit(1);
    }
    ca_pend_io(10.0);

    if (srCurrent > 0.1) {
      if (putCommand) {
        if (verbose)
          fprintf(stdout, "kill beam...\n");
        dump = 1;
        if (ca_put(DBR_SHORT, dumpCHID, &dump) != ECA_NORMAL) {
          fprintf(stderr, "Error dump beam\n");
          exit(1);
        }
        ca_pend_event(1.0);
        dump = 0;
        if (ca_put(DBR_SHORT, dumpCHID, &dump) != ECA_NORMAL) {
          fprintf(stderr, "Error dump beam\n");
          exit(1);
        }
        ca_pend_io(1.0);
        runTime = getTimeInSecs();
        if (verbose)
          fprintf(stdout, "dump beam  took %f seconds\n", runTime - startTime);
        startTime = runTime;
      }
    }
    if (plane == 0) {
      if (verbose)
        fprintf(stdout, "working on x plane\n");
      sprintf(filename, "%s.x.raw", outputfile);
    } else {
      if (verbose)
        fprintf(stdout, "working on y plane\n");
      sprintf(filename, "%s.y.raw", outputfile);
    }
    if (!SDDS_InitializeOutput(outTable, SDDS_BINARY, 1, NULL, NULL, filename) ||
        !SDDS_DefineSimpleColumn(outTable, "Index", NULL, SDDS_LONG))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < 40; i++) {
      defineOutTable((void *)i);
    }
    if (!SDDS_WriteLayout(outTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    runTime = getTimeInSecs();
    if (verbose)
      fprintf(stdout, "setup output file took %f seconds\n", runTime - startTime);
    startTime = runTime;
    runTime = getTimeInSecs();

    /*reading data */

    for (step = 0; step < numTraj; step++) {
      if (!SDDS_StartTable(outTable, datapoints) ||
          !SDDS_SetColumn(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, index, datapoints, 0))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      /*arm bpms */
      if (putCommand) {
        for (i = 0; i < IOCS; i++) {
          SetArm((void *)i);
        }
        ca_pend_event(0.1);
        if (ca_pend_io(30.0) != ECA_NORMAL) {
          fprintf(stderr, "Error inject beam or arm bpms\n");
          exit(1);
        }
        /*inject */
        inj = 1;
        if (ca_put(DBR_SHORT, injCHID, &inj) != ECA_NORMAL) {
          fprintf(stderr, "Error inject beam\n");
          exit(1);
        }
        if (ca_pend_io(30.0) != ECA_NORMAL) {
          fprintf(stderr, "Error inject beam or arm bpms\n");
          exit(1);
        }
        runTime = getTimeInSecs();
        if (verbose)
          fprintf(stdout, "inject beam and arm bpms took %f seconds\n", runTime - startTime);
        startTime = runTime;
      }
      /*wait for 1 seconds before checking the arming state */
      /*ca_pend_event(1.0); */

      /*check if disarmed, skipping for now */
      if (verbose)
        fprintf(stdout, "waiting for bpm to discharge...\n");
      for (i = 0; i < IOCS; i++)
        disarmedA[i] = 0;
      while (1) {
        for (i = 0; i < IOCS; i++) {
          if (skip[i] || disarmedA[i]) {
            armed[i] = 0;
            continue;
          }
          ReadArmState((void *)i);
        }
        if (ca_pend_io(30.0) != ECA_NORMAL) {
          fprintf(stderr, "Error reading arm state\n");
          exit(1);
        }
        disarmed = 1;
        for (i = 0; i < IOCS; i++) {
          if (!skip[i] && armed[i]) {
            disarmed = 0;
            break;
          } else {
            disarmedA[i] = 1;
          }
        }
        if (disarmed)
          break;
        if (test)
          break;
        ca_pend_event(0.1);
      }
      runTime = getTimeInSecs();
      if (verbose)
        fprintf(stdout, "reading arm state and wait for disarmed took %f seconds\n", runTime - startTime);
      startTime = runTime;
      for (i = 0; i < IOCS; i++) {
        /* ReadBPMData((void *)i); */
        ReadBPMData((void *)i);
      }
      if (ca_pend_io(60.0) != ECA_NORMAL) {
        fprintf(stderr, "Error reading bpm data\n");
        exit(1);
      }
      runTime = getTimeInSecs();
      if (verbose)
        fprintf(stdout, "reading bpm data took %f seconds\n", runTime - startTime);
      startTime = runTime;

      for (sector = 0; sector < 40; sector++) {
        setOutTable((void *)sector);
      }
      if (!SDDS_WritePage(outTable))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_Terminate(outTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    runTime = getTimeInSecs();
    if (verbose)
      fprintf(stdout, "write output file took %f seconds\n", runTime - startTime);
    startTime = runTime;
  }

  startTime = runTime;

  free(index);
  for (i = 0; i < BPMs; i++) {
    free(bpmData[i]);
    free(sumData[i]);
  }
  free(bpmData);
  free(sumData);
  if (disarmedA)
    free(disarmedA);
  if (armCHID)
    free(armCHID);
  if (ramCHID)
    free(ramCHID);
  if (xCHID)
    free(xCHID);
  if (yCHID)
    free(yCHID);
  for (i = 0; i < IOCS; i++) {
    if (xRam)
      free(xRam[i]);
    if (yRam)
      free(yRam[i]);
  }
  if (xRam)
    free(xRam);
  if (yRam)
    free(yRam);
  if (sumxCHID)
    free(sumxCHID);
  if (sumyCHID)
    free(sumyCHID);
  runTime = getTimeInSecs();
  if (verbose)
    fprintf(stdout, "Total took %f seconds\n", runTime - startTime0);
  return 0;
}

void ReadRAMdata(char *filename, int32_t **ramData, int32_t *points) {
  long i;
  SDDS_DATASET inSet;
  char *par = NULL;

  if (!SDDS_InitializeInput(&inSet, filename))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  i = 0;
  while (SDDS_ReadPage(&inSet) >= 0) {
    if (!SDDS_GetParameter(&inSet, "WaveformPV", &par))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (wild_match(par, "*SetWF")) {
      *points = SDDS_CountRowsOfInterest(&inSet);
      ramData[i] = (int32_t *)SDDS_GetColumn(&inSet, "Waveform");
      i++;
    }
    free(par);
  }
  SDDS_Terminate(&inSet);
}
