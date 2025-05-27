/* Header for logfunc library */

#ifndef INClogDaemonLibh
#define INClogDaemonLibh

#include <BSlib.h>
#include <stdarg.h>
#include "logDaemonConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

struct logHandle {
  int sockfd;
  BSDATA bsData;
  char sourceId[STDBUF];
};
typedef struct logHandle LOGHANDLE;

/*****************/
/* logDaemon API */
/*****************/
int logOpen(LOGHANDLE *logHandle, char *serviceId, char *sourceId,
            char *tagList);
int logString(LOGHANDLE logHandle, char *valueList);
int logArguments(LOGHANDLE logHandle, ...);
int logArray(LOGHANDLE, char *valueArray[]);
int logClose(LOGHANDLE logHandle);

/****************/
/* Return codes */
/****************/
#define LOG_ERROR -1
#define LOG_OK 0
#define LOG_TOOBIG 1
#define LOG_INVALID 2

#ifdef __cplusplus
}
#endif

#endif
