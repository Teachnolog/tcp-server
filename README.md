# tcp-server

Проект, чтобы потрогать многопоточность для себя.

 Многопоточный TCP-сервер на C++17 с использованием POSIX-сокетов. Клиенты подключаются по TCP и работают с общим key-value хранилищем через простой текстовый протокол.

Основное:
- сокеты: `socket`, `bind`, `listen`, `poll`, `accept`, `recv`, `send`
- свой thread pool на `std::thread` (очередь задач + `std::mutex` + `std::condition_variable`)
- RAII-обертка над сокетом, `shared_ptr` для соединений
- если все потоки заняты и очередь заполнена, клиент получает `ERR server busy`
- корректное завершение по Ctrl+C / SIGTERM
- таймаут неактивных клиентов и ограничение длины строки

## Сборка

Нужен Linux или macOS (на Windows — WSL или Docker), компилятор с C++17 и CMake 3.16+.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Запуск:

```bash
./build/server_app --port 8080 --threads 8
```

Остальные параметры можно посмотреть через `./build/server_app --help`.

Через Docker:

```bash
docker build -t tcp-server .
docker run --rm -p 8080:8080 tcp-server
```

## Протокол

Одна команда — одна строка, на каждую команду сервер отвечает одной строкой.

```
PING                  -> PONG
ECHO <text>           -> <text>
SET <key> <value>     -> OK
GET <key>             -> OK <value> | NOT_FOUND
DEL <key>             -> OK | NOT_FOUND
INCR <key> [delta]    -> OK <number>
SIZE                  -> OK <count>
STATS                 -> OK connections_total=... 
HELP                  -> OK commands: ...
QUIT                  -> BYE
```

При ошибке сервер отвечает `ERR <описание>`.

Пример:

```
$ nc localhost 8080
SET name Georgiy
OK
GET name
OK Georgiy
INCR visits
OK 1
QUIT
BYE
```

## Тесты

Тесты написаны на GoogleTest, он скачивается автоматически через FetchContent. Есть модульные тесты (thread pool, хранилище, парсер команд) и интеграционные: они поднимают сервер на свободном порту и подключают к нему настоящих клиентов.

Проверить отсутствие гонок можно через ThreadSanitizer:

```bash
cmake -S . -B build-tsan -DCMAKE_CXX_COMPILER=clang++ -DTCP_SERVER_SANITIZER=thread
cmake --build build-tsan && ctest --test-dir build-tsan
```

## Нагрузочный тест

`bench_app` открывает несколько соединений, и каждое отправляет `INCR` в один и тот же ключ. В конце утилита проверяет, что итоговое значение счетчика равно количеству запросов.

```bash
./build/bench_app --port 8080 --clients 8 --requests 20000
```

На моей машине в Docker получалось от 45k до 130k запросов в секунду при 8 потоках (сильно зависит от запуска).

## Что у меня в планах

- сейчас один клиент занимает поток на все время соединения; для большого числа соединений лучше перейти на epoll
- поддержка IPv6
- сохранение данных на диск
