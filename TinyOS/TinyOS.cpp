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
#include "TinyOS.h"

#include "userConfig.h"

void debug_printf(const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    OutputDebugStringA(buffer);
}

// タスク情報構造体
struct TaskInfo {
	HANDLE threadHandle;
	DWORD threadId;
	const char* taskName;
	VP_INT taskData;
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
CRITICAL_SECTION criticalSection;




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

// 実行タスクが存在するかどうかを返す
bool isTaskExist() {
	return running_task->isExist;
}

// スケジューラー（ディスパッチャー）関数、一定間隔（Tick時間）で呼ばれることが前提
void StartDispatcher() {
	// 時間待ち
	if (!waitTimeQueue.empty()) {
		for (size_t q_len = waitTimeQueue.size(); q_len; q_len--) {
			std::shared_ptr<TaskInfo> wai_tim_tsk = waitTimeQueue.front();
			waitTimeQueue.pop();
			if (!--wai_tim_tsk->dly_tim) {
				wai_tim_tsk->isWaiting = false;
				readyQueue.push(wai_tim_tsk);
				debug_printf("Wakeup task: %s\n", wai_tim_tsk->taskName);
			}
			else {
				waitTimeQueue.push(wai_tim_tsk);
			}
		}
	}

	// 実行可能タスクを一周回す
	if (!readyQueue.empty()) {	// TODO: レディーキューが空になるまで繰り返す
		running_task = readyQueue.front();
		readyQueue.pop();
		if (running_task->isExist && !running_task->isWaiting) {
			debug_printf("Dispatching: %s\n", running_task->taskName);
			// タスクに実行権を渡す
			SetEvent(running_task->excuteEvent);
			WaitForSingleObject(yieldEvent, INFINITE);
			if (!running_task->isWaiting) readyQueue.push(running_task); // 再度レディーキューに追加
		}
	}
}

// タスクの実行権を譲る関数（リネーム済み）
static void TaskYield() {
	// 現在のタスクが他のタスクに実行権を譲る
	SetEvent(yieldEvent);
	WaitForSingleObject(running_task->excuteEvent, INFINITE);
}

// ------------------------------------------

// なぜかラムダ関数が使えないので、スレッド関数を定義
static DWORD WINAPI TaskThreadFunction(void* param) {
	// 生ポインタを shared_ptr に再構築（ちょっとここ難しい！分からん！後で分かるんかな？）
	std::shared_ptr<TaskInfo>* taskInfoPtr = static_cast<std::shared_ptr<TaskInfo>*>(param);
	std::shared_ptr<TaskInfo> taskInfo = *taskInfoPtr;
	while (taskInfo->isExist) {
		// レディーキューに接続する
		readyQueue.push(taskInfo);
		WaitForSingleObject(taskInfo->excuteEvent, INFINITE);
		// ユーザー定義のタスク関数を実行
		debug_printf("Task %s is running\n", taskInfo->taskName);
		taskInfo->taskFunction(taskInfo->taskData);
	}
	return 0;
}

static size_t task_counter = 0;

static void ActivateTask(std::shared_ptr<TaskInfo> taskinfo) {
	if (taskinfo->isExist) {
		if (taskinfo->isWaiting) {
			taskinfo->isWaiting = false;
			readyQueue.push(taskinfo); // レディーキューに追加
		}
	}
}

static void DeleteTask(std::shared_ptr<TaskInfo> taskinfo) {
	if (taskinfo->isExist) {
		taskinfo->isExist = false;
		task_counter--;
	}
}

// ユーザー定義タスクの生成関数
void CreateTask(ID tskid, const char* name, TaskFunction taskFunction, VP_INT taskData) {
	std::shared_ptr<TaskInfo> taskInfo = std::make_shared<TaskInfo>();
	taskInfo->taskName = name;
	taskInfo->taskData = taskData;
	taskInfo->isExist = true;
	taskInfo->isWaiting = true;
	taskInfo->taskFunction = std::move(taskFunction);
	taskInfo->excuteEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	// shared_ptr を格納するためのポインタを用意
	auto taskInfoPtr = new std::shared_ptr<TaskInfo>(taskInfo);

	manager.registerContext(tskid, taskInfo);

	taskInfo->threadHandle = CreateThread(
		nullptr,
		0,
		TaskThreadFunction,
		taskInfoPtr,
		0,
		&taskInfo->threadId
		);

	if (taskInfo->threadHandle == nullptr) {
		debug_printf("Failed to create thread for %s\n", name);
		return;
	}

	tasks.push_back(taskInfo);
	task_counter++;

	// TODO: 起動時ACTフラグがあれれば、タスクを起動
	ActivateTask(taskInfo);
}

void ViewTaskInfo() {
	debug_printf("Task Name\tTask ID\t\tTask waiting\n");
	debug_printf("----------------------------------------\n");
	for (auto& task : tasks) {
		debug_printf("%s\t%d\t\t%s\n", task->taskName, task->threadId, (task->isWaiting ? "Yes" : "No"));
	}
	debug_printf("----------------------------------------\n");
}

// ------------------------------------------

void ActionTask(ID tskid) {
	std::shared_ptr<TaskInfo> taskinfo = manager.getContext(tskid);
	ActivateTask(taskinfo);
	if (running_task->isExist) TaskYield(); // 実行権を譲る
}

void TermitTask(ID tskid) {
	std::shared_ptr<TaskInfo> taskinfo = manager.getContext(tskid);
	DeleteTask(taskinfo);
	if (running_task->isExist) TaskYield(); // 実行権を譲る
}

void SleepTask() {
	if (!running_task->isExist) return; // 終了したタスクはスリープにすぐ戻る
	running_task->isWaiting = true;
	TaskYield(); // 実行権を譲る
}

void iWakeupTask(ID tskid) {
	/* Critical ====> */ EnterCriticalSection(&criticalSection);
	std::shared_ptr<TaskInfo> taskinfo = manager.getContext(tskid);
	taskinfo->isWaiting = false;
	readyQueue.push(taskinfo); // レディーキューに追加
	/* <==== Critical */ LeaveCriticalSection(&criticalSection);
}

void WakeupTask(ID tskid) {
	iWakeupTask(tskid);
	if (running_task->isExist) TaskYield(); // 実行権を譲る
}

void DelayTask(RELTIM dlytim) {
	if (dlytim) {
		running_task->isWaiting = true; // 自タスクを待ち状態にする
		running_task->dly_tim = dlytim;
		waitTimeQueue.push(running_task);
	}
	if (running_task->isExist) TaskYield(); // 実行権を譲る
}

// タスク情報構造体
struct FlagInfo {
	FLGPTN flgptn;
	std::queue<std::shared_ptr<TaskInfo>> waitQueue;
};

std::unordered_map<ID, FlagInfo> flagTable; // フラグ管理用マップ

void iSetFlag(ID flgid, FLGPTN setptn) {

	/* Critical ====> */ EnterCriticalSection(&criticalSection);

	FLGPTN currentFlags = (flagTable[flgid].flgptn |= setptn); // フラグの設定

	debug_printf("Set Flag 1 acquired flag: %d\n", currentFlags);

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
				debug_printf("Resume Flag 1 task: %s\n", task->taskName);
				task->isWaiting = false;
				readyQueue.push(task); // 再度レディーキューに追加
				task->waitptn = currentFlags;	// 本当は使い回しは良くないが、待ちパターンに解除パターンを入れて戻す
				// break;
			}
		}
		if (task->isWaiting) flagTable[flgid].waitQueue.push(task);
	}

	/* <==== Critical */ LeaveCriticalSection(&criticalSection);

}

void SetFlag(ID flgid, FLGPTN setptn) {
	iSetFlag(flgid, setptn);
	if (running_task->isExist) TaskYield(); // 実行権を譲る
}

void ClearFlag(ID flgid, FLGPTN clearptn) {
	/* Critical ====> */ EnterCriticalSection(&criticalSection);
	flagTable[flgid].flgptn &= clearptn; // フラグのクリア
	/* <==== Critical */ LeaveCriticalSection(&criticalSection);
	if (running_task->isExist) TaskYield(); // 実行権を譲る
}

void WaitFlg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn) {

	/* Critical ====> */ EnterCriticalSection(&criticalSection);

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
		/* <==== Critical */ LeaveCriticalSection(&criticalSection);
		if (p_flgptn) *p_flgptn = currentFlags;
	}
	else {
		running_task->isWaiting = true; // 自タスクを待ち状態にする
		running_task->waitptn = waiptn;
		running_task->waitmode = wfmode;
		flagTable[flgid].waitQueue.push(running_task);
		/* <==== Critical */ LeaveCriticalSection(&criticalSection);
		if (running_task->isExist) TaskYield(); // 実行権を譲る
		if (p_flgptn) *p_flgptn = running_task->waitptn;	// 解除パターンを受け取る
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




void iSendDataQueue(ID dtqid, VP_INT data) {

	/* Critical ====> */ EnterCriticalSection(&criticalSection);

	dataQueueTable[dtqid].data = data;
	++dataQueueTable[dtqid].count;

	debug_printf("Send DataQueue data: %d\n", data);

	if (!dataQueueTable[dtqid].waitQueue.empty()) {
		std::shared_ptr<TaskInfo> task = dataQueueTable[dtqid].waitQueue.front();
		dataQueueTable[dtqid].waitQueue.pop();
		if (task->isExist && task->isWaiting) {
			debug_printf("Send DataQueue task: %s\n", task->taskName);
			task->receptData = dataQueueTable[dtqid].data;
			--dataQueueTable[dtqid].count;
			task->isWaiting = false;
			readyQueue.push(task); // 再度レディーキューに追加
		}
		if (task->isWaiting) dataQueueTable[dtqid].waitQueue.push(task);
	}

	/* <==== Critical */ LeaveCriticalSection(&criticalSection);

}

void pSendDataQueue(ID dtqid, VP_INT data) {
	iSendDataQueue(dtqid, data);
	if (running_task->isExist) TaskYield(); // 実行権を譲る
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
	if (running_task->isExist) TaskYield(); // 実行権を譲る
		*p_data = running_task->receptData;	// キューからデータを受け取る
	}
}

void ReferenceDataQueue(ID dtqid, T_RDTQ *pk_rdtq) {
	if (pk_rdtq) {
		pk_rdtq->sdtqcnt = dataQueueTable[dtqid].count;
	}
}

// ------------------------------------------

int startupTinyOS() {

	debug_printf("------- SYSTEM START -------\n");

	InitializeCriticalSection(&criticalSection);

	configTinyOS();

	// スケジューラーを開始
	yieldEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (yieldEvent == nullptr) {
		debug_printf("Failed to setup TinyOS.\n");
		return -1;
	}
	SetEvent(yieldEvent);
	return 0;
}

int stopRequestTinyOS() {
	for (auto& task : tasks) {
		DeleteTask(task);
		SetEvent(task->excuteEvent); // Wake up the task to let it exit
	}
	return 0;
}

int cleanupTinyOS() {
	// クリーンアップ
	for (auto& task : tasks) {
		WaitForSingleObject(task->threadHandle, INFINITE);
		CloseHandle(task->threadHandle);
		CloseHandle(task->excuteEvent);
	}
	CloseHandle(yieldEvent);
	debug_printf("------- SYSTEM END -------\n");
	return 0;
}

enum {
	WM_USER_TIMER = WM_USER + 1,
	WM_USER_TIMER2,
};

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
	case WM_CREATE:
		SetTimer(hWnd, WM_USER_TIMER, 500, nullptr); // タイマーイベントを設定
		SetTimer(hWnd, WM_USER_TIMER2, 10000, nullptr); // タイマーイベントを設定
		break;
	case WM_CLOSE:
		if (task_counter) {
			stopRequestTinyOS();
		}
		KillTimer(hWnd, WM_USER_TIMER); // タイマーイベントを破棄
		cleanupTinyOS();
		DestroyWindow(hWnd);
		break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
	case WM_TIMER:	// タイマーイベント
		if (wParam == WM_USER_TIMER) {
			StartDispatcher();
		}
		else if (wParam == WM_USER_TIMER2) {
			static int count = 0;
			// 非タスクからの呼び出しを想定
			switch (count++) {
			case 0:
				debug_printf("ActionTask(ID_MMM)\n");
				iWakeupTask(ID_MMM);
				break;
			case 1:
				// wait
				break;
			case 2:
				debug_printf("iSendDataQueue(ID_CCC, 765)\n");
				iSendDataQueue(ID_CCC, (VP_INT)765);
				break;
			case 3:
				debug_printf("iSetFlag(ID_AAA, 0x01)\n");
				iSetFlag(ID_AAA, 0x01);
				break;
			case 4:
				debug_printf("iSetFlag(ID_AAA, 0x02)\n");
				iSetFlag(ID_AAA, 0x02);
				break;
			default:
				debug_printf("count = 0\n");
				count = 0;
				break;
			}
		}
		break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// エントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // ウィンドウクラスの登録
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("TinyOSWindowClass");
    RegisterClass(&wc);

    // ウィンドウの作成
    HWND hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        TEXT("TinyOS"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hWnd == NULL) {
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);

	if (startupTinyOS()) {
		debug_printf("Failed to setup TinyOS.\n");
		return -1;
	}

    // メッセージループ
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
