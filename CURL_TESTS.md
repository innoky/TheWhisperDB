# Тестовые cURL запросы для TheWhisperDB REST API

Коллекция готовых команд для тестирования полного REST API.

> **Предварительно:** Убедитесь, что сервер запущен на `http://localhost:8080`

---

## Оглавление

1. [Health Check](#health-check)
2. [CRUD операции с узлами](#crud-операции-с-узлами)
   - [Получение списка узлов (GET)](#получение-списка-узлов)
   - [Получение узла по ID (GET)](#получение-узла-по-id)
   - [Создание узла (POST)](#создание-узла)
   - [Обновление узла (PUT)](#обновление-узла)
   - [Удаление узла (DELETE)](#удаление-узла)
3. [Работа с файлами](#работа-с-файлами)
4. [Фильтрация и поиск](#фильтрация-и-поиск)
5. [Тестирование ошибок](#тестирование-ошибок)
6. [Скрипты автотестирования](#скрипты-автотестирования)

---

## Health Check

### Проверка работоспособности сервера

```bash
curl -s http://localhost:8080/health | jq .
```

**Ожидаемый ответ:**
```json
{
  "status": "ok",
  "service": "TheWhisperDB",
  "nodes_count": 0
}
```

---

## CRUD операции с узлами

### Получение списка узлов

#### Все узлы

```bash
curl -s http://localhost:8080/api/nodes | jq .
```

**Ответ:**
```json
{
  "status": "success",
  "count": 2,
  "nodes": [
    {"id": 1, "title": "Лекция 1", ...},
    {"id": 2, "title": "Лекция 2", ...}
  ]
}
```

#### С фильтрацией по предмету

```bash
curl -s "http://localhost:8080/api/nodes?subject=Математика" | jq .
```

#### С фильтрацией по автору

```bash
curl -s "http://localhost:8080/api/nodes?author=Иванов" | jq .
```

#### С фильтрацией по курсу

```bash
curl -s "http://localhost:8080/api/nodes?course=101" | jq .
```

#### Поиск по названию (частичное совпадение)

```bash
curl -s "http://localhost:8080/api/nodes?title=Лекция" | jq .
```

#### По тегу

```bash
curl -s "http://localhost:8080/api/nodes?tag=алгоритмы" | jq .
```

#### Комбинированный фильтр

```bash
curl -s "http://localhost:8080/api/nodes?subject=Информатика&course=101" | jq .
```

---

### Получение узла по ID

```bash
curl -s http://localhost:8080/api/nodes/1 | jq .
```

**Ответ:**
```json
{
  "status": "success",
  "node": {
    "id": 1,
    "title": "Введение в алгоритмы",
    "author": "Иванов",
    "subject": "Информатика",
    "course": 101,
    "description": "Базовые понятия",
    "date": "2024-01-15 10:00:00",
    "tags": ["алгоритмы", "основы"],
    "storage_path": "2024/01/15/file.pdf",
    "LinkedNodes": []
  },
  "files": [
    "2024/01/15/lecture_1705312800000_1234.pdf"
  ]
}
```

---

### Создание узла

#### Минимальный запрос (только обязательные поля)

```bash
curl -s -X POST http://localhost:8080/api/nodes \
  -H "Content-Type: application/json" \
  -d '{"title":"Новая лекция","author":"Иванов","subject":"Математика"}' | jq .
```

**Ответ:**
```json
{
  "status": "success",
  "nodeId": "1",
  "files": []
}
```

#### Полный набор полей

```bash
curl -s -X POST http://localhost:8080/api/nodes \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Введение в алгоритмы",
    "author": "Петров А.А.",
    "subject": "Информатика",
    "course": 101,
    "description": "Базовые понятия теории алгоритмов",
    "tags": ["алгоритмы", "структуры данных"],
    "LinkedNodes": [2, 3]
  }' | jq .
```

#### С файлом (multipart)

```bash
curl -s -X POST http://localhost:8080/api/nodes \
  -F 'metadata={"title":"Лекция с файлом","author":"Иванов","subject":"Физика"}' \
  -F 'file=@lecture.pdf' | jq .
```

#### С несколькими файлами

```bash
curl -s -X POST http://localhost:8080/api/nodes \
  -F 'metadata={"title":"Материалы курса","author":"Доцент","subject":"Химия","course":201}' \
  -F 'lecture=@lecture.pdf' \
  -F 'notes=@notes.txt' \
  -F 'slides=@presentation.pptx' | jq .
```

---

### Обновление узла

#### Обновление одного поля

```bash
curl -s -X PUT http://localhost:8080/api/nodes/1 \
  -H "Content-Type: application/json" \
  -d '{"title":"Обновленное название"}' | jq .
```

**Ответ:**
```json
{
  "status": "success",
  "message": "Node updated",
  "node": {
    "id": 1,
    "title": "Обновленное название",
    ...
  }
}
```

#### Обновление нескольких полей

```bash
curl -s -X PUT http://localhost:8080/api/nodes/1 \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Новое название",
    "description": "Обновленное описание",
    "tags": ["новый", "тег"]
  }' | jq .
```

#### Добавление связей

```bash
curl -s -X PUT http://localhost:8080/api/nodes/1 \
  -H "Content-Type: application/json" \
  -d '{"LinkedNodes": [2, 3, 4]}' | jq .
```

---

### Удаление узла

```bash
curl -s -X DELETE http://localhost:8080/api/nodes/1 | jq .
```

**Ответ:**
```json
{
  "status": "success",
  "message": "Node deleted",
  "deletedId": "1"
}
```

---

## Работа с файлами

### Получение списка файлов узла

```bash
curl -s http://localhost:8080/api/nodes/1/files | jq .
```

**Ответ:**
```json
{
  "status": "success",
  "nodeId": "1",
  "files": [
    "2024/01/15/lecture_1705312800000_1234.pdf",
    "2024/01/15/notes_1705312800001_5678.txt"
  ]
}
```

### Добавление файла к существующему узлу

```bash
curl -s -X POST http://localhost:8080/api/nodes/1/files \
  -F 'file=@additional_notes.pdf' | jq .
```

**Ответ:**
```json
{
  "status": "success",
  "nodeId": "1",
  "addedFiles": [
    "2024/01/15/additional_notes_1705312900000_9012.pdf"
  ]
}
```

### Добавление нескольких файлов

```bash
curl -s -X POST http://localhost:8080/api/nodes/1/files \
  -F 'file1=@doc1.pdf' \
  -F 'file2=@doc2.pdf' \
  -F 'file3=@image.png' | jq .
```

---

## Фильтрация и поиск

### Примеры запросов с фильтрами

```bash
# Все лекции по математике
curl -s "http://localhost:8080/api/nodes?subject=Математика" | jq '.nodes[] | {id, title}'

# Материалы автора Иванов
curl -s "http://localhost:8080/api/nodes?author=Иванов" | jq '.nodes[] | {id, title, subject}'

# Курс 101
curl -s "http://localhost:8080/api/nodes?course=101" | jq '.nodes'

# Поиск по названию
curl -s "http://localhost:8080/api/nodes?title=Введение" | jq '.nodes[] | .title'

# По тегу "алгоритмы"
curl -s "http://localhost:8080/api/nodes?tag=алгоритмы" | jq '.count'

# Комбинация: предмет + курс
curl -s "http://localhost:8080/api/nodes?subject=Информатика&course=101" | jq '.nodes'
```

---

## Тестирование ошибок

### Узел не найден (404)

```bash
curl -s http://localhost:8080/api/nodes/999 | jq .
```

**Ответ:**
```json
{
  "status": "error",
  "message": "Node not found: 999"
}
```

### Неверный метод (405)

```bash
curl -s -X DELETE http://localhost:8080/api/nodes | jq .
```

**Ответ:**
```json
{
  "status": "error",
  "message": "Method not allowed"
}
```

### Отсутствует обязательное поле (400)

```bash
curl -s -X POST http://localhost:8080/api/nodes \
  -H "Content-Type: application/json" \
  -d '{"title":"Без автора"}' | jq .
```

### Невалидный JSON (400)

```bash
curl -s -X POST http://localhost:8080/api/nodes \
  -H "Content-Type: application/json" \
  -d '{invalid}' | jq .
```

### Пустое тело запроса

```bash
curl -s -X POST http://localhost:8080/api/nodes \
  -H "Content-Type: application/json" | jq .
```

### Endpoint не найден (404)

```bash
curl -s http://localhost:8080/api/unknown | jq .
```

---

## Скрипты автотестирования

### Полный CRUD тест

```bash
#!/bin/bash
set -e

BASE="http://localhost:8080"

echo "=== TheWhisperDB API Test ==="
echo ""

# 1. Health check
echo "[1] Health check..."
curl -s "$BASE/health" | jq -r '.status'

# 2. Create node
echo "[2] Creating node..."
NODE_ID=$(curl -s -X POST "$BASE/api/nodes" \
  -H "Content-Type: application/json" \
  -d '{"title":"Test Node","author":"Tester","subject":"Testing"}' | jq -r '.nodeId')
echo "Created node ID: $NODE_ID"

# 3. Get node
echo "[3] Getting node..."
curl -s "$BASE/api/nodes/$NODE_ID" | jq -r '.node.title'

# 4. Update node
echo "[4] Updating node..."
curl -s -X PUT "$BASE/api/nodes/$NODE_ID" \
  -H "Content-Type: application/json" \
  -d '{"title":"Updated Title","description":"Added description"}' | jq -r '.message'

# 5. Verify update
echo "[5] Verifying update..."
curl -s "$BASE/api/nodes/$NODE_ID" | jq -r '.node.title'

# 6. List all nodes
echo "[6] Listing nodes..."
curl -s "$BASE/api/nodes" | jq -r '.count'

# 7. Delete node
echo "[7] Deleting node..."
curl -s -X DELETE "$BASE/api/nodes/$NODE_ID" | jq -r '.message'

# 8. Verify deletion
echo "[8] Verifying deletion..."
STATUS=$(curl -s "$BASE/api/nodes/$NODE_ID" | jq -r '.status')
if [ "$STATUS" = "error" ]; then
  echo "Node deleted successfully"
else
  echo "ERROR: Node still exists!"
  exit 1
fi

echo ""
echo "=== All tests passed! ==="
```

### Тест с файлами

```bash
#!/bin/bash
BASE="http://localhost:8080"

# Create test file
echo "Test content" > /tmp/test_file.txt

# Create node with file
echo "Creating node with file..."
RESULT=$(curl -s -X POST "$BASE/api/nodes" \
  -F 'metadata={"title":"File Test","author":"Bot","subject":"Testing"}' \
  -F 'file=@/tmp/test_file.txt')

NODE_ID=$(echo "$RESULT" | jq -r '.nodeId')
echo "Node ID: $NODE_ID"

# Get files
echo "Files:"
curl -s "$BASE/api/nodes/$NODE_ID/files" | jq -r '.files[]'

# Add another file
echo "Additional content" > /tmp/test_file2.txt
curl -s -X POST "$BASE/api/nodes/$NODE_ID/files" \
  -F 'file=@/tmp/test_file2.txt' | jq -r '.addedFiles[]'

# Cleanup
rm /tmp/test_file.txt /tmp/test_file2.txt
curl -s -X DELETE "$BASE/api/nodes/$NODE_ID" | jq -r '.message'
```

### Нагрузочный тест

```bash
#!/bin/bash
URL="http://localhost:8080/api/nodes"
COUNT=${1:-100}
PARALLEL=${2:-10}

echo "Load test: $COUNT requests, $PARALLEL parallel"

# Create nodes
echo "Creating $COUNT nodes..."
time seq 1 $COUNT | xargs -P $PARALLEL -I {} \
  curl -s -X POST "$URL" \
  -H "Content-Type: application/json" \
  -d '{"title":"Load Test {}","author":"Bot","subject":"Benchmark"}' \
  > /dev/null

# Count created
echo "Nodes in DB:"
curl -s "$URL" | jq '.count'
```

---

## Полезные однострочники

```bash
# Количество узлов
curl -s http://localhost:8080/api/nodes | jq '.count'

# Только ID и названия
curl -s http://localhost:8080/api/nodes | jq '.nodes[] | {id, title}'

# Список предметов
curl -s http://localhost:8080/api/nodes | jq -r '[.nodes[].subject] | unique | .[]'

# Проверка статуса сервера
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/health

# Время ответа
curl -s -o /dev/null -w "Time: %{time_total}s\n" http://localhost:8080/api/nodes

# Создать и сразу получить ID
curl -s -X POST http://localhost:8080/api/nodes \
  -H "Content-Type: application/json" \
  -d '{"title":"Quick","author":"Me","subject":"Test"}' | jq -r '.nodeId'

# Удалить все узлы (осторожно!)
for id in $(curl -s http://localhost:8080/api/nodes | jq -r '.nodes[].id'); do
  curl -s -X DELETE "http://localhost:8080/api/nodes/$id"
done
```

---

## Legacy API (обратная совместимость)

Старый endpoint `/api/upload` по-прежнему работает:

```bash
curl -s -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Legacy","author":"Old","subject":"Compat"}' \
  -F 'file=@document.pdf' | jq .
```

---

## Переменные окружения

```bash
export WHISPERDB_URL="http://localhost:8080"

# Примеры использования
curl -s "$WHISPERDB_URL/health" | jq .
curl -s "$WHISPERDB_URL/api/nodes" | jq '.count'
```

---

*Документация для TheWhisperDB REST API v2.0*
