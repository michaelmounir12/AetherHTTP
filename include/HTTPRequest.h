#pragma once

#include <sstream>
#include <string>
#include <string_view>

// Simple HTTP request line parser for basic HTTP/1.x GET requests.
// Example supported first line:
//   "GET /index.html HTTP/1.1\r\n"
//
// This class focuses only on extracting:
//   - method  (e.g. "GET")
//   - path    (e.g. "/index.html")
//   - version (e.g. "HTTP/1.1")
class HTTPRequest {
public:
  // Parse a raw HTTP request string (at least the request line).
  // Returns true on success, false on failure.
  bool parse(std::string_view raw) {
    const auto pos = raw.find("\r\n");
    const std::string_view line_view =
        (pos == std::string_view::npos) ? raw : raw.substr(0, pos);

    std::istringstream line(std::string(line_view));
    std::string m;
    std::string p;
    std::string v;
    if (!(line >> m >> p >> v)) {
      return false;
    }

    // For this basic implementation we only support GET.
    if (m != "GET") {
      return false;
    }

    // Basic sanity checks.
    if (p.empty() || v.rfind("HTTP/", 0) != 0) {
      return false;
    }

    method_ = std::move(m);
    path_ = std::move(p);
    version_ = std::move(v);
    return true;
  }

  const std::string& method() const { return method_; }
  const std::string& path() const { return path_; }
  const std::string& version() const { return version_; }

private:
  std::string method_;
  std::string path_;
  std::string version_;
};

