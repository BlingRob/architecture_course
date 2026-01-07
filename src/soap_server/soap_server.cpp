#include "soap_server.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

SOAPServer::SOAPServer(net::io_context &ioc, const tcp::endpoint &endpoint)
    : ioc_(ioc), acceptor(ioc_, endpoint) {
  //   std::cout << "SOAP Server starting on port " << port << std::endl;
}

void SOAPServer::Start() { DoAccept(); }

void SOAPServer::Stop() { acceptor.close(); }

void SOAPServer::DoAccept() {
  acceptor.async_accept([this](beast::error_code ec, tcp::socket socket) {
    if (!ec) {
      std::make_shared<std::thread>([this,
                                     socket = std::move(socket)]() mutable {
        HandleSession(std::make_shared<tcp::socket>(std::move(socket)));
      })->detach();
    }
    DoAccept();
  });
}

void SOAPServer::HandleSession(std::shared_ptr<tcp::socket> socket) {
  try {
    beast::flat_buffer buffer;

    while (true) {
      http::request<http::string_body> req;
      http::read(*socket, buffer, req);

      // Проверяем, что это POST запрос (SOAP обычно использует POST)
      if (req.method() == http::verb::post) {
        std::string soapResponse = HandleSOAPRequest(req.body());

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "TaskManagement SOAP Server");
        res.set(http::field::content_type, "text/xml; charset=utf-8");
        res.body() = soapResponse;
        res.prepare_payload();

        http::write(*socket, res);
      } else if (req.method() == http::verb::get) {
        // Возвращаем WSDL при GET запросе
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "TaskManagement SOAP Server");
        res.set(http::field::content_type, "text/xml; charset=utf-8");

        // Читаем WSDL файл
        std::ifstream wsdlFile("taskmanagement.wsdl");
        if (wsdlFile.is_open()) {
          std::stringstream buffer;
          buffer << wsdlFile.rdbuf();
          res.body() = buffer.str();
        } else {
          res.body() = "<?xml version=\"1.0\"?>"
                       "<wsdl:definitions "
                       "xmlns:wsdl=\"http://schemas.xmlsoap.org/wsdl/\">"
                       "<wsdl:message>WSDL доступен по адресу: "
                       "taskmanagement.wsdl</wsdl:message>"
                       "</wsdl:definitions>";
        }
        res.prepare_payload();
        http::write(*socket, res);
      } else {
        http::response<http::string_body> res{http::status::bad_request,
                                              req.version()};
        res.set(http::field::server, "TaskManagement SOAP Server");
        res.set(http::field::content_type, "text/plain");
        res.body() = "Invalid request method";
        res.prepare_payload();
        http::write(*socket, res);
      }

      // Проверяем, нужно ли закрыть соединение
      if (!req.keep_alive()) {
        break;
      }
    }

    socket->shutdown(tcp::socket::shutdown_send);
  } catch (const std::exception &e) {
    std::cerr << "Session error: " << e.what() << std::endl;
  }
}

std::string SOAPServer::HandleSOAPRequest(const std::string &soapRequest) {
  try {
    pugi::xml_document doc = ParseSOAPRequest(soapRequest);
    std::string responseBody = ProcessSOAPMessage(doc);
    return CreateSOAPResponse(responseBody);
  } catch (const std::exception &e) {
    return CreateSOAPFault("SOAP-ENV:Server", e.what());
  }
}

pugi::xml_document
SOAPServer::ParseSOAPRequest(const std::string &soapRequest) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(soapRequest.c_str());

  if (!result) {
    throw std::runtime_error("Failed to parse SOAP request: " +
                             std::string(result.description()));
  }

  return doc;
}

std::string SOAPServer::ProcessSOAPMessage(pugi::xml_document &doc) {
  // Получаем корневой элемент Envelope
  pugi::xml_node envelopeNode = doc.child("soap:Envelope");
  if (!envelopeNode) {
    envelopeNode = doc.child("Envelope");
    if (!envelopeNode) {
      throw std::runtime_error("SOAP Envelope not found");
    }
  }

  // Получаем элемент Body
  pugi::xml_node bodyNode = envelopeNode.child("soap:Body");
  if (!bodyNode) {
    bodyNode = envelopeNode.child("Body");
    if (!bodyNode) {
      throw std::runtime_error("SOAP Body not found");
    }
  }

  // Определяем операцию
  for (pugi::xml_node child = bodyNode.first_child(); child;
       child = child.next_sibling()) {
    std::string nodeName = child.name();

    if (nodeName == "CreateTaskRequest" ||
        nodeName == "tns:CreateTaskRequest") {
      return HandleCreateTask(child);
    } else if (nodeName == "GetTaskRequest" ||
               nodeName == "tns:GetTaskRequest") {
      return HandleGetTask(child);
    } else if (nodeName == "UpdateTaskRequest" ||
               nodeName == "tns:UpdateTaskRequest") {
      return HandleUpdateTask(child);
    } else if (nodeName == "DeleteTaskRequest" ||
               nodeName == "tns:DeleteTaskRequest") {
      return HandleDeleteTask(child);
    }
  }

  throw std::runtime_error("Unknown SOAP operation");
}

std::string SOAPServer::CreateSOAPResponse(const std::string &responseBody) {
  std::stringstream ss;
  ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ss << "<soap:Envelope "
        "xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n";
  ss << "  <soap:Body>\n";
  ss << responseBody << "\n";
  ss << "  </soap:Body>\n";
  ss << "</soap:Envelope>\n";
  return ss.str();
}

std::string SOAPServer::CreateSOAPFault(const std::string &errorCode,
                                        const std::string &errorMessage) {
  std::stringstream ss;
  ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ss << "<soap:Envelope "
        "xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n";
  ss << "  <soap:Body>\n";
  ss << "    <soap:Fault>\n";
  ss << "      <faultcode>" << errorCode << "</faultcode>\n";
  ss << "      <faultstring>" << errorMessage << "</faultstring>\n";
  ss << "    </soap:Fault>\n";
  ss << "  </soap:Body>\n";
  ss << "</soap:Envelope>\n";
  return ss.str();
}

std::string SOAPServer::GetNodeValue(pugi::xml_node &parent,
                                     const std::string &nodeName) {
  pugi::xml_node node = parent.child(nodeName.c_str());
  if (!node) {
    // Попробовать найти с префиксом
    std::string prefixedName = "tns:" + nodeName;
    node = parent.child(prefixedName.c_str());
  }

  if (node) {
    return node.text().as_string();
  }

  return "";
}

bool SOAPServer::ValidateTaskData(const std::string &taskId,
                                  const std::string &title) {
  if (taskId.empty()) {
    throw std::runtime_error("TaskID is required");
  }

  if (title.empty()) {
    throw std::runtime_error("Title is required");
  }

  // Проверка длины
  if (taskId.length() > 100) {
    throw std::runtime_error("TaskID is too long (max 100 characters)");
  }

  if (title.length() > 200) {
    throw std::runtime_error("Title is too long (max 200 characters)");
  }

  return true;
}

std::string SOAPServer::HandleCreateTask(pugi::xml_node &requestNode) {
  // Получаем элемент Task
  pugi::xml_node taskNode = requestNode.child("Task");
  if (!taskNode) {
    taskNode = requestNode.child("tns:Task");
    if (!taskNode) {
      throw std::runtime_error("Task element not found");
    }
  }

  // Извлекаем данные задачи
  std::string taskId = GetNodeValue(taskNode, "TaskID");
  std::string title = GetNodeValue(taskNode, "Title");
  std::string description = GetNodeValue(taskNode, "Description");
  std::string statusStr = GetNodeValue(taskNode, "Status");

  // Валидация
  ValidateTaskData(taskId, title);

  // Создаем задачу
  TaskStatus status =
      statusStr.empty() ? TaskStatus::NEW : StringToTaskStatus(statusStr);
  Task task(taskId, title, description, status);

  // Сохраняем задачу
  if (!taskManager.CreateTask(task)) {
    throw std::runtime_error("Task with ID '" + taskId + "' already exists");
  }

  return CreateXMLResponse(task);
}

std::string SOAPServer::HandleGetTask(pugi::xml_node &requestNode) {
  std::string taskId = GetNodeValue(requestNode, "TaskID");

  if (taskId.empty()) {
    throw std::runtime_error("TaskID is required");
  }

  auto task = taskManager.GetTask(taskId);
  if (!task) {
    throw std::runtime_error("Task with ID '" + taskId + "' not found");
  }

  return CreateXMLResponse(*task);
}

std::string SOAPServer::HandleUpdateTask(pugi::xml_node &requestNode) {
  // Получаем элемент Task
  pugi::xml_node taskNode = requestNode.child("Task");
  if (!taskNode) {
    taskNode = requestNode.child("tns:Task");
    if (!taskNode) {
      throw std::runtime_error("Task element not found");
    }
  }

  // Извлекаем данные задачи
  std::string taskId = GetNodeValue(taskNode, "TaskID");
  std::string title = GetNodeValue(taskNode, "Title");
  std::string description = GetNodeValue(taskNode, "Description");
  std::string statusStr = GetNodeValue(taskNode, "Status");

  if (taskId.empty()) {
    throw std::runtime_error("TaskID is required");
  }

  // Проверяем существование задачи
  auto existingTask = taskManager.GetTask(taskId);
  if (!existingTask) {
    throw std::runtime_error("Task with ID '" + taskId + "' not found");
  }

  // Если title не указан, используем существующий
  if (title.empty()) {
    title = existingTask->Title;
  }

  // Если description не указан, используем существующий
  if (description.empty()) {
    description = existingTask->Description;
  }

  // Если status не указан, используем существующий
  TaskStatus status;
  if (statusStr.empty()) {
    status = existingTask->Status;
  } else {
    status = StringToTaskStatus(statusStr);
  }

  // Создаем обновленную задачу
  Task task(taskId, title, description, status);

  // Обновляем задачу
  if (!taskManager.UpdateTask(task)) {
    throw std::runtime_error("Failed to update task with ID '" + taskId + "'");
  }

  return CreateXMLResponse(task);
}

std::string SOAPServer::HandleDeleteTask(pugi::xml_node &requestNode) {
  std::string taskId = GetNodeValue(requestNode, "TaskID");

  if (taskId.empty()) {
    throw std::runtime_error("TaskID is required");
  }

  bool success = taskManager.DeleteTask(taskId);
  return CreateDeleteResponse(success);
}

std::string SOAPServer::CreateXMLResponse(const Task &task) {
  pugi::xml_document doc;

  // Создаем корневой элемент Task
  pugi::xml_node taskNode = doc.append_child("Task");

  // Добавляем дочерние элементы
  pugi::xml_node idNode = taskNode.append_child("TaskID");
  idNode.append_child(pugi::node_pcdata).set_value(task.TaskID.c_str());

  pugi::xml_node titleNode = taskNode.append_child("Title");
  titleNode.append_child(pugi::node_pcdata).set_value(task.Title.c_str());

  pugi::xml_node descNode = taskNode.append_child("Description");
  descNode.append_child(pugi::node_pcdata).set_value(task.Description.c_str());

  pugi::xml_node statusNode = taskNode.append_child("Status");
  statusNode.append_child(pugi::node_pcdata)
      .set_value(TaskStatusToString(task.Status).c_str());

  // Сериализуем XML в строку
  std::stringstream ss;
  doc.save(ss, "  ");
  return ss.str();
}

std::string SOAPServer::CreateDeleteResponse(bool success) {
  pugi::xml_document doc;

  pugi::xml_node responseNode = doc.append_child("DeleteTaskResponse");

  pugi::xml_node successNode = responseNode.append_child("Success");
  successNode.append_child(pugi::node_pcdata)
      .set_value(success ? "true" : "false");

  std::stringstream ss;
  doc.save(ss, "  ");
  return ss.str();
}