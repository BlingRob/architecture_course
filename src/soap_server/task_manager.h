#pragma once

#include "task.h"
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class TaskManager {
public:
  TaskManager(const std::string &filePath = "tasks.dat");
  ~TaskManager();

  // Основные операции
  bool CreateTask(const Task &task);
  std::optional<Task> GetTask(const std::string &taskId) const;
  bool UpdateTask(const Task &task);
  bool DeleteTask(const std::string &taskId);

  // Дополнительные операции
  std::vector<Task> GetAllTasks() const;
  std::vector<Task> GetTasksByStatus(TaskStatus status) const;
  size_t GetTaskCount() const;

  // Поиск
  std::vector<Task> SearchTasks(const std::string &keyword) const;

  // Callback setters
  void SetCreateCallback(std::function<void(const Task &)> callback);
  void SetUpdateCallback(std::function<void(const Task &)> callback);
  void SetDeleteCallback(std::function<void(const std::string &)> callback);

  // Статистика
  struct Statistics {
    size_t totalTasks;
    size_t newTasks;
    size_t inProgressTasks;
    size_t completedTasks;
  };

  Statistics GetStatistics() const;

private:
  std::unordered_map<std::string, Task> tasks;
  mutable std::mutex tasksMutex;
  std::string dataFilePath;

  // Callbacks для событий
  std::function<void(const Task &)> onCreate;
  std::function<void(const Task &)> onUpdate;
  std::function<void(const std::string &)> onDelete;

  void SaveToFile() const;
  void LoadFromFile();
};