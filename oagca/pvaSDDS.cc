#include "pvaSDDS.h"
#include <unordered_map>

typedef std::unordered_multimap<std::string, long> Mymap;
typedef std::unordered_multimap<std::string, long>::iterator MymapIterator;

/*
  Allocate memory for the pva structure.
  repeats is currently only used for "get" requests where you plan to do statistics over a few readings.
*/
void allocPVA(PVA_OVERALL *pva, long PVs) {
  allocPVA(pva, PVs, 0);
}

void allocPVA(PVA_OVERALL *pva, long PVs, long repeats) {
  long i, j;
  pva->numPVs = PVs;
  pva->prevNumPVs = 0;
  pva->pvaData = (PVA_DATA_ALL_READINGS *)malloc(sizeof(PVA_DATA_ALL_READINGS) * pva->numPVs);
  if (repeats < 2) {
    for (j = 0; j < pva->numPVs; j++) {
      pva->pvaData[j].getData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
      pva->pvaData[j].getData[0].values = NULL;
      pva->pvaData[j].getData[0].stringValues = NULL;
    }
  } else {
    for (j = 0; j < pva->numPVs; j++) {
      pva->pvaData[j].getData = (PVA_DATA *)malloc(sizeof(PVA_DATA) * repeats);
      for (i = 0; i < repeats; i++) {
        pva->pvaData[j].getData[i].values = NULL;
        pva->pvaData[j].getData[i].stringValues = NULL;
      }
    }
  }
  for (j = 0; j < pva->numPVs; j++) {
    pva->pvaData[j].putData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
    pva->pvaData[j].monitorData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
    pva->pvaData[j].putData[0].values = NULL;
    pva->pvaData[j].putData[0].stringValues = NULL;
    pva->pvaData[j].monitorData[0].values = NULL;
    pva->pvaData[j].monitorData[0].stringValues = NULL;
  }
  for (j = 0; j < pva->numPVs; j++) {
    pva->pvaData[j].numGetElements = 0;
    pva->pvaData[j].numPutElements = 0;
    pva->pvaData[j].numMonitorElements = 0;
    pva->pvaData[j].numGetReadings = 0;
    pva->pvaData[j].numMonitorReadings = 0; // Don't expect this to ever be greater than 1
    pva->pvaData[j].numeric = false;
    pva->pvaData[j].nonnumeric = false;
    pva->pvaData[j].pvEnumeratedStructure = false;
    pva->pvaData[j].haveGetPtr = false;
    pva->pvaData[j].havePutPtr = false;
    pva->pvaData[j].haveMonitorPtr = false;
    pva->pvaData[j].units = NULL;
    pva->pvaData[j].alarmSeverity = 0;
    pva->pvaData[j].L1Ptr = j;
    pva->pvaData[j].L2Ptr = j;
    pva->pvaData[j].skip = false;
  }
  pva->numNotConnected = PVs;
  pva->limitGetReadings = false;
  pva->useStateChangeCallbacks = false;
  pva->useGetCallbacks = false;
  pva->useMonitorCallbacks = false;
  pva->usePutCallbacks = false;
  pva->includeAlarmSeverity = false;

  pva->numMultiChannels = 1;
  pva->pvaClientMultiChannelPtr.resize(pva->numMultiChannels);

  pva->pvaClientGetPtr.resize(pva->numPVs);
  pva->pvaClientPutPtr.resize(pva->numPVs);
  pva->pvaClientMonitorPtr.resize(pva->numPVs);

  return;
}

void reallocPVA(PVA_OVERALL *pva, long PVs) {
  reallocPVA(pva, PVs, 0);
}

void reallocPVA(PVA_OVERALL *pva, long PVs, long repeats) {
  long i, j;
  pva->prevNumPVs = pva->numPVs;
  pva->numPVs = PVs;
  pva->pvaData = (PVA_DATA_ALL_READINGS *)realloc(pva->pvaData, sizeof(PVA_DATA_ALL_READINGS) * pva->numPVs);
  pva->pvaChannelNames.resize(pva->numPVs);
  pva->pvaProvider.resize(pva->numPVs);

  if (repeats < 2) {
    for (j = pva->prevNumPVs; j < pva->numPVs; j++) {
      pva->pvaData[j].getData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
      pva->pvaData[j].getData[0].values = NULL;
      pva->pvaData[j].getData[0].stringValues = NULL;
    }
  } else {
    for (j = pva->prevNumPVs; j < pva->numPVs; j++) {
      pva->pvaData[j].getData = (PVA_DATA *)malloc(sizeof(PVA_DATA) * repeats);
      for (i = 0; i < repeats; i++) {
        pva->pvaData[j].getData[i].values = NULL;
        pva->pvaData[j].getData[i].stringValues = NULL;
      }
    }
  }
  for (j = pva->prevNumPVs; j < pva->numPVs; j++) {
    pva->pvaData[j].putData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
    pva->pvaData[j].monitorData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
    pva->pvaData[j].putData[0].values = NULL;
    pva->pvaData[j].putData[0].stringValues = NULL;
    pva->pvaData[j].monitorData[0].values = NULL;
    pva->pvaData[j].monitorData[0].stringValues = NULL;
  }
  for (j = pva->prevNumPVs; j < pva->numPVs; j++) {
    pva->pvaData[j].numGetElements = 0;
    pva->pvaData[j].numPutElements = 0;
    pva->pvaData[j].numMonitorElements = 0;
    pva->pvaData[j].numGetReadings = 0;
    pva->pvaData[j].numMonitorReadings = 0; // Don't expect this to ever be greater than 1
    pva->pvaData[j].numeric = false;
    pva->pvaData[j].nonnumeric = false;
    pva->pvaData[j].pvEnumeratedStructure = false;
    pva->pvaData[j].haveGetPtr = false;
    pva->pvaData[j].havePutPtr = false;
    pva->pvaData[j].haveMonitorPtr = false;
    pva->pvaData[j].units = NULL;
    pva->pvaData[j].alarmSeverity = 0;
    pva->pvaData[j].L1Ptr = j;
    pva->pvaData[j].L2Ptr = j;
    pva->pvaData[j].skip = false;
  }
  pva->numNotConnected += pva->numPVs - pva->prevNumPVs;

  pva->numMultiChannels++;
  pva->pvaClientMultiChannelPtr.resize(pva->numMultiChannels);

  pva->pvaClientGetPtr.resize(pva->numPVs);
  pva->pvaClientPutPtr.resize(pva->numPVs);
  pva->pvaClientMonitorPtr.resize(pva->numPVs);

  return;
}

/*
  Free memory for the pva structure.
*/
void freePVA(PVA_OVERALL *pva) {
  long i, j, k;

  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    //get variables
    for (j = 0; j < pva->pvaData[i].numGetReadings; j++) {
      if (pva->pvaData[i].getData[j].values) {
        free(pva->pvaData[i].getData[j].values);
      }
      if (pva->pvaData[i].getData[j].stringValues) {
        for (k = 0; k < pva->pvaData[i].numGetElements; k++) {
          if (pva->pvaData[i].getData[j].stringValues[k])
            free(pva->pvaData[i].getData[j].stringValues[k]);
        }
        free(pva->pvaData[i].getData[j].stringValues);
      }
    }
    //monitor variables
    if (pva->pvaData[i].monitorData[0].values) {
      free(pva->pvaData[i].monitorData[0].values);
    }
    if (pva->pvaData[i].monitorData[0].stringValues) {
      for (k = 0; k < pva->pvaData[i].numMonitorElements; k++) {
        if (pva->pvaData[i].monitorData[0].stringValues[k])
          free(pva->pvaData[i].monitorData[0].stringValues[k]);
      }
      free(pva->pvaData[i].monitorData[0].stringValues);
    }
    //put variables
    if (pva->pvaData[i].putData[0].values) {
      free(pva->pvaData[i].putData[0].values);
    }
    if (pva->pvaData[i].putData[0].stringValues) {
      //Do not free the individual strings here. They are freed in PutPVAValues
      free(pva->pvaData[i].putData[0].stringValues);
    }
    //pva client pointers
    if (pva->pvaData[i].haveGetPtr == false) {
      pva->pvaClientGetPtr[i].reset();
    }
    if (pva->pvaData[i].havePutPtr == false) {
      pva->pvaClientPutPtr[i].reset();
    }
    if (pva->pvaData[i].haveMonitorPtr == false) {
      pva->pvaClientMonitorPtr[i].reset();
    }

    free(pva->pvaData[i].getData);
    free(pva->pvaData[i].putData);
    free(pva->pvaData[i].monitorData);
    if (pva->pvaData[i].units) {
      free(pva->pvaData[i].units);
    }
  }
  free(pva->pvaData);

  return;
}

/*
  Free the "get" readings
*/
void freePVAGetReadings(PVA_OVERALL *pva) {
  long i, j, k;
  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    for (j = 0; j < pva->pvaData[i].numGetReadings; j++) {
      if (pva->limitGetReadings == false) {
        if (pva->pvaData[i].getData[j].values) {
          free(pva->pvaData[i].getData[j].values);
          pva->pvaData[i].getData[j].values = NULL;
        }
      }
      if (pva->pvaData[i].getData[j].stringValues) {
        for (k = 0; k < pva->pvaData[i].numGetElements; k++) {
          if (pva->pvaData[i].getData[j].stringValues[k]) {
            free(pva->pvaData[i].getData[j].stringValues[k]);
            pva->pvaData[i].getData[j].stringValues[k] = NULL;
          }
        }
        if (pva->limitGetReadings == false) {
          free(pva->pvaData[i].getData[j].stringValues);
          pva->pvaData[i].getData[j].stringValues = NULL;
        }
      }
    }
    if (pva->limitGetReadings == false) {
      pva->pvaData[i].numGetReadings = 0;
    }
  }
  return;
}

/*
  Free the "monitor" readings
*/
void freePVAMonitorReadings(PVA_OVERALL *pva) {
  long i, k;
  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].monitorData[0].values) {
      free(pva->pvaData[i].monitorData[0].values);
      pva->pvaData[i].monitorData[0].values = NULL;
    }
    if (pva->pvaData[i].monitorData[0].stringValues) {
      for (k = 0; k < pva->pvaData[i].numMonitorElements; k++) {
        if (pva->pvaData[i].monitorData[0].stringValues[k])
          free(pva->pvaData[i].monitorData[0].stringValues[k]);
      }
      free(pva->pvaData[i].monitorData[0].stringValues);
      pva->pvaData[i].monitorData[0].stringValues = NULL;
    }
    pva->pvaData[i].numMonitorReadings = 0;
  }
  return;
}

/*
  Connect to the PVs using PvaClientMultiChannel
*/
void ConnectPVA(PVA_OVERALL *pva, double pendIOTime) {
  long i, j, n, num = 0, numInternalPVs;
  size_t pos;
  epics::pvData::shared_vector<std::string> namesTmp(pva->numPVs);
  epics::pvData::shared_vector<std::string> subnames(pva->numPVs);
  epics::pvData::Status status;
  epics::pvaClient::PvaClientChannelArray pvaClientChannelArray;
  epics::pvData::shared_vector<epics::pvData::boolean> connected(pva->numPVs);
  Mymap m;
  MymapIterator mIter;

  i = n = 0;
  for (j = 0; j < pva->numPVs; j++) {
    if (pva->pvaProvider[j].compare("pva") == 0) {
      pos = pva->pvaChannelNames[j].find('.');
      if (pos == std::string::npos) {
        namesTmp[j] = pva->pvaChannelNames[j];
        subnames[j] = "";
      } else {
        namesTmp[j] = pva->pvaChannelNames[j].substr(0, pos);
        subnames[j] = pva->pvaChannelNames[j].substr(pos + 1);
      }
    } else {
      namesTmp[j] = pva->pvaChannelNames[j];
      subnames[j] = "";
    }
    mIter = m.find(namesTmp[j]);
    if (mIter == m.end()) {
      m.insert(Mymap::value_type(namesTmp[j], j));
      pva->pvaData[j].L1Ptr = j;
      pva->pvaData[j].L2Ptr = i;
      i++;
    } else {
      pva->pvaData[j].L1Ptr = mIter->second;
      pva->pvaData[j].L2Ptr = pva->pvaData[pva->pvaData[j].L1Ptr].L2Ptr;
    }
  }

  if (pva->numMultiChannels == 1) {
    pva->numInternalPVs = numInternalPVs = i;
    epics::pvData::shared_vector<std::string> names(pva->numInternalPVs);
    epics::pvData::shared_vector<std::string> provider(pva->numInternalPVs);
    epics::pvData::shared_vector<const std::string> constProvider;
    ;
    for (j = 0; j < pva->numPVs; j++) {
      names[pva->pvaData[j].L2Ptr] = namesTmp[j];
      provider[pva->pvaData[j].L2Ptr] = pva->pvaProvider[j];
    }
    pva->pvaChannelNamesTop = freeze(names);
    pva->pvaChannelNamesSub = freeze(subnames);
    constProvider = freeze(provider);
    //Connect to PVs all at once
    pva->pvaClientPtr = epics::pvaClient::PvaClient::get("pva ca");
    //pva->pvaClientPtr->setDebug(true);
    pva->pvaClientMultiChannelPtr[0] = epics::pvaClient::PvaClientMultiChannel::create(pva->pvaClientPtr, pva->pvaChannelNamesTop, "pva", numInternalPVs, constProvider);
    status = pva->pvaClientMultiChannelPtr[0]->connect(pendIOTime);

    pva->isInternalConnected = pva->pvaClientMultiChannelPtr[0]->getIsConnected();

    pvaClientChannelArray = pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray();
  } else {
    //This will execute if we are adding additional PVs. It is sort of a hack
    pva->prevNumInternalPVs = pva->numInternalPVs;
    pva->numInternalPVs = i;
    numInternalPVs = pva->numInternalPVs - pva->prevNumInternalPVs;
    epics::pvData::shared_vector<std::string> names(pva->numInternalPVs);
    epics::pvData::shared_vector<std::string> newnames(numInternalPVs);
    epics::pvData::shared_vector<std::string> provider(numInternalPVs);
    epics::pvData::shared_vector<const std::string> constNames;
    ;
    epics::pvData::shared_vector<const std::string> constProvider;
    ;
    for (j = 0; j < pva->numPVs; j++) {
      names[pva->pvaData[j].L2Ptr] = namesTmp[j];
      if (pva->pvaData[j].L2Ptr >= pva->prevNumInternalPVs) {
        newnames[pva->pvaData[j].L2Ptr - pva->prevNumInternalPVs] = namesTmp[j];
        provider[pva->pvaData[j].L2Ptr - pva->prevNumInternalPVs] = pva->pvaProvider[j];
      }
    }
    pva->pvaChannelNamesTop = freeze(names);
    pva->pvaChannelNamesSub = freeze(subnames);
    constNames = freeze(newnames);
    constProvider = freeze(provider);

    pva->pvaClientMultiChannelPtr[pva->numMultiChannels - 1] = epics::pvaClient::PvaClientMultiChannel::create(pva->pvaClientPtr, constNames, "pva", numInternalPVs, constProvider);
    status = pva->pvaClientMultiChannelPtr[pva->numMultiChannels - 1]->connect(pendIOTime);

    pva->isInternalConnected = pva->pvaClientMultiChannelPtr[0]->getIsConnected();
    for (j = 1; j < pva->numMultiChannels; j++) {
      epics::pvData::shared_vector<epics::pvData::boolean> isConnected;
      isConnected = pva->pvaClientMultiChannelPtr[j]->getIsConnected();
      std::copy(isConnected.begin(), isConnected.end(), std::back_inserter(pva->isInternalConnected));
    }
    pvaClientChannelArray = pva->pvaClientMultiChannelPtr[pva->numMultiChannels - 1]->getPvaClientChannelArray();
  }

  for (j = 0; j < pva->numPVs; j++) {
    connected[j] = pva->isInternalConnected[pva->pvaData[j].L2Ptr];
    if (connected[j] == false) {
      num++;
    }
  }
  pva->isConnected = connected;
  pva->numNotConnected = num;
  for (j = 0; j < numInternalPVs; j++) {
    if (pva->useStateChangeCallbacks) {
      pvaClientChannelArray[j]->setStateChangeRequester((epics::pvaClient::PvaClientChannelStateChangeRequesterPtr)pva->stateChangeReqPtr);
    }
  }
}

/*
  Read the PV values over the network and place the values in the pva structure.
*/
long GetPVAValues(PVA_OVERALL *pva) {
  long result;
  PVA_OVERALL **pvaArray;
  pvaArray = (PVA_OVERALL **)malloc(sizeof(PVA_OVERALL *));
  pvaArray[0] = pva;
  result = GetPVAValues(pvaArray, 1);
  free(pvaArray);
  return (result);
}

long GetPVAValues(PVA_OVERALL **pva, long count) {
  long i, num = 0, n;
  epics::pvData::Status status;
  epics::pvaClient::PvaClientChannelArray pvaClientChannelArray;

  for (n = 0; n < count; n++) {
    if (pva[n] != NULL) {
      pva[n]->isInternalConnected = pva[n]->pvaClientMultiChannelPtr[0]->getIsConnected();
      pvaClientChannelArray = pva[n]->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray();
      for (i = 1; i < pva[n]->numMultiChannels; i++) {
        epics::pvData::shared_vector<epics::pvData::boolean> isConnected;
        epics::pvaClient::PvaClientChannelArray pvaClientChannelArrayAdd;
        isConnected = pva[n]->pvaClientMultiChannelPtr[i]->getIsConnected();
        std::copy(isConnected.begin(), isConnected.end(), std::back_inserter(pva[n]->isInternalConnected));
        pvaClientChannelArrayAdd = pva[n]->pvaClientMultiChannelPtr[i]->getPvaClientChannelArray();
        std::copy(pvaClientChannelArrayAdd.begin(), pvaClientChannelArrayAdd.end(), std::back_inserter(pvaClientChannelArray));
      }
      for (i = 0; i < pva[n]->numPVs; i++) {
        if (pva[n]->pvaData[i].skip == true) {
          continue;
        }
        pva[n]->isConnected[i] = pva[n]->isInternalConnected[pva[n]->pvaData[i].L2Ptr];
        if (pva[n]->isConnected[i]) {
          if (pva[n]->pvaData[i].haveGetPtr == false) {
            pva[n]->pvaClientGetPtr[i] = pvaClientChannelArray[pva[n]->pvaData[i].L2Ptr]->createGet(pva[n]->pvaChannelNamesSub[i]);
            pva[n]->pvaData[i].haveGetPtr = true;
            if (pva[n]->useGetCallbacks) {
              pva[n]->pvaClientGetPtr[i]->setRequester((epics::pvaClient::PvaClientGetRequesterPtr)pva[n]->getReqPtr);
            }
          }
          try {
            pva[n]->pvaClientGetPtr[i]->issueGet();
          } catch (std::exception &e) {
            num++;
            pva[n]->isConnected[i] = false;
            //std::cerr << "Error: invalid sub-field name: " + pva[n]->pvaChannelNamesSub[i] + "\n";
            //return 1;
          }
        } else {
          num++;
        }
      }
      pva[n]->numNotConnected = num;
    }
  }

  for (n = 0; n < count; n++) {
    if ((pva[n] != NULL) && (pva[n]->useGetCallbacks == false)) {
      for (i = 0; i < pva[n]->numPVs; i++) {
        if (pva[n]->pvaData[i].skip == true) {
          continue;
        }
        if (pva[n]->isConnected[i]) {
          status = pva[n]->pvaClientGetPtr[i]->waitGet();
          if (!status.isSuccess()) {
            fprintf(stderr, "error: %s did not respond to the \"get\" request\n", pva[n]->pvaChannelNames[i].c_str());
            pva[n]->isConnected[i] = false;
            pva[n]->numNotConnected++;
            //return (1);
          }
        }
      }
    }
  }
  for (n = 0; n < count; n++) {
    if ((pva[n] != NULL) && (pva[n]->useGetCallbacks == false)) {
      if (ExtractPVAValues(pva[n]) == 1) {
        return (1);
      }
    }
  }
  return (0);
}

long ExtractScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode) {
  epics::pvData::ScalarConstPtr scalarConstPtr;
  epics::pvData::PVScalarPtr pvScalarPtr;
  long i = 0;
  scalarConstPtr = std::tr1::static_pointer_cast<const epics::pvData::Scalar>(PVFieldPtr->getField());
  pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(PVFieldPtr);

  if (monitorMode) {
    i = 0;
    if (pva->pvaData[index].numMonitorReadings == 0) {
      pva->pvaData[index].fieldType = scalarConstPtr->getType(); //should always be epics::pvData::scalar
      pva->pvaData[index].scalarType = scalarConstPtr->getScalarType();
      pva->pvaData[index].numMonitorElements = 1;
    } else {
      if (pva->pvaData[index].nonnumeric) {
        if (pva->pvaData[index].monitorData[0].stringValues[0])
          free(pva->pvaData[index].monitorData[0].stringValues[0]);
      }
    }
  } else {
    i = pva->pvaData[index].numGetReadings;
    if (pva->pvaData[index].numGetReadings == 0) {
      pva->pvaData[index].fieldType = scalarConstPtr->getType(); //should always be epics::pvData::scalar
      pva->pvaData[index].scalarType = scalarConstPtr->getScalarType();
      pva->pvaData[index].numGetElements = 1;
    } else if (pva->limitGetReadings) {
      i = 0;
    }
  }
  switch (pva->pvaData[index].scalarType) {
  case epics::pvData::pvDouble:
  case epics::pvData::pvFloat:
  case epics::pvData::pvLong:
  case epics::pvData::pvULong:
  case epics::pvData::pvInt:
  case epics::pvData::pvUInt:
  case epics::pvData::pvShort:
  case epics::pvData::pvUShort:
  case epics::pvData::pvByte:
  case epics::pvData::pvUByte: {
    if (monitorMode) {
      if (pva->pvaData[index].monitorData[0].values == NULL) {
        pva->pvaData[index].monitorData[0].values = (double *)malloc(sizeof(double));
        pva->pvaData[index].numeric = true;
      }
      pva->pvaData[index].monitorData[0].values[0] = pvScalarPtr->getAs<double>();
    } else {
      if (pva->pvaData[index].getData[i].values == NULL) {
        pva->pvaData[index].getData[i].values = (double *)malloc(sizeof(double));
        pva->pvaData[index].numeric = true;
      }
      pva->pvaData[index].getData[i].values[0] = pvScalarPtr->getAs<double>();
    }
    break;
  }
  case epics::pvData::pvString:
  case epics::pvData::pvBoolean: {
    std::string s = pvScalarPtr->getAs<std::string>();
    if (monitorMode) {
      if (pva->pvaData[index].monitorData[0].stringValues == NULL) {
        pva->pvaData[index].monitorData[0].stringValues = (char **)malloc(sizeof(char *) * 1);
      }
      pva->pvaData[index].monitorData[0].stringValues[0] = (char *)malloc(sizeof(char) * (s.length() + 1));
      strcpy(pva->pvaData[index].monitorData[0].stringValues[0], s.c_str());
      if (pva->pvaData[index].numMonitorReadings == 0) {
        pva->pvaData[index].nonnumeric = true;
      }
    } else {
      if (pva->pvaData[index].getData[i].stringValues == NULL) {
        pva->pvaData[index].getData[i].stringValues = (char **)malloc(sizeof(char *) * 1);
      }
      pva->pvaData[index].getData[i].stringValues[0] = (char *)malloc(sizeof(char) * (s.length() + 1));
      strcpy(pva->pvaData[index].getData[i].stringValues[0], s.c_str());
      if (pva->pvaData[index].numGetReadings == 0) {
        pva->pvaData[index].nonnumeric = true;
      }
    }
    break;
  }
  default: {
    std::cerr << "ERROR: Need code to handle scalar type " << pva->pvaData[index].scalarType << std::endl;
    return (1);
  }
  }
  if (monitorMode) {
    pva->pvaData[index].numMonitorReadings = 1;
  } else {
    if (pva->limitGetReadings) {
      pva->pvaData[index].numGetReadings = 1;
    } else {
      pva->pvaData[index].numGetReadings++;
    }
  }
  return (0);
}

long ExtractNTScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      if (ExtractScalarValue(pva, index, PVFieldPtrArray[j], monitorMode)) {
        return (1);
      }
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long ExtractScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode) {
  epics::pvData::ScalarArrayConstPtr scalarArrayConstPtr;
  epics::pvData::PVScalarArrayPtr pvScalarArrayPtr;
  long i = 0;
  scalarArrayConstPtr = std::tr1::static_pointer_cast<const epics::pvData::ScalarArray>(PVFieldPtr->getField());
  pvScalarArrayPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalarArray>(PVFieldPtr);

  if (monitorMode) {
    i = 0;
    if (pva->pvaData[index].numMonitorReadings == 0) {
      pva->pvaData[index].fieldType = scalarArrayConstPtr->getType(); //should always be epics::pvData::scalar
      pva->pvaData[index].scalarType = scalarArrayConstPtr->getElementType();
      pva->pvaData[index].numMonitorElements = pvScalarArrayPtr->getLength();
    } else {
      if (pva->pvaData[index].nonnumeric) {
        for (long k = 0; k < pva->pvaData[index].numMonitorElements; k++) {
          if (pva->pvaData[index].monitorData[0].stringValues[k])
            free(pva->pvaData[index].monitorData[0].stringValues[k]);
        }
      }
    }
  } else {
    i = pva->pvaData[index].numGetReadings;
    if (pva->pvaData[index].numGetReadings == 0) {
      pva->pvaData[index].fieldType = scalarArrayConstPtr->getType(); //should always be epics::pvData::scalar
      pva->pvaData[index].scalarType = scalarArrayConstPtr->getElementType();
      pva->pvaData[index].numGetElements = pvScalarArrayPtr->getLength();
    } else if (pva->limitGetReadings) {
      i = 0;
    }
  }
  switch (pva->pvaData[index].scalarType) {
  case epics::pvData::pvDouble:
  case epics::pvData::pvFloat:
  case epics::pvData::pvLong:
  case epics::pvData::pvULong:
  case epics::pvData::pvInt:
  case epics::pvData::pvUInt:
  case epics::pvData::pvShort:
  case epics::pvData::pvUShort:
  case epics::pvData::pvByte:
  case epics::pvData::pvUByte: {
    epics::pvData::PVDoubleArray::const_svector dataVector;
    pvScalarArrayPtr->PVScalarArray::getAs<double>(dataVector);
    if (monitorMode) {
      if (pva->pvaData[index].monitorData[0].values == NULL) {
        pva->pvaData[index].monitorData[0].values = (double *)malloc(sizeof(double) * pva->pvaData[index].numMonitorElements);
        pva->pvaData[index].numeric = true;
      }
      std::copy(dataVector.begin(), dataVector.begin() + pva->pvaData[index].numMonitorElements, pva->pvaData[index].monitorData[0].values);
    } else {
      if (pva->pvaData[index].getData[i].values == NULL) {
        pva->pvaData[index].getData[i].values = (double *)malloc(sizeof(double) * pva->pvaData[index].numGetElements);
        pva->pvaData[index].numeric = true;
      }
      std::copy(dataVector.begin(), dataVector.begin() + pva->pvaData[index].numGetElements, pva->pvaData[index].getData[i].values);
    }
    break;
  }
  case epics::pvData::pvString:
  case epics::pvData::pvBoolean: {
    epics::pvData::PVStringArray::const_svector dataVector;
    pvScalarArrayPtr->PVScalarArray::getAs<std::string>(dataVector);
    if (monitorMode) {
      if (pva->pvaData[index].monitorData[0].stringValues == NULL) {
        pva->pvaData[index].monitorData[0].stringValues = (char **)malloc(sizeof(char *) * pva->pvaData[index].numMonitorElements);
        pva->pvaData[index].nonnumeric = true;
      }
      for (long k = 0; k < pva->pvaData[index].numMonitorElements; k++) {
        pva->pvaData[index].monitorData[0].stringValues[k] = (char *)malloc(sizeof(char) * (dataVector[k].length() + 1));
        strcpy(pva->pvaData[index].monitorData[0].stringValues[k], dataVector[k].c_str());
      }
    } else {
      if (pva->pvaData[index].getData[i].stringValues == NULL) {
        pva->pvaData[index].getData[i].stringValues = (char **)malloc(sizeof(char *) * pva->pvaData[index].numGetElements);
        pva->pvaData[index].nonnumeric = true;
      }
      for (long k = 0; k < pva->pvaData[index].numGetElements; k++) {
        pva->pvaData[index].getData[i].stringValues[k] = (char *)malloc(sizeof(char) * (dataVector[k].length() + 1));
        strcpy(pva->pvaData[index].getData[i].stringValues[k], dataVector[k].c_str());
      }
    }
    break;
  }
  default: {
    std::cerr << "ERROR: Need code to handle scalar array type " << pva->pvaData[index].scalarType << std::endl;
    return (1);
  }
  }
  if (monitorMode) {
    pva->pvaData[index].numMonitorReadings = 1;
  } else {
    if (pva->limitGetReadings) {
      pva->pvaData[index].numGetReadings = 1;
    } else {
      pva->pvaData[index].numGetReadings++;
    }
  }
  return (0);
}

long ExtractNTScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      if (ExtractScalarArrayValue(pva, index, PVFieldPtrArray[j], monitorMode)) {
        return (1);
      }
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long ExtractNTEnumValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode) {
  long i, j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      epics::pvData::PVStructurePtr pvStructurePtr;
      epics::pvData::PVEnumerated pvEnumerated;
      std::string s;
      pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
      pvEnumerated.attach(pvStructurePtr);
      if (monitorMode) {
        if (pva->pvaData[index].numMonitorReadings == 0) {
          pva->pvaData[index].fieldType = pvStructurePtr->getField()->getType(); //should always be epics::pvData::structure
          pva->pvaData[index].pvEnumeratedStructure = true;
          pva->pvaData[index].numMonitorElements = 1;
          pva->pvaData[index].numeric = true;
          pva->pvaData[index].nonnumeric = true;
          pva->pvaData[index].scalarType = epics::pvData::pvString;
          pva->pvaData[index].monitorData[0].values = (double *)malloc(sizeof(double));
          pva->pvaData[index].monitorData[0].stringValues = (char **)malloc(sizeof(char *) * 1);
        } else {
          if (pva->pvaData[index].monitorData[0].stringValues[0])
            free(pva->pvaData[index].monitorData[0].stringValues[0]);
        }
        pva->pvaData[index].monitorData[0].values[0] = pvEnumerated.getIndex();
        s = pvEnumerated.getChoice();
        pva->pvaData[index].monitorData[0].stringValues[0] = (char *)malloc(sizeof(char) * (s.length() + 1));
        strcpy(pva->pvaData[index].monitorData[0].stringValues[0], s.c_str());
        pva->pvaData[index].numMonitorReadings = 1;
      } else {
        i = pva->pvaData[index].numGetReadings;
        if (pva->pvaData[index].numGetReadings == 0) {
          pva->pvaData[index].fieldType = pvStructurePtr->getField()->getType(); //should always be epics::pvData::structure
          pva->pvaData[index].pvEnumeratedStructure = true;
          pva->pvaData[index].numGetElements = 1;
          pva->pvaData[index].numeric = true;
          pva->pvaData[index].nonnumeric = true;
          pva->pvaData[index].scalarType = epics::pvData::pvString;
        } else if (pva->limitGetReadings) {
          i = 0;
        }
        if (pva->pvaData[index].getData[i].values == NULL) {
          pva->pvaData[index].getData[i].values = (double *)malloc(sizeof(double));
        }
        if (pva->pvaData[index].getData[i].stringValues == NULL) {
          pva->pvaData[index].getData[i].stringValues = (char **)malloc(sizeof(char *) * 1);
        }
        pva->pvaData[index].getData[i].values[0] = pvEnumerated.getIndex();
        s = pvEnumerated.getChoice();
        pva->pvaData[index].getData[i].stringValues[0] = (char *)malloc(sizeof(char) * (s.length() + 1));
        strcpy(pva->pvaData[index].getData[i].stringValues[0], s.c_str());
        if (pva->limitGetReadings) {
          pva->pvaData[index].numGetReadings = 1;
        } else {
          pva->pvaData[index].numGetReadings++;
        }
      }
      pvEnumerated.detach();
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long ExtractStructureValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode) {
  long fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  epics::pvData::PVStructurePtr pvStructurePtr;
  pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtr);

  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  if (fieldCount > 1) {
    pvStructurePtr->dumpValue(std::cerr);
    fprintf(stderr, "Error: sub-field is not specific enough\n");
    return (1);
  }
  fieldName = PVFieldPtrArray[0]->getFieldName();
  switch (PVFieldPtrArray[0]->getField()->getType()) {
  case epics::pvData::scalar: {
    if (ExtractScalarValue(pva, index, PVFieldPtrArray[0], monitorMode)) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::scalarArray: {
    if (ExtractScalarArrayValue(pva, index, PVFieldPtrArray[0], monitorMode)) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::structure: {
    if (ExtractStructureValue(pva, index, PVFieldPtrArray[0], monitorMode)) {
      return (1);
    }
    return (0);
    break;
  }
  default: {
    std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
    return (1);
  }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long ExtractPVAValues(PVA_OVERALL *pva) {
  long i, j;
  std::string id;
  bool monitorMode = false;
  epics::pvData::PVStructurePtr pvStructurePtr;
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->isConnected[i]) {
      pvStructurePtr = pva->pvaClientGetPtr[i]->getData()->getPVStructure();
      id = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getID();
      if (id == "epics:nt/NTScalar:1.0") {
        if (ExtractNTScalarValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "epics:nt/NTScalarArray:1.0") {
        if (ExtractNTScalarArrayValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "epics:nt/NTEnum:1.0") {
        if (ExtractNTEnumValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "structure") {
        epics::pvData::PVFieldPtrArray PVFieldPtrArray;
        long fieldCount;
        PVFieldPtrArray = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getPVFields();
        fieldCount = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getNumberFields();
        if (fieldCount > 1) {
          if (PVFieldPtrArray[0]->getFieldName() != "value") {
            pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
            fprintf(stderr, "Error: sub-field is not specific enough\n");
            return (1);
          }
        }
        if (fieldCount == 0) {
          fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[i].c_str());
          return (1);
        }
        switch (PVFieldPtrArray[0]->getField()->getType()) {
        case epics::pvData::scalar: {
          if (ExtractScalarValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::scalarArray: {
          if (ExtractScalarArrayValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::structure: {
          if (ExtractStructureValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        default: {
          std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
          return (1);
        }
        }
        if (pva->includeAlarmSeverity && (fieldCount > 1)) {
          for (j = 0; j < fieldCount; j++) {
            if (PVFieldPtrArray[j]->getFieldName() == "alarm") {
              if (PVFieldPtrArray[j]->getField()->getType() == epics::pvData::structure) {
                epics::pvData::PVStructurePtr alarmStructurePtr;
                epics::pvData::PVFieldPtrArray AlarmFieldPtrArray;
                long alarmFieldCount;

                alarmStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
                alarmFieldCount = alarmStructurePtr->getStructure()->getNumberFields();
                AlarmFieldPtrArray = alarmStructurePtr->getPVFields();
                if (alarmFieldCount > 0) {
                  if (AlarmFieldPtrArray[0]->getFieldName() == "severity") {
                    epics::pvData::PVScalarPtr pvScalarPtr;
                    pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(AlarmFieldPtrArray[0]);
                    pva->pvaData[i].alarmSeverity = pvScalarPtr->getAs<int>();
                  } else {
                    pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
                    fprintf(stderr, "Error: alarm->severity field is not where it was expected to be\n");
                    return (1);
                  }
                }
              }
              break;
            }
          }
        }
      } else {
#ifdef DEBUG
        pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
#endif
        std::cerr << "Error: unrecognized structure ID (" << id << ")" << std::endl;
        return (1);
      }
    }
  }
  return (0);
}

long count_chars(char *string, char c) {
  long i = 0;
  while (*string) {
    if (*string++ == c)
      i++;
  }
  return i;
}

long PutScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr) {
  epics::pvData::PVScalarPtr pvScalarPtr;
  pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(PVFieldPtr);
  try {
    if (pva->pvaData[index].numeric) {
      pvScalarPtr->putFrom<double>(pva->pvaData[index].putData[0].values[0]);
    } else {
      pvScalarPtr->putFrom<std::string>(pva->pvaData[index].putData[0].stringValues[0]);
    }
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return (0);
}

long PutNTScalarValue(PVA_OVERALL *pva, long index) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getPVFields();
  fieldCount = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      if (PutScalarValue(pva, index, PVFieldPtrArray[j])) {
        return (1);
      }
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long PutScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr) {
  long n;
  epics::pvData::PVScalarArrayPtr pvScalarArrayPtr;
  pvScalarArrayPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalarArray>(PVFieldPtr);
  try {
    if (pva->pvaData[index].numeric) {
      epics::pvData::shared_vector<double> values(pva->pvaData[index].numPutElements);
      for (n = 0; n < pva->pvaData[index].numPutElements; n++) {
        values[n] = pva->pvaData[index].putData[0].values[n];
      }
      pvScalarArrayPtr->setLength(pva->pvaData[index].numPutElements);
      pvScalarArrayPtr->putFrom(freeze(values));
    } else {
      epics::pvData::shared_vector<std::string> values(pva->pvaData[index].numPutElements);
      for (n = 0; n < pva->pvaData[index].numPutElements; n++) {
        values[n] = pva->pvaData[index].putData[0].stringValues[n];
      }
      pvScalarArrayPtr->setLength(pva->pvaData[index].numPutElements);
      pvScalarArrayPtr->putFrom(freeze(values));
    }
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return (0);
}

long PutNTScalarArrayValue(PVA_OVERALL *pva, long index) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getPVFields();
  fieldCount = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      if (PutScalarArrayValue(pva, index, PVFieldPtrArray[j])) {
        return (1);
      }
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long PutNTEnumValue(PVA_OVERALL *pva, long index) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getPVFields();
  fieldCount = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      epics::pvData::PVStructurePtr pvStructurePtr;
      epics::pvData::PVEnumerated pvEnumerated;
      bool result;
      pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
      result = pvEnumerated.attach(pvStructurePtr);
      if (result) {
        try {
          int enumindex, numChoices;
          epics::pvData::PVStringArray::const_svector choices;
          numChoices = pvEnumerated.getNumberChoices();

          if (pva->pvaData[index].putData[0].stringValues != NULL) {
            enumindex = -1;
            choices = pvEnumerated.getChoices();
            for (size_t i = 0; i < choices.size(); i++) {
              if (pva->pvaData[index].putData[0].stringValues[0] == choices[i]) {
                enumindex = i;
              }
            }
            if (enumindex == -1) {
              if (sscanf(pva->pvaData[index].putData[0].stringValues[0], "%d", &enumindex) != 1) {
                fprintf(stderr, "error: value (%s) for %s is not a valid option.\n", pva->pvaData[index].putData[0].stringValues[0], pva->pvaChannelNames[index].c_str());
                return (1);
              }
              if ((enumindex < 0) || (enumindex >= numChoices)) {
                fprintf(stderr, "error: value (%s) for %s is out of range.\n", pva->pvaData[index].putData[0].stringValues[0], pva->pvaChannelNames[index].c_str());
                return (1);
              }
            }
          } else {
            enumindex = pva->pvaData[index].putData[0].values[0];
            if ((enumindex < 0) || (enumindex >= numChoices)) {
              fprintf(stderr, "error: value (%s) for %s is out of range.\n", pva->pvaData[index].putData[0].stringValues[0], pva->pvaChannelNames[index].c_str());
              return (1);
            }
          }
          pvEnumerated.setIndex(enumindex);
          pvEnumerated.setIndex(enumindex);
          pvEnumerated.detach();
        } catch (std::exception &e) {
          std::cerr << "Error: " << e.what() << "\n";
          return 1;
        }
        return (0);
      } else {
        std::cerr << "Error: Need code to handle a non-enumerated structure" << std::endl;
        return (1);
      }
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long PutStructureValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr) {
  long fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  epics::pvData::PVStructurePtr pvStructurePtr;
  pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtr);

  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  if (fieldCount > 1) {
    pvStructurePtr->dumpValue(std::cerr);
    fprintf(stderr, "Error: sub-field is not specific enough\n");
    return (1);
  }
  fieldName = PVFieldPtrArray[0]->getFieldName();
  switch (PVFieldPtrArray[0]->getField()->getType()) {
  case epics::pvData::scalar: {
    if (PutScalarValue(pva, index, PVFieldPtrArray[0])) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::scalarArray: {
    if (PutScalarArrayValue(pva, index, PVFieldPtrArray[0])) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::structure: {
    if (PutStructureValue(pva, index, PVFieldPtrArray[0])) {
      return (1);
    }
    return (0);
    break;
  }
  default: {
    std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
    return (1);
  }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long PrepPut(PVA_OVERALL *pva, long index, double value) {
  pva->pvaData[index].numPutElements = 1;
  if (pva->pvaData[index].numeric) {
    if (pva->pvaData[index].putData[0].values == NULL) {
      pva->pvaData[index].putData[0].values = (double *)malloc(sizeof(double));
    }
    pva->pvaData[index].putData[0].values[0] = value;
  } else {
    char buffer[100];
    if (pva->pvaData[index].putData[0].stringValues == NULL) {
      pva->pvaData[index].putData[0].stringValues = (char **)malloc(sizeof(char *));
    } else {
      //if (pva->pvaData[index].putData[0].stringValues[0])
      //free(pva->pvaData[index].putData[0].stringValues[0]);
    }
    snprintf(buffer, sizeof(buffer), "%lf", value);
    pva->pvaData[index].putData[0].stringValues[0] = (char *)malloc(sizeof(char) * (strlen(buffer) + 1));
    strcpy(pva->pvaData[index].putData[0].stringValues[0], buffer);
  }
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, double *value, long length) {
  int i;

  if (pva->pvaData[index].numPutElements > 0) {
    if (pva->pvaData[index].numeric && (pva->pvaData[index].pvEnumeratedStructure == false)) {
      if (pva->pvaData[index].numPutElements != length) {
        if (pva->pvaData[index].putData[0].values) {
          free(pva->pvaData[index].putData[0].values);
          pva->pvaData[index].putData[0].values = NULL;
        }
      }
    } else {
      if (pva->pvaData[index].putData[0].stringValues) {
        for (i = 0; i < pva->pvaData[index].numPutElements; i++) {
          if (pva->pvaData[index].putData[0].stringValues[i]) {
            free(pva->pvaData[index].putData[0].stringValues[i]);
          }
        }
        if (pva->pvaData[index].numPutElements != length) {
          free(pva->pvaData[index].putData[0].stringValues);
          pva->pvaData[index].putData[0].stringValues = NULL;
        }
      }
    }
  }

  pva->pvaData[index].numPutElements = length;
  if (pva->pvaData[index].numeric) {
    if (pva->pvaData[index].putData[0].values == NULL) {
      pva->pvaData[index].putData[0].values = (double *)malloc(sizeof(double) * length);
    }
    for (i = 0; i < length; i++) {
      pva->pvaData[index].putData[0].values[i] = value[i];
    }
  } else {
    char buffer[100];
    if (pva->pvaData[index].putData[0].stringValues == NULL) {
      pva->pvaData[index].putData[0].stringValues = (char **)malloc(sizeof(char *) * length);
    }
    for (i = 0; i < length; i++) {
      snprintf(buffer, sizeof(buffer), "%lf", value[i]);
      pva->pvaData[index].putData[0].stringValues[i] = (char *)malloc(sizeof(char) * (strlen(buffer) + 1));
      strcpy(pva->pvaData[index].putData[0].stringValues[i], buffer);
    }
  }
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, int64_t value) {
  pva->pvaData[index].numPutElements = 1;
  if (pva->pvaData[index].numeric) {
    if (pva->pvaData[index].putData[0].values == NULL) {
      pva->pvaData[index].putData[0].values = (double *)malloc(sizeof(double));
    }
    pva->pvaData[index].putData[0].values[0] = (double)value;
  } else {
    char buffer[100];
    if (pva->pvaData[index].putData[0].stringValues == NULL) {
      pva->pvaData[index].putData[0].stringValues = (char **)malloc(sizeof(char *));
    } else {
      //if (pva->pvaData[index].putData[0].stringValues[0])
      //free(pva->pvaData[index].putData[0].stringValues[0]);
    }
    snprintf(buffer, sizeof(buffer), "%lld", value);
    pva->pvaData[index].putData[0].stringValues[0] = (char *)malloc(sizeof(char) * (strlen(buffer) + 1));
    strcpy(pva->pvaData[index].putData[0].stringValues[0], buffer);
  }
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, int64_t *value, long length) {
  int i;

  if (pva->pvaData[index].numPutElements > 0) {
    if (pva->pvaData[index].numeric && (pva->pvaData[index].pvEnumeratedStructure == false)) {
      if (pva->pvaData[index].numPutElements != length) {
        if (pva->pvaData[index].putData[0].values) {
          free(pva->pvaData[index].putData[0].values);
          pva->pvaData[index].putData[0].values = NULL;
        }
      }
    } else {
      if (pva->pvaData[index].putData[0].stringValues) {
        for (i = 0; i < pva->pvaData[index].numPutElements; i++) {
          if (pva->pvaData[index].putData[0].stringValues[i]) {
            free(pva->pvaData[index].putData[0].stringValues[i]);
          }
        }
        if (pva->pvaData[index].numPutElements != length) {
          free(pva->pvaData[index].putData[0].stringValues);
          pva->pvaData[index].putData[0].stringValues = NULL;
        }
      }
    }
  }

  pva->pvaData[index].numPutElements = length;
  if (pva->pvaData[index].numeric) {
    if (pva->pvaData[index].putData[0].values == NULL) {
      pva->pvaData[index].putData[0].values = (double *)malloc(sizeof(double) * length);
    }
    for (i = 0; i < length; i++) {
      pva->pvaData[index].putData[0].values[i] = (double)value[i];
    }
  } else {
    char buffer[100];
    if (pva->pvaData[index].putData[0].stringValues == NULL) {
      pva->pvaData[index].putData[0].stringValues = (char **)malloc(sizeof(char *) * length);
    }
    for (i = 0; i < length; i++) {
      snprintf(buffer, sizeof(buffer), "%lld", value[i]);
      pva->pvaData[index].putData[0].stringValues[i] = (char *)malloc(sizeof(char) * (strlen(buffer) + 1));
      strcpy(pva->pvaData[index].putData[0].stringValues[i], buffer);
    }
  }
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, char *value) {
  pva->pvaData[index].numPutElements = 1;
  if (pva->pvaData[index].numeric && (pva->pvaData[index].pvEnumeratedStructure == false)) {
    if (pva->pvaData[index].putData[0].values == NULL) {
      pva->pvaData[index].putData[0].values = (double *)malloc(sizeof(double));
    }
    if (sscanf(value, "%le", &(pva->pvaData[index].putData[0].values[0])) != 1) {
      fprintf(stderr, "error: value (%s) for %s is not numerical\n", value, pva->pvaChannelNames[index].c_str());
      return (1);
    }
  } else {
    if (pva->pvaData[index].putData[0].stringValues == NULL) {
      pva->pvaData[index].putData[0].stringValues = (char **)malloc(sizeof(char *));
    } else {
      //if (pva->pvaData[index].putData[0].stringValues[0])
      //free(pva->pvaData[index].putData[0].stringValues[0]);
    }
    pva->pvaData[index].putData[0].stringValues[0] = (char *)malloc(sizeof(char) * (strlen(value) + 1));
    strcpy(pva->pvaData[index].putData[0].stringValues[0], value);
  }
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, char **value, long length) {
  int i;

  if (pva->pvaData[index].numPutElements > 0) {
    if (pva->pvaData[index].numeric && (pva->pvaData[index].pvEnumeratedStructure == false)) {
      if (pva->pvaData[index].numPutElements != length) {
        if (pva->pvaData[index].putData[0].values) {
          free(pva->pvaData[index].putData[0].values);
          pva->pvaData[index].putData[0].values = NULL;
        }
      }
    } else {
      if (pva->pvaData[index].putData[0].stringValues) {
        for (i = 0; i < pva->pvaData[index].numPutElements; i++) {
          if (pva->pvaData[index].putData[0].stringValues[i]) {
            free(pva->pvaData[index].putData[0].stringValues[i]);
          }
        }
        if (pva->pvaData[index].numPutElements != length) {
          free(pva->pvaData[index].putData[0].stringValues);
          pva->pvaData[index].putData[0].stringValues = NULL;
        }
      }
    }
  }

  pva->pvaData[index].numPutElements = length;
  if (pva->pvaData[index].numeric && (pva->pvaData[index].pvEnumeratedStructure == false)) {
    if (pva->pvaData[index].putData[0].values == NULL) {
      pva->pvaData[index].putData[0].values = (double *)malloc(sizeof(double) * length);
    }
    for (i = 0; i < length; i++) {
      if (sscanf(value[i], "%le", &(pva->pvaData[index].putData[0].values[i])) != 1) {
        fprintf(stderr, "error: value (%s) for %s is not numerical\n", value[i], pva->pvaChannelNames[index].c_str());
        return (1);
      }
    }
  } else {
    if (pva->pvaData[index].putData[0].stringValues == NULL) {
      pva->pvaData[index].putData[0].stringValues = (char **)malloc(sizeof(char *) * length);
    }
    for (i = 0; i < length; i++) {
      pva->pvaData[index].putData[0].stringValues[i] = (char *)malloc(sizeof(char) * (strlen(value[i]) + 1));
      strcpy(pva->pvaData[index].putData[0].stringValues[i], value[i]);
    }
  }
  return (0);
}

/*
  Put the values from the pva structure and send them to the PVs. See cavput.cc for an example on how to populate this pva structure.
*/
long PutPVAValues(PVA_OVERALL *pva) {
  long i, j, num = 0;
  std::string id;
  epics::pvData::Status status;
  epics::pvaClient::PvaClientChannelArray pvaClientChannelArray;

  pva->isInternalConnected = pva->pvaClientMultiChannelPtr[0]->getIsConnected();
  pvaClientChannelArray = pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray();
  for (i = 1; i < pva->numMultiChannels; i++) {
    epics::pvData::shared_vector<epics::pvData::boolean> isConnected;
    epics::pvaClient::PvaClientChannelArray pvaClientChannelArrayAdd;
    isConnected = pva->pvaClientMultiChannelPtr[i]->getIsConnected();
    std::copy(isConnected.begin(), isConnected.end(), std::back_inserter(pva->isInternalConnected));
    pvaClientChannelArrayAdd = pva->pvaClientMultiChannelPtr[i]->getPvaClientChannelArray();
    std::copy(pvaClientChannelArrayAdd.begin(), pvaClientChannelArrayAdd.end(), std::back_inserter(pvaClientChannelArray));
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    pva->isConnected[i] = pva->isInternalConnected[pva->pvaData[i].L2Ptr];
    if (pva->isConnected[i] == false) {
      if (pva->pvaData[i].numPutElements > 0) {
        fprintf(stderr, "Error: Can't put value to %s. Not connected.\n", pva->pvaChannelNames[i].c_str());
        return (1);
      }
      num++;
    } else if ((pva->pvaData[i].numPutElements > 0) && (pva->pvaData[i].havePutPtr == false)) {
      pva->pvaClientPutPtr[i] = pvaClientChannelArray[pva->pvaData[i].L2Ptr]->createPut(pva->pvaChannelNamesSub[i]);
      pva->pvaData[i].havePutPtr = true;
      if (pva->useGetCallbacks) {
        pva->pvaClientPutPtr[i]->setRequester((epics::pvaClient::PvaClientPutRequesterPtr)pva->putReqPtr);
      }
    }
  }
  pva->numNotConnected = num;

  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].numPutElements > 0) {
#ifdef DEBUG
      pva->pvaClientPutPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
#endif
      //get the id string from the GetPtr instead of the PutPtr
      id = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getID();
      if (id == "epics:nt/NTScalar:1.0") {
        if (PutNTScalarValue(pva, i)) {
          return (1);
        }
      } else if (id == "epics:nt/NTScalarArray:1.0") {
        if (PutNTScalarArrayValue(pva, i)) {
          return (1);
        }
      } else if (id == "epics:nt/NTEnum:1.0") {
        if (PutNTEnumValue(pva, i)) {
          return (1);
        }
      } else if (id == "structure") {
        epics::pvData::PVFieldPtrArray PVFieldPtrArray;
        long fieldCount;
        PVFieldPtrArray = pva->pvaClientPutPtr[i]->getData()->getPVStructure()->getPVFields();
        fieldCount = pva->pvaClientPutPtr[i]->getData()->getPVStructure()->getStructure()->getNumberFields();
        if (fieldCount > 1) {
          if (PVFieldPtrArray[0]->getFieldName() != "value") {
            pva->pvaClientPutPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
            fprintf(stderr, "Error: sub-field is not specific enough\n");
            return (1);
          }
        }
        if (fieldCount == 0) {
          fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[i].c_str());
          return (1);
        }
        switch (PVFieldPtrArray[0]->getField()->getType()) {
        case epics::pvData::scalar: {
          if (PutScalarValue(pva, i, PVFieldPtrArray[0])) {
            return (1);
          }
          break;
        }
        case epics::pvData::scalarArray: {
          if (PutScalarArrayValue(pva, i, PVFieldPtrArray[0])) {
            return (1);
          }
          break;
        }
        case epics::pvData::structure: {
          if (PutStructureValue(pva, i, PVFieldPtrArray[0])) {
            return (1);
          }
          break;
        }
        default: {
          std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
          return (1);
        }
        }
      } else {
        std::cerr << "Error: unrecognized structure ID (" << id << ")" << std::endl;
        return (1);
      }
    }
  }

  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].numPutElements > 0) {
      pva->pvaClientPutPtr[i]->issuePut();
    }
  }

  if (pva->useGetCallbacks == false) {
    for (i = 0; i < pva->numPVs; i++) {
      if (pva->pvaData[i].skip == true) {
        continue;
      }
      if (pva->pvaData[i].numPutElements > 0) {
        status = pva->pvaClientPutPtr[i]->waitPut();
        if (!status.isSuccess()) {
          fprintf(stderr, "error: %s did not respond to the \"put\" request\n", pva->pvaChannelNames[i].c_str());
          return (1);
        }
      }
    }
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].numPutElements > 0) {
      if (pva->pvaData[i].putData[0].stringValues != NULL) {
        for (j = 0; j < pva->pvaData[i].numPutElements; j++) {
          free(pva->pvaData[i].putData[0].stringValues[j]);
        }
      }
      pva->pvaData[i].numPutElements = 0;
    }
  }
  return (0);
}

/*
  Start monitoring the PVs. Use the PollMonitoredPVA values to identify if an event has occured.
  FIX THIS There is a unique problem of what to do with PVs that are not connected when the program starts but become connected later
*/
long MonitorPVAValues(PVA_OVERALL *pva) {
  long i, num;
  epics::pvData::Status status;
  epics::pvaClient::PvaClientChannelArray pvaClientChannelArray;

  if (pva == NULL) {
    return (0);
  }
  num = 0;
  pva->isInternalConnected = pva->pvaClientMultiChannelPtr[0]->getIsConnected();
  pvaClientChannelArray = pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray();
  for (i = 1; i < pva->numMultiChannels; i++) {
    epics::pvData::shared_vector<epics::pvData::boolean> isConnected;
    epics::pvaClient::PvaClientChannelArray pvaClientChannelArrayAdd;
    isConnected = pva->pvaClientMultiChannelPtr[i]->getIsConnected();
    std::copy(isConnected.begin(), isConnected.end(), std::back_inserter(pva->isInternalConnected));
    pvaClientChannelArrayAdd = pva->pvaClientMultiChannelPtr[i]->getPvaClientChannelArray();
    std::copy(pvaClientChannelArrayAdd.begin(), pvaClientChannelArrayAdd.end(), std::back_inserter(pvaClientChannelArray));
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    pva->isConnected[i] = pva->isInternalConnected[pva->pvaData[i].L2Ptr];
    if (pva->isConnected[i]) {
      if (pva->pvaData[i].haveMonitorPtr == false) {
        pva->pvaClientMonitorPtr[i] = pvaClientChannelArray[pva->pvaData[i].L2Ptr]->createMonitor(pva->pvaChannelNamesSub[i]);
        pva->pvaData[i].haveMonitorPtr = true;
        if (pva->useMonitorCallbacks) {
          pva->pvaClientMonitorPtr[i]->setRequester((epics::pvaClient::PvaClientMonitorRequesterPtr)pva->monitorReqPtr);
        }
        pva->pvaClientMonitorPtr[i]->issueConnect();
        status = pva->pvaClientMonitorPtr[i]->waitConnect();
        if (!status.isSuccess()) {
          fprintf(stderr, "error: %s did not respond to the \"waitConnect\" request\n", pva->pvaChannelNames[i].c_str());
          return (1);
        }
        pva->pvaClientMonitorPtr[i]->start();
      }
    } else {
      num++;
    }
  }
  pva->numNotConnected = num;
  return (0);
}

void PausePVAMonitoring(PVA_OVERALL **pva, long count) {
  long i;
  for (i = 0; i < count; i++) {
    PausePVAMonitoring(pva[i]);
  }
}

void PausePVAMonitoring(PVA_OVERALL *pva) {
  long i;
  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->isConnected[i]) {
      pva->pvaClientMonitorPtr[i]->stop();
    }
  }
}

void ResumePVAMonitoring(PVA_OVERALL **pva, long count) {
  long i;
  for (i = 0; i < count; i++) {
    ResumePVAMonitoring(pva[i]);
  }
}

void ResumePVAMonitoring(PVA_OVERALL *pva) {
  long i;
  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->isConnected[i]) {
      pva->pvaClientMonitorPtr[i]->start();
    }
  }
}

/*
  Check to see if an event has occured on a monitored PV and if so, place the data into the pva structure.
  Returns number of events found or -1 for error
*/
long PollMonitoredPVA(PVA_OVERALL *pva) {
  long result;
  PVA_OVERALL **pvaArray;
  pvaArray = (PVA_OVERALL **)malloc(sizeof(PVA_OVERALL *));
  pvaArray[0] = pva;
  result = PollMonitoredPVA(pvaArray, 1);
  free(pvaArray);
  return (result);
}

/* Returns number of events found or -1 for error
 */
long PollMonitoredPVA(PVA_OVERALL **pva, long count) {
  long result = 0, i, n;
  std::string id;
  bool monitorMode = true, connectionChange = false;
  ;
  epics::pvData::PVStructurePtr pvStructurePtr;

  for (n = 0; n < count; n++) {
    if (pva[n] != NULL) {
      //A PV which was initially unconnected may have connected and we need to start monitoring it
      for (i = 0; i < pva[n]->numMultiChannels; i++) {
        if (pva[n]->pvaClientMultiChannelPtr[i]->connectionChange()) {
          connectionChange = true;
        }
      }
      if (connectionChange) {
        if (MonitorPVAValues(pva[n]) != 0) {
          return (1);
        }
        connectionChange = false;
      }

      for (long i = 0; i < pva[n]->numPVs; i++) {
        if (pva[n]->pvaData[i].skip == true) {
          continue;
        }
        if (pva[n]->isConnected[i]) {
          if (pva[n]->pvaClientMonitorPtr[i]->poll()) {
            result++;
            pvStructurePtr = pva[n]->pvaClientMonitorPtr[i]->getData()->getPVStructure();
            id = pvStructurePtr->getStructure()->getID();
            if (id == "epics:nt/NTScalar:1.0") {
              if (ExtractNTScalarValue(pva[n], i, pvStructurePtr, monitorMode)) {
                return (-1);
              }
            } else if (id == "epics:nt/NTScalarArray:1.0") {
              if (ExtractNTScalarArrayValue(pva[n], i, pvStructurePtr, monitorMode)) {
                return (-1);
              }
            } else if (id == "epics:nt/NTEnum:1.0") {
              if (ExtractNTEnumValue(pva[n], i, pvStructurePtr, monitorMode)) {
                return (-1);
              }
            } else if (id == "structure") {
              epics::pvData::PVFieldPtrArray PVFieldPtrArray;
              long fieldCount;
              PVFieldPtrArray = pvStructurePtr->getPVFields();
              fieldCount = pvStructurePtr->getStructure()->getNumberFields();
              if (fieldCount > 1) {
                if (PVFieldPtrArray[0]->getFieldName() != "value") {
                  pvStructurePtr->dumpValue(std::cerr);
                  fprintf(stderr, "Error: sub-field is not specific enough\n");
                  return (-1);
                }
              }
              switch (PVFieldPtrArray[0]->getField()->getType()) {
              case epics::pvData::scalar: {
                if (ExtractScalarValue(pva[n], i, PVFieldPtrArray[0], monitorMode)) {
                  return (-1);
                }
                break;
              }
              case epics::pvData::scalarArray: {
                if (ExtractScalarArrayValue(pva[n], i, PVFieldPtrArray[0], monitorMode)) {
                  return (-1);
                }
                break;
              }
              case epics::pvData::structure: {
                if (ExtractStructureValue(pva[n], i, PVFieldPtrArray[0], monitorMode)) {
                  return (-1);
                }
                break;
              }
              default: {
                std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
                return (-1);
              }
              }
            }
            pva[n]->pvaClientMonitorPtr[i]->releaseEvent();
          }
        }
      }
    }
  }
  return result;
}

/*
  Wait for an event on a monitored PV and place the data into the pva structure.
  result: -1 no event, 0 event, 1 error
*/
long WaitEventMonitoredPVA(PVA_OVERALL *pva, long index, double secondsToWait) {
  long result = -1;
  std::string id;
  bool monitorMode = true;
  epics::pvData::PVStructurePtr pvStructurePtr;
  for (long i = index; i <= index; i++) {
    if (pva->isConnected[i]) {
      if (pva->pvaClientMonitorPtr[i]->waitEvent(secondsToWait)) {
        pvStructurePtr = pva->pvaClientMonitorPtr[i]->getData()->getPVStructure();
        id = pvStructurePtr->getStructure()->getID();
        if (id == "epics:nt/NTScalar:1.0") {
          if (ExtractNTScalarValue(pva, i, pvStructurePtr, monitorMode)) {
            return (1);
          }
        } else if (id == "epics:nt/NTScalarArray:1.0") {
          if (ExtractNTScalarArrayValue(pva, i, pvStructurePtr, monitorMode)) {
            return (1);
          }
        } else if (id == "epics:nt/NTEnum:1.0") {
          if (ExtractNTEnumValue(pva, i, pvStructurePtr, monitorMode)) {
            return (1);
          }
        } else if (id == "structure") {
          epics::pvData::PVFieldPtrArray PVFieldPtrArray;
          long fieldCount;
          PVFieldPtrArray = pvStructurePtr->getPVFields();
          fieldCount = pvStructurePtr->getStructure()->getNumberFields();
          if (fieldCount > 1) {
            if (PVFieldPtrArray[0]->getFieldName() != "value") {
              pvStructurePtr->dumpValue(std::cerr);
              fprintf(stderr, "Error: sub-field is not specific enough\n");
              return (1);
            }
          }
          switch (PVFieldPtrArray[0]->getField()->getType()) {
          case epics::pvData::scalar: {
            if (ExtractScalarValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
              return (1);
            }
            break;
          }
          case epics::pvData::scalarArray: {
            if (ExtractScalarArrayValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
              return (1);
            }
            break;
          }
          case epics::pvData::structure: {
            if (ExtractStructureValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
              return (1);
            }
            break;
          }
          default: {
            std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
            return (1);
          }
          }
        }
        pva->pvaClientMonitorPtr[i]->releaseEvent();
        result = 0;
      }
    }
  }
  return result;
}

long ExtractPVAUnits(PVA_OVERALL *pva) {
  long i, j, n, fieldCount, fieldCount2;
  epics::pvData::PVStructurePtr pvStructurePtr;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray, PVFieldPtrArray2;
  epics::pvData::PVScalarPtr pvScalarPtr;
  std::string s;
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    pva->pvaData[i].units = NULL;
    if (pva->isConnected[i]) {
      PVFieldPtrArray = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getPVFields();
      fieldCount = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getNumberFields();
      for (j = 0; j < fieldCount; j++) {
        if (PVFieldPtrArray[j]->getFieldName() == "display") {
          pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
          PVFieldPtrArray2 = pvStructurePtr->getPVFields();
          fieldCount2 = pvStructurePtr->getStructure()->getNumberFields();
          for (n = 0; n < fieldCount2; n++) {
            if (PVFieldPtrArray2[n]->getFieldName() == "units") {
              pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(PVFieldPtrArray2[n]);
              s = pvScalarPtr->getAs<std::string>();
              pva->pvaData[i].units = (char *)malloc(sizeof(char) * (s.length() + 1));
              strcpy(pva->pvaData[i].units, s.c_str());
              break;
            }
          }
          break;
        }
      }
    }
  }
  return (0);
}

std::string GetProviderName(PVA_OVERALL *pva, long index) {
  if (pva->isConnected[index] == false)
    return "unknown";
  return pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray()[pva->pvaData[index].L2Ptr]->getChannel()->getProvider()->getProviderName();
}
std::string GetRemoteAddress(PVA_OVERALL *pva, long index) {
  if (pva->isConnected[index] == false)
    return "unknown";
  return pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray()[pva->pvaData[index].L2Ptr]->getChannel()->getRemoteAddress();
}
bool HaveReadAccess(PVA_OVERALL *pva, long index) {
  epics::pvData::PVStructurePtr pvStructurePtr;
  size_t fieldCount;
  uint32_t value;

  if (pva->isConnected[index]) {
    pvStructurePtr = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
    fieldCount = pvStructurePtr->getStructure()->getNumberFields();
    if (fieldCount > 0) {
      value = pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray()[pva->pvaData[index].L2Ptr]->getChannel()->getAccessRights(pvStructurePtr->getPVFields()[0]);
      if ((value == 1) || (value == 2))
        return true;
    }
  }
  return false;
/*
  std::string provider;
  epics::pvAccess::ca::CAChannel::shared_pointer caChan;
  if (pva->isConnected[index] == false)
    return false;
  provider = GetProviderName(pva, index);
  if (provider == "ca") {
    caChan = std::dynamic_pointer_cast<epics::pvAccess::ca::CAChannel>(pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray()[pva->pvaData[index].L2Ptr]->getChannel());
    if (ca_read_access(caChan->getChannelID()) == 0)
      return false;
    else
      return true;
  } else {
    return true;
  }
*/
}
bool HaveWriteAccess(PVA_OVERALL *pva, long index) {
  epics::pvData::PVStructurePtr pvStructurePtr;
  size_t fieldCount;
  uint32_t value;

  if (pva->isConnected[index]) {
    pvStructurePtr = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
    fieldCount = pvStructurePtr->getStructure()->getNumberFields();
    if (fieldCount > 0) {
      value = pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray()[pva->pvaData[index].L2Ptr]->getChannel()->getAccessRights(pvStructurePtr->getPVFields()[0]);
      if (value == 2)
        return true;
    }
  }
  return false;
    
/*
  {
    std::string provider;
    epics::pvAccess::ca::CAChannel::shared_pointer caChan;
    provider = GetProviderName(pva, index);
    if (provider == "ca") {
      caChan = std::dynamic_pointer_cast<epics::pvAccess::ca::CAChannel>(pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray()[pva->pvaData[index].L2Ptr]->getChannel());
      if (ca_write_access(caChan->getChannelID()) == 0)
        return false;
      else
        return true;
    } else {
      return true;
    }
  }
*/
}
std::string GetStructureID(PVA_OVERALL *pva, long index) {
  if (pva->isConnected[index] == false)
    return "unknown";
  return pva->pvaClientGetPtr[index]->getData()->getPVStructure()->getStructure()->getID();
}
std::string GetFieldType(PVA_OVERALL *pva, long index) {
  std::string id;
  epics::pvData::PVStructurePtr pvStructurePtr;
  size_t fieldCount;
  if (pva->isConnected[index] == false)
    return "unknown";
  pvStructurePtr = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  id = pvStructurePtr->getStructure()->getID();
  if (id == "epics:nt/NTEnum:1.0") {
    return "ENUM structure";
  } else if (id == "structure") {
    if (fieldCount > 1) {
      if (pvStructurePtr->getPVFields()[0]->getFieldName() != "value") {
        pvStructurePtr->dumpValue(std::cerr);
        fprintf(stderr, "Error: sub-field is not specific enough\n");
        return "unknown";
      }
    } else if (fieldCount == 0) {
      fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[index].c_str());
      return "unknown";
    }
    return epics::pvData::TypeFunc::name(pvStructurePtr->getPVFields()[0]->getField()->getType());
  } else {
    std::cerr << "ERROR: Need code to handle " << id << std::endl;
    return "unknown";
  }
}
bool IsEnumFieldType(PVA_OVERALL *pva, long index) {
  if (pva->isConnected[index] == false)
    return false;
  if (pva->pvaClientGetPtr[index]->getData()->getPVStructure()->getStructure()->getID() == "epics:nt/NTEnum:1.0")
    return true;
  else
    return false;
}
uint32_t GetElementCount(PVA_OVERALL *pva, long index) {
  std::string id;
  epics::pvData::PVStructurePtr pvStructurePtr;
  size_t fieldCount;
  if (pva->isConnected[index] == false)
    return 0;
  pvStructurePtr = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  id = pvStructurePtr->getStructure()->getID();
  if (id == "epics:nt/NTEnum:1.0") {
    return 1;
  } else if (id == "structure") {
    if (fieldCount > 1) {
      if (pvStructurePtr->getPVFields()[0]->getFieldName() != "value") {
        pvStructurePtr->dumpValue(std::cerr);
        fprintf(stderr, "Error: sub-field is not specific enough\n");
        return 0;
      }
    } else if (fieldCount == 0) {
      fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[index].c_str());
      return 0;
    }
    switch (pvStructurePtr->getPVFields()[0]->getField()->getType()) {
    case epics::pvData::scalar: {
      return 1;
    }
    case epics::pvData::scalarArray: {
      return std::tr1::static_pointer_cast<const epics::pvData::PVScalarArray>(pvStructurePtr->getPVFields()[0])->getLength();
    }
    default: {
      std::cerr << "ERROR: Need code to handle " << pvStructurePtr->getPVFields()[0]->getField()->getType() << std::endl;
      return 0;
    }
    }
  } else {
    std::cerr << "ERROR: Need code to handle " << id << std::endl;
    return 0;
  }
}
std::string GetNativeDataType(PVA_OVERALL *pva, long index) {
  std::string id;
  epics::pvData::PVStructurePtr pvStructurePtr;
  size_t fieldCount;
  if (pva->isConnected[index] == false)
    return "unknown";
  pvStructurePtr = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  id = pvStructurePtr->getStructure()->getID();
  if (id == "epics:nt/NTEnum:1.0") {
    return "string";
  } else if (id == "structure") {
    if (fieldCount > 1) {
      if (pvStructurePtr->getPVFields()[0]->getFieldName() != "value") {
        pvStructurePtr->dumpValue(std::cerr);
        fprintf(stderr, "Error: sub-field is not specific enough\n");
        return "unknown";
      }
    } else if (fieldCount == 0) {
      fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[index].c_str());
      return "unknown";
    }
    switch (pvStructurePtr->getPVFields()[0]->getField()->getType()) {
    case epics::pvData::scalar: {
      return epics::pvData::ScalarTypeFunc::name(std::tr1::static_pointer_cast<const epics::pvData::Scalar>(pvStructurePtr->getPVFields()[0]->getField())->getScalarType());
    }
    case epics::pvData::scalarArray: {
      return epics::pvData::ScalarTypeFunc::name(std::tr1::static_pointer_cast<const epics::pvData::ScalarArray>(pvStructurePtr->getPVFields()[0]->getField())->getElementType());
    }
    default: {
      std::cerr << "ERROR: Need code to handle " << pvStructurePtr->getPVFields()[0]->getField()->getType() << std::endl;
      return "unknown";
    }
    }
  } else {
    std::cerr << "ERROR: Need code to handle " << id << std::endl;
    return "unknown";
  }
}
std::string GetUnits(PVA_OVERALL *pva, long index) {
  if (pva->pvaData[index].units)
    return pva->pvaData[index].units;
  else
    return "";
}
uint32_t GetEnumChoices(PVA_OVERALL *pva, long index, char ***enumChoices) {
  uint32_t count = 0;
  std::string id;
  epics::pvData::PVStructurePtr pvStructurePtr;
  size_t fieldCount, n, m;
  if (pva->isConnected[index] == false)
    return 0;
  pvStructurePtr = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  id = pvStructurePtr->getStructure()->getID();
  if (id == "epics:nt/NTEnum:1.0") {
    epics::pvData::PVStringArray::const_svector choices;
    for (n = 0; n < fieldCount; n++) {
      if (pvStructurePtr->getPVFields()[n]->getFieldName() == "value") {
        epics::pvData::PVEnumerated pvEnumerated;
        pvEnumerated.attach(std::tr1::static_pointer_cast<epics::pvData::PVStructure>(pvStructurePtr->getPVFields()[n]));
        choices = pvEnumerated.getChoices();
        count = choices.size();
        *enumChoices = (char**)malloc(sizeof(char*) * count);
        for (m = 0; m < choices.size(); m++) {
          std::string val;
          val = "{" + choices[m] + "}";
          (*enumChoices)[m] = (char *)malloc(sizeof(char) * strlen(val.c_str()) + 1);
          strcpy((*enumChoices)[m], val.c_str());
        }
        break;
      }
    }
    return count;
  } else {
    return 0;
  }
}
