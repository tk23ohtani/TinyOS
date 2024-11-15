#include <windows.h>
#include <iostream>
#include <vector>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>

// タスクの関数プロトタイプ
using TaskFunction = std::function<void()>;

// タスク情報構造体
struct TaskInfo {
	HANDLE threadHandle;
	DWORD threadId;
	std::string taskName;
	bool isExist;
	TaskFunction taskFunction;
};

// グローバル変数
std::vector<std::shared_ptr<TaskInfo>> tasks;
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
}

// スケジューラー（ディスパッチャー）関数
void StartDispatcher() {
	yieldEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	if (yieldEvent == nullptr) {
		std::cerr << "Failed to create yield event." << std::endl;
		return;
	}

	while (true) {
		for (auto& task : tasks) {
			if (task->isExist) {
				// タスクに実行権を渡す
				SetEvent(yieldEvent);
				std::cout << "Dispatching: " << task->taskName << std::endl;
				Sleep(500); // スケジューリングのためのウェイト
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

typedef int ID;
typedef ID ER;
typedef unsigned int UINT;
typedef UINT FLGPTN;

/*----------------------------------------------------------------------*/

#define E_OK					(0x00)	/* 00h  normal exit						*/
#define	E_MEM					(0x01)  /* 01h  can't obtain memory area		*/
#define	E_UNDEF_SVC				(0x02)  /* 02h  illegal system call				*/
#define	E_CONTEXT				(0x03)  /* 03h  context error					*/
#define	E_ACCESS_ADR			(0x04)  /* 04h  access address error			*/
#define	E_ILLEGAL_ADR			(0x05)  /* 05h  illegal address ( aa )			*/
#define	E_PACKET_ADR			(0x06)  /* 06h  illegal address ( pa )			*/
#define	E_BLOCK_ADR				(0x07)  /* 07h  illegal address ( ma, mba )		*/
#define	E_START_ADR				(0x08)  /* 08h  illegal address ( psa )			*/
#define	E_ALREADY_EXIST			(0x09)  /* 09h  existent object					*/
#define	E_NOT_EXIST				(0x0A)  /* 0ah  not exist object				*/
#define	E_NOT_IDLE				(0x0B)  /* 0bh  task status isn't idle			*/
#define	E_NOT_SUSPEND			(0x0C)  /* 0ch  task status isn't suspend		*/
#define	E_IDLE					(0x0D)  /* 0dh  task status is idle				*/
#define	E_ID0					(0x0E)  /* 0eh  task id equal '0'				*/
#define	E_ID_BOUND				(0x0F)  /* 0fh  task id is greater than max_id	*/
#define	E_TIMEOUT				(0x10)  /* 10h  timeout error					*/
#define	E_COUNT_OVER			(0x11)  /* 11h  queue count overflow			*/
#define	E_SELF_TASK				(0x12)  /* 12h  self task no-good				*/
#define	E_DELETE_OBJ			(0x13)  /* 13h  delete obj on something_wait	*/
#define	E_OPTION				(0x14)  /* 14h  can't use this option			*/
#define	E_FLG_WAIT				(0x15)  /* 15h  task already wait on this evf	*/
#define	E_TIMER					(0x16)  /* 16h  can't use timer					*/
#define	E_PRIORITY				(0x17)  /* 17h  priority error					*/
#define	E_INTERRUPT_PRIORITY	(0x18)  /* 18h  interrut priority error			*/
#define	E_NON_CYCLIC			(0x19)  /* 19h  can_cwak to non-cyclic task		*/
#define	E_POOL_SIZE				(0x1A)  /* 1ah  memory pool size error			*/
#define	E_MEMORY_BLOCK			(0x1B)  /* 1bh  rel_blk to not system memory	*/
#define	E_DEVICE_NO				(0x1C)  /* 1ch  illegal device number			*/
#define	E_MES_OVER				(0x1D)  /* 1dh  message buffer overflow			*/
#define	E_ERROR					(0x1E)  /* 1eh  others system error				*/
#define	E_PARAM					(0x1F)  /* 1fh  illegal parameter				*/
#define	E_SVC					(0x40) 	/* 40h  illegal system cal number		*/
#define	E_EXCEPTION				(0x80)  /* 80h  error in exception proc.		*/
#define	E_SYS_OBJECT			(0x81)  /* 81h  this object is system task		*/

void TermitTask(ID tskid) {
	std::shared_ptr<TaskInfo> taskinfo = manager.getContext(tskid);
	taskinfo->isExist = false;
	TaskYield();
}


void SetFlag(ID flgid, FLGPTN setptn) {
}


// ------------------------------------------

int main() {
	// ユーザー定義タスクを作成
	CreateTask(ID_AAA, "Task 1", []() {
		while (true) {
			std::cout << "Running Task 1" << std::endl;
			TaskYield(); // 実行権を譲る
			Sleep(100); // タスク処理のウェイト
		}
	});

	CreateTask(ID_BBB, "Task 2", []() {
		while (true) {
			std::cout << "Running Task 2" << std::endl;
			TaskYield(); // 実行権を譲る
			Sleep(150); // タスク処理のウェイト
		}
	});

	// スケジューラーを開始
	StartDispatcher();

	// クリーンアップ
	for (auto& task : tasks) {
		WaitForSingleObject(task->threadHandle, INFINITE);
		CloseHandle(task->threadHandle);
		// delete task;
	}
	CloseHandle(yieldEvent);
	return 0;
}
