#ifndef DATA_QUEUE_H
#define DATA_QUEUE_H

// �֐��v���g�^�C�v�iC�����N�`���Ō��J�j
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

void TermitTask(ID tskid);
void DelayTask(RELTIM dlytim);

void SetFlag(ID flgid, FLGPTN setptn);
void ClearFlag(ID flgid, FLGPTN clearptn);
void WaitFlg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn);
void ReferenceFlg(ID flgid, T_RFLG *pk_rflg);

void pSendDataQueue(ID dtqid, VP_INT data);
void ReceiveDataQueue(ID dtqid, VP_INT *p_data);
void ReferenceDataQueue(ID dtqid, T_RDTQ *pk_rdtq);

#ifdef __cplusplus
}
#endif

#endif // DATA_QUEUE_H
