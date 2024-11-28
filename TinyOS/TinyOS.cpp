#include <windows.h>
#include <iostream>
#include <vector>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <queue>

#include "kernel.h"

// タスクの関数プロトタイプ
using TaskFunction = std::function<void()>;

// タスク情報構造体
struct TaskInfo {
	HANDLE threadHandle;
	DWORD threadId;
	std::string taskName;
	bool isExist;
	HANDLE excuteEvent;
	bool isWaiting;
	RELTIM dly_tim;
	// --- EVENT FLAG -->
	FLGPTN waitptn;
	MODE waitmode;
	// <-- EVENT FLAG ---
	// --- EVENT FLAG -->
	VP_INT receptData;
	// <-- EVENT FLAG ---
	TaskFunction taskFunction;
};

// グローバル変数
std::vector<std::shared_ptr<TaskInfo>> tasks;
std::queue<std::shared_ptr<TaskInfo>> readyQueue;
std::queue<std::shared_ptr<TaskInfo>> waitTimeQueue;
HANDLE yieldEvent;





// IDの列挙型定義
enum {
	ID_AAA,
	ID_BBB,
	ID_CCC,
	ID_DDD,
	/* --- */
	ID_MAX
};




// ContextManagerクラス - IDでContextを登録・取得する
class ContextManager {
public:
	// コンテキストをIDに基づいて登録
	void registerContext(int id, std::shared_ptr<TaskInfo> context) {
		if (id < 0 || id >= ID_MAX) {
			throw std::out_of_range("Invalid ID");
		}
		contexts_[id] = context;
	}

	// IDに基づいてコンテキストを取得
	std::shared_ptr<TaskInfo> getContext(int id) const {
		auto it = contexts_.find(id);
		if (it != contexts_.end()) {
			return it->second;
		}
		else {
			throw std::out_of_range("Context not found for given ID");
		}
	}

private:
	// コンテキストを格納するマップ
	std::unordered_map<int, std::shared_ptr<TaskInfo>> contexts_;
} manager;






std::shared_ptr<TaskInfo> running_task;

// スケジューラー（ディスパッチャー）関数
void StartDispatcher() {
	yieldEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	if (yieldEvent == nullptr) {
		std::cerr << "Failed to create yield event." << std::endl;
		return;
	}

	while (true) {

		// 時間待ち
		if (!waitTimeQueue.empty()) {
			for (size_t q_len = waitTimeQueue.size(); q_len; q_len--) {
				std::shared_ptr<TaskInfo> wai_tim_tsk = waitTimeQueue.front();
				waitTimeQueue.pop();
				if (!--wai_tim_tsk->dly_tim) {
					wai_tim_tsk->isWaiting = false;
					readyQueue.push(wai_tim_tsk);
					std::cout << "Wakeup task: " << wai_tim_tsk->taskName << std::endl;
				}
				else {
					waitTimeQueue.push(wai_tim_tsk);
				}
			}
		}

		// 実行可能タスクを一周回す
		if (!readyQueue.empty()) {
			for (size_t q_len = readyQueue.size(); q_len; q_len--) {
				running_task = readyQueue.front();
				readyQueue.pop();
				if (running_task->isExist && !running_task->isWaiting) {
					std::cout << "Dispatching: " << running_task->taskName << std::endl;
					// タスクに実行権を渡す
					SetEvent(running_task->excuteEvent);
					ResetEvent(yieldEvent);
					WaitForSingleObject(yieldEvent, INFINITE);
					if (!running_task->isWaiting) readyQueue.push(running_task); // 再度レディーキューに追加
				}
			}
		}

		Sleep(500); // スケジューリングのためのウェイト
	}
}

// タスクの実行権を譲る関数（リネーム済み）
static void TaskYield() {
	// 現在のタスクが他のタスクに実行権を譲る
	SetEvent(yieldEvent);
	WaitForSingleObject(running_task->excuteEvent, INFINITE);
}

// ------------------------------------------

static DWORD WINAPI TaskThreadFunction(void* param) {
	// 生ポインタを shared_ptr に再構築（ちょっとここ難しい！分からん！後で分かるんかな？）
	std::shared_ptr<TaskInfo>* taskInfoPtr = static_cast<std::shared_ptr<TaskInfo>*>(param);
	std::shared_ptr<TaskInfo> taskInfo = *taskInfoPtr;
	while (taskInfo->isExist) {
		// レディーキューに接続する
		readyQueue.push(taskInfo);
		ResetEvent(taskInfo->excuteEvent);
		WaitForSingleObject(taskInfo->excuteEvent, INFINITE);
		// ユーザー定義のタスク関数を実行
		taskInfo->taskFunction();
	}
	return 0;
}

// ユーザー定義タスクの生成関数
void CreateTask(int id, const std::string& name, TaskFunction taskFunction) {
	std::shared_ptr<TaskInfo> taskInfo = std::make_shared<TaskInfo>();
	taskInfo->taskName = name;
	taskInfo->isExist = true;
	taskInfo->isWaiting = false;
	taskInfo->taskFunction = std::move(taskFunction);
	taskInfo->excuteEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	// shared_ptr を格納するためのポインタを用意
	auto taskInfoPtr = new std::shared_ptr<TaskInfo>(taskInfo);

	manager.registerContext(id, taskInfo);

	taskInfo->threadHandle = CreateThread(
		nullptr,
		0,
		TaskThreadFunction,
		taskInfoPtr,
		0,
		&taskInfo->threadId
		);

	if (taskInfo->threadHandle == nullptr) {
		std::cerr << "Failed to create thread for " << name << std::endl;
		return;
	}

	tasks.push_back(taskInfo);
	readyQueue.push(taskInfo); // レディーキューに追加
}

void TermitTask(ID tskid) {
	std::shared_ptr<TaskInfo> taskinfo = manager.getContext(tskid);
	taskinfo->isExist = false;
	TaskYield(); // 実行権を譲る
}


void DelayTask(RELTIM dlytim) {
	if (dlytim) {
		running_task->isWaiting = true; // 自タスクを待ち状態にする
		running_task->dly_tim = dlytim;
		waitTimeQueue.push(running_task);
	}
	TaskYield(); // 実行権を譲る
}

// タスク情報構造体
struct FlagInfo {
	FLGPTN flgptn;
	std::queue<std::shared_ptr<TaskInfo>> waitQueue;
};

// フラグ操作モードの定義
#define TWF_ANDW    0x00u
#define TWF_ORW     0x01u

std::unordered_map<ID, FlagInfo> flagTable; // フラグ管理用マップ

void SetFlag(ID flgid, FLGPTN setptn) {
	FLGPTN currentFlags = (flagTable[flgid].flgptn |= setptn); // フラグの設定

	std::cout << "Set Flag 1 acquired flag: " << currentFlags << std::endl;

	if (!flagTable[flgid].waitQueue.empty()) {
		std::shared_ptr<TaskInfo> task = flagTable[flgid].waitQueue.front();
		flagTable[flgid].waitQueue.pop();
		if (task->isExist && task->isWaiting) {
			bool conditionMet = false;
			if (task->waitmode == TWF_ANDW) {
				conditionMet = ((currentFlags & task->waitptn) == task->waitptn);
			}
			else if (task->waitmode == TWF_ORW) {
				conditionMet = ((currentFlags & task->waitptn) != 0);
			}
			if (conditionMet) {
				std::cout << "Resumu Flag 1 task: " << task->taskName << std::endl;
				task->isWaiting = false;
				readyQueue.push(task); // 再度レディーキューに追加
				task->waitptn = currentFlags;	// 本当は使い回しは良くないが、待ちパターンに解除パターンを入れて戻す
				// break;
			}
		}
		if (task->isWaiting) flagTable[flgid].waitQueue.push(task);
	}

	TaskYield(); // 実行権を譲る
}

void ClearFlag(ID flgid, FLGPTN clearptn) {
	flagTable[flgid].flgptn &= clearptn; // フラグのクリア
	TaskYield(); // 実行権を譲る
}

void WaitFlg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn) {

	// すでにフラグが有効な場合の対処
	FLGPTN currentFlags = flagTable[flgid].flgptn;
	bool conditionMet = false;
	if (wfmode == TWF_ANDW) {
		conditionMet = ((currentFlags & waiptn) == waiptn);
	}
	else if (wfmode == TWF_ORW) {
		conditionMet = ((currentFlags & waiptn) != 0);
	}
	if (conditionMet) {
		*p_flgptn = currentFlags;
	}
	else {
		running_task->isWaiting = true; // 自タスクを待ち状態にする
		running_task->waitptn = waiptn;
		running_task->waitmode = wfmode;
		flagTable[flgid].waitQueue.push(running_task);
		TaskYield(); // 実行権を譲る
		*p_flgptn = running_task->waitptn;	// 解除パターンを受け取る
	}
}

void ReferenceFlg(ID flgid, T_RFLG *pk_rflg) {
	if (pk_rflg) {
		pk_rflg->flgptn = flagTable[flgid].flgptn;
	}
}

// タスク情報構造体
struct DtqInfo {
	VP_INT data;
	UINT count;
	std::queue<std::shared_ptr<TaskInfo>> waitQueue;
};

std::unordered_map<ID, DtqInfo> dataQueueTable; // データキュー管理用マップ




void pSendDataQueue(ID dtqid, VP_INT data) {
	dataQueueTable[dtqid].data = data;
	++dataQueueTable[dtqid].count;

	std::cout << "Send DataQueue data: " << data << std::endl;

	if (!dataQueueTable[dtqid].waitQueue.empty()) {
		std::shared_ptr<TaskInfo> task = dataQueueTable[dtqid].waitQueue.front();
		dataQueueTable[dtqid].waitQueue.pop();
		if (task->isExist && task->isWaiting) {
			std::cout << "Send DataQueue task: " << task->taskName << std::endl;
			task->receptData = dataQueueTable[dtqid].data;
			--dataQueueTable[dtqid].count;
			task->isWaiting = false;
			readyQueue.push(task); // 再度レディーキューに追加
		}
		if (task->isWaiting) dataQueueTable[dtqid].waitQueue.push(task);
	}
	TaskYield(); // 実行権を譲る
}

void ReceiveDataQueue(ID dtqid, VP_INT *p_data) {
	// すでにキューにデータが貯まっている場合の対処
	if (dataQueueTable[dtqid].count) {
		*p_data = dataQueueTable[dtqid].data;
		--dataQueueTable[dtqid].count;
	}
	else {
		running_task->isWaiting = true; // 自タスクを待ち状態にする
		dataQueueTable[dtqid].waitQueue.push(running_task);
		TaskYield(); // 実行権を譲る
		*p_data = running_task->receptData;	// キューからデータを受け取る
	}
}

void ReferenceDataQueue(ID dtqid, T_RDTQ *pk_rdtq) {
	if (pk_rdtq) {
		pk_rdtq->sdtqcnt = dataQueueTable[dtqid].count;
	}
}

// ------------------------------------------

int main() {
	// ユーザー定義タスクを作成
	CreateTask(ID_AAA, "Task 1", []() {
		while (true) {
			std::cout << "Task 1 is waiting for flag." << std::endl;
			FLGPTN resultFlag;
			WaitFlg(ID_AAA, 0x01, TWF_ORW, &resultFlag);
			ClearFlag(ID_AAA, ~0x01);
			std::cout << "Task 1 acquired flag: " << resultFlag << std::endl;
		}
	});

	CreateTask(ID_BBB, "Task 2", []() {
		while (true) {
			std::cout << "Task 2 is waiting for flag." << std::endl;
			FLGPTN resultFlag;
			WaitFlg(ID_AAA, 0x02, TWF_ORW, &resultFlag);
			ClearFlag(ID_AAA, ~0x02);
			std::cout << "Task 2 acquired flag: " << resultFlag << std::endl;
		}
	});

	CreateTask(ID_CCC, "Task 3", []() {
		while (true) {
			std::cout << "Task 3 is waiting for dtq." << std::endl;
			VP_INT dtq_data;
			int data;
			ReceiveDataQueue(ID_AAA, &dtq_data);
			data = (int)dtq_data;
			std::cout << "Task 3 recept data: " << data << std::endl;
		}
	});

	CreateTask(ID_DDD, "Task 4", []() {
		while (true) {
			std::cout << "Task 4 is setting flag." << std::endl;
			DelayTask(10);
			SetFlag(ID_AAA, 0x02);
			std::cout << "Task 4 is setting flag." << std::endl;
			DelayTask(10);
			SetFlag(ID_AAA, 0x01);
			std::cout << "Task 4 is sending data." << std::endl;
			DelayTask(10);
			pSendDataQueue(ID_AAA, (VP_INT)123);
		}
	});

	// スケジューラーを開始
	SetEvent(yieldEvent);
	StartDispatcher();

	// クリーンアップ
	for (auto& task : tasks) {
		WaitForSingleObject(task->threadHandle, INFINITE);
		CloseHandle(task->threadHandle);
		CloseHandle(task->excuteEvent);
	}
	CloseHandle(yieldEvent);

	return 0;
}
