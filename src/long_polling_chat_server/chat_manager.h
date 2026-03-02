#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Chat {

struct Participant {
  int64_t id{};
  std::string name;
  std::string api_key;
};

struct Message {
  int64_t id{};
  int64_t room_id{};
  int64_t author_id{};
  std::string type;
  std::string text;
  std::string image_url;
  int64_t created_at_ms{};
};

struct ChatRoom {
  int64_t id{};
  std::string name;
  std::unordered_set<int64_t> participants;
  std::vector<Message> messages;
};

class ChatManager {
public:
  enum class RoomMutationResult {
    Ok,
    RoomNotFound,
    ParticipantNotFound,
    Forbidden,
    AlreadyMember,
    NotMember
  };

  enum class MessagePostResult {
    Ok,
    RoomNotFound,
    NotInRoom,
    InvalidPayload
  };

  enum class ReadResult {
    Ok,
    RoomNotFound,
    NotInRoom
  };

  Participant CreateParticipant(const std::string &name);
  bool Authenticate(const std::string &api_key, int64_t &participant_id) const;

  ChatRoom CreateRoom(const std::string &name, int64_t creator_id);
  RoomMutationResult AddParticipantToRoom(int64_t room_id,
                                          int64_t requester_id,
                                          int64_t participant_id);
  RoomMutationResult RemoveParticipantFromRoom(int64_t room_id,
                                               int64_t requester_id,
                                               int64_t participant_id);

  MessagePostResult CreateMessage(int64_t room_id, int64_t author_id,
                                const std::string &type,
                                const std::string &text,
                                const std::string &image_url,
                                Message &created_message);

  ReadResult GetMessages(int64_t room_id, int64_t requester_id, int64_t since_id,
                         int64_t from_ts_ms, int64_t to_ts_ms,
                         std::vector<Message> &out_messages) const;

  ReadResult PollMessages(int64_t room_id, int64_t requester_id, int64_t since_id,
                          int64_t from_ts_ms, int timeout_seconds,
                          std::vector<Message> &out_messages);

private:
  static int64_t NowUnixMs();
  static std::vector<Message> FilterMessages(const ChatRoom &room,
                                             int64_t since_id,
                                             int64_t from_ts_ms,
                                             int64_t to_ts_ms);

  std::string GenerateApiKey();

private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::unordered_map<int64_t, Participant> participants_;
  std::unordered_map<std::string, int64_t> api_key_index_;
  std::unordered_map<int64_t, ChatRoom> rooms_;
  int64_t next_participant_id_{1};
  int64_t next_room_id_{1};
  int64_t next_message_id_{1};
  std::mt19937_64 rng_{std::random_device{}()};
};

} // namespace Chat


