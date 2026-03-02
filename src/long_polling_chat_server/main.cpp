#include "chat_manager.h"

#include "httplib.h"
#include "logger.h"

#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <toml.hpp>

using json = nlohmann::json;
using namespace httplib;

namespace {

bool ParseJsonRequest(const Request &req, json &out) {
  try {
    out = json::parse(req.body);
    return true;
  } catch (const json::exception &) {
    return false;
  }
}

void SendError(Response &res, int status, const std::string &message) {
  res.status = status;
  res.set_content(json{{"error", true}, {"message", message}}.dump(),
                  "application/json");
}

json MessageToJson(const Chat::Message &message) {
  return json{{"id", message.id},
              {"roomId", message.room_id},
              {"authorId", message.author_id},
              {"type", message.type},
              {"text", message.text},
              {"imageUrl", message.image_url},
              {"createdAtMs", message.created_at_ms}};
}

std::optional<int64_t> ParseInt64(const std::string &value) {
  try {
    std::size_t pos = 0;
    const auto parsed = std::stoll(value, &pos);
    if (pos != value.size()) {
      return std::nullopt;
    }
    return parsed;
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

int64_t QueryInt64(const Request &req, const std::string &name,
                  int64_t default_value) {
  if (!req.has_param(name)) {
    return default_value;
  }

  const auto raw = req.get_param_value(name);
  const auto parsed = ParseInt64(raw);
  if (!parsed.has_value()) {
    return default_value;
  }

  return *parsed;
}

bool ResolveAuthParticipant(const Request &req, Chat::ChatManager &manager,
                            int64_t &participant_id, Response &res) {
  const std::string api_key = req.get_header_value("X-API-Key");
  if (api_key.empty()) {
    SendError(res, 401, "Missing X-API-Key header");
    return false;
  }

  if (!manager.Authenticate(api_key, participant_id)) {
    SendError(res, 401, "Invalid API key");
    return false;
  }

  return true;
}

} // namespace

int main() {
  try {
    toml::table cfg;
    const std::string config_file_path{"cfg.toml"};

    if (!std::filesystem::exists(config_file_path)) {
      throw std::runtime_error("Config file not found: " + config_file_path);
    }

    cfg = toml::parse_file(config_file_path);
    Logging::LoggerFactory::Init(cfg);
    auto logger = Logging::LoggerFactory::GetLogger(
        cfg["logging"]["filename"].value_or("long_polling_chat_server.log"));

    const std::string address{
        cfg["server_parameters"]["host"].value_or("0.0.0.0")};
    const unsigned short port{static_cast<unsigned short>(
        cfg["server_parameters"]["port"].value_or(17000))};

    Chat::ChatManager chat_manager;
    Server svr;

    svr.new_task_queue = [] { return new ThreadPool(16); };

    svr.Options(".*", [](const Request &, Response &res) {
      res.set_header("Access-Control-Allow-Origin", "*");
      res.set_header("Access-Control-Allow-Methods",
                     "GET, POST, PUT, DELETE, OPTIONS");
      res.set_header("Access-Control-Allow-Headers",
                     "Content-Type, X-API-Key");
      res.status = 204;
    });

    svr.set_pre_routing_handler([](const Request &, Response &res) {
      res.set_header("Access-Control-Allow-Origin", "*");
      return Server::HandlerResponse::Unhandled;
    });

    svr.Get("/", [](const Request &, Response &res) {
      const json response = {
          {"name", "Long Polling Chat API"},
          {"version", "1.0.0"},
          {"auth", "X-API-Key header"},
          {"endpoints",
           {{"POST /participants", "Create participant and API key"},
            {"POST /rooms", "Create room"},
            {"POST /rooms/{id}/participants", "Add participant to room"},
            {"DELETE /rooms/{id}/participants/{participantId}",
             "Remove participant from room"},
            {"POST /rooms/{id}/messages", "Send message to room"},
            {"GET /rooms/{id}/messages", "Get messages with time filters"},
            {"GET /rooms/{id}/messages/poll", "Long polling for new messages"},
            {"GET /health", "Health check"}}}};
      res.set_content(response.dump(), "application/json");
    });

    svr.Get("/health", [](const Request &, Response &res) {
      res.set_content(json{{"status", "ok"}}.dump(), "application/json");
    });

    svr.Post("/participants", [&](const Request &req, Response &res) {
      json body;
      if (!ParseJsonRequest(req, body)) {
        SendError(res, 400, "Invalid JSON body");
        return;
      }

      if (!body.contains("name") || !body["name"].is_string()) {
        SendError(res, 400, "Field 'name' is required");
        return;
      }

      const std::string name = body["name"].get<std::string>();
      if (name.empty()) {
        SendError(res, 400, "Field 'name' must be non-empty");
        return;
      }

      const auto participant = chat_manager.CreateParticipant(name);
      res.status = 201;
      res.set_content(json{{"id", participant.id},
                           {"name", participant.name},
                           {"apiKey", participant.api_key}}
                          .dump(),
                      "application/json");
    });

    svr.Post("/rooms", [&](const Request &req, Response &res) {
      int64_t requester_id = 0;
      if (!ResolveAuthParticipant(req, chat_manager, requester_id, res)) {
        return;
      }

      json body;
      if (!ParseJsonRequest(req, body)) {
        SendError(res, 400, "Invalid JSON body");
        return;
      }

      if (!body.contains("name") || !body["name"].is_string()) {
        SendError(res, 400, "Field 'name' is required");
        return;
      }

      const std::string name = body["name"].get<std::string>();
      if (name.empty()) {
        SendError(res, 400, "Field 'name' must be non-empty");
        return;
      }

      const auto room = chat_manager.CreateRoom(name, requester_id);
      res.status = 201;
      res.set_content(json{{"id", room.id}, {"name", room.name}}.dump(),
                      "application/json");
    });

    svr.Post(R"(/rooms/(\d+)/participants)",
             [&](const Request &req, Response &res) {
               int64_t requester_id = 0;
               if (!ResolveAuthParticipant(req, chat_manager, requester_id,
                                           res)) {
                 return;
               }

               json body;
               if (!ParseJsonRequest(req, body)) {
                 SendError(res, 400, "Invalid JSON body");
                 return;
               }

               if (!body.contains("participantId") ||
                   !body["participantId"].is_number_integer()) {
                 SendError(res, 400, "Field 'participantId' is required");
                 return;
               }

               const int64_t room_id = std::stoll(req.matches[1].str());
               const int64_t participant_id = body["participantId"].get<int64_t>();

               const auto result = chat_manager.AddParticipantToRoom(
                   room_id, requester_id, participant_id);

               if (result == Chat::ChatManager::RoomMutationResult::RoomNotFound) {
                 SendError(res, 404, "Room not found");
                 return;
               }

               if (result ==
                   Chat::ChatManager::RoomMutationResult::ParticipantNotFound) {
                 SendError(res, 404, "Participant not found");
                 return;
               }

               if (result == Chat::ChatManager::RoomMutationResult::Forbidden) {
                 SendError(res, 403, "Requester is not in room");
                 return;
               }

               if (result ==
                   Chat::ChatManager::RoomMutationResult::AlreadyMember) {
                 SendError(res, 409, "Participant is already in room");
                 return;
               }

               res.status = 204;
             });

    svr.Delete(R"(/rooms/(\d+)/participants/(\d+))",
               [&](const Request &req, Response &res) {
                 int64_t requester_id = 0;
                 if (!ResolveAuthParticipant(req, chat_manager, requester_id,
                                             res)) {
                   return;
                 }

                 const int64_t room_id = std::stoll(req.matches[1].str());
                 const int64_t participant_id = std::stoll(req.matches[2].str());

                 const auto result = chat_manager.RemoveParticipantFromRoom(
                     room_id, requester_id, participant_id);

                 if (result ==
                     Chat::ChatManager::RoomMutationResult::RoomNotFound) {
                   SendError(res, 404, "Room not found");
                   return;
                 }

                 if (result == Chat::ChatManager::RoomMutationResult::Forbidden) {
                   SendError(res, 403, "Requester is not in room");
                   return;
                 }

                 if (result == Chat::ChatManager::RoomMutationResult::NotMember) {
                   SendError(res, 404, "Participant is not in room");
                   return;
                 }

                 res.status = 204;
               });

    svr.Post(R"(/rooms/(\d+)/messages)", [&](const Request &req, Response &res) {
      int64_t requester_id = 0;
      if (!ResolveAuthParticipant(req, chat_manager, requester_id, res)) {
        return;
      }

      json body;
      if (!ParseJsonRequest(req, body)) {
        SendError(res, 400, "Invalid JSON body");
        return;
      }

      if (!body.contains("type") || !body["type"].is_string()) {
        SendError(res, 400, "Field 'type' is required");
        return;
      }

      const int64_t room_id = std::stoll(req.matches[1].str());
      const std::string type = body["type"].get<std::string>();
      const std::string text =
          body.contains("text") && body["text"].is_string()
              ? body["text"].get<std::string>()
              : std::string{};
      const std::string image_url =
          body.contains("imageUrl") && body["imageUrl"].is_string()
              ? body["imageUrl"].get<std::string>()
              : std::string{};

      Chat::Message created_message;
      const auto result = chat_manager.CreateMessage(room_id, requester_id, type,
                                                   text, image_url,
                                                   created_message);

      if (result == Chat::ChatManager::MessagePostResult::RoomNotFound) {
        SendError(res, 404, "Room not found");
        return;
      }

      if (result == Chat::ChatManager::MessagePostResult::NotInRoom) {
        SendError(res, 403, "Requester is not in room");
        return;
      }

      if (result == Chat::ChatManager::MessagePostResult::InvalidPayload) {
        SendError(res, 400,
                  "Invalid message payload for selected type (text/image)");
        return;
      }

      res.status = 201;
      res.set_content(MessageToJson(created_message).dump(), "application/json");
    });

    svr.Get(R"(/rooms/(\d+)/messages)", [&](const Request &req, Response &res) {
      int64_t requester_id = 0;
      if (!ResolveAuthParticipant(req, chat_manager, requester_id, res)) {
        return;
      }

      const int64_t room_id = std::stoll(req.matches[1].str());
      const int64_t since_id = QueryInt64(req, "sinceId", 0);
      const int64_t from_ts = QueryInt64(req, "fromTs", 0);
      const int64_t to_ts = QueryInt64(req, "toTs", -1);

      std::vector<Chat::Message> messages;
      const auto result = chat_manager.GetMessages(room_id, requester_id,
                                                   since_id, from_ts, to_ts,
                                                   messages);

      if (result == Chat::ChatManager::ReadResult::RoomNotFound) {
        SendError(res, 404, "Room not found");
        return;
      }

      if (result == Chat::ChatManager::ReadResult::NotInRoom) {
        SendError(res, 403, "Requester is not in room");
        return;
      }

      json items = json::array();
      for (const auto &message : messages) {
        items.push_back(MessageToJson(message));
      }

      res.set_content(items.dump(), "application/json");
    });

    svr.Get(R"(/rooms/(\d+)/messages/poll)",
            [&](const Request &req, Response &res) {
              int64_t requester_id = 0;
              if (!ResolveAuthParticipant(req, chat_manager, requester_id,
                                          res)) {
                return;
              }

              const int64_t room_id = std::stoll(req.matches[1].str());
              const int64_t since_id = QueryInt64(req, "sinceId", 0);
              const int64_t from_ts = QueryInt64(req, "fromTs", 0);
              int timeout = static_cast<int>(QueryInt64(req, "timeout", 25));
              if (timeout < 1) {
                timeout = 1;
              }
              if (timeout > 60) {
                timeout = 60;
              }

              std::vector<Chat::Message> messages;
              const auto result = chat_manager.PollMessages(
                  room_id, requester_id, since_id, from_ts, timeout, messages);

              if (result == Chat::ChatManager::ReadResult::RoomNotFound) {
                SendError(res, 404, "Room not found");
                return;
              }

              if (result == Chat::ChatManager::ReadResult::NotInRoom) {
                SendError(res, 403, "Requester is not in room");
                return;
              }

              if (messages.empty()) {
                res.status = 204;
                return;
              }

              json items = json::array();
              for (const auto &message : messages) {
                items.push_back(MessageToJson(message));
              }

              res.set_content(items.dump(), "application/json");
            });

    LOG_INFO(logger.get(), "Long polling chat server starting on {}:{}", address,
             port);
    std::cout << "Long polling chat server started on http://" << address << ":"
              << port << std::endl;

    svr.listen(address.c_str(), port);
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}


