// Basic TCP server using POSIX sockets that listens on port 8080
// and accepts a single client connection, printing "Client connected".

#include "Logger.h"

#include <cerrno>
#include <cstring>

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

int main() {
  Logger::instance().set_level(Logger::Level::Info);

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

  if (::listen(listen_fd, 1) < 0) {
    Logger::instance().log(Logger::Level::Error,
                           std::string("listen() failed: ") + std::strerror(errno));
    ::close(listen_fd);
    return 1;
  }

  Logger::instance().log(Logger::Level::Info, "Listening on port 8080");

  sockaddr_in client{};
  socklen_t client_len = sizeof(client);
  int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client), &client_len);
  if (client_fd < 0) {
    Logger::instance().log(Logger::Level::Error,
                           std::string("accept() failed: ") + std::strerror(errno));
    ::close(listen_fd);
    return 1;
  }

  Logger::instance().log(Logger::Level::Info, "Client connected");

  ::close(client_fd);
  ::close(listen_fd);
  return 0;
}

#endif
