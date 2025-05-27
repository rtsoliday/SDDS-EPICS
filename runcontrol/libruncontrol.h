#include <alarm.h>
#include <epicsVersion.h>
/*#define NO_ALARM            0*/

#ifdef __cplusplus
extern "C" {
#endif

#define RUNCONTROL_PV_PREFIX "APS:RunControlSlot"
#define RUNCONTROL_COUNT_PV "APS:RunControlCountAO"

typedef struct {
  char pv_hbt[255];
  chid pv_hbt_chid;
  char pv_user[255];
  chid pv_user_chid;
  char pv_host[255];
  chid pv_host_chid;
  char pv_pid[255];
  chid pv_pid_chid;
  char pv_sem[255];
  chid pv_sem_chid;
  char pv_desc[255];
  chid pv_desc_chid;
  char pv_strt[255];
  chid pv_strt_chid;
  char pv_abrt[255];
  chid pv_abrt_chid;
  char pv_susp[255];
  chid pv_susp_chid;
  char pv_hbto[255];
  chid pv_hbto_chid;
  char pv_val[255];
  chid pv_val_chid;
  char pv_msg[255];
  chid pv_msg_chid;
  char pv_alrm[255];
  chid pv_alrm_chid;
  double pendIOtime;
  unsigned int myPid;
} RUNCONTROL_INFO;

int runControlInit(char *pv, char *desc, float timeout, char *handle, RUNCONTROL_INFO *rcInfo, double pendIOtime);
int runControlPing(char *handle, RUNCONTROL_INFO *rcInfo);
int runControlExit(char *handle, RUNCONTROL_INFO *rcInfo);
int runControlLogMessage(char *handle, char *message, short severity, RUNCONTROL_INFO *rcInfo);

#define RUNCONTROL_ERROR -1
#define RUNCONTROL_OK 0
#define RUNCONTROL_ABORT 1
#define RUNCONTROL_DENIED 2
#define RUNCONTROL_TIMEOUT 3

#ifdef __cplusplus
}
#endif
