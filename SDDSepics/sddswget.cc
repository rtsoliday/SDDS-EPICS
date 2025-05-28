/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddswget.c
 * purpose: reads waveform PV data and writes to SDDS file
 * input:
 *   Each page of the input requests data for a single waveform.
 *   Column: WaveformPV --- the name of the waveform process variable
 *               
 * output: each page of the output file corresponds to a single waveform.
 *   Parameters: Time (double, in seconds)
 *               TimeStamp (string)
 *               all column values from input 
 * 
 *   Columns: WaveformData (float or double)
 *            Index (long)
 *
 * Michael Borland, 2001.
 */
#include <complex>
#include <iostream>

#ifdef _WIN32
#  include <winsock.h>
#else
#  include <unistd.h>
#endif

#include <cadef.h>
#include <epicsVersion.h>

#if (EPICS_VERSION > 3)
#  include <pv/pvaClientMultiChannel.h>
#  include "pva/client.h"
#  include "pv/pvData.h"
#endif

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"

#define CLO_PVNAMES 0
#define CLO_INPUTSTYLE 1
#define CLO_PENDIOTIME 2
#define CLO_PIPE 3
#define CLO_PROVIDER 4
#define COMMANDLINE_OPTIONS 5
char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"pvnames",
  (char *)"inputstyle",
  (char *)"pendiotime",
  (char *)"pipe",
  (char *)"provider",
};

#define WMONITOR_STYLE 0
#define WGET_STYLE 1
#define AUTO_STYLE 2
#define INPUT_STYLES 3
char *inputStyleOption[INPUT_STYLES] = {
  (char *)"wmonitor",
  (char *)"wget",
  (char *)"automatic",
};

#define PROVIDER_CA 0
#define PROVIDER_PVA 1
#define PROVIDER_COUNT 2
char *providerOption[PROVIDER_COUNT] = {
  (char *)"ca",
  (char *)"pva",
};

static char *USAGE1 = (char *)"sddswget\n\
    [<inputfile> [-inputStyle={wget|wmonitor|automatic}] | -PVnames=<name>[,<name>]]\n\
    <outputfile> [-pendIOtime=<seconds>] [-pipe[=input][,output]]\n\
    [-provider={ca|pva}]\n\n\
Writes values of process variables to a binary SDDS file.\n\
<inputfile>        By default, has a column WaveformPV that gives the name of the \n\
                   waveforms to read.  This is wmonitor style.  If -inputStyle=wget is given,\n\
                   the input file is instead like the output file from sddswget, meaning\n\
                   that the WaveformPV data is a parameter.\n\
                   If no style is specified and if column WaveformPV is not found but\n\
                   parameter WaveformPV is found, wget style is used.\n\
PVnames            Specifies a list of PV names to read, if input file is not given.\n\
-provider          Defaults to CA (channel access) calls.\n\
<outputfile>       SDDS output file, each page of which one instance of each waveform.\n\
pipe               input/output data to the pipe.\n\
Program by M. Borland.\n\
Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct
{
  char *name;
  long size;
  long index; /* index in the target data set */
  short doCopy;
} SOURCE_DATA;

long ProcessWmonitorInputData(char ***readbackPV, long *readbacks,
                              SDDS_DATASET *SDDSin, SDDS_DATASET *SDDSout,
                              SOURCE_DATA **columnSource0, long *columnSources0,
                              SOURCE_DATA **parameterSource0, long *parameterSources0,
                              char *inputFile, char *outputFile);
long ProcessWgetInputData(char ***readbackPV, long *readbacks,
                          SDDS_DATASET *SDDSin, SDDS_DATASET *SDDSout,
                          SOURCE_DATA **columnSource0, long *columnSources0,
                          SOURCE_DATA **parameterSource0, long *parameterSources0,
                          long **rows, char *inputFile, char *outputFile);
long GetWaveformsDataType(long *biggestDataType, long **dataType, char **waveformPV, long waveforms, double pendIOtime);
#if (EPICS_VERSION > 3)
long GetWaveformsDataTypePVA(long *biggestDataType, long **dataType, char **waveformPV, long waveforms, epics::pvData::PVStructurePtr pvStructurePtr);
#endif
int main(int argc, char **argv) {
  SCANNED_ARG *scArg;
  SDDS_TABLE SDDSin, SDDSout;
  char *inputfile, *outputfile, **bufferCopy = NULL;
  char **waveformPV, *TimeStamp;
  chid *waveformCHID = NULL;
  long waveforms;
  SOURCE_DATA *columnSource, *parameterSource;
  long columnSources = 0, parameterSources = 0;
  long *waveformLength, maxWaveformLength;
  int32_t *indexData;
  void **data = NULL;
  void **waveformData;
  double Time;
  long i_arg, i, j;
  char buffer[32];
  long inputStyle;
  long *rows = NULL, biggestDatatype, *dataType;
  double pendIOtime = 10.0;
  unsigned long pipeFlags = 0;
  long providerMode = 0;
#if (EPICS_VERSION > 3)
  epics::pvaClient::PvaClientPtr pvaClientPtr;
  epics::pvaClient::PvaClientMultiChannelPtr pvaClientMultiChannelPtr;
  epics::pvData::Status status;
  epics::pvaClient::PvaClientNTMultiGetPtr pvaClientNTMultiGetPtr;
  epics::pvaClient::PvaClientNTMultiDataPtr pvaClientNTMultiDataPtr;
  epics::pvData::PVStructurePtr pvStructurePtr;
  epics::pvData::PVUnionArrayPtr pvUnionArrayPtr;
  epics::pvData::PVUnionArray::const_svector pvUnionArrayVector;
  epics::pvData::PVUnionPtr pvUnionPtr;
  epics::pvData::Type type, unionFieldType;
#endif

  SDDS_RegisterProgramName(argv[0]);
  SDDS_CheckDatasetStructureSize(sizeof(SDDS_DATASET));

  argc = scanargs(&scArg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE1);
    return (1);
  }
  inputfile = outputfile = NULL;
  inputStyle = AUTO_STYLE;
  waveformPV = NULL;
  waveforms = 0;
  biggestDatatype = SDDS_DOUBLE;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scArg[i_arg].arg_type == OPTION) {
      delete_chars(scArg[i_arg].list[0], (char *)"_");
      switch (match_string(scArg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (scArg[i_arg].n_items != 2 || sscanf(scArg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb((char *)"invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_INPUTSTYLE:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"no value given for option -inputStyle");
        if ((inputStyle = match_string(scArg[i_arg].list[1], inputStyleOption,
                                       INPUT_STYLES, 0)) < 0)
          SDDS_Bomb((char *)"invalid -inputStyle");
        break;
      case CLO_PROVIDER:
        if (scArg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"no value given for option -provider");
        if ((providerMode = match_string(scArg[i_arg].list[1], providerOption,
                                         PROVIDER_COUNT, 0)) < 0)
          SDDS_Bomb((char *)"invalid -provider");
        break;
      case CLO_PVNAMES:
        if (scArg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -PVnames syntax");
        waveforms = scArg[i_arg].n_items - 1;
        waveformPV = scArg[i_arg].list + 1;
        waveformCHID = (oldChannelNotify **)malloc(sizeof(chid) * waveforms);
        for (i = 0; i < waveforms; i++)
          waveformCHID[i] = NULL;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scArg[i_arg].list + 1, scArg[i_arg].n_items - 1, &pipeFlags))
          bomb((char *)"invalid -pipe syntax", NULL);
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", scArg[i_arg].list[0]);
        return (1);
      }
    } else {
      if (!inputfile)
        inputfile = scArg[i_arg].list[0];
      else if (!outputfile)
        outputfile = scArg[i_arg].list[0];
      else
        SDDS_Bomb((char *)"too many filenames given");
    }
  }
  if ((waveforms || pipeFlags) && inputfile && outputfile)
    SDDS_Bomb((char *)"Two many output files given!");
  if (waveforms) {
    outputfile = inputfile;
    inputfile = NULL;
  }
  if (pipeFlags & USE_STDIN) {
    outputfile = inputfile;
    inputfile = NULL;
  }
  if (!waveforms && !inputfile && !(pipeFlags & USE_STDIN))
    SDDS_Bomb((char *)"neither inputfile nor pvnames given!");
  if ((waveforms && inputfile) || (waveforms && pipeFlags & USE_STDIN) || (inputfile && pipeFlags & USE_STDIN))
    SDDS_Bomb((char *)"too many input filenames given");
  if (!outputfile && !(pipeFlags & USE_STDOUT))
    SDDS_Bomb((char *)"neither output filename nor -pipe=output not given");
  if (outputfile && pipeFlags & USE_STDOUT)
    SDDS_Bomb((char *)"two many output filenames given (both output file and pipe=out given)!");

  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 1, NULL, NULL, outputfile) ||
      !SDDS_DefineSimpleParameter(&SDDSout, "Time", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(&SDDSout, "TimeStamp", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(&SDDSout, "WaveformPV", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleColumn(&SDDSout, "Index", NULL, SDDS_LONG))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (inputfile || pipeFlags & USE_STDIN) {
    SDDS_SetDefaultIOBufferSize(0);
    if (!SDDS_InitializeInput(&SDDSin, inputfile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (inputStyle == AUTO_STYLE) {
      if (SDDS_GetColumnIndex(&SDDSin, (char *)"WaveformPV") >= 0) {
        if (SDDS_GetParameterIndex(&SDDSin, (char *)"WaveformPV") >= 0)
          SDDS_Bomb((char *)"Input file has WaveformPV as a parameter and a column.  Can't determine what type of input it is.  Use -inputStyle option.");
        inputStyle = WMONITOR_STYLE;
      } else if (SDDS_GetParameterIndex(&SDDSin, (char *)"WaveformPV") >= 0)
        inputStyle = WGET_STYLE;
      else
        SDDS_Bomb((char *)"input file is lacking WaveformPV column and parameter");
    } else {
      if (inputStyle == WMONITOR_STYLE &&
          SDDS_GetColumnIndex(&SDDSin, (char *)"WaveformPV") < 0)
        SDDS_Bomb((char *)"WaveformPV column not found");
      if (inputStyle == WGET_STYLE &&
          SDDS_GetParameterIndex(&SDDSin, (char *)"WaveformPV") < 0)
        SDDS_Bomb((char *)"WaveformPV parameter not found");
    }

    if (inputStyle == WMONITOR_STYLE) {
      /* Get list of waveform PV names from input file.
           * Finish set up the output file based on the input file.
           * Set up data need for transferring values from input to output.
           * In this case, each row in the input file results in a page in 
           * the output file.  Columns get turned into parameters and copied
           * through.  Parameters are copied through as well.
           */
      if (!ProcessWmonitorInputData(&waveformPV, &waveforms,
                                    &SDDSin, &SDDSout, &columnSource, &columnSources,
                                    &parameterSource, &parameterSources,
                                    inputfile, outputfile))
        SDDS_Bomb((char *)"problem reading waveform input file");
      if (!waveforms)
        SDDS_Bomb((char *)"no waveform data in input file");
      if (!(data = (void **)SDDS_Malloc(sizeof(*data) * columnSources)))
        SDDS_Bomb((char *)"memory allocation failure (1)");
      for (j = 0; j < columnSources; j++)
        /* these pointers must not be free'd ! */
        if (!(data[j] = SDDS_GetInternalColumn(&SDDSin, columnSource[j].name)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else {
      /* Get list of waveform PV names from input file.
           * Finish set up the output file based on the input file.
           * Set up data need for transferring values from input to output.
           * In this case, each page in the input file results in a page in 
           * the output file.  Columns and parameters are simply copied over.
           */
      if (!ProcessWgetInputData(&waveformPV, &waveforms,
                                &SDDSin, &SDDSout, &columnSource, &columnSources,
                                &parameterSource, &parameterSources,
                                &rows, inputfile, outputfile))
        SDDS_Bomb((char *)"problem reading waveform input file");
      if (!waveforms)
        SDDS_Bomb((char *)"no waveform data in input file");
    }
  }
  dataType = NULL;

  if (providerMode == PROVIDER_PVA) {
#if (EPICS_VERSION > 3)
    //List PV names
    epics::pvData::shared_vector<std::string> pvaChannelNames(waveforms);
    for (i = 0; i < waveforms; i++) {
      pvaChannelNames[i] = waveformPV[i];
    }
    epics::pvData::shared_vector<const std::string> names(freeze(pvaChannelNames));

    //Connect to PVs
    pvaClientPtr = epics::pvaClient::PvaClient::get("pva");
    pvaClientMultiChannelPtr = epics::pvaClient::PvaClientMultiChannel::create(pvaClientPtr, names, "pva");
    status = pvaClientMultiChannelPtr->connect(pendIOtime);
    if (!status.isSuccess()) {
      fprintf(stderr, "Did not connect to: ");
      epics::pvData::shared_vector<epics::pvData::boolean> isConnected = pvaClientMultiChannelPtr->getIsConnected();
      for (i = 0; i < waveforms; ++i) {
        if (!isConnected[i]) {
          fprintf(stderr, "%s\n", waveformPV[i]);
        }
      }
      return (1);
    }

    //Get data from PVs and return a PVStructure
    pvaClientNTMultiGetPtr = pvaClientMultiChannelPtr->createNTGet();
    pvaClientNTMultiGetPtr->get();
    pvaClientNTMultiDataPtr = pvaClientNTMultiGetPtr->getData();
    pvStructurePtr = pvaClientNTMultiDataPtr->getNTMultiChannel()->getPVStructure();

    GetWaveformsDataTypePVA(&biggestDatatype, &dataType, waveformPV, waveforms, pvStructurePtr);
#else
    fprintf(stderr, "-provider=pva is only available with newer versions of EPICS\n");
    return (1);
#endif
  } else {
    GetWaveformsDataType(&biggestDatatype, &dataType, waveformPV, waveforms, pendIOtime);
  }
  if (!SDDS_DefineSimpleColumn(&SDDSout, "Waveform", NULL, biggestDatatype))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /* find the length of each waveform */
  if (!(waveformLength = (long *)malloc(sizeof(*waveformLength) * waveforms)))
    SDDS_Bomb((char *)"memory allocation failure (2)");
  if (!(waveformData = (void **)SDDS_Malloc(sizeof(*waveformData) * waveforms)))
    SDDS_Bomb((char *)"Memory allocation failure (3)");
  getTimeBreakdown(&Time, NULL, NULL, NULL, NULL, NULL, &TimeStamp);

  if (providerMode == PROVIDER_PVA) {
#if (EPICS_VERSION > 3)
    //Get pvUnionArray
    pvUnionArrayPtr = pvStructurePtr->getSubField<epics::pvData::PVUnionArray>("value");
    if (!pvUnionArrayPtr) {
      fprintf(stderr, "Object does not have field value\n");
      return (1);
    }

    //Extract array elements
    pvUnionArrayVector = pvUnionArrayPtr->view();
    for (size_t ii = 0; ii < pvUnionArrayVector.size(); ++ii) {
      pvUnionPtr = pvUnionArrayVector[ii];
      if (!pvUnionPtr) {
        fprintf(stderr, "Object does not have PVUnion field\n");
        return (1);
      }
      type = pvUnionPtr->get()->getField()->getType();
      switch (type) {
      case epics::pvData::union_: {
        pvUnionPtr = pvUnionPtr->get<epics::pvData::PVUnion>();
        if (!pvUnionPtr) {
          fprintf(stderr, "Object does not have PVUnion field\n");
          return (1);
        }
        if (pvUnionPtr->getUnion()->isVariant()) {
          fprintf(stderr, "ERROR: Need code to handle variant union\n");
          return (1);
        } else {
          //Restricted union
          epics::pvData::ScalarArrayConstPtr scalarArrayConstPtr;
          epics::pvData::ScalarType scalarType;
          epics::pvData::PVScalarArrayPtr pvScalarArrayPtr;

          unionFieldType = pvUnionPtr->get()->getField()->getType();
          switch (unionFieldType) {
          case epics::pvData::scalarArray: {
            scalarArrayConstPtr = std::tr1::static_pointer_cast<const epics::pvData::ScalarArray>(pvUnionPtr->get()->getField());
            pvScalarArrayPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalarArray>(pvUnionPtr->get());
            waveformLength[ii] = pvScalarArrayPtr->getLength();
            waveformData[ii] = tmalloc(SizeOfDataType(biggestDatatype) * waveformLength[ii]);
            scalarType = scalarArrayConstPtr->getElementType();
            switch (scalarType) {
            case epics::pvData::pvUByte: {
              epics::pvData::PVUByteArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<uint8_t>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvByte: {
              epics::pvData::PVByteArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<int8_t>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvUShort: {
              epics::pvData::PVUShortArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<unsigned short>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvShort: {
              epics::pvData::PVShortArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<short>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvUInt: {
              epics::pvData::PVUIntArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<unsigned int>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvInt: {
              epics::pvData::PVIntArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<int>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvULong: {
              epics::pvData::PVULongArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<uint64_t>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvLong: {
              epics::pvData::PVLongArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<int64_t>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvFloat: {
              epics::pvData::PVFloatArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<float>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvDouble: {
              epics::pvData::PVDoubleArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<double>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            case epics::pvData::pvBoolean: {
              epics::pvData::PVBooleanArray::const_svector dataVector;
              pvScalarArrayPtr->PVScalarArray::getAs<char>(dataVector);
              switch (biggestDatatype) {
              case SDDS_CHARACTER * -1: { //pvUByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned char *)(waveformData[ii]));
                break;
              }
              case SDDS_CHARACTER: { //pvByte
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (char *)(waveformData[ii]));
                break;
              }
              case SDDS_USHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (short *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG: { //pvUInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG: { //pvInt
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_ULONG * -1: { //pvULong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (uint32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_LONG * -1: { //pvLong
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (int32_t *)(waveformData[ii]));
                break;
              }
              case SDDS_FLOAT: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (float *)(waveformData[ii]));
                break;
              }
              case SDDS_DOUBLE: {
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (double *)(waveformData[ii]));
                break;
              }
              case SDDS_SHORT * -1: { //pvBoolean
                std::copy(dataVector.begin(), dataVector.begin() + waveformLength[ii], (unsigned short *)(waveformData[ii]));
                break;
              }
              default: {
                std::cout << "ERROR: unexpected data type " << std::endl;
                exit(1);
              }
              }
              break;
            }
            default: {
              std::cout << "ERROR: Need code to handle scalar array type " << scalarType << std::endl;
              return (1);
            }
            }
            break;
          }
          default: {
            std::cout << "ERROR: DO NOT KNOW WHAT UNION FIELD THIS IS" << unionFieldType << std::endl;
            return (1);
          }
          }
        }
        break;
      }
      case epics::pvData::unionArray: {
        std::cout << "Error: Need code to handle union array" << std::endl;
        return (1);
        break;
      }
      case epics::pvData::scalar: {
        std::cout << "Error: Need code to handle scalar" << std::endl;
        return (1);
        break;
      }
      case epics::pvData::scalarArray: {
        std::cout << "Error: Need code to handle scalar array" << std::endl;
        return (1);
        break;
      }
      case epics::pvData::structure: {
        std::cout << "Error: Need code to handle structure" << std::endl;
        return (1);
        break;
      }
      case epics::pvData::structureArray: {
        std::cout << "Error: Need code to handle structure array" << std::endl;
        return (1);
        break;
      }
      default: {
        std::cout << "ERROR: DO NOT KNOW WHAT THIS IS " << type << std::endl;
        return (1);
      }
      }
    }
#endif
  } else {
    if (waveformCHID == NULL) {
      waveformCHID = (oldChannelNotify **)malloc(sizeof(chid) * waveforms);
      for (i = 0; i < waveforms; i++)
        waveformCHID[i] = NULL;
    }
    for (i = 0; i < waveforms; i++) {
      if (ca_search(waveformPV[i], &(waveformCHID[i])) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", waveformPV[i]);
        return (1);
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      for (i = 0; i < waveforms; i++) {
        if (ca_state(waveformCHID[i]) != cs_conn)
          fprintf(stderr, "%s not connected\n", waveformPV[i]);
      }
      return (1);
    }
    for (i = 0; i < waveforms; i++)
      waveformLength[i] = ca_element_count(waveformCHID[i]);

    /* read the waveforms */
    if (biggestDatatype == SDDS_STRING)
      bufferCopy = (char **)tmalloc(sizeof(char *) * waveforms);
    for (i = 0; i < waveforms; i++) {
      waveformData[i] = tmalloc(SizeOfDataType(biggestDatatype) * waveformLength[i]);
      if (biggestDatatype == SDDS_STRING) {
        bufferCopy[i] = (char *)tmalloc(sizeof(char) * (40 * waveformLength[i]));
        if (ca_array_get(DBR_STRING, waveformLength[i], waveformCHID[i], bufferCopy[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem reading %s\n", waveformPV[i]);
          return (1);
        }
      } else {
        if (ca_array_get(DataTypeToCaType(biggestDatatype), waveformLength[i],
                         waveformCHID[i], waveformData[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem reading %s\n", waveformPV[i]);
          return (1);
        }
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      return (1);
    }
  }
  maxWaveformLength = 0;
  for (i = 0; i < waveforms; i++) {
    if (biggestDatatype == SDDS_STRING) {
      for (j = 0; j < waveformLength[i]; j++)
        ((char **)waveformData[i])[j] = (char *)&(bufferCopy[i][40 * j]);
    }
    if (waveformLength[i] > maxWaveformLength)
      maxWaveformLength = waveformLength[i];
  }
  if (!(indexData = (int32_t *)SDDS_Malloc(sizeof(*indexData) * maxWaveformLength)))
    SDDS_Bomb((char *)"Memory allocation failure (5)");
  for (i = 0; i < maxWaveformLength; i++)
    indexData[i] = i;

  if ((inputfile || pipeFlags & USE_STDIN) && inputStyle == WGET_STYLE) {
    for (i = 0; i < waveforms; i++)
      if (waveformLength[i] != rows[i]) {
        fprintf(stderr, "The row length (%ld) doesn't match the waveform length (%ld) for page %ld, waveform PV %s (sddswget)\n",
                rows[i], waveformLength[i], i + 1,
                waveformPV[i]);
        return (1);
      }
  }

  for (i = 0; i < waveforms; i++) {
    if (!SDDS_StartPage(&SDDSout, waveformLength[i]) ||
        !SDDS_SetColumn(&SDDSout, SDDS_BY_NAME,
                        waveformData[i], waveformLength[i], "Waveform") ||
        !SDDS_SetColumn(&SDDSout, SDDS_BY_NAME,
                        indexData, waveformLength[i], "Index") ||
        !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "TimeStamp", TimeStamp,
                            "Time", Time,
                            "WaveformPV", waveformPV[i],
                            NULL))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    if (inputfile || pipeFlags & USE_STDIN) {
      if (inputStyle == WMONITOR_STYLE) {
        for (j = 0; j < parameterSources; j++) {
          if (!parameterSource[j].doCopy)
            continue;
          if (!SDDS_GetParameter(&SDDSin, parameterSource[j].name, buffer) ||
              !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE,
                                  parameterSource[j].index, buffer, -1))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        for (j = 0; j < columnSources; j++) {
          if (!columnSource[j].doCopy)
            continue;
          if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE,
                                  columnSource[j].index,
                                  (((char *)data[j]) + i * columnSource[j].size),
                                  -1))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      } else {
        if (!SDDS_GotoPage(&SDDSin, i + 1) || SDDS_ReadPage(&SDDSin) <= 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (j = 0; j < parameterSources; j++) {
          if (!parameterSource[j].doCopy)
            continue;
          if (!SDDS_GetParameter(&SDDSin, parameterSource[j].name, buffer) ||
              !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE,
                                  parameterSource[j].index, buffer, -1))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        for (j = 0; j < columnSources; j++) {
          void *data1;
          if (!columnSource[j].doCopy)
            continue;
          if (!(data1 = SDDS_GetInternalColumn(&SDDSin, columnSource[j].name)) ||
              !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_NAME,
                              data1, rows[i], columnSource[j].name))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (providerMode != PROVIDER_PVA) {
    ca_task_exit();
  }

  if (inputfile || pipeFlags & USE_STDIN)
    if (!SDDS_Terminate(&SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (inputStyle == WMONITOR_STYLE)
    free(data);
  free(indexData);
  free_scanargs(&scArg, argc);
  free(dataType);
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
  for (i = 0; i < parameterSources; i++)
    if (parameterSource[i].name)
      free(parameterSource[i].name);
  if (parameterSources)
    free(parameterSource);
  for (i = 0; i < columnSources; i++)
    if (columnSource[i].name)
      free(columnSource[i].name);
  if (columnSources)
    free(columnSource);
  if (rows)
    free(rows);
  return 0;
}

long ProcessWmonitorInputData(
  char ***readbackPV, long *readbacks,
  SDDS_DATASET *SDDSin, SDDS_DATASET *SDDSout,
  SOURCE_DATA **columnSource0, long *columnSources0,
  SOURCE_DATA **parameterSource0, long *parameterSources0,
  char *inputFile,
  char *outputFile) {
  char **name;
  long i;
  SOURCE_DATA *columnSource = NULL, *parameterSource = NULL;
  int32_t columnSources, parameterSources;

  *readbacks = 0;
  *readbackPV = NULL;

  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDSin, (char *)"WaveformPV", NULL, SDDS_STRING, stderr))) {
    SDDS_SetError((char *)"Missing data in waveform input file");
    return 0;
  }

  /* The following was lifted from sddsexpand.c */
  /* this code transfers all the columns in the input to parameters in the output */
  if (!(name = SDDS_GetColumnNames(SDDSin, &columnSources)) ||
      !(columnSource = (SOURCE_DATA *)SDDS_Malloc(sizeof(*columnSource) * columnSources)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < columnSources; i++) {
    columnSource[i].name = name[i];
    if (SDDS_GetParameterIndex(SDDSout, columnSource[i].name) >= 0) {
      columnSource[i].doCopy = 0;
      continue;
    }
    if (!SDDS_DefineParameterLikeColumn(SDDSout, SDDSin, columnSource[i].name, NULL) ||
        (columnSource[i].index = SDDS_GetParameterIndex(SDDSout, columnSource[i].name)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    columnSource[i].size = SDDS_GetTypeSize(SDDS_GetParameterType(SDDSout, columnSource[i].index));
    columnSource[i].doCopy = 1;
  }

  if (!(name = SDDS_GetParameterNames(SDDSin, &parameterSources)) ||
      !(parameterSource = (SOURCE_DATA *)SDDS_Malloc(sizeof(*parameterSource) * parameterSources)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < parameterSources; i++) {
    parameterSource[i].name = name[i];
    if (SDDS_GetParameterIndex(SDDSout, parameterSource[i].name) >= 0) {
      parameterSource[i].doCopy = 0;
      continue;
    }
    parameterSource[i].doCopy = 1;
    if (!SDDS_TransferParameterDefinition(SDDSout, SDDSin,
                                          parameterSource[i].name, NULL) ||
        (parameterSource[i].index = SDDS_GetParameterIndex(SDDSout, parameterSource[i].name)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    parameterSource[i].size = SDDS_GetTypeSize(SDDS_GetParameterType(SDDSout, parameterSource[i].index));
  }

  *parameterSource0 = parameterSource;
  *columnSource0 = columnSource;
  *parameterSources0 = parameterSources;
  *columnSources0 = columnSources;

  if (0 >= SDDS_ReadPage(SDDSin)) {
    SDDS_SetError((char *)"No data in file");
    return 0;
  }
  if (!(*readbacks = SDDS_CountRowsOfInterest(SDDSin))) {
    SDDS_SetError((char *)"Zero rows defined in input file.\n");
    return 0;
  }
  if (!(*readbackPV = (char **)SDDS_GetColumn(SDDSin, (char *)"WaveformPV"))) {
    SDDS_SetError((char *)"Unable to find WaveformPV or WaveformName columns");
    return 0;
  }
  return 1;
}

long ProcessWgetInputData(
  char ***readbackPV, long *readbacks,
  SDDS_DATASET *SDDSin, SDDS_DATASET *SDDSout,
  SOURCE_DATA **columnSource0, long *columnSources0,
  SOURCE_DATA **parameterSource0, long *parameterSources0,
  long **rows, char *inputFile, char *outputFile) {
  char **name;
  long i, pages;
  SOURCE_DATA *columnSource = NULL, *parameterSource = NULL;
  int32_t columnSources, parameterSources;

  *readbacks = 0;
  *readbackPV = NULL;

  if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(SDDSin, (char *)"WaveformPV", NULL, SDDS_STRING, stderr))) {
    SDDS_SetError((char *)"Missing data in waveform input file");
    return 0;
  }

  /* The following was lifted from sddsexpand.c 
   * this code transfers all the columns in the input to the output 
   */
  if (!(name = SDDS_GetColumnNames(SDDSin, &columnSources)) ||
      !(columnSource = (SOURCE_DATA *)SDDS_Malloc(sizeof(*columnSource) * columnSources)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < columnSources; i++) {
    columnSource[i].name = name[i];
    if (SDDS_GetColumnIndex(SDDSout, columnSource[i].name) >= 0 || strcmp(columnSource[i].name, "Waveform") == 0) {
      columnSource[i].doCopy = 0;
      continue;
    }
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, columnSource[i].name, NULL) ||
        (columnSource[i].index = SDDS_GetColumnIndex(SDDSout, columnSource[i].name)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    columnSource[i].size = SDDS_GetTypeSize(SDDS_GetColumnType(SDDSout, columnSource[i].index));
    columnSource[i].doCopy = 1;
  }

  if (!(name = SDDS_GetParameterNames(SDDSin, &parameterSources)) ||
      !(parameterSource = (SOURCE_DATA *)SDDS_Malloc(sizeof(*parameterSource) * parameterSources)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < parameterSources; i++) {
    parameterSource[i].name = name[i];
    if (SDDS_GetParameterIndex(SDDSout, parameterSource[i].name) >= 0) {
      parameterSource[i].doCopy = 0;
      continue;
    }
    parameterSource[i].doCopy = 1;
    if (!SDDS_TransferParameterDefinition(SDDSout, SDDSin,
                                          parameterSource[i].name, NULL) ||
        (parameterSource[i].index = SDDS_GetParameterIndex(SDDSout, parameterSource[i].name)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    parameterSource[i].size = SDDS_GetTypeSize(SDDS_GetParameterType(SDDSout, parameterSource[i].index));
  }

  *parameterSource0 = parameterSource;
  *columnSource0 = columnSource;
  *parameterSources0 = parameterSources;
  *columnSources0 = columnSources;

  pages = 0;
  *readbackPV = NULL;
  *rows = NULL;
  while (SDDS_ReadPage(SDDSin) > 0) {
    if (!(*readbackPV = (char **)SDDS_Realloc(*readbackPV, sizeof(**readbackPV) * (pages + 1))) ||
        !(*rows = (long *)SDDS_Realloc(*rows, sizeof(**rows) * (pages + 1))))
      SDDS_Bomb((char *)"memory allocation failure");
    if (!SDDS_GetParameter(SDDSin, (char *)"WaveformPV", *readbackPV + pages))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    (*rows)[pages] = SDDS_CountRowsOfInterest(SDDSin);
    pages++;
  }

  *readbacks = pages;
  return 1;
}

long GetWaveformsDataType(long *biggestDataType, long **dataType, char **waveformPV, long waveforms, double pendIOtime) {
  long i, stringType, numericType;
  chid *channelID;
  channelID = NULL;
  stringType = numericType = 0;

  ca_task_initialize();
  if (!(channelID = (chid *)malloc(sizeof(*channelID) * waveforms)))
    SDDS_Bomb((char *)"Memory allocation failure!");
  if (!(*dataType = (long *)malloc(sizeof(**dataType) * waveforms)))
    SDDS_Bomb((char *)"Memory allocation failure!");
  for (i = 0; i < waveforms; i++) {
    if (ca_search(waveformPV[i], channelID + i) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n",
              waveformPV[i]);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < waveforms; i++) {
      if (ca_state(channelID[i]) != cs_conn)
        fprintf(stderr, "%s not connected\n", waveformPV[i]);
    }
    exit(1);
  }

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
    case DBF_LONG:
      (*dataType)[i] = SDDS_LONG;
      numericType = 1;
      break;
    case DBF_SHORT:
      (*dataType)[i] = SDDS_SHORT;
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
    fprintf(stderr, "Error: can not save the string type waveform and numeric waveform data in one file");
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

#if (EPICS_VERSION > 3)
long GetWaveformsDataTypePVA(long *biggestDataType, long **dataType, char **waveformPV, long waveforms, epics::pvData::PVStructurePtr pvStructurePtr) {
  epics::pvData::PVUnionArrayPtr pvUnionArrayPtr;
  epics::pvData::PVUnionArray::const_svector pvUnionArrayVector;
  epics::pvData::PVUnionPtr pvUnionPtr;
  epics::pvData::Type type, unionFieldType;
  long i, stringType, numericType;
  stringType = numericType = 0;

  if (!(*dataType = (long *)malloc(sizeof(**dataType) * waveforms)))
    SDDS_Bomb((char *)"Memory allocation failure!");

  //Get pvUnionArray
  pvUnionArrayPtr = pvStructurePtr->getSubField<epics::pvData::PVUnionArray>("value");
  if (!pvUnionArrayPtr) {
    fprintf(stderr, "Object does not have field value\n");
    exit(1);
  }

  //Extract array elements
  pvUnionArrayVector = pvUnionArrayPtr->view();
  for (size_t j = 0; j < pvUnionArrayVector.size(); ++j) {
    pvUnionPtr = pvUnionArrayVector[j];
    if (!pvUnionPtr) {
      fprintf(stderr, "Object does not have PVUnion field\n");
      exit(1);
    }
    type = pvUnionPtr->get()->getField()->getType();
    switch (type) {
    case epics::pvData::union_: {
      pvUnionPtr = pvUnionPtr->get<epics::pvData::PVUnion>();
      if (!pvUnionPtr) {
        fprintf(stderr, "Object does not have PVUnion field\n");
        exit(1);
      }
      if (pvUnionPtr->getUnion()->isVariant()) {
        fprintf(stderr, "ERROR: Need code to handle variant union\n");
        exit(1);
      } else {
        //Restricted union
        epics::pvData::ScalarArrayConstPtr scalarArrayConstPtr;
        epics::pvData::ScalarType scalarType;
        epics::pvData::PVScalarArrayPtr pvScalarArrayPtr;

        unionFieldType = pvUnionPtr->get()->getField()->getType();
        switch (unionFieldType) {
        case epics::pvData::scalarArray: {
          scalarArrayConstPtr = std::tr1::static_pointer_cast<const epics::pvData::ScalarArray>(pvUnionPtr->get()->getField());
          scalarType = scalarArrayConstPtr->getElementType();
          switch (scalarType) {
          case epics::pvData::pvUByte: {
            (*dataType)[j] = SDDS_CHARACTER * -1;
            numericType = 1;
            break;
          }
          case epics::pvData::pvByte: {
            (*dataType)[j] = SDDS_CHARACTER;
            numericType = 1;
            break;
          }
          case epics::pvData::pvUShort: {
            (*dataType)[j] = SDDS_USHORT;
            numericType = 1;
            break;
          }
          case epics::pvData::pvShort: {
            (*dataType)[j] = SDDS_SHORT;
            numericType = 1;
            break;
          }
          case epics::pvData::pvUInt: {
            (*dataType)[j] = SDDS_ULONG;
            numericType = 1;
            break;
          }
          case epics::pvData::pvInt: {
            (*dataType)[j] = SDDS_LONG;
            numericType = 1;
            break;
          }
          case epics::pvData::pvULong: {
            (*dataType)[j] = SDDS_ULONG * -1;
            ;
            numericType = 1;
            break;
          }
          case epics::pvData::pvLong: {
            (*dataType)[j] = SDDS_LONG * -1;
            ;
            numericType = 1;
            break;
          }
          case epics::pvData::pvFloat: {
            (*dataType)[j] = SDDS_FLOAT;
            numericType = 1;
            break;
          }
          case epics::pvData::pvDouble: {
            (*dataType)[j] = SDDS_DOUBLE;
            numericType = 1;
            break;
          }
          case epics::pvData::pvBoolean: {
            (*dataType)[j] = SDDS_SHORT * -1;
            numericType = 1;
            break;
          }
          default: {
            std::cout << "ERROR: Need code to handle scalar array type " << scalarType << std::endl;
            exit(1);
          }
          }
          break;
        }
        default: {
          std::cout << "ERROR: DO NOT KNOW WHAT UNION FIELD THIS IS" << unionFieldType << std::endl;
          exit(1);
        }
        }
      }
      break;
    }
    case epics::pvData::unionArray: {
      std::cout << "Error: Need code to handle union array" << std::endl;
      exit(1);
      break;
    }
    case epics::pvData::scalar: {
      std::cout << "Error: Need code to handle scalar" << std::endl;
      exit(1);
      break;
    }
    case epics::pvData::scalarArray: {
      std::cout << "Error: Need code to handle scalar array" << std::endl;
      exit(1);
      break;
    }
    case epics::pvData::structure: {
      std::cout << "Error: Need code to handle structure" << std::endl;
      exit(1);
      break;
    }
    case epics::pvData::structureArray: {
      std::cout << "Error: Need code to handle structure array" << std::endl;
      exit(1);
      break;
    }
    default: {
      std::cout << "ERROR: DO NOT KNOW WHAT THIS IS " << type << std::endl;
      exit(1);
    }
    }
  }
  if (stringType && numericType) {
    fprintf(stderr, "Error: can not save the string type waveform and numeric waveform data in one file");
    exit(1);
  }
  *biggestDataType = labs((*dataType)[0]);
  for (i = 1; i < waveforms; i++) {
    if (labs((*dataType)[i]) < *biggestDataType)
      *biggestDataType = labs((*dataType)[i]);
  }
  return 0;
}
#endif
