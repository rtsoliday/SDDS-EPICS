/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* routines for setting sr corrector mode */
#ifndef SDDSSRCORR_H_INCLUDED
#define SDDSSRCORR_H_INCLUDED 1

/*#include <tsDefs.h>*/
#include <cadef.h>
/* #include <libdev.h> */
#include <time.h>
#include "SDDS.h"
#include <epicsVersion.h>

#ifdef __cplusplus
extern "C" {
#endif

#define corrTypes 5
#define sourceModes 2
extern char *corrType[corrTypes];
extern char *sourceMode[sourceModes];

void set_source_mode(char *plane, char *sourceModeInput, long dryRun, double pendIOTime);
void setup_corr_mode_ca(char *plane, double pendIOTime);
void set_corr_mode(char *plane, char *mode, long dryRun, double pendIOTime, long unified);

#endif
#ifdef __cplusplus
}
#endif
