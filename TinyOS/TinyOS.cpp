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
typedef UINT MODE;

#define E_OK					(0x00)	/* 00h  normal exit						*/

void TermitTask(ID tskid) {
	std::shared_ptr<TaskInfo> taskinfo = manager.getContext(tskid);
	taskinfo->isExist = false;
	TaskYield();
}

// フラグ操作モードの定義
#define TWF_ANDW    0x00u
#define TWF_ORW     0x01u

std::unordered_map<ID, FLGPTN> flagTable; // フラグ管理用マップ

void SetFlag(ID flgid, FLGPTN setptn) {
	flagTable[flgid] |= setptn; // フラグの設定
	TaskYield(); // 実行権を譲る
	SetEvent(yieldEvent); // 待機中のタスクを再開
}

void ClearFlag(ID flgid, FLGPTN clearptn) {
	flagTable[flgid] &= ~clearptn; // フラグのクリア
	TaskYield(); // 実行権を譲る
}

void WaitFlg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn) {
	while (true) {
		FLGPTN currentFlags = flagTable[flgid];

		bool conditionMet = false;
		if (wfmode == TWF_ANDW) {
			conditionMet = ((currentFlags & waiptn) == waiptn);
		}
		else if (wfmode == TWF_ORW) {
			conditionMet = ((currentFlags & waiptn) != 0);
		}

		if (conditionMet) {
			*p_flgptn = currentFlags;
			break;
		}

		TaskYield(); // 実行権を譲る
		Sleep(10); // ウェイトを入れて他のタスクに実行権を譲る
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
			std::cout << "Task 2 is setting flag." << std::endl;
			SetFlag(ID_AAA, 0x01);
			Sleep(150); // タスク処理のウェイト
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
