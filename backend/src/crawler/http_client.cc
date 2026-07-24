// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "crawler/http_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>

namespace dse {
namespace crawler {

FetchResult HttpClient::Get(const std::string& host, int port,
                             const std::string& path,
                             const std::string& user_agent,
                             std::chrono::milliseconds timeout) {
  FetchResult result;

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  std::string port_str = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
    result.error = "DNS resolution failed";
    return result;
  }

  int fd = -1;
  for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;

    timeval tv{};
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);

  if (fd < 0) {
    result.error = "connect() failed";
    return result;
  }

  std::ostringstream req;
  req << "GET " << path << " HTTP/1.1\r\n"
      << "Host: " << host << "\r\n"
      << "User-Agent: " << user_agent << "\r\n"
      << "Accept: text/html\r\n"
      << "Connection: close\r\n\r\n";
  std::string req_str = req.str();

  size_t sent = 0;
  while (sent < req_str.size()) {
    ssize_t n = send(fd, req_str.data() + sent, req_str.size() - sent, 0);
    if (n <= 0) { result.error = "send() failed"; close(fd); return result; }
    sent += n;
  }

  std::string raw;
  char buf[8192];
  ssize_t n;
  while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
    raw.append(buf, n);
    if (raw.size() > 16 * 1024 * 1024) break;  // 16MB safety cap
  }
  close(fd);

  size_t header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    result.error = "malformed response";
    return result;
  }

  std::istringstream header_stream(raw.substr(0, header_end));
  std::string status_line;
  std::getline(header_stream, status_line);
  size_t first_space = status_line.find(' ');
  if (first_space != std::string::npos) {
    result.status_code = std::atoi(status_line.c_str() + first_space + 1);
  }

  std::string header_line;
  bool chunked = false;
  while (std::getline(header_stream, header_line)) {
    if (!header_line.empty() && header_line.back() == '\r') header_line.pop_back();
    std::string lower = header_line;
    for (auto& c : lower) c = std::tolower(static_cast<unsigned char>(c));
    if (lower.rfind("content-type:", 0) == 0) {
      result.content_type = header_line.substr(13);
    }
    if (lower.rfind("transfer-encoding:", 0) == 0 &&
        lower.find("chunked") != std::string::npos) {
      chunked = true;
    }
  }

  std::string body = raw.substr(header_end + 4);
  if (chunked) {
    // De-chunk the transfer-encoded body.
    std::string decoded;
    size_t pos = 0;
    while (pos < body.size()) {
      size_t line_end = body.find("\r\n", pos);
      if (line_end == std::string::npos) break;
      size_t chunk_size = std::stoul(body.substr(pos, line_end - pos), nullptr, 16);
      if (chunk_size == 0) break;
      pos = line_end + 2;
      decoded.append(body, pos, chunk_size);
      pos += chunk_size + 2;  // skip trailing CRLF
    }
    body = decoded;
  }

  result.body = body;
  result.success = result.status_code >= 200 && result.status_code < 400;
  return result;
}

}  // namespace crawler
}  // namespace dse
