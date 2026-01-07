#include "task_manager.h"
#include <algorithm>
#include <fstream>
#include <iostream>

TaskManager::TaskManager(const std::string &filePath) : dataFilePath(filePath) {
  LoadFromFile();
}

TaskManager::~TaskManager() { SaveToFile(); }

void TaskManager::LoadFromFile() {
  std::lock_guard lock(tasksMutex);

  std::ifstream file(dataFilePath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Could not open data file, starting with empty task list."
              << std::endl;
    return;
  }

  size_t count;
  file.read(reinterpret_cast<char *>(&count), sizeof(count));

  for (size_t i = 0; i < count; ++i) {
    Task task;

    // Читаем TaskID
    size_t idLength;
    file.read(reinterpret_cast<char *>(&idLength), sizeof(idLength));
    char *idBuffer = new char[idLength + 1];
    file.read(idBuffer, idLength);
    idBuffer[idLength] = '\0';
    task.TaskID = idBuffer;
    delete[] idBuffer;

    // Читаем Title
    size_t titleLength;
    file.read(reinterpret_cast<char *>(&titleLength), sizeof(titleLength));
    char *titleBuffer = new char[titleLength + 1];
    file.read(titleBuffer, titleLength);
    titleBuffer[titleLength] = '\0';
    task.Title = titleBuffer;
    delete[] titleBuffer;

    // Читаем Description
    size_t descLength;
    file.read(reinterpret_cast<char *>(&descLength), sizeof(descLength));
    char *descBuffer = new char[descLength + 1];
    file.read(descBuffer, descLength);
    descBuffer[descLength] = '\0';
    task.Description = descBuffer;
    delete[] descBuffer;

    // Читаем Status
    int statusInt;
    file.read(reinterpret_cast<char *>(&statusInt), sizeof(statusInt));
    task.Status = static_cast<TaskStatus>(statusInt);

    tasks[task.TaskID] = task;
  }

  file.close();
  std::cout << "Loaded " << count << " tasks from file." << std::endl;
}

void TaskManager::SaveToFile() const {
  std::lock_guard lock(tasksMutex);

  std::ofstream file(dataFilePath, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    std::cerr << "Could not open data file for writing." << std::endl;
    return;
  }

  size_t count = tasks.size();
  file.write(reinterpret_cast<const char *>(&count), sizeof(count));

  for (const auto &[id, task] : tasks) {
    // Записываем TaskID
    size_t idLength = task.TaskID.size();
    file.write(reinterpret_cast<const char *>(&idLength), sizeof(idLength));
    file.write(task.TaskID.c_str(), idLength);

    // Записываем Title
    size_t titleLength = task.Title.size();
    file.write(reinterpret_cast<const char *>(&titleLength),
               sizeof(titleLength));
    file.write(task.Title.c_str(), titleLength);

    // Записываем Description
    size_t descLength = task.Description.size();
    file.write(reinterpret_cast<const char *>(&descLength), sizeof(descLength));
    file.write(task.Description.c_str(), descLength);

    // Записываем Status
    int statusInt = static_cast<int>(task.Status);
    file.write(reinterpret_cast<const char *>(&statusInt), sizeof(statusInt));
  }

  file.close();
  std::cout << "Saved " << count << " tasks to file." << std::endl;
}

bool TaskManager::CreateTask(const Task &task) {
  std::lock_guard lock(tasksMutex);

  if (tasks.find(task.TaskID) != tasks.end()) {
    return false;
  }

  tasks[task.TaskID] = task;

  if (onCreate) {
    onCreate(task);
  }

  return true;
}

std::optional<Task> TaskManager::GetTask(const std::string &taskId) const {
  std::lock_guard lock(tasksMutex);

  auto it = tasks.find(taskId);
  if (it != tasks.end()) {
    return it->second;
  }

  return std::nullopt;
}

bool TaskManager::UpdateTask(const Task &task) {
  std::lock_guard lock(tasksMutex);

  auto it = tasks.find(task.TaskID);
  if (it == tasks.end()) {
    return false;
  }

  it->second = task;

  if (onUpdate) {
    onUpdate(task);
  }

  return true;
}

bool TaskManager::DeleteTask(const std::string &taskId) {
  std::lock_guard lock(tasksMutex);

  auto it = tasks.find(taskId);
  if (it == tasks.end()) {
    return false;
  }

  tasks.erase(it);

  if (onDelete) {
    onDelete(taskId);
  }

  return true;
}

std::vector<Task> TaskManager::GetAllTasks() const {
  std::lock_guard lock(tasksMutex);

  std::vector<Task> result;
  result.reserve(tasks.size());

  for (const auto &[id, task] : tasks) {
    result.push_back(task);
  }

  return result;
}

std::vector<Task> TaskManager::GetTasksByStatus(TaskStatus status) const {
  std::lock_guard lock(tasksMutex);

  std::vector<Task> result;

  for (const auto &[id, task] : tasks) {
    if (task.Status == status) {
      result.push_back(task);
    }
  }

  return result;
}

size_t TaskManager::GetTaskCount() const {
  std::lock_guard lock(tasksMutex);
  return tasks.size();
}

std::vector<Task> TaskManager::SearchTasks(const std::string &keyword) const {
  std::lock_guard lock(tasksMutex);

  std::vector<Task> result;
  std::string lowerKeyword = keyword;
  std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(),
                 ::tolower);

  for (const auto &[id, task] : tasks) {
    std::string lowerTitle = task.Title;
    std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(),
                   ::tolower);

    std::string lowerDesc = task.Description;
    std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(),
                   ::tolower);

    if (lowerTitle.find(lowerKeyword) != std::string::npos ||
        lowerDesc.find(lowerKeyword) != std::string::npos) {
      result.push_back(task);
    }
  }

  return result;
}

void TaskManager::SetCreateCallback(
    std::function<void(const Task &)> callback) {
  onCreate = callback;
}

void TaskManager::SetUpdateCallback(
    std::function<void(const Task &)> callback) {
  onUpdate = callback;
}

void TaskManager::SetDeleteCallback(
    std::function<void(const std::string &)> callback) {
  onDelete = callback;
}

TaskManager::Statistics TaskManager::GetStatistics() const {
  std::lock_guard lock(tasksMutex);

  Statistics stats{};
  stats.totalTasks = tasks.size();

  for (const auto &[id, task] : tasks) {
    switch (task.Status) {
    case TaskStatus::NEW:
      stats.newTasks++;
      break;
    case TaskStatus::IN_PROGRESS:
      stats.inProgressTasks++;
      break;
    case TaskStatus::COMPLETED:
      stats.completedTasks++;
      break;
    }
  }

  return stats;
}