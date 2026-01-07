#include "httplib.h"
#include "logger.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <toml.hpp>
#include <vector>

using json = nlohmann::json;
using namespace httplib;

// Класс для хранения данных об авторе
class Author {
public:
  int id;
  std::string firstName;
  std::string lastName;
  std::string dob; // Дата рождения в формате YYYY-MM-DD

  Author() : id(0) {}
  Author(int id, const std::string &fn, const std::string &ln,
         const std::string &dob)
      : id(id), firstName(fn), lastName(ln), dob(dob) {}

  json toJson() const {
    return {{"id", id},
            {"firstName", firstName},
            {"lastName", lastName},
            {"dob", dob}};
  }

  json toJsonWithBookCount(int bookCount) const {
    auto j = toJson();
    j["booksWritten"] = bookCount;
    return j;
  }
};

// Класс для хранения данных о книге
class Book {
public:
  int id;
  std::string title;
  std::string genre;
  int year;
  int authorId;

  Book() : id(0), year(0), authorId(0) {}
  Book(int id, const std::string &t, const std::string &g, int y, int aid)
      : id(id), title(t), genre(g), year(y), authorId(aid) {}

  json toJson() const {
    return {{"id", id},
            {"title", title},
            {"genre", genre},
            {"year", year},
            {"authorId", authorId}};
  }
};

// Класс для хранения данных (простая "база данных" в памяти)
class Database {
private:
  std::map<int, Author> authors;
  std::map<int, Book> books;
  int nextAuthorId = 1;
  int nextBookId = 1;
  std::mutex mtx;

public:
  // Авторы
  Author addAuthor(const std::string &firstName, const std::string &lastName,
                   const std::string &dob) {
    std::lock_guard<std::mutex> lock(mtx);
    Author author(nextAuthorId, firstName, lastName, dob);
    authors[nextAuthorId] = author;
    nextAuthorId++;
    return author;
  }

  Author getAuthor(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = authors.find(id);
    if (it != authors.end()) {
      return it->second;
    }
    return Author(); // Пустой автор если не найден
  }

  bool updateAuthor(int id, const std::string &firstName,
                    const std::string &lastName, const std::string &dob) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = authors.find(id);
    if (it != authors.end()) {
      it->second.firstName = firstName;
      it->second.lastName = lastName;
      it->second.dob = dob;
      return true;
    }
    return false;
  }

  bool deleteAuthor(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    // Проверяем, есть ли книги у автора
    for (const auto &pair : books) {
      if (pair.second.authorId == id) {
        return false; // Нельзя удалить автора с книгами
      }
    }

    return authors.erase(id) > 0;
  }

  std::vector<Author> getAllAuthors() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Author> result;
    for (const auto &pair : authors) {
      result.push_back(pair.second);
    }
    return result;
  }

  int getBookCountForAuthor(int authorId) {
    std::lock_guard<std::mutex> lock(mtx);
    int count = 0;
    for (const auto &pair : books) {
      if (pair.second.authorId == authorId) {
        count++;
      }
    }
    return count;
  }

  // Книги
  Book addBook(const std::string &title, const std::string &genre, int year,
               int authorId) {
    std::lock_guard<std::mutex> lock(mtx);
    // Проверяем существование автора
    if (authors.find(authorId) == authors.end()) {
      throw std::runtime_error("Author not found");
    }

    Book book(nextBookId, title, genre, year, authorId);
    books[nextBookId] = book;
    nextBookId++;
    return book;
  }

  Book getBook(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = books.find(id);
    if (it != books.end()) {
      return it->second;
    }
    return Book(); // Пустая книга если не найдена
  }

  bool updateBook(int id, const std::string &title, const std::string &genre,
                  int year, int authorId) {
    std::lock_guard<std::mutex> lock(mtx);
    // Проверяем существование автора
    if (authors.find(authorId) == authors.end()) {
      return false;
    }

    auto it = books.find(id);
    if (it != books.end()) {
      it->second.title = title;
      it->second.genre = genre;
      it->second.year = year;
      it->second.authorId = authorId;
      return true;
    }
    return false;
  }

  bool deleteBook(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    return books.erase(id) > 0;
  }

  std::vector<Book> getAllBooks() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Book> result;
    for (const auto &pair : books) {
      result.push_back(pair.second);
    }
    return result;
  }

  std::vector<Book> getBooksByGenre(const std::string &genre) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Book> result;
    for (const auto &pair : books) {
      if (pair.second.genre == genre) {
        result.push_back(pair.second);
      }
    }
    return result;
  }

  std::vector<Book> getPaginatedBooks(int page, int limit) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Book> allBooks;
    for (const auto &pair : books) {
      allBooks.push_back(pair.second);
    }

    // Применяем пагинацию
    int start = (page - 1) * limit;
    if (start < 0)
      start = 0;
    if (start >= (int)allBooks.size()) {
      return {};
    }

    int end = std::min(start + limit, (int)allBooks.size());
    return std::vector<Book>(allBooks.begin() + start, allBooks.begin() + end);
  }

  std::vector<Book> getFilteredAndPaginatedBooks(const std::string &genre,
                                                 int page, int limit) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Book> filtered;

    // Фильтрация по жанру
    for (const auto &pair : books) {
      if (genre.empty() || pair.second.genre == genre) {
        filtered.push_back(pair.second);
      }
    }

    // Применяем пагинацию
    int start = (page - 1) * limit;
    if (start < 0)
      start = 0;
    if (start >= (int)filtered.size()) {
      return {};
    }

    int end = std::min(start + limit, (int)filtered.size());
    return std::vector<Book>(filtered.begin() + start, filtered.begin() + end);
  }
};

// Глобальная база данных
Database db;

// Функция для парсинга JSON запроса
bool parseJsonRequest(const Request &req, json &j) {
  try {
    j = json::parse(req.body);
    return true;
  } catch (const json::exception &e) {
    return false;
  }
}

// Функция для создания ответа с ошибкой
void sendError(Response &res, int status, const std::string &message) {
  json error = {{"error", true}, {"message", message}};
  res.set_content(error.dump(), "application/json");
  res.status = status;
}

int main() {
  Server svr;

  // Добавление нескольких тестовых данных
  auto author1 = db.addAuthor("Лев", "Толстой", "1828-09-09");
  auto author2 = db.addAuthor("Фёдор", "Достоевский", "1821-11-11");
  auto author3 = db.addAuthor("Айзек", "Азимов", "1920-01-02");

  db.addBook("Война и мир", "Роман", 1869, author1.id);
  db.addBook("Анна Каренина", "Роман", 1877, author1.id);
  db.addBook("Преступление и наказание", "Роман", 1866, author2.id);
  db.addBook("Идиот", "Роман", 1869, author2.id);
  db.addBook("Я, робот", "Фантастика", 1950, author3.id);
  db.addBook("Основание", "Фантастика", 1951, author3.id);

  // Корневой маршрут - информация о API
  svr.Get("/", [](const Request &req, Response &res) {
    json response = {
        {"name", "Library Management API"},
        {"version", "1.0.0"},
        {"description", "REST API для управления книгами и авторами"},
        {"endpoints",
         {{"GET /books", "Получить список всех книг"},
          {"GET /books/{id}", "Получить книгу по ID"},
          {"POST /books", "Добавить новую книгу"},
          {"PUT /books/{id}", "Обновить книгу"},
          {"DELETE /books/{id}", "Удалить книгу"},
          {"GET /authors", "Получить список всех авторов"},
          {"GET /authors/{id}", "Получить автора по ID"},
          {"POST /authors", "Добавить нового автора"},
          {"PUT /authors/{id}", "Обновить автора"},
          {"DELETE /authors/{id}", "Удалить автора"}}}};
    res.set_content(response.dump(), "application/json");
  });

  // ========== КНИГИ ==========

  // Получение всех книг с фильтрацией и пагинацией
  svr.Get("/books", [](const Request &req, Response &res) {
    std::string genre = req.get_param_value("genre");
    std::string pageStr = req.get_param_value("page");
    std::string limitStr = req.get_param_value("limit");

    int page = 1;
    int limit = 10;

    if (!pageStr.empty())
      page = std::stoi(pageStr);
    if (!limitStr.empty())
      limit = std::stoi(limitStr);

    if (page < 1)
      page = 1;
    if (limit < 1)
      limit = 10;
    if (limit > 100)
      limit = 100;

    auto books = db.getFilteredAndPaginatedBooks(genre, page, limit);
    json result = json::array();
    for (const auto &book : books) {
      result.push_back(book.toJson());
    }

    // Добавляем информацию о пагинации
    json response = {{"page", page},
                     {"limit", limit},
                     {"total", result.size()},
                     {"books", result}};

    res.set_content(response.dump(), "application/json");
  });

  // Получение книги по ID
  svr.Get(R"(/books/(\d+))", [](const Request &req, Response &res) {
    int id = std::stoi(req.matches[1]);
    auto book = db.getBook(id);

    if (book.id == 0) {
      sendError(res, 404, "Книга не найдена");
      return;
    }

    res.set_content(book.toJson().dump(), "application/json");
  });

  // Добавление новой книги
  svr.Post("/books", [](const Request &req, Response &res) {
    json j;
    if (!parseJsonRequest(req, j)) {
      sendError(res, 400, "Неверный формат JSON");
      return;
    }

    try {
      std::string title = j.at("title");
      std::string genre = j.at("genre");
      int year = j.at("year");
      int authorId = j.at("authorId");

      // Валидация
      if (title.empty() || genre.empty() || year < 0) {
        sendError(res, 400, "Неверные данные книги");
        return;
      }

      auto book = db.addBook(title, genre, year, authorId);

      res.status = 201;
      res.set_content(book.toJson().dump(), "application/json");
    } catch (const json::exception &e) {
      sendError(res, 400, "Отсутствуют обязательные поля");
    } catch (const std::runtime_error &e) {
      sendError(res, 404, "Автор не найден");
    }
  });

  // Обновление книги
  svr.Put(R"(/books/(\d+))", [](const Request &req, Response &res) {
    int id = std::stoi(req.matches[1]);
    json j;
    if (!parseJsonRequest(req, j)) {
      sendError(res, 400, "Неверный формат JSON");
      return;
    }

    try {
      std::string title = j.at("title");
      std::string genre = j.at("genre");
      int year = j.at("year");
      int authorId = j.at("authorId");

      bool success = db.updateBook(id, title, genre, year, authorId);
      if (!success) {
        sendError(res, 404, "Книга или автор не найдены");
        return;
      }

      auto updatedBook = db.getBook(id);
      res.set_content(updatedBook.toJson().dump(), "application/json");
    } catch (const json::exception &e) {
      sendError(res, 400, "Отсутствуют обязательные поля");
    }
  });

  // Удаление книги
  svr.Delete(R"(/books/(\d+))", [](const Request &req, Response &res) {
    int id = std::stoi(req.matches[1]);
    bool success = db.deleteBook(id);

    if (!success) {
      sendError(res, 404, "Книга не найдена");
      return;
    }

    res.status = 204;
  });

  // ========== АВТОРЫ ==========

  // Получение всех авторов
  svr.Get("/authors", [](const Request &req, Response &res) {
    auto authors = db.getAllAuthors();
    json result = json::array();

    // Бонус: добавляем количество книг для каждого автора
    bool includeBookCount = req.get_param_value("includeBooks") == "true";

    for (const auto &author : authors) {
      if (includeBookCount) {
        int bookCount = db.getBookCountForAuthor(author.id);
        result.push_back(author.toJsonWithBookCount(bookCount));
      } else {
        result.push_back(author.toJson());
      }
    }

    res.set_content(result.dump(), "application/json");
  });

  // Получение автора по ID
  svr.Get(R"(/authors/(\d+))", [](const Request &req, Response &res) {
    int id = std::stoi(req.matches[1]);
    auto author = db.getAuthor(id);

    if (author.id == 0) {
      sendError(res, 404, "Автор не найден");
      return;
    }

    // Бонус: добавляем количество книг
    bool includeBookCount = req.get_param_value("includeBooks") == "true";
    if (includeBookCount) {
      int bookCount = db.getBookCountForAuthor(author.id);
      res.set_content(author.toJsonWithBookCount(bookCount).dump(),
                      "application/json");
    } else {
      res.set_content(author.toJson().dump(), "application/json");
    }
  });

  // Добавление нового автора
  svr.Post("/authors", [](const Request &req, Response &res) {
    json j;
    if (!parseJsonRequest(req, j)) {
      sendError(res, 400, "Неверный формат JSON");
      return;
    }

    try {
      std::string firstName = j.at("firstName");
      std::string lastName = j.at("lastName");
      std::string dob = j.at("dob");

      // Валидация
      if (firstName.empty() || lastName.empty()) {
        sendError(res, 400, "Имя и фамилия обязательны");
        return;
      }

      auto author = db.addAuthor(firstName, lastName, dob);

      res.status = 201;
      res.set_content(author.toJson().dump(), "application/json");
    } catch (const json::exception &e) {
      sendError(res, 400, "Отсутствуют обязательные поля");
    }
  });

  // Обновление автора
  svr.Put(R"(/authors/(\d+))", [](const Request &req, Response &res) {
    int id = std::stoi(req.matches[1]);
    json j;
    if (!parseJsonRequest(req, j)) {
      sendError(res, 400, "Неверный формат JSON");
      return;
    }

    try {
      std::string firstName = j.at("firstName");
      std::string lastName = j.at("lastName");
      std::string dob = j.at("dob");

      bool success = db.updateAuthor(id, firstName, lastName, dob);
      if (!success) {
        sendError(res, 404, "Автор не найден");
        return;
      }

      auto updatedAuthor = db.getAuthor(id);
      res.set_content(updatedAuthor.toJson().dump(), "application/json");
    } catch (const json::exception &e) {
      sendError(res, 400, "Отсутствуют обязательные поля");
    }
  });

  // Удаление автора
  svr.Delete(R"(/authors/(\d+))", [](const Request &req, Response &res) {
    int id = std::stoi(req.matches[1]);
    bool success = db.deleteAuthor(id);

    if (!success) {
      sendError(res, 400, "Нельзя удалить автора, у которого есть книги");
      return;
    }

    res.status = 204;
  });

  // Маршрут для проверки здоровья сервера
  svr.Get("/health", [](const Request &req, Response &res) {
    json response = {
        {"status", "ok"},
        {"timestamp",
         std::chrono::system_clock::now().time_since_epoch().count()}};
    res.set_content(response.dump(), "application/json");
  });

  // Обработка CORS (Cross-Origin Resource Sharing)
  svr.Options(".*", [](const Request &req, Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods",
                   "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
  });

  // Добавляем CORS заголовки ко всем ответам
  svr.set_pre_routing_handler([](const Request &req, Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    return Server::HandlerResponse::Unhandled;
  });

  toml::table cfg;

  const std::string config_file_path{"cfg.toml"};

  if (!std::filesystem::exists(config_file_path)) {
    throw std::runtime_error("Config file not found: " + config_file_path);
  }

  cfg = toml::parse_file(config_file_path);

  Logging::LoggerFactory::Init(cfg);

  // Конфигурация
  const std::string address{
      cfg["server_parameters"]["host"].value_or("0.0.0.0")};
  const unsigned short port{static_cast<unsigned short>(
      cfg["server_parameters"]["port"].value_or(15000))};

  std::cout << std::format("Сервер запущен на http://{}:{}", address, port)
            << std::endl;
  std::cout << "Доступные эндпоинты:" << std::endl;
  std::cout << "  GET  / - информация о API" << std::endl;
  std::cout << "  GET  /books - список книг (genre, page, limit параметры)"
            << std::endl;
  std::cout << "  GET  /books/{id} - книга по ID" << std::endl;
  std::cout << "  POST /books - добавить книгу" << std::endl;
  std::cout << "  PUT  /books/{id} - обновить книгу" << std::endl;
  std::cout << "  DELETE /books/{id} - удалить книгу" << std::endl;
  std::cout << "  GET  /authors - список авторов (includeBooks=true параметр)"
            << std::endl;
  std::cout << "  GET  /authors/{id} - автор по ID" << std::endl;
  std::cout << "  POST /authors - добавить автора" << std::endl;
  std::cout << "  PUT  /authors/{id} - обновить автора" << std::endl;
  std::cout << "  DELETE /authors/{id} - удалить автора" << std::endl;
  std::cout << "  GET  /health - проверка здоровья сервера" << std::endl;

  svr.listen(address.c_str(), port);

  return 0;
}