#ifndef __TINYOS_H__
#define __TINYOS_H__

#include <string>
#include <functional>

// タスクの関数プロトタイプ
using TaskFunction = std::function<void()>;

// ユーザータスクの定義はこの関数でユーザーが定義する
int configTinyOS();

// タスクの生成にはこの関数を使用する
void CreateTask(int id, const std::string& name, TaskFunction taskFunction);

// for DEBUG
void ViewTaskInfo();

#endif // __TINYOS_H__
