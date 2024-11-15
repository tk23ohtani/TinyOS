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
std::vector<TaskInfo*> tasks;
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
	void registerContext(int id, TaskInfo* context) {
		if (id < 0 || id >= ID_MAX) {
			throw std::out_of_range("Invalid ID");
		}
		contexts_[id] = context;
	}

	// IDに基づいてコンテキストを取得
	TaskInfo* getContext(int id) const {
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
	std::unordered_map<int, TaskInfo*> contexts_;
} manager;




// ユーザー定義タスクの生成関数
void CreateTask(int id, const std::string& name, TaskFunction taskFunction) {
	// TaskInfoを動的に割り当て
	TaskInfo* taskInfo = new TaskInfo;
	taskInfo->taskName = name;
	taskInfo->isExist = true;
	taskInfo->taskFunction = std::move(taskFunction);

	manager.registerContext(id, taskInfo);

	taskInfo->threadHandle = CreateThread(
		nullptr,
		0,
		[](LPVOID param) -> DWORD {
		auto* taskInfo = reinterpret_cast<TaskInfo*>(param);
		while (taskInfo->isExist) {
			// ユーザー定義のタスク関数を実行
			taskInfo->taskFunction();
			// 実行権を譲る
			WaitForSingleObject(yieldEvent, INFINITE);
		}
		return 0;
	},
		taskInfo,
		0,
		&taskInfo->threadId
		);

	if (taskInfo->threadHandle == nullptr) {
		std::cerr << "Failed to create thread for " << name << std::endl;
		delete taskInfo;
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
#define	E_OK 0

void TermitTask(ID tskid) {
	TaskInfo* taskinfo = manager.getContext(tskid);
	taskinfo->isExist = false;
	TaskYield();
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
		delete task;
	}
	CloseHandle(yieldEvent);
	return 0;
}
