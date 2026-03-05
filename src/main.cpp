// Basic HTTP server using POSIX sockets that listens on port 8080
// and handles multiple clients using a ThreadPool.
// Each client is handled by a worker thread from the pool, parsing
// a basic HTTP GET request and responding with 200 OK + HTML.

#include "Logger.h"
#include "ThreadPool.h"
#include "HTTPRequest.h"

#include <cerrno>
#include <cstring>
#include <exception>
#include <fstream>
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

namespace {

void send_http_response(int client_fd,
                        std::string_view status_line,
                        std::string_view content_type,
                        const std::string& body) {
  std::string response;
  response.reserve(256 + body.size());
  response.append(status_line);
  response.append("\r\n");
  response.append("Content-Type: ");
  response.append(content_type);
  response.append("\r\nConnection: close\r\nContent-Length: ");
  response.append(std::to_string(body.size()));
  response.append("\r\n\r\n");
  response.append(body);

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
}

std::string read_file_to_string(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::string data;
  in.seekg(0, std::ios::end);
  const std::streampos size = in.tellg();
  if (size > 0) {
    data.resize(static_cast<std::size_t>(size));
    in.seekg(0, std::ios::beg);
    in.read(data.data(), size);
  }
  return data;
}

std::string guess_content_type(const std::string& path) {
  auto dot_pos = path.rfind('.');
  if (dot_pos == std::string::npos) {
    return "application/octet-stream";
  }
  const std::string ext = path.substr(dot_pos + 1);
  if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
  if (ext == "css") return "text/css; charset=utf-8";
  if (ext == "js") return "application/javascript";
  if (ext == "json") return "application/json";
  if (ext == "png") return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "gif") return "image/gif";
  if (ext == "txt") return "text/plain; charset=utf-8";
  return "application/octet-stream";
}

} // namespace

void handle_client(int client_fd, std::string client_ip) {
  Logger::instance().log(Logger::Level::Info, "Client connected from " + client_ip);

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
                           "Request from " + client_ip + " " + req.method() + " " + req.path() +
                             " " + req.version());

    // Basic routing.
    if (req.path() == "/" || req.path() == "/index.html") {
      // Serve index.html if present, otherwise a simple built-in page.
      const std::string file_path = "index.html";
      std::string body = read_file_to_string(file_path);
      std::string content_type = "text/html; charset=utf-8";
      if (!body.empty()) {
        content_type = guess_content_type(file_path);
      } else {
        body =
            "<!DOCTYPE html><html><head><title>Home</title></head>"
            "<body><h1>Welcome</h1><p>This is the home page.</p></body></html>";
      }
      send_http_response(client_fd,
                         "HTTP/1.1 200 OK",
                         content_type,
                         body);
    } else if (req.path() == "/about" || req.path() == "/about.html") {
      // Serve about.html if present, otherwise a simple built-in page.
      const std::string file_path = "about.html";
      std::string body = read_file_to_string(file_path);
      std::string content_type = "text/html; charset=utf-8";
      if (!body.empty()) {
        content_type = guess_content_type(file_path);
      } else {
        body =
            "<!DOCTYPE html><html><head><title>About</title></head>"
            "<body><h1>About</h1><p>Basic about page.</p></body></html>";
      }
      send_http_response(client_fd,
                         "HTTP/1.1 200 OK",
                         content_type,
                         body);
    } else if (req.path() == "/files") {
      // Simple placeholder for /files route.
      const std::string body =
          "<!DOCTYPE html><html><head><title>Files</title></head>"
          "<body><h1>Files</h1><p>Files route placeholder.</p></body></html>";
      send_http_response(client_fd,
                         "HTTP/1.1 200 OK",
                         "text/html; charset=utf-8",
                         body);
    } else {
      const std::string body_404 =
          "<!DOCTYPE html><html><head><title>404 Not Found</title></head>"
          "<body><h1>404 Not Found</h1></body></html>";
      send_http_response(client_fd,
                         "HTTP/1.1 404 Not Found",
                         "text/html; charset=utf-8",
                         body_404);
    }
  } else {
    Logger::instance().log(Logger::Level::Warn,
                           "Failed to parse HTTP request from " + client_ip);

    const std::string body_400 =
        "<!DOCTYPE html><html><head><title>400 Bad Request</title></head>"
        "<body><h1>400 Bad Request</h1><p>Your request could not be understood.</p></body></html>";
    send_http_response(client_fd,
                       "HTTP/1.1 400 Bad Request",
                       "text/html; charset=utf-8",
                       body_400);
  }

  ::close(client_fd);
  Logger::instance().log(Logger::Level::Info, "Client disconnected from " + client_ip);
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

    char ip_buf[INET_ADDRSTRLEN] = {};
    const char* ip_str =
        ::inet_ntop(AF_INET, &client.sin_addr, ip_buf, static_cast<socklen_t>(sizeof(ip_buf)));
    std::string client_ip = ip_str ? ip_str : "unknown";

    // Queue client handling in the thread pool. If the pool rejects
    // the task for any reason, make sure we close the client socket.
    try {
      pool.enqueue([client_fd, client_ip] { handle_client(client_fd, client_ip); });
    } catch (const std::exception& ex) {
      Logger::instance().log(Logger::Level::Error,
                             std::string("Failed to enqueue client: ") + ex.what());
      ::close(client_fd);
    } catch (...) {
      Logger::instance().log(Logger::Level::Error,
                             "Failed to enqueue client: unknown error");
      ::close(client_fd);
    }
  }
}

#endif
