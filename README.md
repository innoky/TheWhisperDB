# TheWhisperDB

**TheWhisperDB** — графовая база данных. Система предназначена для хранения, организации и быстрого доступа к учебным материалам (лекции, конспекты, заметки) в виде графа знаний.

---

## Оглавление

1. [Обзор системы](#обзор-системы)
2. [Архитектура](#архитектура)
3. [Компоненты системы](#компоненты-системы)
   - [Ядро базы данных (Core)](#ядро-базы-данных-core)
   - [HTTP-сервер](#http-сервер)
   - [Файловое хранилище](#файловое-хранилище)
4. [Модель данных](#модель-данных)
5. [REST API](#rest-api)
6. [Формат хранения данных](#формат-хранения-данных)
7. [Сборка и запуск](#сборка-и-запуск)
8. [Конфигурация](#конфигурация)
9. [Примеры использования](#примеры-использования)
10. [Разработка](#разработка)

---

## Обзор системы

TheWhisperDB — это специализированная in-memory графовая база данных с REST API интерфейсом. Основные характеристики:

| Характеристика | Описание |
|----------------|----------|
| **Тип БД** | Графовая, in-memory с персистентностью |
| **Язык** | C++17 |
| **Протокол** | HTTP/1.1 REST API |
| **Хранение файлов** | Файловая система с иерархией по датам |

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
│                     Business Logic Layer                        │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    UploadHandler                         │   │
│  │  - Валидация метаданных                                  │   │
│  │  - Обработка загрузок                                    │   │
│  │  - Формирование ответов                                  │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Core Layer                               │
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
│                      Storage Layer                              │
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

## Компоненты системы

### Ядро базы данных (Core)

#### GraphDB

Главный класс базы данных. Управляет узлами графа и их связями с файлами.

**Особенности реализации:**

- **HashMap для узлов** — O(1) доступ по ID
- **Автосохранение** — `saveToJson()` вызывается после каждой модификации
- **Graceful shutdown** — деструктор сохраняет данные
- **Обработка сигналов** — SIGINT перехватывается для корректного завершения

#### Node (GNode)

Представляет единичный узел графа знаний — учебный материал.

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

#### endpoint

Абстракция для регистрации обработчиков маршрутов.

---

### Файловое хранилище

#### FileStorage

Управляет физическим хранением файлов на диске.

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
       └── database.wdb существует?
           └── Да → loadFromJson()
           └── Нет → createJson()

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
```

### Запуск

```bash
./server
```

### Остановка

`Ctrl+C` для корректного завершения:
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

*Документация актуальна для версии с поддержкой MultipartParser (январь 2026)*
