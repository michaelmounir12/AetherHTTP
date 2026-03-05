// Basic TCP server using POSIX sockets that listens on port 8080
// and handles multiple clients using a ThreadPool.
// Each client is handled by a worker thread from the pool, echoing
// received messages back to the client.

#include "Logger.h"
#include "ThreadPool.h"

#include <cerrno>
#include <cstring>
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

  char buffer[4096];
  for (;;) {
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

    ssize_t total_sent = 0;
    while (total_sent < n) {
      const ssize_t sent = ::send(client_fd, buffer + total_sent,
                                  static_cast<std::size_t>(n - total_sent), 0);
      if (sent < 0) {
        if (errno == EINTR) continue;
        Logger::instance().log(Logger::Level::Warn,
                               std::string("send() failed: ") + std::strerror(errno));
        goto out;
      }
      total_sent += sent;
    }
  }

out:
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
