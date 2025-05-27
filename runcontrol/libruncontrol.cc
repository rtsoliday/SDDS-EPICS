#include <complex>
#include <iostream>
#include <cstdlib>
#include <string.h>
#include <ctime>

#if defined(_WIN32)
#  include <winsock.h>
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>
#endif

#include <cadef.h>
#include <epicsVersion.h>
#if (EPICS_VERSION > 3)
#  include "pvaSDDS.h"
#  include "pv/thread.h"
#endif

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"

#include "libruncontrol.h"

#if (EPICS_VERSION > 3)
#  include "./libruncontrolPVA.cc"
#else
#  include "./libruncontrolCA.cc"
#endif
