#pragma once

#include "task_manager.h"
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <memory>
#include <pugixml.hpp>
#include <string>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class SOAPServer {
public:
  SOAPServer(net::io_context &ioc, const tcp::endpoint &endpoint);

  void Start();
  void Stop();

private:
  // SOAP обработчики
  std::string HandleSOAPRequest(const std::string &soapRequest);
  std::string ProcessSOAPMessage(pugi::xml_document &doc);

  // XML парсинг с pugixml
  pugi::xml_document ParseSOAPRequest(const std::string &soapRequest);
  std::string CreateSOAPResponse(const std::string &responseBody);
  std::string CreateSOAPFault(const std::string &errorCode,
                              const std::string &errorMessage);

  // Обработчики операций
  std::string HandleCreateTask(pugi::xml_node &bodyNode);
  std::string HandleGetTask(pugi::xml_node &bodyNode);
  std::string HandleUpdateTask(pugi::xml_node &bodyNode);
  std::string HandleDeleteTask(pugi::xml_node &bodyNode);

  // Вспомогательные методы
  std::string GetNodeValue(pugi::xml_node &parent, const std::string &nodeName);
  std::string CreateXMLResponse(const Task &task);
  std::string CreateDeleteResponse(bool success);

  // Валидация
  bool ValidateTaskData(const std::string &taskId, const std::string &title);

  void DoAccept();
  void HandleSession(std::shared_ptr<tcp::socket> socket);

private:
  net::io_context &ioc_;
  tcp::acceptor acceptor;
  TaskManager taskManager;
};