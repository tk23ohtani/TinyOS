#ifndef DATA_QUEUE_H
#define DATA_QUEUE_H

// 関数プロトタイプ（Cリンク形式で公開）
#ifdef __cplusplus
extern "C" {
#endif

typedef int ID;
typedef ID ER;
typedef unsigned int UINT;
typedef unsigned long UW;
typedef char VB;
typedef void *VP;
typedef VP VP_INT;
typedef UINT FLGPTN;
typedef UINT MODE;
typedef UW RELTIM;

#define E_OK					(0x00)	/* 00h  normal exit						*/

// フラグ操作モードの定義
#define TWF_ANDW    0x00u
#define TWF_ORW     0x01u

typedef struct t_rflg {
	ID          wtskid;
	FLGPTN      flgptn;
	VB    const *name;
} T_RFLG;

typedef struct t_rdtq {
	ID          stskid;
	ID          rtskid;
	UINT        sdtqcnt;
	VB    const *name;
} T_RDTQ;

void ActionTask(ID tskid);
void TermitTask(ID tskid);
void SleepTask();
void WakeupTask(ID tskid);
void DelayTask(RELTIM dlytim);

void SetFlag(ID flgid, FLGPTN setptn);
void ClearFlag(ID flgid, FLGPTN clearptn);
void WaitFlg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn);
void ReferenceFlg(ID flgid, T_RFLG *pk_rflg);

void pSendDataQueue(ID dtqid, VP_INT data);
void ReceiveDataQueue(ID dtqid, VP_INT *p_data);
void ReferenceDataQueue(ID dtqid, T_RDTQ *pk_rdtq);

bool isTaskExist();
// タスクを無限ループで実行する場合はこのマクロを使用すること
#define TASK_FOREVER while(isTaskExist())

/* for DEBUG */
void debug_printf(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif // DATA_QUEUE_H
