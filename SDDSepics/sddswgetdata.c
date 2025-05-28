/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddswgetdata.c
 * purpose: reads waveform PV data and writes to no header SDDS file in column major order
 * input:
 *   Each page of the input requests data for a single waveform.
 *   Column: WaveformPV --- the name of the waveform process variable
 *               
 * output: only the data of the waveforms are written in output file in column order.
 * modified from sddswget, 4/1/2013 H. Shang
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#define CLO_PVNAMES 0
#define CLO_INPUTSTYLE 1
#define CLO_PENDIOTIME 2
#define CLO_PIPE 3
#define COMMANDLINE_OPTIONS 4
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "pvnames",
  "inputstyle",
  "pendiotime",
  "pipe",
};

#define WMONITOR_STYLE 0
#define WGET_STYLE 1
#define AUTO_STYLE 2
#define INPUT_STYLES 3
char *inputStyleOption[INPUT_STYLES] = {
  "wmonitor",
  "wget",
  "automatic",
};

static char *USAGE1 = "sddswget\n\
    [<inputfile> [-inputStyle={wget|wmonitor}] | -PVnames=<name>[,<name>]]\n\
    <outputfile> [-pendIOtime=<seconds>] [-pipe[=input][,output]] \n\n\
Writes values of process variables to a binary file in column major order without SDDS header.\n\
<inputfile>        By default, has a column WaveformPV that gives the name of the \n\
                   waveforms to read.  This is wmonitor style.  If -inputStyle=wget is given,\n\
                   the input file is instead like the output file from sddswget, meaning\n\
                   that the WaveformPV data is a parameter.\n\
                   If no style is specified and if column WaveformPV is not found but\n\
                   parameter WaveformPV is found, wget style is used.\n\
PVnames            Specifies a list of PV names to read, if input file is not given.\n\
<outputfile>       SDDS output file, each page of which one instance of each waveform.\n\
pipe               input/output data to the pipe.\n\
Program by H. Shang.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

long ProcessWmonitorInputData(char ***readbackPV, long *readbacks, SDDS_DATASET *SDDSin);
long ProcessWgetInputData(char ***readbackPV, long *readbacks, SDDS_DATASET *SDDSin);
long GetWaveformsDataType(long *biggestDataType, long **dataType, char **waveformPV, long waveforms);

int main(int argc, char **argv) {
  SCANNED_ARG *scArg;
  SDDS_TABLE SDDSin, SDDS_dummy;
  char *inputfile, *outputfile, **bufferCopy = NULL;
  char **waveformPV;
  chid *waveformCHID = NULL;
  long waveforms;
  long *waveformLength;
  void **waveformData;
  long i_arg, i, j;
  long inputStyle;
  long biggestDatatype, *dataType;
  double pendIOtime = 30.0;
  unsigned long pipeFlags = 0;
  FILE *fileID;
  SDDS_FILEBUFFER *fBuffer = NULL;

  SDDS_RegisterProgramName(argv[0]);
  SDDS_CheckDatasetStructureSize(sizeof(SDDS_DATASET));

  argc = scanargs(&scArg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE1);
    exit(1);
  }
  inputfile = outputfile = NULL;
  inputStyle = AUTO_STYLE;
  waveformPV = NULL;
  waveforms = 0;
  biggestDatatype = SDDS_DOUBLE;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scArg[i_arg].arg_type == OPTION) {
      delete_chars(scArg[i_arg].list[0], "_");
      switch (match_string(scArg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (scArg[i_arg].n_items != 2 || sscanf(scArg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_INPUTSTYLE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb("no value given for option -inputStyle");
        if ((inputStyle = match_string(scArg[i_arg].list[1], inputStyleOption,
                                       INPUT_STYLES, 0)) < 0)
          SDDS_Bomb("invalid -inputStyle");
        break;
      case CLO_PVNAMES:
        if (scArg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -PVnames syntax");
        waveforms = scArg[i_arg].n_items - 1;
        waveformPV = scArg[i_arg].list + 1;
        waveformCHID = malloc(sizeof(chid) * waveforms);
        for (i = 0; i < waveforms; i++)
          waveformCHID[i] = NULL;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scArg[i_arg].list + 1, scArg[i_arg].n_items - 1, &pipeFlags))
          bomb("invalid -pipe syntax", NULL);
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", scArg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!inputfile)
        inputfile = scArg[i_arg].list[0];
      else if (!outputfile)
        outputfile = scArg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames given");
    }
  }
  if ((waveforms || pipeFlags) && inputfile && outputfile)
    SDDS_Bomb("Two many output files given!");
  if (waveforms) {
    outputfile = inputfile;
    inputfile = NULL;
  }
  if (pipeFlags & USE_STDIN) {
    outputfile = inputfile;
    inputfile = NULL;
  }
  if (!waveforms && !inputfile && !(pipeFlags & USE_STDIN))
    SDDS_Bomb("neither inputfile nor pvnames given!");
  if ((waveforms && inputfile) || (waveforms && pipeFlags & USE_STDIN) || (inputfile && pipeFlags & USE_STDIN))
    SDDS_Bomb("too many input filenames given");
  if (!outputfile && !(pipeFlags & USE_STDOUT))
    SDDS_Bomb("neither output filename nor -pipe=output not given");
  if (outputfile && pipeFlags & USE_STDOUT)
    SDDS_Bomb("two many output filenames given (both output file and pipe=out given)!");

  if (!outputfile) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
      fprintf(stderr, "error: unable to set stdout to binary mode\n");
      exit(1);
    }
#endif
    fileID = stdout;
  } else {
    if (!(fileID = fopen(outputfile, "wb"))) {
      fprintf(stderr, "unable to open output file for writing\n");
      exit(1);
    }
  }

  if (inputfile || pipeFlags & USE_STDIN) {
    SDDS_SetDefaultIOBufferSize(0);
    if (!SDDS_InitializeInput(&SDDSin, inputfile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (inputStyle == AUTO_STYLE) {
      if (SDDS_GetColumnIndex(&SDDSin, "WaveformPV") >= 0) {
        if (SDDS_GetParameterIndex(&SDDSin, "WaveformPV") >= 0)
          SDDS_Bomb("Input file has WaveformPV as a parameter and a column.  Can't determine what type of input it is.  Use -inputStyle option.");
        inputStyle = WMONITOR_STYLE;
      } else if (SDDS_GetParameterIndex(&SDDSin, "WaveformPV") >= 0)
        inputStyle = WGET_STYLE;
      else
        SDDS_Bomb("input file is lacking WaveformPV column and parameter");
    } else {
      if (inputStyle == WMONITOR_STYLE &&
          SDDS_GetColumnIndex(&SDDSin, "WaveformPV") < 0)
        SDDS_Bomb("WaveformPV column not found");
      if (inputStyle == WGET_STYLE &&
          SDDS_GetParameterIndex(&SDDSin, "WaveformPV") < 0)
        SDDS_Bomb("WaveformPV parameter not found");
    }

    if (inputStyle == WMONITOR_STYLE) {
      /* Get list of waveform PV names from input file. */
      if (!ProcessWmonitorInputData(&waveformPV, &waveforms, &SDDSin))
        SDDS_Bomb("problem reading waveform input file");
      if (!waveforms)
        SDDS_Bomb("no waveform data in input file");
    } else {
      /* Get list of waveform PV names from input file. */
      if (!ProcessWgetInputData(&waveformPV, &waveforms, &SDDSin))
        SDDS_Bomb("problem reading waveform input file");
      if (!waveforms)
        SDDS_Bomb("no waveform data in input file");
    }
  }
  dataType = NULL;
  GetWaveformsDataType(&biggestDatatype, &dataType, waveformPV, waveforms);

  /* find the length of each waveform */
  if (!(waveformLength = malloc(sizeof(*waveformLength) * waveforms)))
    SDDS_Bomb("memory allocation failure (2)");
  if (waveformCHID == NULL) {
    waveformCHID = malloc(sizeof(chid) * waveforms);
    for (i = 0; i < waveforms; i++)
      waveformCHID[i] = NULL;
  }
  for (i = 0; i < waveforms; i++) {
    if (ca_search(waveformPV[i], &(waveformCHID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", waveformPV[i]);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < waveforms; i++) {
      if (ca_state(waveformCHID[i]) != cs_conn)
        fprintf(stderr, "%s not connected\n", waveformPV[i]);
    }
    exit(1);
  }
  for (i = 0; i < waveforms; i++)
    waveformLength[i] = ca_element_count(waveformCHID[i]);

  /* read the waveforms */
  if (!(waveformData = SDDS_Malloc(sizeof(*waveformData) * waveforms)))
    SDDS_Bomb("Memory allocation failure (3)");
  if (biggestDatatype == SDDS_STRING)
    bufferCopy = tmalloc(sizeof(char *) * waveforms);
  for (i = 0; i < waveforms; i++) {
    waveformData[i] = tmalloc(SDDS_type_size[dataType[i] - 1] * waveformLength[i]);
    if (biggestDatatype == SDDS_STRING) {
      bufferCopy[i] = tmalloc(sizeof(char) * (40 * waveformLength[i]));
      if (ca_array_get(DBR_STRING, waveformLength[i], waveformCHID[i], bufferCopy[i]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading %s\n", waveformPV[i]);
        exit(1);
      }
      for (j = 0; j < waveformLength[i]; j++)
        ((char **)waveformData[i])[j] = (char *)&(bufferCopy[i][40 * j]);
    } else {
      if (ca_array_get(DataTypeToCaType(dataType[i]), waveformLength[i],
                       waveformCHID[i], waveformData[i]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading %s\n", waveformPV[i]);
        exit(1);
      }
    }
  }

  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    exit(1);
  }

  fBuffer = &SDDS_dummy.fBuffer;
  fBuffer->buffer = NULL;
  if (!fBuffer->buffer) {
    if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * SDDS_FILEBUFFER_SIZE))) {
      fprintf(stderr, "Unable to do buffered read--allocation failure\n");
      exit(1);
    }
    fBuffer->bufferSize = SDDS_FILEBUFFER_SIZE;
    fBuffer->bytesLeft = SDDS_FILEBUFFER_SIZE;
  }
  for (i = 0; i < waveforms; i++) {
    switch (dataType[i]) {
    case SDDS_STRING:
      for (j = 0; j < waveformLength[i]; j++) {
        if (!SDDS_WriteBinaryString(*((char **)waveformData[i] + j), fileID, fBuffer)) {
          fprintf(stderr, "Unable to write data--failure writing string\n");
          exit(1);
        }
      }
      break;
    default:
      if (!SDDS_BufferedWrite((char *)waveformData[i], SDDS_type_size[dataType[i] - 1] * waveformLength[i], fileID, fBuffer)) {
        fprintf(stderr, "Unable to write data--failure writing numerica values\n");
        exit(1);
      }
      break;
    }
  }
  if (!SDDS_FlushBuffer(fileID, fBuffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteBinaryPage)");
    return 0;
  }
  fclose(fileID);
  for (i = 0; i < waveforms; i++) {
    free(waveformData[i]);
  }
  if (waveformCHID)
    free(waveformCHID);
  if (inputfile || pipeFlags & USE_STDIN) {
    for (i = 0; i < waveforms; i++) {
      free(waveformPV[i]);
    }
    free(waveformPV);
  }
  if (biggestDatatype == SDDS_STRING) {
    for (i = 0; i < waveforms; i++)
      free(bufferCopy[i]);
    free(bufferCopy);
  }
  free(waveformData);
  free(waveformLength);
  ca_task_exit();
  return 0;
}

long ProcessWmonitorInputData(char ***readbackPV, long *readbacks, SDDS_DATASET *SDDSin) {
  *readbacks = 0;
  *readbackPV = NULL;

  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDSin, "WaveformPV", NULL, SDDS_STRING, stderr))) {
    SDDS_SetError("Missing data in waveform input file");
    return 0;
  }

  if (0 >= SDDS_ReadPage(SDDSin)) {
    SDDS_SetError("No data in file");
    return 0;
  }
  if (!(*readbacks = SDDS_CountRowsOfInterest(SDDSin))) {
    SDDS_SetError("Zero rows defined in input file.\n");
    return 0;
  }
  if (!(*readbackPV = (char **)SDDS_GetColumn(SDDSin, "WaveformPV"))) {
    SDDS_SetError("Unable to find WaveformPV or WaveformName columns");
    return 0;
  }
  if (!SDDS_Terminate(SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}

long ProcessWgetInputData(char ***readbackPV, long *readbacks, SDDS_DATASET *SDDSin) {
  long pages;

  *readbacks = 0;
  *readbackPV = NULL;

  if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(SDDSin, "WaveformPV", NULL, SDDS_STRING, stderr))) {
    SDDS_SetError("Missing data in waveform input file");
    return 0;
  }
  pages = 0;
  *readbackPV = NULL;
  while (SDDS_ReadPage(SDDSin) > 0) {
    if (!(*readbackPV = SDDS_Realloc(*readbackPV, sizeof(**readbackPV) * (pages + 1))))
      SDDS_Bomb("memory allocation failure");
    if (!SDDS_GetParameter(SDDSin, "WaveformPV", *readbackPV + pages))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    pages++;
  }
  *readbacks = pages;
  if (!SDDS_Terminate(SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}

long GetWaveformsDataType(long *biggestDataType, long **dataType, char **waveformPV, long waveforms) {
  long i, stringType, numericType;
  chid *channelID;
  channelID = NULL;
  stringType = numericType = 0;

  ca_task_initialize();
  if (!(channelID = (chid *)malloc(sizeof(*channelID) * waveforms)))
    SDDS_Bomb("Memory allocation failure!");
  if (!(*dataType = malloc(sizeof(**dataType) * waveforms)))
    SDDS_Bomb("Memory allocation failure!");
  for (i = 0; i < waveforms; i++) {
    if (ca_search(waveformPV[i], channelID + i) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n",
              waveformPV[i]);
      exit(1);
    }
  }
  ca_pend_io(30);
  for (i = 0; i < waveforms; i++) {
    switch (ca_field_type(channelID[i])) {
    case DBF_STRING:
    case DBF_ENUM:
      (*dataType)[i] = SDDS_STRING;
      stringType = 1;
      break;
    case DBF_DOUBLE:
      (*dataType)[i] = SDDS_DOUBLE;
      numericType = 1;
      break;
    case DBF_FLOAT:
      (*dataType)[i] = SDDS_FLOAT;
      numericType = 1;
      break;
    case DBF_INT64:
      (*dataType)[i] = SDDS_LONG64;
      numericType = 1;
      break;
    case DBF_UINT64:
      (*dataType)[i] = SDDS_ULONG64;
      numericType = 1;
      break;
    case DBF_LONG:
      (*dataType)[i] = SDDS_LONG;
      numericType = 1;
      break;
    case DBF_ULONG:
      (*dataType)[i] = SDDS_ULONG;
      numericType = 1;
      break;
    case DBF_SHORT:
      (*dataType)[i] = SDDS_SHORT;
      numericType = 1;
      break;
    case DBF_USHORT:
      (*dataType)[i] = SDDS_USHORT;
      numericType = 1;
      break;
    case DBF_CHAR:
      (*dataType)[i] = SDDS_CHARACTER;
      /*numericType=1;*/
      break;
    case DBF_NO_ACCESS:
      fprintf(stderr, "Error: No access to PV %s\n", waveformPV[i]);
      exit(1);
      break;
    default:
      fprintf(stderr, "Error: invalid datatype of PV %s\n", waveformPV[i]);
      exit(1);
      break;
    }
  }
  if (stringType && numericType) {
    fprintf(stderr, "Error: can not save the string type waveform and numeric waveform data in one file1");
    exit(1);
  }
  *biggestDataType = (*dataType)[0];
  for (i = 1; i < waveforms; i++) {
    if ((*dataType)[i] < *biggestDataType)
      *biggestDataType = (*dataType)[i];
  }
  free(channelID);

  return 0;
}
