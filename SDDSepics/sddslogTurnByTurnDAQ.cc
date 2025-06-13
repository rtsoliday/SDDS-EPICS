
#include <complex>
#include <iostream>

#include <pv/pvaClientMultiChannel.h>
#include <pv/convert.h>
#include "pv/pvData.h"

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#define NTimeUnitNames 4
static char *TimeUnitNames[NTimeUnitNames] = {
  (char *)"seconds",
  (char *)"minutes",
  (char *)"hours",
  (char *)"days",
};

static double TimeUnitFactor[NTimeUnitNames] = {
  1,
  60,
  3600,
  86400};

#define CLO_SAMPLEINTERVAL 0
#define CLO_STEPS 1
#define CLO_TIME 2
#define CLO_PENDIOTIME 3
#define CLO_VERBOSE 4
#define COMMANDLINE_OPTIONS 5
static char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"sampleInterval", (char *)"steps", (char *)"time", (char *)"pendiotime", (char *)"verbose"};

static char *USAGE = (char *)"sddslogTurnByTurnDAQ <outputfile> \n\
[-sampleInterval=<real-value>[,<time-units>]] \n\
[-steps=<integer-value> | -time=<real-value>[,<time-units>]] \n\
[-pendIOtime=<value>] [-verbose]\n\n\
It connects to tbt_data over the EPICS7 PVA network and stores the data in an sdds file.\n";

int main(int argc, char *argv[]) {
  epics::pvaClient::PvaClientPtr pvaClientPtr;
  epics::pvaClient::PvaClientChannelPtr PvaClientChannelPtr;
  epics::pvaClient::PvaClientGetPtr PvaClientGetPtr;
  epics::pvaClient::PvaClientGetDataPtr PvaClientGetDataPtr;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  epics::pvData::Status status;
  epics::pvData::PVStructurePtr pvStructurePtr;
  epics::pvData::Type type;
  long Nsteps = 0, step;
  double targetTime, timeToWait;
  size_t fieldCount;
  int nElements;
  SDDS_TABLE SDDS_table;
  int32_t n_rows = -1;
  SCANNED_ARG *s_arg;
  char *outputfile = NULL;
  long i_arg, optionCode;
  double SampleTimeInterval;
  long TimeUnits;
  bool steps_set = 0, totalTimeSet = 0, verbose = 0;
  double TotalTime = 0, pendIOtime = 10.0;

  SampleTimeInterval = -1; // The actual update rate on the IOC is .20457 seconds per update at the time of writing this

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE);
    return (1);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], (char *)"_");
      switch (optionCode = match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_SAMPLEINTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&SampleTimeInterval, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -sampleInterval\n");
          return (1);
        }
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) >= 0)
            SampleTimeInterval *= TimeUnitFactor[TimeUnits];
          else {
            fprintf(stderr, "unknown/ambiguous time units given for -sampleInterval\n");
            return (1);
          }
        }
        break;
      case CLO_STEPS:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_long(&Nsteps, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -steps\n");
          return (1);
        }
        steps_set = 1;
        break;
      case CLO_TIME:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&TotalTime, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -time\n");
          return (1);
        }
        totalTimeSet = 1;
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) != 0) {
            TotalTime *= TimeUnitFactor[TimeUnits];
          }
        }
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0) {
          fprintf(stderr, "invalid -pendIOtime syntax\n");
          return (1);
        }
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        return (1);
      }
    } else {
      if (outputfile) {
        fprintf(stderr, "more than one output file given\n");
        return (1);
      }
      outputfile = s_arg[i_arg].list[0];
    }
  }
  if (!outputfile) {
    fprintf(stderr, "no output file given\n");
    return (1);
  }
  if (!steps_set && !totalTimeSet) {
    fprintf(stderr, "missing both -steps and -time options\n");
    return (1);
  }
  if (SampleTimeInterval < 0) {
    fprintf(stderr, "missing -sampleInterval option\n");
    return (1);
  }
  if (totalTimeSet) {
    if (steps_set) {
      printf("Option time supersedes option steps\n");
    }
    Nsteps = TotalTime / SampleTimeInterval + 1;
  }

  //Connect to PV
  if (verbose) {
    fprintf(stdout, "Connecting to PVA tbt_data\n");
  }
  pvaClientPtr = epics::pvaClient::PvaClient::get("pva");
  try {
    PvaClientChannelPtr = pvaClientPtr->channel("tbt_data", "pva", pendIOtime);
  } catch (std::exception &e) {
    std::cerr << "exception " << e.what() << std::endl;
    return (1);
  }

  targetTime = getTimeInSecs() + SampleTimeInterval;
  //Get values over PVA
  if (verbose) {
    fprintf(stdout, "Getting tbt_data values for step 1\n");
  }
  PvaClientGetPtr = PvaClientChannelPtr->createGet("field()");
  PvaClientGetPtr->issueGet();
  status = PvaClientGetPtr->waitGet();
  if (!status.isSuccess()) {
    fprintf(stderr, "Some PVs did not respond to a \"get\" request\n");
    return (1);
  }

  //Extract Data
  PvaClientGetDataPtr = PvaClientGetPtr->getData();
  pvStructurePtr = PvaClientGetDataPtr->getPVStructure();
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();

  //Open SDDS file
  if (!SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, 1, NULL, NULL, outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }

  //Define Columns and Parameters
  for (size_t i = 0; i < fieldCount; ++i) {
    type = PVFieldPtrArray[i]->getField()->getType();
    switch (type) {
    case epics::pvData::scalar: {
      epics::pvData::ScalarConstPtr scalarConstPtr;
      epics::pvData::ScalarType scalarType;
      scalarConstPtr = std::tr1::static_pointer_cast<const epics::pvData::Scalar>(PVFieldPtrArray[i]->getField());
      scalarType = scalarConstPtr->getScalarType();
      switch (scalarType) {
      case epics::pvData::pvBoolean: //Closest in SDDS is USHORT
      {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_USHORT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvUByte: //Closest in SDDS is USHORT
      {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_USHORT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvByte: {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_CHARACTER)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvUShort: {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_USHORT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvShort: {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_SHORT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvUInt: {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_ULONG)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvInt: {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_LONG)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvULong: //Closest in SDDS is SDDS_ULONG which is 32bits and will not hold large 64bit values
      {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_ULONG)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvLong: //Closest in SDDS is SDDS_LONG which is 32bits and will not hold large 64bit values
      {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_LONG)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvFloat: {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_FLOAT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvDouble: {
        if (!SDDS_DefineSimpleParameter(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_DOUBLE)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
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
    case epics::pvData::scalarArray: {
      epics::pvData::ScalarArrayConstPtr scalarArrayConstPtr;
      epics::pvData::ScalarType scalarType;
      epics::pvData::PVScalarArrayPtr pvScalarArrayPtr;

      scalarArrayConstPtr = std::tr1::static_pointer_cast<const epics::pvData::ScalarArray>(PVFieldPtrArray[i]->getField());
      pvScalarArrayPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalarArray>(PVFieldPtrArray[i]);
      nElements = pvScalarArrayPtr->getLength();
      scalarType = scalarArrayConstPtr->getElementType();
      if (nElements == 0) {
        continue;
      }
      if (n_rows == -1) {
        n_rows = nElements;
      } else if (n_rows != nElements) {
        fprintf(stderr, "Error: The number or elements in different scalar arrays differs, which is unexpected\n");
        fprintf(stderr, "Error: %d vs %d\n", n_rows, nElements);
        return (1);
      }
      switch (scalarType) {
      case epics::pvData::pvBoolean: //Closest in SDDS is SDDS_USHORT
      {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_USHORT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvUByte: //Closest in SDDS is SDDS_USHORT
      {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_USHORT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvByte: {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_CHARACTER)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvUShort: {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_USHORT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvShort: {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_SHORT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvUInt: {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_ULONG)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvInt: {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_LONG)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvULong: //Closest in SDDS is SDDS_ULONG which is 32bits and will not hold large 64bit values
      {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_ULONG)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvLong: //Closest in SDDS is SDDS_LONG which is 32bits and will not hold large 64bit values
      {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_LONG)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvFloat: {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_FLOAT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        break;
      }
      case epics::pvData::pvDouble: {
        if (!SDDS_DefineSimpleColumn(&SDDS_table, PVFieldPtrArray[i]->getFieldName().c_str(), NULL, SDDS_DOUBLE)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
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
      std::cout << "ERROR: Need code to handle type " << type << std::endl;
      return (1);
    }
    }
  }

  //Save header
  if (!SDDS_SaveLayout(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (!SDDS_WriteLayout(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (n_rows == -1) {
    n_rows = 0;
  }

  for (step = 1; step <= Nsteps; step++) {

    if (step > 1) {
      timeToWait = targetTime - getTimeInSecs();
      if (timeToWait > 0) {
        usleepSystemIndependent(timeToWait * 1000000);
        targetTime = targetTime + SampleTimeInterval;
      } else {
        targetTime = getTimeInSecs() + SampleTimeInterval;
      }

      //Get values over PVA
      if (verbose) {
        fprintf(stdout, "Getting tbt_data values for step %ld\n", step);
      }
      //PvaClientGetPtr = PvaClientChannelPtr->createGet("field()");
      PvaClientGetPtr->issueGet();
      status = PvaClientGetPtr->waitGet();
      if (!status.isSuccess()) {
        fprintf(stderr, "Some PVs did not respond to a \"get\" request\n");
        return (1);
      }

      //Extract Data
      PvaClientGetDataPtr = PvaClientGetPtr->getData();
      pvStructurePtr = PvaClientGetDataPtr->getPVStructure();
      PVFieldPtrArray = pvStructurePtr->getPVFields();
      if (fieldCount != pvStructurePtr->getStructure()->getNumberFields()) {
        fprintf(stderr, "The number of fields changed\n");
        return (1);
      }
    }

    //Start page
    if (!SDDS_StartTable(&SDDS_table, n_rows)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }

    //Write column and parameter data
    for (size_t i = 0; i < fieldCount; ++i) {
      type = PVFieldPtrArray[i]->getField()->getType();
      switch (type) {
      case epics::pvData::scalar: {
        epics::pvData::ScalarConstPtr scalarConstPtr;
        epics::pvData::ScalarType scalarType;
        epics::pvData::PVScalarPtr pvScalarPtr;
        scalarConstPtr = std::tr1::static_pointer_cast<const epics::pvData::Scalar>(PVFieldPtrArray[i]->getField());
        pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(PVFieldPtrArray[i]);
        scalarType = scalarConstPtr->getScalarType();

        switch (scalarType) {
        case epics::pvData::pvBoolean: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<unsigned short>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvUByte: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<unsigned short>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvByte: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<char>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvUShort: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<unsigned short>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvShort: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<short>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvUInt: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<unsigned int>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvInt: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<int>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvULong: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<unsigned int>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvLong: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<int>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvFloat: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<float>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          break;
        }
        case epics::pvData::pvDouble: {
          if (!SDDS_SetParameter(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, PVFieldPtrArray[i]->getFieldName().c_str(), pvScalarPtr->getAs<double>())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
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
      case epics::pvData::scalarArray: {
        epics::pvData::ScalarArrayConstPtr scalarArrayConstPtr;
        epics::pvData::ScalarType scalarType;
        epics::pvData::PVScalarArrayPtr pvScalarArrayPtr;

        scalarArrayConstPtr = std::tr1::static_pointer_cast<const epics::pvData::ScalarArray>(PVFieldPtrArray[i]->getField());
        pvScalarArrayPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalarArray>(PVFieldPtrArray[i]);
        nElements = pvScalarArrayPtr->getLength();
        scalarType = scalarArrayConstPtr->getElementType();
        if (nElements == 0) {
          continue;
        }
        if (n_rows == -1) {
          n_rows = nElements;
        } else if (n_rows != nElements) {
          fprintf(stderr, "Error: The number or elements in different scalar arrays differs, which is unexpected\n");
          fprintf(stderr, "Error: %d vs %d\n", n_rows, nElements);
          return (1);
        }
        switch (scalarType) {
        case epics::pvData::pvBoolean: {
          unsigned short *data;
          data = (unsigned short *)malloc(sizeof(unsigned short) * n_rows);
          epics::pvData::PVUShortArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<unsigned short>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvUByte: {
          unsigned short *data;
          data = (unsigned short *)malloc(sizeof(unsigned short) * n_rows);
          epics::pvData::PVUShortArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<unsigned short>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvByte: {
          short *data;
          data = (short *)malloc(sizeof(short) * n_rows);
          epics::pvData::PVByteArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<int8_t>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvUShort: {
          unsigned short *data;
          data = (unsigned short *)malloc(sizeof(unsigned short) * n_rows);
          epics::pvData::PVUShortArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<unsigned short>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvShort: {
          short *data;
          data = (short *)malloc(sizeof(short) * n_rows);
          epics::pvData::PVShortArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<short>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvUInt: {
          uint32_t *data;
          data = (uint32_t *)malloc(sizeof(uint32_t) * n_rows);
          epics::pvData::PVUIntArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<unsigned int>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvInt: {
          int32_t *data;
          data = (int32_t *)malloc(sizeof(int32_t) * n_rows);
          epics::pvData::PVIntArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<int>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvULong: {
          uint32_t *data;
          data = (uint32_t *)malloc(sizeof(uint32_t) * n_rows);
          epics::pvData::PVUIntArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<unsigned int>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvLong: {
          int32_t *data;
          data = (int32_t *)malloc(sizeof(int32_t) * n_rows);
          epics::pvData::PVIntArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<int>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvFloat: {
          float *data;
          data = (float *)malloc(sizeof(float) * n_rows);
          epics::pvData::PVFloatArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<float>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
          break;
        }
        case epics::pvData::pvDouble: {
          double *data;
          data = (double *)malloc(sizeof(double) * n_rows);
          epics::pvData::PVDoubleArray::const_svector dataVector;
          pvScalarArrayPtr->PVScalarArray::getAs<double>(dataVector);
          std::copy(dataVector.begin(), dataVector.begin() + n_rows, data);
          if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, n_rows, PVFieldPtrArray[i]->getFieldName().c_str())) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (n_rows)
            free(data);
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
        std::cout << "ERROR: Need code to handle type " << type << std::endl;
        return (1);
      }
      }
    }
    if (!SDDS_WriteTable(&SDDS_table)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
  }
  //Close SDDS file
  if (verbose) {
    fprintf(stdout, "Data written to %s\n", outputfile);
  }

  if (!SDDS_Terminate(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }

  return (0);
}
