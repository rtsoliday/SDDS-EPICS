/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "sddspcasServer.h"
#include "gddApps.h"

#define myPI 3.14159265358979323846

//
// SUN C++ does not have RAND_MAX yet
//
#if !defined(RAND_MAX)
//
// Apparently SUN C++ is using the SYSV version of rand
//
#  if 0
#    define RAND_MAX INT_MAX
#  else
#    define RAND_MAX SHRT_MAX
#  endif
#endif

//
// exScalarPV::scan
//
void exScalarPV::scan() {
  caStatus status;
  double radians;
  smartGDDPointer pDD;
  double limit;
  int gddStatus;

  //
  // update current time (so we are not required to do
  // this every time that we write the PV which impacts
  // throughput under sunos4 because gettimeofday() is
  // slow)
  //
  this->currentTime = epicsTime::getCurrent();

  if (this->info.getType() == aitEnumString) {
    pDD = new gddScalar(gddAppType_value, aitEnumString);
  } else if (this->info.getType() == aitEnumUint8) {
    pDD = new gddScalar(gddAppType_value, aitEnumUint8);
  } else if (this->info.getType() == aitEnumInt8) {
    pDD = new gddScalar(gddAppType_value, aitEnumInt8);
  } else if (this->info.getType() == aitEnumUint16) {
    pDD = new gddScalar(gddAppType_value, aitEnumUint16);
  } else if (this->info.getType() == aitEnumInt16) {
    pDD = new gddScalar(gddAppType_value, aitEnumInt16);
  } else if (this->info.getType() == aitEnumUint32) {
    pDD = new gddScalar(gddAppType_value, aitEnumUint32);
  } else if (this->info.getType() == aitEnumInt32) {
    pDD = new gddScalar(gddAppType_value, aitEnumInt32);
  } else if (this->info.getType() == aitEnumFloat32) {
    pDD = new gddScalar(gddAppType_value, aitEnumFloat32);
  } else {
    pDD = new gddScalar(gddAppType_value, aitEnumFloat64);
  }
  if (!pDD.valid()) {
    return;
  }

  //
  // smart pointer class manages reference count after this point
  //
  gddStatus = pDD->unreference();
  assert(!gddStatus);

  if (this->info.getType() == aitEnumString) {
    char newValue[AIT_FIXED_STRING_SIZE];
    if (this->pValue.valid()) {
      return;
    } else {
      sprintf(newValue, "");
    }
    *pDD = newValue;
  } else if (this->info.getType() == aitEnumUint8) {
    unsigned char newValue;
    if (this->pValue.valid()) {
      return;
    } else {
      newValue = 0;
    }
    *pDD = newValue;
  } else if (this->info.getType() == aitEnumInt8) {
    char newValue;
    if (this->pValue.valid()) {
      return;
    } else {
      newValue = 0;
    }
    *pDD = newValue;
  } else if (this->info.getType() == aitEnumUint16) {
    unsigned short newValue;
    if (this->pValue.valid()) {
      return;
    } else {
      newValue = 0;
    }
    *pDD = newValue;
  } else if (this->info.getType() == aitEnumInt16) {
    short newValue;
    if (this->pValue.valid()) {
      return;
    } else {
      newValue = 0;
    }
    *pDD = newValue;
  } else if (this->info.getType() == aitEnumUint32) {
    unsigned int newValue;
    if (this->pValue.valid()) {
      return;
    } else {
      newValue = 0;
    }
    *pDD = newValue;
  } else if (this->info.getType() == aitEnumInt32) {
    int newValue;
    if (this->pValue.valid()) {
      return;
    } else {
      newValue = 0;
    }
    *pDD = newValue;
  } else if (this->info.getType() == aitEnumFloat32) {
    float newValue;
    radians = (rand() * 2.0 * myPI) / RAND_MAX;
    if (this->pValue.valid()) {
      this->pValue->getConvert(newValue);
    } else {
      newValue = 0.0f;
    }
    if ((double)this->info.getScanPeriod() > 0) {
      newValue += (float)(sin(radians) / 10.0);
    }
    limit = (double)this->info.getHopr();
    newValue = (float)tsMin((double)newValue, limit);
    limit = (double)this->info.getLopr();
    newValue = (float)tsMax((double)newValue, limit);
    *pDD = newValue;
  } else {
    double newValue;
    radians = (rand() * 2.0 * myPI) / RAND_MAX;
    if (this->pValue.valid()) {
      this->pValue->getConvert(newValue);
    } else {
      newValue = 0.0f;
    }
    if ((double)this->info.getScanPeriod() > 0) {
      newValue += (double)(sin(radians) / 10.0);
    }
    limit = (double)this->info.getHopr();
    newValue = tsMin(newValue, limit);
    limit = (double)this->info.getLopr();
    newValue = tsMax(newValue, limit);
    *pDD = newValue;
  }

  aitTimeStamp gddts = this->currentTime;
  pDD->setTimeStamp(&gddts);
  status = this->update(*pDD);
  if (status != S_casApp_success) {
    errMessage(status, "scalar scan update failed\n");
  }
}

//
// exScalarPV::updateValue ()
//
// NOTES:
// 1) This should have a test which verifies that the
// incoming value in all of its various data types can
// be translated into a real number?
// 2) We prefer to unreference the old PV value here and
// reference the incoming value because this will
// result in each value change events retaining an
// independent value on the event queue.
//
caStatus exScalarPV::updateValue(const gdd &valueIn) {
  //
  // Really no need to perform this check since the
  // server lib verifies that all requests are in range
  //
  if (!valueIn.isScalar()) {
    return S_casApp_outOfBounds;
  }

  if (!pValue.valid()) {
    if (this->info.getType() == aitEnumString) {
      this->pValue = new gddScalar(gddAppType_value, aitEnumString);
    } else if (this->info.getType() == aitEnumUint8) {
      this->pValue = new gddScalar(gddAppType_value, aitEnumUint8);
    } else if (this->info.getType() == aitEnumInt8) {
      this->pValue = new gddScalar(gddAppType_value, aitEnumInt8);
    } else if (this->info.getType() == aitEnumUint16) {
      this->pValue = new gddScalar(gddAppType_value, aitEnumUint16);
    } else if (this->info.getType() == aitEnumInt16) {
      this->pValue = new gddScalar(gddAppType_value, aitEnumInt16);
    } else if (this->info.getType() == aitEnumUint32) {
      this->pValue = new gddScalar(gddAppType_value, aitEnumUint32);
    } else if (this->info.getType() == aitEnumInt32) {
      this->pValue = new gddScalar(gddAppType_value, aitEnumInt32);
    } else if (this->info.getType() == aitEnumFloat32) {
      this->pValue = new gddScalar(gddAppType_value, aitEnumFloat32);
    } else {
      this->pValue = new gddScalar(gddAppType_value, aitEnumFloat64);
    }
    if (!pValue.valid()) {
      return S_casApp_noMemory;
    }
  }
  this->pValue->put(&valueIn);

  return S_casApp_success;
}
