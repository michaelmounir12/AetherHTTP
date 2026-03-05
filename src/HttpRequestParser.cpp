#include "HttpRequestParser.h"

#include <cctype>
#include <sstream>

static bool starts_with(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

std::string HttpRequestParser::trim(std::string s) {
  auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };
  while (!s.empty() && is_ws(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
  while (!s.empty() && is_ws(static_cast<unsigned char>(s.back()))) s.pop_back();
  return s;
}

std::optional<HttpRequest> HttpRequestParser::parse(std::string_view data) const {
  const auto header_end = data.find("\r\n\r\n");
  if (header_end == std::string_view::npos) return std::nullopt;

  const std::string_view header_block = data.substr(0, header_end);
  const std::string_view body_block = data.substr(header_end + 4);

  std::istringstream in(std::string(header_block));
  std::string line;

  HttpRequest req;

  if (!std::getline(in, line)) return std::nullopt;
  if (!line.empty() && line.back() == '\r') line.pop_back();

  {
    std::istringstream rl(line);
    if (!(rl >> req.method >> req.target >> req.version)) return std::nullopt;
    if (!starts_with(req.version, "HTTP/")) return std::nullopt;
  }

  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) break;

    const auto pos = line.find(':');
    if (pos == std::string::npos) return std::nullopt;

    auto key = trim(line.substr(0, pos));
    auto value = trim(line.substr(pos + 1));
    if (key.empty()) return std::nullopt;
    req.headers.emplace(std::move(key), std::move(value));
  }

  auto it = req.headers.find("Content-Length");
  if (it != req.headers.end()) {
    try {
      const std::size_t len = static_cast<std::size_t>(std::stoul(it->second));
      if (body_block.size() < len) return std::nullopt;
      req.body = std::string(body_block.substr(0, len));
    } catch (...) {
      return std::nullopt;
    }
  } else {
    req.body = std::string(body_block);
  }

  return req;
}

