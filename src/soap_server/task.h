#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>


enum class TaskStatus { NEW, IN_PROGRESS, COMPLETED };

std::string TaskStatusToString(TaskStatus status);
TaskStatus StringToTaskStatus(const std::string &statusStr);

struct Task {
  std::string TaskID;
  std::string Title;
  std::string Description;
  TaskStatus Status;

  Task() : Status(TaskStatus::NEW) {}

  Task(const std::string &id, const std::string &title, const std::string &desc,
       TaskStatus status = TaskStatus::NEW)
      : TaskID(id), Title(title), Description(desc), Status(status) {}
};