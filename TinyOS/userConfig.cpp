#include "TinyOS.h"
#include "kernel.h"
#include "userConfig.h"

int configTinyOS() {

	// ユーザー定義タスクを作成
	CreateTask(ID_AAA, "Task 1", [](VP_INT) {
		TASK_FOREVER {
			VP_INT dtq_data;
			int data;
			ReceiveDataQueue(ID_AAA, &dtq_data);
			data = (int)dtq_data;
			debug_printf("Task 1 recept data: %d\n", data);
			if (data == 123) {
				DelayTask(3);
				debug_printf("Task 1 is setting flag.\n");
				SetFlag(ID_AAA, 0x01);
			}
		}
	}, NULL);

	CreateTask(ID_BBB, "Task 2", [](VP_INT) {
		TASK_FOREVER {
			VP_INT dtq_data;
			int data;
			ReceiveDataQueue(ID_BBB, &dtq_data);
			data = (int)dtq_data;
			debug_printf("Task 2 recept data: %d\n", data);
			if (data == 456) {
				DelayTask(5);
				debug_printf("Task 2 is setting flag.\n");
				SetFlag(ID_AAA, 0x02);
			}
		}
	}, NULL);

	CreateTask(ID_CCC, "Task 3", [](VP_INT) {
		TASK_FOREVER {
			VP_INT dtq_data;
			int data;
			debug_printf("Task 3 is waiting for data.\n");
			ReceiveDataQueue(ID_CCC, &dtq_data);
			data = (int)dtq_data;
			debug_printf("Task 3 recept data: %d\n", data);
			if (data >= 700) {
				if (data == 789) {
					debug_printf("Task 3 is sending data.\n");
					pSendDataQueue(ID_AAA, (VP_INT)123);
		
					debug_printf("Task 3 is sending data.\n");
					pSendDataQueue(ID_BBB, (VP_INT)456);
				}
				FLGPTN resultFlag;
				debug_printf("Task 3 is waiting for flag.\n");
				WaitFlg(ID_AAA, 0x01|0x02, TWF_ANDW, &resultFlag);
				ClearFlag(ID_AAA, ~(0x01|0x02));
				debug_printf("Task 3 acquired flag: %d\n", resultFlag);
			}
		}
	}, NULL);

	CreateTask(ID_MMM, "Task Master", [](VP_INT) {
		TASK_FOREVER {
			debug_printf("Task Master is sleeping...\n");
			SleepTask();
			debug_printf("Task Master is awake!\n");

			debug_printf("Task Master is sending data.\n");
			pSendDataQueue(ID_CCC, (VP_INT)789);
		}
	}, NULL);

	return 0;
}
