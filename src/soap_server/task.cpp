#include "task.h"

std::string TaskStatusToString(TaskStatus status) {
  switch (status) {
  case TaskStatus::NEW:
    return "New";
  case TaskStatus::IN_PROGRESS:
    return "In Progress";
  case TaskStatus::COMPLETED:
    return "Completed";
  default:
    return "Unknown";
  }
}

TaskStatus StringToTaskStatus(const std::string &statusStr) {
  if (statusStr == "New" || statusStr == "new")
    return TaskStatus::NEW;
  if (statusStr == "In Progress" || statusStr == "in progress")
    return TaskStatus::IN_PROGRESS;
  if (statusStr == "Completed" || statusStr == "completed")
    return TaskStatus::COMPLETED;
  return TaskStatus::NEW;
}