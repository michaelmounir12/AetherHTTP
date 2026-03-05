#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

struct HttpRequest {
  std::string method;
  std::string target;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

class HttpRequestParser {
public:
  // Parses an HTTP/1.x request. Expects CRLF line endings.
  // Returns std::nullopt if the request is incomplete or invalid.
  std::optional<HttpRequest> parse(std::string_view data) const;

private:
  static std::string trim(std::string s);
};

