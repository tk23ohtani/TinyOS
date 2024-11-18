#include <windows.h>
#include <iostream>
#include <vector>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <queue>

// ------------------------------------------

typedef int ID;
typedef ID ER;
typedef unsigned int UINT;
typedef UINT FLGPTN;
typedef UINT MODE;

#define E_OK					(0x00)	/* 00h  normal exit						*/

// ------------------------------------------

// タスクの関数プロトタイプ
using TaskFunction = std::function<void()>;

// タスク情報構造体
struct TaskInfo {
	HANDLE threadHandle;
	DWORD threadId;
	std::string taskName;
	bool isExist;
	bool isWaiting;
	// --- EVENT FLAG -->
	FLGPTN waitptn;
	MODE waitmode;
	// <-- EVENT FLAG ---
	TaskFunction taskFunction;
};

// グローバル変数
std::vector<std::shared_ptr<TaskInfo>> tasks;
std::queue<std::shared_ptr<TaskInfo>> readyQueue;
HANDLE yieldEvent;





// IDの列挙型定義
enum {
	ID_AAA,
	ID_BBB,
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




// ユーザー定義タスクの生成関数
void CreateTask(int id, const std::string& name, TaskFunction taskFunction) {
	std::shared_ptr<TaskInfo> taskInfo = std::make_shared<TaskInfo>();
	taskInfo->taskName = name;
	taskInfo->isExist = true;
	taskInfo->isWaiting = false;
	taskInfo->taskFunction = std::move(taskFunction);

	// shared_ptr を格納するためのポインタを用意
	auto taskInfoPtr = new std::shared_ptr<TaskInfo>(taskInfo);

	manager.registerContext(id, taskInfo);

	taskInfo->threadHandle = CreateThread(
		nullptr,
		0,
		[](LPVOID param) -> DWORD {
		// 生ポインタを shared_ptr に再構築（ちょっとここ難しい！分からん！後で分かるんかな？）
		std::shared_ptr<TaskInfo>* taskInfoPtr = static_cast<std::shared_ptr<TaskInfo>*>(param);
		std::shared_ptr<TaskInfo> taskInfo = *taskInfoPtr;
		while (taskInfo->isExist) {
			// ユーザー定義のタスク関数を実行
			taskInfo->taskFunction();
			// 実行権を譲る
			WaitForSingleObject(yieldEvent, INFINITE);
		}
		return 0;
	},
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


std::shared_ptr<TaskInfo> running_task;

// スケジューラー（ディスパッチャー）関数
void StartDispatcher() {
	yieldEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	if (yieldEvent == nullptr) {
		std::cerr << "Failed to create yield event." << std::endl;
		return;
	}

	while (true) {
		if (!readyQueue.empty()) {
			running_task = readyQueue.front();
			readyQueue.pop();
			if (running_task->isExist && !running_task->isWaiting) {

				// タスクに実行権を渡す
				SetEvent(yieldEvent);
				std::cout << "Dispatching: " << running_task->taskName << std::endl;
				Sleep(500); // スケジューリングのためのウェイト

				if (!running_task->isWaiting) readyQueue.push(running_task); // 再度レディーキューに追加

			}
		}
	}
}

// タスクの実行権を譲る関数（リネーム済み）
void TaskYield() {
	// 現在のタスクが他のタスクに実行権を譲る
	ResetEvent(yieldEvent);
	WaitForSingleObject(yieldEvent, INFINITE);
}

// ------------------------------------------

void TermitTask(ID tskid) {
	std::shared_ptr<TaskInfo> taskinfo = manager.getContext(tskid);
	taskinfo->isExist = false;
	TaskYield();
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

	flagTable[flgid].waitQueue;
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
				task->isWaiting = false;
				readyQueue.push(task); // 再度レディーキューに追加
				task->waitptn = currentFlags;	// 本当は使い回しは良くないが、待ちパターンに解除パターンを入れて戻す
				// break;
			}
		}
		if (!task->isWaiting) flagTable[flgid].waitQueue.push(task);
	}

	TaskYield(); // 実行権を譲る
}

void ClearFlag(ID flgid, FLGPTN clearptn) {
	flagTable[flgid].flgptn &= clearptn; // フラグのクリア
	TaskYield(); // 実行権を譲る
}

void WaitFlg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn) {
	running_task->isWaiting = true; // 自タスクを待ち状態にする
	running_task->waitptn = waiptn;
	running_task->waitmode = wfmode;
	flagTable[flgid].waitQueue.push(running_task);
	TaskYield(); // 実行権を譲る
	*p_flgptn = running_task->waitptn;	// 解除パターンを受け取る
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
			SetFlag(ID_AAA, 0x02);
			Sleep(1500); // タスク処理のウェイト
		}
	});

	CreateTask(ID_BBB, "Task 2", []() {
		while (true) {
			std::cout << "Task 2 is setting flag." << std::endl;
			SetFlag(ID_AAA, 0x01);
			Sleep(1500); // タスク処理のウェイト
			FLGPTN resultFlag;
			WaitFlg(ID_AAA, 0x02, TWF_ORW, &resultFlag);
			ClearFlag(ID_AAA, ~0x02);
		}
	});

	// スケジューラーを開始
	StartDispatcher();

	// クリーンアップ
	for (auto& task : tasks) {
		WaitForSingleObject(task->threadHandle, INFINITE);
		CloseHandle(task->threadHandle);
	}
	CloseHandle(yieldEvent);

	return 0;
}
