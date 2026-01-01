# Тестовые cURL запросы для TheWhisperDB

Коллекция готовых команд для тестирования API.

> **Предварительно:** Убедитесь, что сервер запущен на `http://localhost:8080`

---

## Оглавление

1. [Проверка работоспособности](#проверка-работоспособности)
2. [Загрузка с метаданными](#загрузка-с-метаданными)
3. [Загрузка файлов](#загрузка-файлов)
4. [Различные форматы метаданных](#различные-форматы-метаданных)
5. [Тестирование ошибок](#тестирование-ошибок)
6. [Продвинутые примеры](#продвинутые-примеры)

---

## Проверка работоспособности

### Тестовый endpoint (базовая проверка)

```bash
curl -X POST http://localhost:8080/test \
  -F 'data=hello world'
```

**Ожидаемый ответ:**
```
Test endpoint. Got 1 parts.
Part 0: name="data", size=11 bytes
```

### Проверка с несколькими частями

```bash
curl -X POST http://localhost:8080/test \
  -F 'field1=value1' \
  -F 'field2=value2' \
  -F 'field3=value3'
```

**Ожидаемый ответ:**
```
Test endpoint. Got 3 parts.
Part 0: name="field1", size=6 bytes
Part 1: name="field2", size=6 bytes
Part 2: name="field3", size=6 bytes
```

---

## Загрузка с метаданными

### Минимальный запрос (только обязательные поля)

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Тестовая лекция","author":"Иванов","subject":"Математика"}'
```

**Ожидаемый ответ:**
```json
{
  "status": "success",
  "nodeId": "1",
  "files": []
}
```

### Полный набор метаданных

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={
    "title": "Введение в алгоритмы",
    "author": "Петров А.А.",
    "subject": "Информатика",
    "course": 101,
    "description": "Базовые понятия теории алгоритмов и структур данных",
    "tags": ["алгоритмы", "структуры данных", "основы"],
    "date": "2024-01-15 10:30:00"
  }'
```

### С указанием связей (LinkedNodes)

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={
    "title": "Практика по алгоритмам",
    "author": "Петров А.А.",
    "subject": "Информатика",
    "course": 101,
    "LinkedNodes": [1, 2, 3]
  }'
```

---

## Загрузка файлов

### Один текстовый файл

```bash
# Создаем тестовый файл
echo "Это содержимое тестового файла" > /tmp/test.txt

# Загружаем
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Заметки","author":"Студент","subject":"Физика"}' \
  -F 'file=@/tmp/test.txt'
```

**Ожидаемый ответ:**
```json
{
  "status": "success",
  "nodeId": "2",
  "files": [
    {
      "originalName": "test.txt",
      "storedPath": "2024/01/15/test_1705312800000_1234.txt"
    }
  ]
}
```

### Несколько файлов

```bash
# Создаем тестовые файлы
echo "Файл 1" > /tmp/file1.txt
echo "Файл 2" > /tmp/file2.txt
echo "Файл 3" > /tmp/file3.txt

# Загружаем все
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Пакет документов","author":"Менеджер","subject":"Документация"}' \
  -F 'file1=@/tmp/file1.txt' \
  -F 'file2=@/tmp/file2.txt' \
  -F 'file3=@/tmp/file3.txt'
```

### PDF файл

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Лекция в PDF","author":"Профессор","subject":"История"}' \
  -F 'document=@/path/to/lecture.pdf;type=application/pdf'
```

### Изображение

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Диаграмма","author":"Дизайнер","subject":"Графика"}' \
  -F 'image=@/path/to/diagram.png;type=image/png'
```

### Бинарный файл (архив)

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Исходный код","author":"Разработчик","subject":"Программирование"}' \
  -F 'archive=@/path/to/source.zip;type=application/zip'
```

### Смешанные типы файлов

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={
    "title": "Проект с документацией",
    "author": "Команда разработки",
    "subject": "Разработка ПО",
    "tags": ["проект", "документация", "код"]
  }' \
  -F 'readme=@README.md;type=text/markdown' \
  -F 'source=@main.cpp;type=text/x-c++src' \
  -F 'diagram=@architecture.png;type=image/png'
```

---

## Различные форматы метаданных

### Course как строка (автоконвертация)

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Тест","author":"Автор","subject":"Предмет","course":"123"}'
```

### Tags как строка (через запятую)

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={
    "title": "Материал с тегами",
    "author": "Автор",
    "subject": "Предмет",
    "tags": "тег1, тег2, тег3"
  }'
```

### Минимальный JSON (однострочный)

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"T","author":"A","subject":"S"}'
```

### Кириллица в метаданных

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={
    "title": "Лекция по русскому языку",
    "author": "Пушкин Александр Сергеевич",
    "subject": "Литература",
    "description": "Анализ произведения «Евгений Онегин»",
    "tags": ["поэзия", "классика", "XIX век"]
  }'
```

### Специальные символы

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={
    "title": "Формулы: E=mc², ∑, ∫, √",
    "author": "Эйнштейн",
    "subject": "Физика",
    "description": "Теория относительности (специальная & общая)"
  }'
```

---

## Тестирование ошибок

### Отсутствует title

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"author":"Автор","subject":"Предмет"}'
```

**Ожидаемый ответ:**
```json
{
  "status": "error",
  "message": "Invalid metadata: Missing required field: title"
}
```

### Отсутствует author

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Заголовок","subject":"Предмет"}'
```

**Ожидаемый ответ:**
```json
{
  "status": "error",
  "message": "Invalid metadata: Missing required field: author"
}
```

### Отсутствует subject

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Заголовок","author":"Автор"}'
```

**Ожидаемый ответ:**
```json
{
  "status": "error",
  "message": "Invalid metadata: Missing required field: subject"
}
```

### Пустой title

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"","author":"Автор","subject":"Предмет"}'
```

**Ожидаемый ответ:**
```json
{
  "status": "error",
  "message": "Invalid metadata: Title must be a non-empty string"
}
```

### Невалидный JSON

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={invalid json}'
```

**Ожидаемый ответ:**
```json
{
  "status": "error",
  "message": "Invalid JSON metadata: ..."
}
```

### Пустой запрос

```bash
curl -X POST http://localhost:8080/api/upload
```

**Ожидаемый ответ:**
```json
{
  "status": "error",
  "message": "No data received"
}
```

### Невалидный course

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Тест","author":"А","subject":"П","course":"abc"}'
```

**Ожидаемый ответ:**
```json
{
  "status": "error",
  "message": "Invalid metadata: Course must be a valid number"
}
```

### Неверный endpoint

```bash
curl -X POST http://localhost:8080/api/nonexistent \
  -F 'data=test'
```

**Ожидаемый ответ:**
```
Not Found
```

### Неверный HTTP метод

```bash
curl -X GET http://localhost:8080/api/upload
```

**Ожидаемый ответ:**
```
Method Not Allowed
```

---

## Продвинутые примеры

### Verbose режим (отладка)

```bash
curl -v -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Debug","author":"Dev","subject":"Test"}'
```

### С выводом заголовков ответа

```bash
curl -i -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Headers","author":"Dev","subject":"Test"}'
```

### Сохранение ответа в файл

```bash
curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Save","author":"Dev","subject":"Test"}' \
  -o response.json
```

### Таймаут запроса

```bash
curl -X POST http://localhost:8080/api/upload \
  --max-time 10 \
  -F 'metadata={"title":"Timeout","author":"Dev","subject":"Test"}' \
  -F 'file=@large_file.bin'
```

### Тихий режим с выводом только тела

```bash
curl -s -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Silent","author":"Dev","subject":"Test"}' | jq .
```

### Цикл загрузок (bash)

```bash
for i in {1..5}; do
  curl -s -X POST http://localhost:8080/api/upload \
    -F "metadata={\"title\":\"Документ $i\",\"author\":\"Бот\",\"subject\":\"Тестирование\"}"
  echo ""
done
```

### Загрузка из stdin

```bash
echo "Содержимое файла из stdin" | curl -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Stdin","author":"Pipe","subject":"Test"}' \
  -F 'file=@-;filename=stdin.txt'
```

### Параллельные запросы (xargs)

```bash
seq 1 10 | xargs -P 5 -I {} curl -s -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Parallel {}","author":"xargs","subject":"Concurrency"}'
```

---

## Скрипты для тестирования

### Полный тест (test_api.sh)

```bash
#!/bin/bash

BASE_URL="http://localhost:8080"

echo "=== Тестирование TheWhisperDB API ==="
echo ""

# Тест 1: Проверка сервера
echo "[1] Проверка тестового endpoint..."
curl -s -X POST "$BASE_URL/test" -F 'ping=pong'
echo -e "\n"

# Тест 2: Минимальная загрузка
echo "[2] Минимальная загрузка метаданных..."
curl -s -X POST "$BASE_URL/api/upload" \
  -F 'metadata={"title":"API Test","author":"Script","subject":"Testing"}' | jq .
echo ""

# Тест 3: Загрузка с файлом
echo "[3] Загрузка с файлом..."
echo "Test file content" > /tmp/api_test.txt
curl -s -X POST "$BASE_URL/api/upload" \
  -F 'metadata={"title":"With File","author":"Script","subject":"Testing"}' \
  -F 'file=@/tmp/api_test.txt' | jq .
rm /tmp/api_test.txt
echo ""

# Тест 4: Ошибка валидации
echo "[4] Тест ошибки (отсутствует title)..."
curl -s -X POST "$BASE_URL/api/upload" \
  -F 'metadata={"author":"Script","subject":"Testing"}' | jq .
echo ""

echo "=== Тестирование завершено ==="
```

### Нагрузочный тест (load_test.sh)

```bash
#!/bin/bash

URL="http://localhost:8080/api/upload"
COUNT=${1:-100}
PARALLEL=${2:-10}

echo "Нагрузочный тест: $COUNT запросов, $PARALLEL параллельных"

time seq 1 $COUNT | xargs -P $PARALLEL -I {} \
  curl -s -X POST "$URL" \
  -F 'metadata={"title":"Load Test {}","author":"Benchmark","subject":"Performance"}' \
  > /dev/null

echo "Готово!"
```

**Использование:**
```bash
chmod +x load_test.sh
./load_test.sh 100 10  # 100 запросов, 10 параллельных
```

---

## Полезные однострочники

```bash
# Проверить что сервер работает
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/test -F 'x=y'

# Получить только nodeId из ответа
curl -s -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"T","author":"A","subject":"S"}' | jq -r '.nodeId'

# Измерить время ответа
curl -s -o /dev/null -w "Time: %{time_total}s\n" -X POST http://localhost:8080/api/upload \
  -F 'metadata={"title":"Benchmark","author":"Timer","subject":"Test"}'

# Загрузить все .txt файлы из директории
for f in *.txt; do
  curl -s -X POST http://localhost:8080/api/upload \
    -F "metadata={\"title\":\"$f\",\"author\":\"Batch\",\"subject\":\"Import\"}" \
    -F "file=@$f"
done
```

---

## Переменные окружения

Для удобства можно задать переменные:

```bash
export WHISPERDB_URL="http://localhost:8080"

# Использование
curl -X POST "$WHISPERDB_URL/api/upload" \
  -F 'metadata={"title":"Env Test","author":"Shell","subject":"Config"}'
```

---

*Документация актуальна для TheWhisperDB с MultipartParser*
