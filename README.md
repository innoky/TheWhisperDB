# TheWhisperDB

**TheWhisperDB** — графовая база данных, разработанная для образовательной платформы TheWhisper. Система предназначена для хранения, организации и быстрого доступа к учебным материалам (лекции, конспекты, заметки) в виде связанного графа знаний.

---

## Оглавление

1. [Обзор системы](#обзор-системы)
2. [Архитектура](#архитектура)
3. [Структура проекта](#структура-проекта)
4. [Компоненты системы](#компоненты-системы)
   - [Ядро базы данных (Core)](#ядро-базы-данных-core)
   - [HTTP-сервер](#http-сервер)
   - [Файловое хранилище](#файловое-хранилище)
5. [Модель данных](#модель-данных)
6. [REST API](#rest-api)
7. [Формат хранения данных](#формат-хранения-данных)
8. [Сборка и запуск](#сборка-и-запуск)
9. [Конфигурация](#конфигурация)
10. [Примеры использования](#примеры-использования)
11. [Разработка](#разработка)

---

## Обзор системы

TheWhisperDB — это специализированная in-memory графовая база данных с REST API интерфейсом. Основные характеристики:

| Характеристика | Описание |
|----------------|----------|
| **Тип БД** | Графовая, in-memory с персистентностью |
| **Язык** | C++17 |
| **Протокол** | HTTP/1.1 REST API |
| **Формат данных** | JSON |
| **Хранение файлов** | Файловая система с иерархией по датам |
| **Целевая нагрузка** | Тысячи узлов |

### Ключевые возможности

- **In-memory хранение** — все узлы графа находятся в оперативной памяти для быстрого доступа O(1)
- **Персистентность** — автоматическое сохранение в JSON-файл при изменениях
- **Граф знаний** — узлы могут быть связаны между собой (LinkedNodes)
- **Файловые вложения** — к каждому узлу можно прикрепить множество файлов
- **REST API** — полноценный HTTP-интерфейс для интеграции
- **Multipart upload** — поддержка загрузки бинарных файлов

---

## Архитектура

```
┌─────────────────────────────────────────────────────────────────┐
│                        HTTP Client                               │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      HTTP Layer (wServer)                        │
│  ┌─────────────┐  ┌──────────────────┐  ┌───────────────────┐   │
│  │   Router    │  │ MultipartParser  │  │     Endpoint      │   │
│  └─────────────┘  └──────────────────┘  └───────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Business Logic Layer                         │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    UploadHandler                         │    │
│  │  - Валидация метаданных                                  │    │
│  │  - Обработка загрузок                                    │    │
│  │  - Формирование ответов                                  │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Core Layer                                │
│  ┌──────────────────────┐      ┌────────────────────────────┐   │
│  │       GraphDB        │      │           Node             │   │
│  │                      │      │                            │   │
│  │  - nodes (HashMap)   │◄────►│  - id, title, author       │   │
│  │  - nodeFiles (Map)   │      │  - course, subject, tags   │   │
│  │  - CRUD операции     │      │  - LinkedNodes (связи)     │   │
│  │  - Сериализация      │      │  - storage_path            │   │
│  └──────────────────────┘      └────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Storage Layer                               │
│  ┌──────────────────────┐      ┌────────────────────────────┐   │
│  │     FileStorage      │      │      database.wdb          │   │
│  │                      │      │      (JSON файл)           │   │
│  │  storage/            │      │                            │   │
│  │  └── YYYY/           │      │  - nodes[]                 │   │
│  │      └── MM/         │      │  - nodeFiles{}             │   │
│  │          └── DD/     │      │  - size                    │   │
│  │              └── *   │      │                            │   │
│  └──────────────────────┘      └────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Поток данных при загрузке файла

```
1. HTTP POST /api/upload (multipart/form-data)
           │
           ▼
2. wServer: парсинг HTTP-запроса
           │
           ▼
3. MultipartParser: извлечение частей
   - metadata (JSON)
   - files (бинарные данные)
           │
           ▼
4. UploadHandler: валидация и обработка
   - Проверка обязательных полей
   - Добавление timestamp
           │
           ▼
5. GraphDB.addNode(): создание узла
   - Генерация уникального ID
   - Сохранение в HashMap
           │
           ▼
6. GraphDB.addFileToNode(): сохранение файлов
           │
           ▼
7. FileStorage.saveFile():
   - Генерация пути (YYYY/MM/DD/)
   - Генерация уникального имени
   - Запись на диск
           │
           ▼
8. GraphDB.saveToJson(): персистентность
           │
           ▼
9. HTTP Response (JSON)
```

---

## Структура проекта

```
TheWhisperDB/
│
├── src/                              # Исходный код
│   ├── main.cpp                      # Точка входа, регистрация endpoints
│   │
│   ├── core/                         # Ядро базы данных
│   │   ├── GraphDB.cpp               # Реализация графовой БД
│   │   └── GNode.cpp                 # Реализация узла графа
│   │
│   ├── http/                         # HTTP-модуль
│   │   └── MultipartParser.cpp       # Парсер multipart/form-data
│   │
│   └── server/                       # Серверные компоненты
│       ├── wserver.cpp               # HTTP-сервер (Boost.ASIO)
│       ├── FileStorage.cpp           # Файловое хранилище
│       └── UploadHandler.cpp         # Обработчик загрузок
│
├── include/                          # Заголовочные файлы
│   ├── config.hpp                    # Конфигурация путей
│   │
│   ├── core/
│   │   ├── GraphDB.hpp               # Интерфейс GraphDB
│   │   └── GNode.hpp                 # Интерфейс Node
│   │
│   ├── http/
│   │   └── MultipartParser.hpp       # Интерфейс парсера
│   │
│   ├── server/
│   │   ├── wserver.hpp               # Интерфейс сервера
│   │   ├── endpoint.hpp              # Абстракция endpoint
│   │   ├── FileStorage.hpp           # Интерфейс хранилища
│   │   └── UploadHandler.hpp         # Интерфейс обработчика
│   │
│   └── const/
│       └── rest_enums.hpp            # HTTP методы (GET, POST, etc.)
│
├── data/                             # Данные БД
│   └── database.wdb                  # JSON-файл с узлами
│
├── storage/                          # Файловое хранилище
│   └── YYYY/MM/DD/                   # Иерархия по датам
│
├── utils/
│   └── generate_db.py                # Генератор тестовых данных
│
├── CMakeLists.txt                    # Конфигурация сборки
├── README.md                         # Документация
├── API_README.md                     # Документация API
└── LICENCE                           # Лицензия
```

---

## Компоненты системы

### Ядро базы данных (Core)

#### GraphDB

Главный класс базы данных. Управляет узлами графа и их связями с файлами.

```cpp
class GraphDB {
public:
    // Конструктор/деструктор
    GraphDB();   // Загружает БД из файла или создает новую
    ~GraphDB();  // Сохраняет БД и освобождает память

    // CRUD операции с узлами
    std::string addNode(json& j, const vector<pair<string, string>>& files = {});
    Node find(const std::string& id) const;
    bool deleteNode(const std::string& id);

    // Операции с файлами
    std::string addFileToNode(const std::string& nodeId,
                               const std::string& filename,
                               const std::vector<uint8_t>& content);
    bool removeFileFromNode(const std::string& nodeId, const std::string& filePath);
    std::vector<std::string> getNodeFiles(const std::string& nodeId) const;

    // Сериализация
    std::string serialize() const;  // JSON-строка всех узлов
    void saveToJson();              // Сохранить на диск

private:
    std::unordered_map<std::string, Node*> nodes;           // ID -> Node*
    std::unordered_map<std::string, std::vector<std::string>> nodeFiles;  // ID -> [пути]
    int size;
    std::unique_ptr<FileStorage> fileStorage;

    void loadFromJson();       // Загрузка из файла
    void createJson();         // Создание нового файла БД
    std::string generateNodeId();  // Автоинкремент ID
};
```

**Особенности реализации:**

- **HashMap для узлов** — O(1) доступ по ID
- **Автосохранение** — `saveToJson()` вызывается после каждой модификации
- **Graceful shutdown** — деструктор сохраняет данные
- **Обработка сигналов** — SIGINT перехватывается для корректного завершения

#### Node (GNode)

Представляет единичный узел графа знаний — учебный материал.

```cpp
class Node {
public:
    Node(const nlohmann::json& json);
    Node(const nlohmann::json& json, int id);

    nlohmann::json to_json() const;
    std::string to_str();

    // Getters
    int getId() const;
    std::string getTitle() const;
    std::string getStoragePath() const;

    // Setters
    void setStoragePath(const std::string& path);

private:
    int id;                         // Уникальный идентификатор
    std::string title;              // Название (обязательное)
    int course;                     // ID курса
    std::string subject;            // Предмет/категория
    std::string description;        // Описание
    std::string author;             // Автор
    std::string date;               // Дата создания/изменения
    std::vector<std::string> tags;  // Теги для категоризации
    std::string storage_path;       // Путь к основному файлу
    std::vector<int> LinkedNodes;   // Связи с другими узлами
};
```

**Поддерживаемые форматы полей:**

| Поле | Тип | Альтернативный тип | Обязательное |
|------|-----|-------------------|--------------|
| `id` | int | — | Генерируется автоматически |
| `title` | string | — | Да |
| `author` | string | — | Да |
| `subject` | string | — | Да |
| `course` | int | string (конвертируется) | Нет |
| `description` | string | — | Нет |
| `date` | string | — | Нет (генерируется) |
| `tags` | array | string (через запятую) | Нет |
| `LinkedNodes` | array[int] | — | Нет |

---

### HTTP-сервер

#### wServer

Асинхронный HTTP-сервер на базе Boost.ASIO.

```cpp
class wServer {
public:
    wServer();
    void add_endpoint(const endpoint& ep);  // Регистрация маршрута
    void run(uint16_t port);                // Запуск сервера

private:
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<std::string, endpoint> handlers_;
};
```

**Возможности:**

- Поддержка HTTP методов: GET, POST, PUT, PATCH
- Парсинг multipart/form-data
- Content-Length обработка
- Лимит размера тела запроса (10 MB)
- Обработка ошибок с корректными HTTP-кодами

**HTTP коды ответов:**

| Код | Описание | Когда возвращается |
|-----|----------|-------------------|
| 200 | OK | Успешный запрос |
| 400 | Bad Request | Некорректный JSON, отсутствует boundary |
| 404 | Not Found | Endpoint не найден |
| 405 | Method Not Allowed | Неподдерживаемый HTTP метод |
| 413 | Payload Too Large | Тело запроса > 10 MB |

#### MultipartParser

Парсер для multipart/form-data запросов.

```cpp
namespace whisperdb::http {

struct MultipartPart {
    std::string name;           // Имя поля формы
    std::string filename;       // Имя файла (пусто если не файл)
    std::string content_type;   // MIME-тип
    std::vector<uint8_t> data;  // Бинарные данные

    bool isFile() const;              // Проверка: это файл?
    std::string dataAsString() const; // Данные как строка
};

class MultipartParser {
public:
    // Парсинг тела запроса
    static std::vector<MultipartPart> parse(
        const std::string& body,
        const std::string& boundary
    );

    // Извлечение boundary из Content-Type
    static std::string extractBoundary(const std::string& content_type);

    // Подсчет частей без полного парсинга
    static size_t countParts(const std::string& body, const std::string& boundary);
};

}
```

#### endpoint

Абстракция для регистрации обработчиков маршрутов.

```cpp
class endpoint {
public:
    endpoint(
        std::function<std::string(const std::vector<MultipartPart>&)> handler,
        HttpRequest rest_type,  // GET, POST, PUT, PATCH
        const std::string& path
    );

    std::string get_path() const;
    HttpRequest get_rest_type() const;
    std::function<...> get_handler() const;
};
```

---

### Файловое хранилище

#### FileStorage

Управляет физическим хранением файлов на диске.

```cpp
class FileStorage {
public:
    explicit FileStorage(const std::string& basePath);

    // Сохранение файлов
    std::string saveFile(const std::string& filename, const std::string& content);
    std::string saveFile(const std::string& filename, const std::vector<uint8_t>& content);

    // Чтение и удаление
    std::string readFile(const std::string& filepath) const;
    bool deleteFile(const std::string& filepath) const;

    // Утилиты
    std::string getFullPath(const std::string& filepath) const;

private:
    std::string basePath_;  // Корневая директория (по умолчанию: "storage")

    std::string generateStoragePath() const;      // YYYY/MM/DD
    std::string generateUniqueFilename(const std::string& original) const;
};
```

**Стратегия именования файлов:**

```
Оригинальное имя: lecture_notes.pdf
Сохраненное имя:  lecture_notes_1704067200000_5432.pdf
                  └────┬─────┘ └─────┬──────┘ └─┬─┘
                  base name    timestamp(ms)  random
```

**Структура директорий:**

```
storage/
├── 2024/
│   ├── 01/
│   │   ├── 15/
│   │   │   ├── lecture_1705312800000_1234.pdf
│   │   │   └── notes_1705312800001_5678.txt
│   │   └── 16/
│   │       └── ...
│   └── 02/
│       └── ...
└── 2025/
    └── ...
```

---

## Модель данных

### Схема узла (Node)

```json
{
  "id": 1,
  "title": "Введение в алгоритмы",
  "course": 101,
  "subject": "Информатика",
  "description": "Базовые понятия теории алгоритмов",
  "author": "Иванов И.И.",
  "date": "2024-01-15 10:30:00",
  "tags": ["алгоритмы", "основы", "теория"],
  "storage_path": "2024/01/15/lecture_1705312800000_1234.pdf",
  "LinkedNodes": [2, 5, 7]
}
```

### Схема базы данных (database.wdb)

```json
{
  "size": 3,
  "nodes": [
    {
      "id": 1,
      "title": "Лекция 1",
      "course": 101,
      "subject": "Математика",
      "description": "Введение",
      "author": "Петров",
      "date": "2024-01-15 10:00:00",
      "tags": ["введение"],
      "storage_path": "2024/01/15/lec1_1705312800000_1234.pdf",
      "LinkedNodes": [2]
    },
    {
      "id": 2,
      "title": "Лекция 2",
      "...": "..."
    }
  ],
  "nodeFiles": {
    "1": [
      "2024/01/15/lec1_1705312800000_1234.pdf",
      "2024/01/15/notes_1705312800001_5678.txt"
    ],
    "2": [
      "2024/01/16/lec2_1705399200000_9012.pdf"
    ]
  }
}
```

### Граф связей

```
     ┌─────────┐
     │ Node 1  │
     │ Лекция  │
     └────┬────┘
          │ LinkedNodes: [2, 3]
    ┌─────┴─────┐
    ▼           ▼
┌─────────┐ ┌─────────┐
│ Node 2  │ │ Node 3  │
│ Практика│ │ Тест    │
└────┬────┘ └─────────┘
     │ LinkedNodes: [4]
     ▼
┌─────────┐
│ Node 4  │
│ Решения │
└─────────┘
```

---

## REST API

### Endpoints

#### POST /api/upload

Загрузка файлов с метаданными и создание нового узла.

**Request:**
```
POST /api/upload HTTP/1.1
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary

------WebKitFormBoundary
Content-Disposition: form-data; name="metadata"

{"title":"Лекция 1","author":"Иванов","subject":"Математика"}
------WebKitFormBoundary
Content-Disposition: form-data; name="file"; filename="lecture.pdf"
Content-Type: application/pdf

<binary data>
------WebKitFormBoundary--
```

**Response (успех):**
```json
{
  "status": "success",
  "nodeId": "1",
  "files": [
    {
      "originalName": "lecture.pdf",
      "storedPath": "2024/01/15/lecture_1705312800000_1234.pdf"
    }
  ]
}
```

**Response (ошибка):**
```json
{
  "status": "error",
  "message": "Missing required field: title"
}
```

#### POST /test

Тестовый endpoint для проверки работоспособности.

**Response:**
```
Test endpoint. Got 2 parts.
Part 0: name="metadata", size=64 bytes
Part 1: name="file", filename="test.txt", size=1024 bytes
```

### Обязательные поля метаданных

| Поле | Тип | Описание |
|------|-----|----------|
| `title` | string | Название материала |
| `author` | string | Автор |
| `subject` | string | Предмет/категория |

### Опциональные поля

| Поле | Тип | Описание | По умолчанию |
|------|-----|----------|--------------|
| `course` | int/string | ID курса | — |
| `description` | string | Описание | — |
| `tags` | array | Массив тегов | — |
| `date` | string | Дата создания | Текущее время |
| `LinkedNodes` | array[int] | Связи с узлами | — |

---

## Формат хранения данных

### Путь к файлу базы данных

Задается в `include/config.hpp`:

```cpp
const std::string DB_FILE_PATH = "/path/to/database.wdb";
```

### Путь к файловому хранилищу

По умолчанию: `./storage/` (относительно рабочей директории).

Задается при создании `FileStorage`:

```cpp
fileStorage = std::make_unique<FileStorage>("storage");
```

### Жизненный цикл данных

```
1. Запуск сервера
   └── GraphDB::initGraphDB()
       ├── database.wdb существует?
       │   ├── Да → loadFromJson()
       │   └── Нет → createJson()

2. Операция записи (addNode, addFileToNode, deleteNode)
   └── saveToJson() — синхронная запись на диск

3. Завершение (SIGINT или деструктор)
   └── saveToJson() — финальное сохранение
```

---

## Сборка и запуск

### Требования

- **Компилятор:** GCC 9+ или Clang 10+ (с поддержкой C++17)
- **CMake:** 3.20+
- **Boost:** с компонентом ASIO
- **nlohmann/json:** (header-only)

### Установка зависимостей

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake libboost-all-dev nlohmann-json3-dev
```

**macOS (Homebrew):**
```bash
brew install cmake boost nlohmann-json
```

**Arch Linux:**
```bash
sudo pacman -S cmake boost nlohmann-json
```

### Сборка

```bash
# Клонирование репозитория
git clone https://github.com/your-repo/TheWhisperDB.git
cd TheWhisperDB

# Создание директории сборки
mkdir build && cd build

# Конфигурация
cmake ..

# Сборка
make -j$(nproc)

# Результат: исполняемый файл ./server
```

### Запуск

```bash
# Из директории сборки
./server

# Или с указанием рабочей директории
cd /path/to/TheWhisperDB
./build/server
```

**Вывод при запуске:**
```
Server listening on port 8080
```

### Остановка

Нажмите `Ctrl+C` для корректного завершения:
```
^C
SIGINT received, saving database...
```

---

## Конфигурация

### config.hpp

```cpp
// include/config.hpp
#pragma once
#include <string>

// Путь к файлу базы данных
const std::string DB_FILE_PATH = "/path/to/data/database.wdb";
```

### Константы в коде

| Параметр | Файл | Значение | Описание |
|----------|------|----------|----------|
| Порт сервера | main.cpp | 8080 | HTTP порт |
| Макс. размер запроса | wserver.cpp | 10 MB | Лимит тела запроса |
| Путь хранилища | GraphDB.cpp | "storage" | Директория файлов |

---

## Примеры использования

### cURL: загрузка файла с метаданными

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Лекция по алгоритмам","author":"Иванов И.И.","subject":"Информатика","course":101,"tags":["алгоритмы","сортировка"]}' \
  -F 'file=@/path/to/lecture.pdf'
```

### cURL: загрузка нескольких файлов

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Практикум","author":"Петров","subject":"Программирование"}' \
  -F 'file1=@task1.cpp' \
  -F 'file2=@task2.cpp' \
  -F 'file3=@readme.txt'
```

### Python: загрузка через requests

```python
import requests
import json

url = "http://localhost:8080/api/upload"

metadata = {
    "title": "Конспект лекции",
    "author": "Сидоров А.А.",
    "subject": "Физика",
    "course": 201,
    "tags": ["механика", "кинематика"]
}

files = {
    "metadata": (None, json.dumps(metadata)),
    "file": ("notes.pdf", open("notes.pdf", "rb"), "application/pdf")
}

response = requests.post(url, files=files)
print(response.json())
```

### JavaScript (Node.js): загрузка через fetch

```javascript
const FormData = require('form-data');
const fs = require('fs');
const fetch = require('node-fetch');

const form = new FormData();

form.append('metadata', JSON.stringify({
    title: 'Homework 1',
    author: 'Student',
    subject: 'Math'
}));

form.append('file', fs.createReadStream('homework.pdf'));

fetch('http://localhost:8080/api/upload', {
    method: 'POST',
    body: form
})
.then(res => res.json())
.then(data => console.log(data));
```

---

## Разработка

### Добавление нового endpoint

```cpp
// В main.cpp

endpoint my_endpoint(
    [](const std::vector<MultipartPart>& parts) -> std::string {
        // Обработка запроса
        json response;
        response["status"] = "ok";
        return response.dump();
    },
    HttpRequest::GET,  // или POST, PUT, PATCH
    "/api/my-endpoint"
);

server->add_endpoint(my_endpoint);
```

### Структура MultipartPart

```cpp
for (const auto& part : parts) {
    if (part.isFile()) {
        // Это файл
        std::string filename = part.filename;
        std::vector<uint8_t> data = part.data;
        // Обработка файла...
    } else {
        // Это текстовое поле
        std::string value = part.dataAsString();
        // Обработка значения...
    }
}
```

### Работа с GraphDB напрямую

```cpp
// Получение экземпляра БД
auto db = std::make_shared<GraphDB>();

// Создание узла
json nodeData = {
    {"title", "Test Node"},
    {"author", "Developer"},
    {"subject", "Testing"}
};
std::string nodeId = db->addNode(nodeData);

// Поиск узла
Node node = db->find(nodeId);
std::cout << node.getTitle() << std::endl;

// Добавление файла к узлу
std::vector<uint8_t> content = {'H', 'e', 'l', 'l', 'o'};
db->addFileToNode(nodeId, "test.txt", content);

// Получение списка файлов узла
auto files = db->getNodeFiles(nodeId);

// Удаление узла (удаляет и связанные файлы)
db->deleteNode(nodeId);
```

---

## Лицензия

Данный проект является закрытым программным обеспечением.
Все права защищены.

**Автор:** Романов И.А.
**Контакт:** romanovinno@gmail.com

Подробности см. в файле [LICENCE](./LICENCE).

---

## Разработчики

**Романов Иннокентий Александрович**

---

*Документация актуальна для версии с поддержкой MultipartParser (январь 2025)*
