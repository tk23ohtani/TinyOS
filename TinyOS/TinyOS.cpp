#include <windows.h>
#include <iostream>
#include <vector>
#include <functional>
#include <string>

// タスクの関数プロトタイプ
using TaskFunction = std::function<void()>;

// タスク情報構造体
struct TaskInfo {
	HANDLE threadHandle;
	DWORD threadId;
	std::string taskName;
	bool isActive;
	TaskFunction taskFunction;
};

// グローバル変数
std::vector<TaskInfo*> tasks;
HANDLE yieldEvent;

// ユーザー定義タスクの生成関数
void CreateTask(const std::string& name, TaskFunction taskFunction) {
	// TaskInfoを動的に割り当て
	TaskInfo* taskInfo = new TaskInfo;
	taskInfo->taskName = name;
	taskInfo->isActive = true;
	taskInfo->taskFunction = std::move(taskFunction);

	taskInfo->threadHandle = CreateThread(
		nullptr,
		0,
		[](LPVOID param) -> DWORD {
		auto* taskInfo = reinterpret_cast<TaskInfo*>(param);
		while (taskInfo->isActive) {
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
			if (task->isActive) {
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

int main() {
	// ユーザー定義タスクを作成
	CreateTask("Task 1", []() {
		while (true) {
			std::cout << "Running Task 1" << std::endl;
			TaskYield(); // 実行権を譲る
			Sleep(100); // タスク処理のウェイト
		}
	});

	CreateTask("Task 2", []() {
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
