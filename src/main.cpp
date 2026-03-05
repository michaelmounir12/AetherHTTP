// Basic HTTP server using POSIX sockets that listens on port 8080
// and handles multiple clients using a ThreadPool.
// Each client is handled by a worker thread from the pool, parsing
// a basic HTTP GET request and responding with 200 OK + HTML.

#include "Logger.h"
#include "ThreadPool.h"
#include "HTTPRequest.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <thread> // for std::thread::hardware_concurrency

#if !defined(__linux__)
int main() {
  Logger::instance().log(Logger::Level::Error,
                         "This server example is intended to be built/run on Linux (POSIX sockets).");
  return 1;
}
#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void handle_client(int client_fd) {
  Logger::instance().log(Logger::Level::Info, "Client connected");

  // Read a basic HTTP request (we only care about the request line).
  std::string request_data;
  request_data.reserve(1024);

  char buffer[1024];
  bool done = false;
  while (!done) {
    const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (n == 0) {
      // Client closed connection
      break;
    }
    if (n < 0) {
      if (errno == EINTR) continue;
      Logger::instance().log(Logger::Level::Warn,
                             std::string("recv() failed: ") + std::strerror(errno));
      break;
    }

    request_data.append(buffer, buffer + n);

    // Stop once we've read the end of headers.
    if (request_data.find("\r\n\r\n") != std::string::npos ||
        request_data.size() > 8192) {
      done = true;
    }
  }

  HTTPRequest req;
  if (!request_data.empty() && req.parse(request_data)) {
    Logger::instance().log(Logger::Level::Info,
                           "HTTP " + req.method() + " " + req.path() + " " + req.version());

    const std::string body =
        "<!DOCTYPE html>\n"
        "<html><head><title>Simple Server</title></head>"
        "<body><h1>Hello from C++ HTTP server</h1>"
        "<p>You requested: " +
        req.path() +
        "</p></body></html>";

    std::string response;
    response.reserve(256 + body.size());
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/html; charset=utf-8\r\n";
    response += "Connection: close\r\n";
    response += "Content-Length: ";
    response += std::to_string(body.size());
    response += "\r\n\r\n";
    response += body;

    const char* p = response.data();
    std::size_t remaining = response.size();
    while (remaining > 0) {
      const ssize_t sent = ::send(client_fd, p, remaining, 0);
      if (sent < 0) {
        if (errno == EINTR) continue;
        Logger::instance().log(Logger::Level::Warn,
                               std::string("send() failed: ") + std::strerror(errno));
        break;
      }
      p += sent;
      remaining -= static_cast<std::size_t>(sent);
    }
  } else {
    Logger::instance().log(Logger::Level::Warn, "Failed to parse HTTP request");
  }

  ::close(client_fd);
  Logger::instance().log(Logger::Level::Info, "Client disconnected");
}

int main() {
  Logger::instance().set_level(Logger::Level::Info);

  // Fixed-size thread pool; adjust size as needed.
  const std::size_t thread_count =
      std::max<std::size_t>(4, std::thread::hardware_concurrency());
  ThreadPool pool(thread_count);

  int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    Logger::instance().log(Logger::Level::Error,
                           std::string("socket() failed: ") + std::strerror(errno));
    return 1;
  }

  int yes = 1;
  ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(8080);

  if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    Logger::instance().log(Logger::Level::Error,
                           std::string("bind() failed: ") + std::strerror(errno));
    ::close(listen_fd);
    return 1;
  }

  if (::listen(listen_fd, SOMAXCONN) < 0) {
    Logger::instance().log(Logger::Level::Error,
                           std::string("listen() failed: ") + std::strerror(errno));
    ::close(listen_fd);
    return 1;
  }

  Logger::instance().log(Logger::Level::Info, "Listening on port 8080");

  for (;;) {
    sockaddr_in client{};
    socklen_t client_len = sizeof(client);
    int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client), &client_len);
    if (client_fd < 0) {
      Logger::instance().log(Logger::Level::Error,
                             std::string("accept() failed: ") + std::strerror(errno));
      continue;
    }

    // Queue client handling in the thread pool.
    pool.enqueue([client_fd] { handle_client(client_fd); });
  }
}

#endif
