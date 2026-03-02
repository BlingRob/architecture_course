#include "chat_manager.h"

#include <algorithm>
#include <chrono>

namespace Chat {

Participant ChatManager::CreateParticipant(const std::string &name) {
  std::lock_guard<std::mutex> lock(mutex_);

  Participant participant;
  participant.id = next_participant_id_++;
  participant.name = name;
  participant.api_key = GenerateApiKey();

  participants_[participant.id] = participant;
  api_key_index_[participant.api_key] = participant.id;

  return participant;
}

bool ChatManager::Authenticate(const std::string &api_key,
                               int64_t &participant_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = api_key_index_.find(api_key);
  if (it == api_key_index_.end()) {
    return false;
  }

  participant_id = it->second;
  return true;
}

ChatRoom ChatManager::CreateRoom(const std::string &name, int64_t creator_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  ChatRoom room;
  room.id = next_room_id_++;
  room.name = name;
  room.participants.insert(creator_id);

  rooms_[room.id] = room;
  return room;
}

ChatManager::RoomMutationResult
ChatManager::AddParticipantToRoom(int64_t room_id, int64_t requester_id,
                                  int64_t participant_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return RoomMutationResult::RoomNotFound;
  }

  if (participants_.find(participant_id) == participants_.end()) {
    return RoomMutationResult::ParticipantNotFound;
  }

  auto &room = room_it->second;
  if (!room.participants.contains(requester_id)) {
    return RoomMutationResult::Forbidden;
  }

  if (room.participants.contains(participant_id)) {
    return RoomMutationResult::AlreadyMember;
  }

  room.participants.insert(participant_id);
  return RoomMutationResult::Ok;
}

ChatManager::RoomMutationResult
ChatManager::RemoveParticipantFromRoom(int64_t room_id, int64_t requester_id,
                                       int64_t participant_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return RoomMutationResult::RoomNotFound;
  }

  auto &room = room_it->second;
  if (!room.participants.contains(requester_id)) {
    return RoomMutationResult::Forbidden;
  }

  if (!room.participants.contains(participant_id)) {
    return RoomMutationResult::NotMember;
  }

  room.participants.erase(participant_id);
  return RoomMutationResult::Ok;
}

ChatManager::MessagePostResult
ChatManager::CreateMessage(int64_t room_id, int64_t author_id,
                         const std::string &type, const std::string &text,
                         const std::string &image_url,
                         Message &created_message) {
  std::lock_guard<std::mutex> lock(mutex_);

  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return MessagePostResult::RoomNotFound;
  }

  auto &room = room_it->second;
  if (!room.participants.contains(author_id)) {
    return MessagePostResult::NotInRoom;
  }

  const bool is_text = type == "text";
  const bool is_image = type == "image";
  if (!is_text && !is_image) {
    return MessagePostResult::InvalidPayload;
  }

  if (is_text && text.empty()) {
    return MessagePostResult::InvalidPayload;
  }

  if (is_image && image_url.empty()) {
    return MessagePostResult::InvalidPayload;
  }

  Message message;
  message.id = next_message_id_++;
  message.room_id = room_id;
  message.author_id = author_id;
  message.type = type;
  message.text = text;
  message.image_url = image_url;
  message.created_at_ms = NowUnixMs();

  room.messages.push_back(message);
  created_message = message;

  cv_.notify_all();
  return MessagePostResult::Ok;
}

ChatManager::ReadResult ChatManager::GetMessages(
    int64_t room_id, int64_t requester_id, int64_t since_id, int64_t from_ts_ms,
    int64_t to_ts_ms, std::vector<Message> &out_messages) const {
  std::lock_guard<std::mutex> lock(mutex_);

  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return ReadResult::RoomNotFound;
  }

  const auto &room = room_it->second;
  if (!room.participants.contains(requester_id)) {
    return ReadResult::NotInRoom;
  }

  out_messages = FilterMessages(room, since_id, from_ts_ms, to_ts_ms);
  return ReadResult::Ok;
}

ChatManager::ReadResult ChatManager::PollMessages(
    int64_t room_id, int64_t requester_id, int64_t since_id, int64_t from_ts_ms,
    int timeout_seconds, std::vector<Message> &out_messages) {
  std::unique_lock<std::mutex> lock(mutex_);

  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return ReadResult::RoomNotFound;
  }

  auto &room = room_it->second;
  if (!room.participants.contains(requester_id)) {
    return ReadResult::NotInRoom;
  }

  out_messages = FilterMessages(room, since_id, from_ts_ms, -1);
  if (!out_messages.empty()) {
    return ReadResult::Ok;
  }

  if (timeout_seconds < 1) {
    timeout_seconds = 1;
  }

  cv_.wait_for(lock, std::chrono::seconds(timeout_seconds), [&] {
    const auto available = FilterMessages(room, since_id, from_ts_ms, -1);
    return !available.empty();
  });

  out_messages = FilterMessages(room, since_id, from_ts_ms, -1);
  return ReadResult::Ok;
}

int64_t ChatManager::NowUnixMs() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
      .count();
}

std::vector<Message> ChatManager::FilterMessages(const ChatRoom &room,
                                                 int64_t since_id,
                                                 int64_t from_ts_ms,
                                                 int64_t to_ts_ms) {
  std::vector<Message> result;
  result.reserve(room.messages.size());

  for (const auto &message : room.messages) {
    if (message.id <= since_id) {
      continue;
    }

    if (from_ts_ms > 0 && message.created_at_ms < from_ts_ms) {
      continue;
    }

    if (to_ts_ms > 0 && message.created_at_ms > to_ts_ms) {
      continue;
    }

    result.push_back(message);
  }

  return result;
}

std::string ChatManager::GenerateApiKey() {
  static constexpr char hex[] = "0123456789abcdef";
  std::string key;
  key.reserve(32);

  for (int i = 0; i < 32; ++i) {
    const uint64_t value = rng_() & 0x0F;
    key.push_back(hex[value]);
  }

  return key;
}

} // namespace Chat


