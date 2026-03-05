#include "HttpRequestParser.h"
#include "Logger.h"
#include "ThreadPool.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>

#if !defined(__linux__)
int main() {
  Logger::instance().log(Logger::Level::Error,
                         "This server example is intended to be built/run on Linux (POSIX sockets).");
  return 1;
}
#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

class UniqueFd {
public:
  UniqueFd() = default;
  explicit UniqueFd(int fd) : fd_(fd) {}
  ~UniqueFd() { reset(); }

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this == &other) return *this;
    reset();
    fd_ = other.fd_;
    other.fd_ = -1;
    return *this;
  }

  int get() const { return fd_; }
  explicit operator bool() const { return fd_ >= 0; }

  void reset(int new_fd = -1) {
    if (fd_ >= 0) ::close(fd_);
    fd_ = new_fd;
  }

private:
  int fd_ = -1;
};

std::string errno_string() {
  return std::string(std::strerror(errno));
}

bool send_all(int fd, std::string_view data) {
  const char* p = data.data();
  std::size_t remaining = data.size();
  while (remaining > 0) {
    const ssize_t n = ::send(fd, p, remaining, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    p += static_cast<std::size_t>(n);
    remaining -= static_cast<std::size_t>(n);
  }
  return true;
}

// Read until we have a full HTTP request (headers + optional Content-Length body).
// Returns empty string on error/EOF.
std::string read_http_request(int fd, std::size_t max_bytes = 1024 * 1024) {
  std::string buf;
  buf.reserve(4096);

  std::size_t needed_total = 0; // 0 = unknown; otherwise header_end+4+content_length
  for (;;) {
    char tmp[4096];
    const ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
    if (n == 0) return {}; // EOF
    if (n < 0) {
      if (errno == EINTR) continue;
      return {};
    }

    buf.append(tmp, tmp + n);
    if (buf.size() > max_bytes) return {};

    if (needed_total == 0) {
      const auto header_end = buf.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        needed_total = header_end + 4;

        // Best-effort Content-Length extraction (case sensitive, basic).
        const std::string header_block = buf.substr(0, header_end);
        const std::string needle = "\r\nContent-Length:";
        std::size_t pos = header_block.find(needle);
        if (pos == std::string::npos) {
          // Also handle header starting at first line after request line
          if (header_block.rfind("Content-Length:", 0) == 0) pos = 0;
        } else {
          pos += 2; // skip leading CRLF to align at header start
        }

        if (pos != std::string::npos) {
          const std::size_t value_start = header_block.find(':', pos);
          if (value_start != std::string::npos) {
            const std::size_t value_end = header_block.find("\r\n", value_start);
            std::string value = header_block.substr(value_start + 1,
                                                    (value_end == std::string::npos)
                                                      ? std::string::npos
                                                      : (value_end - (value_start + 1)));
            // trim spaces
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
            try {
              const std::size_t cl = static_cast<std::size_t>(std::stoul(value));
              needed_total += cl;
            } catch (...) {
              return {};
            }
          }
        }
      }
    }

    if (needed_total != 0 && buf.size() >= needed_total) return buf;
  }
}

std::string make_response_200(std::string_view body, std::string_view content_type = "text/plain") {
  std::string resp;
  resp.reserve(256 + body.size());
  resp += "HTTP/1.1 200 OK\r\n";
  resp += "Connection: close\r\n";
  resp += "Content-Type: ";
  resp += content_type;
  resp += "\r\n";
  resp += "Content-Length: ";
  resp += std::to_string(body.size());
  resp += "\r\n\r\n";
  resp.append(body);
  return resp;
}

std::string make_response_400() {
  constexpr std::string_view body = "Bad Request\n";
  std::string resp;
  resp.reserve(256);
  resp += "HTTP/1.1 400 Bad Request\r\n";
  resp += "Connection: close\r\n";
  resp += "Content-Type: text/plain\r\n";
  resp += "Content-Length: ";
  resp += std::to_string(body.size());
  resp += "\r\n\r\n";
  resp += body;
  return resp;
}

} // namespace

int main() {
  Logger::instance().set_level(Logger::Level::Info);

  // Avoid process termination on broken pipe.
  ::signal(SIGPIPE, SIG_IGN);

  const uint16_t port = 8080;
  const std::size_t threads = std::max<std::size_t>(2, std::thread::hardware_concurrency());
  ThreadPool pool(threads);
  HttpRequestParser parser;

  UniqueFd listen_fd(::socket(AF_INET, SOCK_STREAM, 0));
  if (!listen_fd) {
    Logger::instance().log(Logger::Level::Error, std::string("socket() failed: ") + errno_string());
    return 1;
  }

  int yes = 1;
  if (::setsockopt(listen_fd.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    Logger::instance().log(Logger::Level::Warn, std::string("setsockopt(SO_REUSEADDR) failed: ") + errno_string());
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (::bind(listen_fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    Logger::instance().log(Logger::Level::Error, std::string("bind() failed: ") + errno_string());
    return 1;
  }

  if (::listen(listen_fd.get(), SOMAXCONN) < 0) {
    Logger::instance().log(Logger::Level::Error, std::string("listen() failed: ") + errno_string());
    return 1;
  }

  Logger::instance().log(Logger::Level::Info,
                         "Listening on 0.0.0.0:" + std::to_string(port) + " with " +
                           std::to_string(threads) + " threads");

  for (;;) {
    sockaddr_in client{};
    socklen_t client_len = sizeof(client);
    int client_fd = ::accept(listen_fd.get(), reinterpret_cast<sockaddr*>(&client), &client_len);
    if (client_fd < 0) {
      if (errno == EINTR) continue;
      Logger::instance().log(Logger::Level::Warn, std::string("accept() failed: ") + errno_string());
      continue;
    }

    pool.enqueue([client_fd, &parser] {
      UniqueFd fd(client_fd);

      std::string req_bytes = read_http_request(fd.get());
      if (req_bytes.empty()) return;

      auto req = parser.parse(req_bytes);
      if (!req) {
        const auto resp = make_response_400();
        (void)send_all(fd.get(), resp);
        return;
      }

      const std::string body =
        "Hello from C++ server\n"
        "Method: " +
        req->method + "\nTarget: " + req->target + "\n";

      const auto resp = make_response_200(body);
      (void)send_all(fd.get(), resp);
    });
  }
}

#endif

