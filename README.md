# Multithreaded HTTP Server (C++)

### Overview

This project is a simple **multithreaded HTTP server** written in C++20 for **Linux (POSIX sockets)**. It demonstrates:

- A reusable **thread pool** for handling multiple client connections concurrently.
- Basic **HTTP request parsing** (GET requests).
- **Routing** for `/`, `/about`, and `/files`.
- **Static file serving** for `index.html` and `about.html`.
- Proper **HTTP responses** with `Content-Type` and `Content-Length`.
- A simple **Logger** that writes to `server.log` with timestamps, levels, and client IP info.

> Note: The server uses POSIX APIs (`socket`, `bind`, `listen`, `accept`, etc.) and must be built and run on a Linux environment.

---

### Project Structure

- `CMakeLists.txt` – CMake build configuration.
- `src/`
  - `main.cpp` – Entry point; sets up the listening socket, thread pool, routing, and HTTP handling.
  - `ThreadPool.cpp` – Thread pool implementation.
  - `HttpRequestParser.cpp` (or `HTTPRequest` implementation, depending on your version) – Basic HTTP request parsing.
  - `Logger.cpp` – Logging implementation.
- `include/`
  - `ThreadPool.h` – Thread pool interface.
  - `HTTPRequest.h` – Simple HTTP request-line parser (`GET` requests).
  - `Logger.h` – Logger interface.
- `server.log` – Created at runtime; contains timestamped log entries.
- `index.html`, `about.html` – Optional static files served if present (otherwise built‑in HTML is returned).

---

### Requirements

- CMake **3.20+**
- A C++20 compiler (e.g. `g++` 11+, Clang 13+)
- Linux with POSIX sockets

---

### Building

From the project root:

```bash
cmake -S . -B build
cmake --build build -j
```

This will produce the server binary:

```bash
./build/server
```

---

### Running the Server

Run the server on port **8080**:

```bash
./build/server
```

You should see log output in `server.log` as requests come in.

Open a browser (or use `curl`) on the same machine:

- Home route:

  ```bash
  curl -v http://localhost:8080/
  ```

- About route:

  ```bash
  curl -v http://localhost:8080/about
  ```

- Files route:

  ```bash
  curl -v http://localhost:8080/files
  ```

If `index.html` or `about.html` exist in the project directory, their contents are served; otherwise, a built‑in HTML page is returned.

---

### Logging

The `Logger` writes log entries to `server.log`, for example:

- Server startup and listening port
- Client connections and disconnections
- Parsed HTTP requests (`METHOD path version` with client IP)
- Warnings/errors for socket operations and invalid requests

You can tail the log while the server is running:

```bash
tail -f server.log
```

---

### Thread Pool

The server uses a fixed‑size `ThreadPool`:

- A set of worker threads is created at startup.
- Each accepted client connection is enqueued as a task.
- Workers pull tasks from a shared queue and handle clients independently.
- The pool shuts down cleanly on destruction, joining all worker threads and releasing any remaining queued tasks.

This design avoids creating a new `std::thread` per connection, improving scalability and reducing overhead.

---

### Known Limitations / Next Steps

- HTTP support is intentionally minimal (basic GET only, no keep‑alive, no HTTPS).
- Error handling is simple and can be expanded (e.g. more detailed error pages).
- Static file serving is limited and does not yet support directory traversal, MIME database, or caching.

Possible improvements:

- Add support for additional HTTP methods (HEAD, POST).
- Serve files from a configurable document root.
- Add configuration options (port, thread count, log level) via command‑line flags or config file.
- Implement connection keep‑alive and more robust HTTP parsing.

