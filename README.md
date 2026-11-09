# Bloom — Minimal C Backend Server

A lightweight, zero-dependency HTTP/1.1 backend server written from scratch in C for Windows (Winsock2 + MSVC). 

This is an educational weekend project built to explore low-level computer networking, TCP socket programming, HTTP/1.1 protocol parsing, thread synchronization, and systems programming fundamentals without relying on third-party frameworks or libraries.

---

## Table of Contents

- [Motivation & Goals](#motivation--goals)
- [Key Features](#key-features)
- [Architecture Overview](#architecture-overview)
- [Project Structure](#project-structure)
- [Prerequisites & Toolchain](#prerequisites--toolchain)
- [Building](#building)
- [Running](#running)
  - [Command-Line Options](#command-line-options)
  - [Configuration File (`server.ini`)](#configuration-file-serverini)
- [API Reference & Examples](#api-reference--examples)
- [Modules & Implementation Details](#modules--implementation-details)
- [Educational Insights & Next Steps](#educational-insights--next-steps)
- [License](#license)

---

## Motivation & Goals

Modern web frameworks abstract away the underlying networking stack. The purpose of this project was to lift the hood and understand:

1. **Socket Mechanics:** How the operating system manages TCP sockets via the Winsock2 API (`socket`, `bind`, `listen`, `accept`, `recv`, `send`).
2. **HTTP/1.1 Wire Protocol:** Manually framing, parsing, and crafting raw HTTP request lines, headers, chunking/content lengths, status lines, and responses.
3. **Concurrency:** Designing a thread-safe task queue and worker thread pool using Win32 primitives (`_beginthreadex`, `CRITICAL_SECTION`, and `CONDITION_VARIABLE`).
4. **Data Serialization:** Building a minimal JSON tokenizer, recursive-descent parser, and serializer completely from scratch in standard C.
5. **Robustness:** Implementing Windows Structured Exception Handling (SEH) to catch handler crashes without bringing down the server process.

---

## Key Features

- **Pure C (C99/C11)**: Zero third-party library dependencies; uses standard C runtime and native Windows APIs.
- **Winsock2 TCP Core**: Connection handling with socket timeouts, reusable addresses (`SO_REUSEADDR`), and graceful shutdown on `Ctrl+C`.
- **Custom HTTP/1.1 Parser**:
  - Request line parsing (Method, Path, HTTP version).
  - URL query string extraction and percent-decoding (`%20`, `+`, etc.).
  - Case-insensitive header parsing and lookup (`Content-Length`, `Content-Type`, `Authorization`, etc.).
  - Body payload extraction respecting `Content-Length`.
- **Pattern-Based Router**: Path parameter matching (e.g., `/api/users/:id`) with support for `GET`, `POST`, `PUT`, `PATCH`, `DELETE`, and `OPTIONS`.
- **Handcrafted JSON Engine**:
  - In-memory AST supporting `null`, booleans, IEEE-754 numbers, strings with unicode `\uXXXX` escape sequences, arrays, and objects.
  - Serialization with compact and pretty-printed formatting.
- **Win32 Thread Pool**: Worker threads processing incoming client sockets concurrently via condition variable signaling.
- **Crash Resilience (SEH)**: Handlers wrapped in Win32 `__try / __except` blocks to isolate exceptions and return a `500 Internal Server Error` instead of crashing.
- **Color-Coded Thread-Safe Logger**: Output to console (colorized via `SetConsoleTextAttribute`) and rotating/appending log file with timestamped levels (`DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`).
- **CORS Support**: Automatic preflight `OPTIONS` resolution and customizable headers (`Access-Control-Allow-Origin`, `Methods`, `Headers`).
- **INI Configuration Loader**: Custom INI configuration parser supporting comments, trimming, and type-safe casting.
- **In-Memory User Store**: Thread-safe sample RESTful resource with CRUD operations.
- **Extensible Modules**: Scaffolding for token-bucket Rate Limiting and Bearer Token Authentication middleware.

---

## Architecture Overview

```
                      +-----------------------------+
                      |       Client (curl/app)     |
                      +-----------------------------+
                                     |  TCP (HTTP/1.1)
                                     v
                      +-----------------------------+
                      |     Winsock2 Listen Loop    |
                      |   (main thread: accept())   |
                      +-----------------------------+
                                     |
                                     v  Enqueues client socket
                      +-----------------------------+
                      |       Win32 Thread Pool     |
                      |   (Worker Threads, CV, CS)  |
                      +-----------------------------+
                                     |
               +---------------------+---------------------+
               |                                           |
               v                                           v
+-----------------------------+             +-----------------------------+
|    http_parser (raw buf)    |             |       logger (Win32 CS)     |
|   - Request Line / Query    |             |   - Console (colored)       |
|   - Headers / Content-Len   |             |   - Log file (server.log)   |
|   - URL Decoder             |             +-----------------------------+
+-----------------------------+
               |
               v
+-----------------------------+
|        router_dispatch      |
|  - Pattern Matching (:id)   |
|  - Method Matching          |
+-----------------------------+
               |
        +------+------+-----------------------+
        |             |                       |
        v             v                       v
 [handler_health] [handler_echo]      [handler_*_user]
        |             |                       |
        |             |                       v
        |             +-------------> [Thread-Safe User Store]
        |                                     |
        +-------------------------------------+
                          |
                          v
            +-----------------------------+
            |      json (AST Parser)      |
            +-----------------------------+
                          |
                          v
            +-----------------------------+
            |     http_response_to_str    |
            |   - Status Line & Headers   |
            |   - CORS Headers            |
            |   - Payload & Content-Len   |
            +-----------------------------+
                          |
                          v  send() over TCP
                   [Client Socket]
```

---

## Project Structure

```
bloom/
├── build.bat                 # MSVC compilation and link script
├── README.md                 # Project documentation
├── .gitignore                # Git ignore rules for MSVC & build artifacts
├── config/
│   └── server.ini            # Server configuration (host, port, threads, limits)
├── include/
│   ├── config.h              # INI configuration loader header
│   ├── handlers.h            # REST handlers, rate limiter, auth middleware declarations
│   ├── http_parser.h         # HTTP request structures and parser prototypes
│   ├── http_response.h       # HTTP status codes, response builder prototypes
│   ├── json.h                # JSON parser, serializer, and value AST definitions
│   ├── logger.h              # Thread-safe color logging system prototypes
│   ├── router.h              # Routing table, pattern matching, dispatch declarations
│   ├── server.h              # Server core structure, lifecycle and config types
│   └── thread_pool.h         # Win32 worker thread pool definitions
└── src/
    ├── main.c                # Program entry point, CLI flag parsing, signal handling
    ├── server.c              # Winsock setup, accept loop, client connection worker, CORS
    ├── router.c              # Path segment tokenization, parameterized route matching
    ├── http_parser.c         # HTTP/1.1 raw stream parsing, URL decoding
    ├── http_response.c       # Response buffer generation, header formatting
    ├── thread_pool.c         # FIFO task queue, worker threads, condition variables
    ├── json.c                # Tokenizer, recursive-descent parser, serializer, memory cleanup
    ├── logger.c              # Mutex-protected console coloring and file logging
    ├── config.c              # INI parser with key-value store
    └── handlers.c            # Built-in REST endpoints, in-memory user CRUD, rate limiter, auth
```

---

## Prerequisites & Toolchain

- **Operating System:** Windows 10 / 11 / Windows Server
- **Compiler:** Microsoft Visual C++ (MSVC) — Visual Studio 2019, 2022, or Visual Studio Build Tools.
- **Libraries Linked:** `ws2_32.lib` (Winsock), `kernel32.lib`, `user32.lib` (included with Windows SDK).

---

## Building

### Option 1: Using `build.bat` (Recommended)

1. Open **Developer Command Prompt for VS** (or **Developer PowerShell for VS**).
2. Navigate to the project root and run:

```cmd
build.bat
```

> **Note:** The `build.bat` script automatically searches standard Visual Studio installation directories for `cl.exe` if it is not already in your system `PATH`. The compiled binary will be placed in `build\CBackendServer.exe`.

### Option 2: Manual Compilation via `cl.exe`

From a Developer Command Prompt:

```cmd
cl.exe /W4 /O2 /MD /D_WINSOCK_DEPRECATED_NO_WARNINGS ^
    /Iinclude ^
    /Febuild\CBackendServer.exe ^
    src\main.c src\logger.c src\json.c src\config.c ^
    src\http_parser.c src\http_response.c src\router.c ^
    src\thread_pool.c src\handlers.c src\server.c ^
    /link ws2_32.lib kernel32.lib user32.lib
```

---

## Running

Launch the server with default configuration:

```cmd
build\CBackendServer.exe
```

Or pass command-line arguments:

```cmd
build\CBackendServer.exe --port 8080 --host 0.0.0.0 --threads 8 --debug
```

Press `Ctrl+C` to initiate a clean shutdown.

### Command-Line Options

| Flag | Description | Default |
|------|-------------|---------|
| `--port <n>` | TCP listening port | `8080` (or value in `server.ini`) |
| `--host <addr>` | Bind IP address (`0.0.0.0` or `127.0.0.1`) | `0.0.0.0` |
| `--threads <n>` | Worker thread pool size | `8` |
| `--log <file>` | Path to output log file | `server.log` |
| `--debug` | Enable debug-level log output | Off (`LOG_INFO`) |
| `-h`, `--help` | Show command-line help screen | — |

### Configuration File (`server.ini`)

The server reads `config/server.ini` at startup. Command-line flags override configuration file settings.

```ini
[server]
port = 8080
host = 0.0.0.0
threads = 8
log_file = server.log
log_level = info
max_body_size = 4194304       # 4 MB max body size
keep_alive_timeout = 65       # Socket timeout in seconds

[cors]
enabled = true
allowed_origins = *
allowed_methods = GET, POST, PUT, DELETE, OPTIONS, PATCH
allowed_headers = Content-Type, Authorization
```

---

## API Reference & Examples

### Endpoint Summary

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/` | Welcome message and API directory (JSON) |
| `GET` | `/health` | Server health check, timestamp, and uptime |
| `POST` | `/api/echo` | Echoes back method, path, query params, and parsed JSON payload |
| `GET` | `/api/users` | Lists all active users |
| `GET` | `/api/users/:id` | Fetches a specific user by ID |
| `POST` | `/api/users` | Creates a new user (expects `{"name": "...", "email": "..."}`) |
| `PUT` | `/api/users/:id` | Updates user details |
| `PATCH`| `/api/users/:id` | Partial update of user details |
| `DELETE`| `/api/users/:id` | Soft-deletes a user |
| `GET` | `/api/env` | Dumps client request headers, HTTP version, and query metadata |
| `GET` | `/api/server-info` | Platform information, CPU cores, memory stats, and route list |
| `OPTIONS`| `*` | Automatic CORS preflight response (when CORS is enabled) |

---

### Quick `curl` Examples

#### 1. Health Check
```bash
curl -i http://localhost:8080/health
```

#### 2. Root Index
```bash
curl http://localhost:8080/
```

#### 3. Echo Request (Payload Reflection)
```bash
curl -X POST http://localhost:8080/api/echo?query=network_test \
  -H "Content-Type: application/json" \
  -d "{\"message\": \"Hello from low-level C!\", \"tags\": [\"winsock\", \"c\"]}"
```

#### 4. List All Users
```bash
curl http://localhost:8080/api/users
```

#### 5. Create a New User
```bash
curl -X POST http://localhost:8080/api/users \
  -H "Content-Type: application/json" \
  -d "{\"name\": \"Grace Hopper\", \"email\": \"grace@navy.mil\"}"
```

#### 6. Get a User by ID
```bash
curl http://localhost:8080/api/users/1
```

#### 7. Update a User
```bash
curl -X PUT http://localhost:8080/api/users/1 \
  -H "Content-Type: application/json" \
  -d "{\"name\": \"Alice Cooper\", \"email\": \"alice.cooper@test.com\"}"
```

#### 8. Delete a User
```bash
curl -X DELETE http://localhost:8080/api/users/1
```

#### 9. Server & Platform Diagnostics
```bash
curl http://localhost:8080/api/server-info
```

#### 10. CORS Preflight Check
```bash
curl -i -X OPTIONS http://localhost:8080/api/users \
  -H "Origin: http://localhost:3000" \
  -H "Access-Control-Request-Method: POST"
```

---

## Modules & Implementation Details

### 1. HTTP Parser (`src/http_parser.c`)
- Slices raw buffer up to `\r\n\r\n` (or `\n\n`) to parse headers.
- Automatically handles query strings (`?key=val`) and applies URL-decoding (`+` to space, `%XX` to byte).
- Resolves standard and custom headers into a fixed lookup array.
- Reads body content bounded by `Content-Length` and `max_body_size`.

### 2. Router (`src/router.c`)
- Implements a linked list of route patterns.
- Tokenizes URL paths by `/` delimiters.
- Matches literal segments (case-insensitive) and supports wildcard dynamic parameters prefixed with `:` (e.g. `:id`).

### 3. Thread Pool (`src/thread_pool.c`)
- Fixed number of worker threads spawned at startup using `_beginthreadex`.
- Queue synchronization protected by `CRITICAL_SECTION`.
- Workers sleep via `SleepConditionVariableCS` and are woken on demand when tasks are submitted.
- Draining and graceful thread join on server shutdown.

### 4. JSON Engine (`src/json.c`)
- Zero external dependencies.
- Recursive descent parser validating structure and generating a heap-allocated `JsonValue` tree.
- Full support for `JSON_NULL`, `JSON_BOOL`, `JSON_NUMBER`, `JSON_STRING`, `JSON_ARRAY`, and `JSON_OBJECT`.
- Handles string escaping and Unicode code points (`\uXXXX`).
- Safe recursive deallocation with `json_free()`.

### 5. Exception Safety (`src/server.c`)
- Handlers run inside Win32 Structured Exception Handling (`__try` / `__except`).
- If an uncaught access violation or segfault occurs inside a user-defined route handler, the server intercepts `0x...` exception codes, logs the incident, sends a clean `500 Internal Server Error`, and continues serving subsequent connections.

---

## Educational Insights & Next Steps

Building a backend in C exposes many networking realities that modern frameworks hide:
- **Streaming & Packet Fragmentation:** TCP is a byte stream, not a message protocol. Requests may arrive fragmented across multiple `recv()` calls.
- **Resource Cleanup:** In C, every `malloc` for headers, URLs, and bodies requires rigorous cleanup across every early-return path.
- **Thread Boundaries:** Shared state (like the `g_users` store or logger handle) must be guarded with synchronization primitives to prevent race conditions.

### Future Experiments to Explore

- [ ] **I/O Completion Ports (IOCP):** Transitioning from the current thread-per-connection / thread-pool model to Windows IOCP for high-scalability asynchronous I/O.
- [ ] **HTTP Keep-Alive Pipelining:** Reusing the same socket across multiple consecutive HTTP/1.1 requests until the timeout expires or `Connection: close` is received.
- [ ] **Static File Serving:** Adding `GET /static/*` with MIME-type detection and `TransmitFile` Win32 optimization.
- [ ] **TLS / HTTPS:** Implementing transport-layer encryption using the Windows SChannel (Security Support Provider Interface) API without OpenSSL.

---

## License

This project was developed for educational and experimental purposes. Feel free to use, modify, and learn from it.
