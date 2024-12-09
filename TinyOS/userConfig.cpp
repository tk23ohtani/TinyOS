#include "TinyOS.h"
#include "kernel.h"
#include "userConfig.h"

int configTinyOS() {

	// ユーザー定義タスクを作成
	CreateTask(ID_AAA, "Task 1", []() {
		TASK_FOREVER {
			debug_printf("Task 1 is waiting for flag.\n");
			FLGPTN resultFlag;
			WaitFlg(ID_AAA, 0x01, TWF_ORW, &resultFlag);
			ClearFlag(ID_AAA, ~0x01);
			debug_printf("Task 1 acquired flag: %d\n", resultFlag);
		}
	});

	CreateTask(ID_BBB, "Task 2", []() {
		TASK_FOREVER {
			debug_printf("Task 2 is waiting for flag.\n");
			FLGPTN resultFlag;
			WaitFlg(ID_AAA, 0x02, TWF_ORW, &resultFlag);
			ClearFlag(ID_AAA, ~0x02);
			debug_printf("Task 2 acquired flag: %d\n", resultFlag);
		}
	});

	CreateTask(ID_CCC, "Task 3", []() {
		TASK_FOREVER {
			debug_printf("Task 3 is waiting for dtq.\n");
			VP_INT dtq_data;
			int data;
			ReceiveDataQueue(ID_AAA, &dtq_data);
			data = (int)dtq_data;
			debug_printf("Task 3 recept data: %d\n", data);
		}
	});

	CreateTask(ID_DDD, "Task 4", []() {
		TASK_FOREVER {
			debug_printf("Task 4 is sleeping...\n");
			SleepTask();
			debug_printf("Task 4 is awake!\n");
		}
	});

	CreateTask(ID_MMM, "Task Master", []() {
		TASK_FOREVER {
			debug_printf("Task Master is setting flag.\n");
			ViewTaskInfo();
			DelayTask(10);
			SetFlag(ID_AAA, 0x02);
			debug_printf("Task Master is setting flag.\n");
			ViewTaskInfo();
			DelayTask(10);
			SetFlag(ID_AAA, 0x01);
			debug_printf("Task Master is sending data.\n");
			ViewTaskInfo();
			DelayTask(10);
			pSendDataQueue(ID_AAA, (VP_INT)123);
			ViewTaskInfo();
			DelayTask(10);
			WakeupTask(ID_DDD);
			ViewTaskInfo();
			DelayTask(10);
		}
	});

	return 0;
}
