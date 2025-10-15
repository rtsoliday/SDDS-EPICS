/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* file: pvMultList.h
 * purpose: include file for routines for multiplying lists/ranges to make PV names.
 * M. Borland, 1995.
 */
#ifndef pvMultListInclude
#define pvMultListInclude

#ifdef __cplusplus
extern "C" {
#endif

#define VALUE_GIVEN 0x0001UL

typedef struct {
  double numericalValue;
  double mean, median, sigma, min, max, spread, stDev, rms, MAD;
  char *name, *value;
} PV_VALUE;

typedef struct {
  char *string, *value;
  unsigned long flags;
} TERM_LIST;

void multiplyWithList(PV_VALUE **PVvalue, long *PVvalues,
                      TERM_LIST *List, long listEntries);
void multiplyWithRange(PV_VALUE **PVvalue, long *PVvalues, long begin, long end, long interval, char *format);
PV_VALUE *excludePVsFromLists(PV_VALUE *PVvalue, long PVs, long  *kept, char **excludePatterns, long excludeCount);

#define BEGIN_GIVEN 0x0001U
#define END_GIVEN 0x0002U
#define FORMAT_GIVEN 0x0004U
#define INTERVAL_GIVEN 0x0008U

#ifdef __cplusplus
}
#endif

#endif
