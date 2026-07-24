// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "http/http_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <stdexcept>

#include "common/logger.h"

namespace dse {
namespace http {

namespace {

constexpr int kBacklog = 128;
constexpr size_t kReadChunk = 8192;
constexpr size_t kMaxRequestBytes = 4 * 1024 * 1024;  // 4 MB safety cap

std::string UrlDecode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      int value = std::stoi(s.substr(i + 1, 2), nullptr, 16);
      out += static_cast<char>(value);
      i += 2;
    } else if (s[i] == '+') {
      out += ' ';
    } else {
      out += s[i];
    }
  }
  return out;
}

void ParseQueryString(const std::string& qs,
                       std::map<std::string, std::string>* out) {
  std::istringstream stream(qs);
  std::string pair;
  while (std::getline(stream, pair, '&')) {
    auto eq = pair.find('=');
    if (eq == std::string::npos) continue;
    std::string key = UrlDecode(pair.substr(0, eq));
    std::string value = UrlDecode(pair.substr(eq + 1));
    (*out)[key] = value;
  }
}

// Parses a raw HTTP/1.1 request. Returns false on malformed input.
bool ParseRequest(const std::string& raw, HttpRequest* request) {
  size_t header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) return false;

  std::istringstream stream(raw.substr(0, header_end));
  std::string request_line;
  std::getline(stream, request_line);
  if (!request_line.empty() && request_line.back() == '\r') {
    request_line.pop_back();
  }

  std::istringstream line_stream(request_line);
  std::string full_path;
  line_stream >> request->method >> full_path >> request->version;
  if (request->method.empty() || full_path.empty()) return false;

  auto qpos = full_path.find('?');
  if (qpos != std::string::npos) {
    request->path = full_path.substr(0, qpos);
    request->query_string = full_path.substr(qpos + 1);
    ParseQueryString(request->query_string, &request->query_params);
  } else {
    request->path = full_path;
  }

  std::string header_line;
  while (std::getline(stream, header_line)) {
    if (!header_line.empty() && header_line.back() == '\r') {
      header_line.pop_back();
    }
    if (header_line.empty()) continue;
    auto colon = header_line.find(':');
    if (colon == std::string::npos) continue;
    std::string key = header_line.substr(0, colon);
    size_t val_start = header_line.find_first_not_of(' ', colon + 1);
    std::string value =
        val_start == std::string::npos ? "" : header_line.substr(val_start);
    request->headers[key] = value;
  }

  request->body = raw.substr(header_end + 4);
  return true;
}

std::string SerializeResponse(const HttpResponse& response) {
  std::ostringstream out;
  out << "HTTP/1.1 " << response.status_code << " " << response.status_text
      << "\r\n";
  for (const auto& [key, value] : response.headers) {
    out << key << ": " << value << "\r\n";
  }
  out << "Content-Length: " << response.body.size() << "\r\n";
  out << "Connection: close\r\n";
  out << "Access-Control-Allow-Origin: *\r\n";
  out << "\r\n" << response.body;
  return out.str();
}

}  // namespace

HttpServer::HttpServer(const std::string& bind_address, int port,
                        size_t num_threads)
    : bind_address_(bind_address), port_(port), pool_(num_threads) {}

HttpServer::~HttpServer() {
  Stop();
  if (listen_fd_ >= 0) close(listen_fd_);
  if (epoll_fd_ >= 0) close(epoll_fd_);
}

void HttpServer::Get(const std::string& path, Router::Handler handler) {
  router_.AddRoute("GET", path, std::move(handler));
}

void HttpServer::Post(const std::string& path, Router::Handler handler) {
  router_.AddRoute("POST", path, std::move(handler));
}

int HttpServer::CreateListenSocket() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) throw std::runtime_error("socket() failed");

  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  if (bind_address_ == "0.0.0.0") {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    inet_pton(AF_INET, bind_address_.c_str(), &addr.sin_addr);
  }

  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    throw std::runtime_error("bind() failed on port " + std::to_string(port_));
  }
  if (listen(fd, kBacklog) < 0) {
    close(fd);
    throw std::runtime_error("listen() failed");
  }
  return fd;
}

void HttpServer::Run() {
  listen_fd_ = CreateListenSocket();
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) throw std::runtime_error("epoll_create1() failed");

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd_;
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);

  running_ = true;
  DSE_LOG_INFO("http_server", "Listening on " + bind_address_ + ":" +
                                   std::to_string(port_));
  AcceptLoop();
}

void HttpServer::Stop() { running_ = false; }

void HttpServer::AcceptLoop() {
  constexpr int kMaxEvents = 64;
  epoll_event events[kMaxEvents];

  while (running_) {
    int n = epoll_wait(epoll_fd_, events, kMaxEvents, 500 /* ms */);
    for (int i = 0; i < n; ++i) {
      if (events[i].data.fd == listen_fd_) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(listen_fd_,
                                reinterpret_cast<sockaddr*>(&client_addr),
                                &len);
        if (client_fd < 0) continue;
        // Hand the connection off to the thread pool so the accept loop
        // never blocks on slow clients.
        pool_.Enqueue([this, client_fd] { HandleConnection(client_fd); });
      }
    }
  }
}

void HttpServer::HandleConnection(int client_fd) {
  std::string raw;
  char buf[kReadChunk];
  ssize_t n;
  // Read until we see the end of headers or hit the safety cap. This is a
  // simple blocking read per connection; the thread pool provides the
  // concurrency, matching a thread-per-request-but-bounded model.
  while (raw.find("\r\n\r\n") == std::string::npos &&
         raw.size() < kMaxRequestBytes &&
         (n = recv(client_fd, buf, sizeof(buf), 0)) > 0) {
    raw.append(buf, n);
  }

  HttpRequest request;
  HttpResponse response;
  if (raw.empty() || !ParseRequest(raw, &request)) {
    response = HttpResponse::Json(400, R"({"error":"malformed request"})");
  } else if (!router_.Dispatch(request, &response)) {
    response = HttpResponse::Json(404, R"({"error":"not found"})");
  }

  std::string out = SerializeResponse(response);
  size_t sent = 0;
  while (sent < out.size()) {
    ssize_t written = send(client_fd, out.data() + sent, out.size() - sent, 0);
    if (written <= 0) break;
    sent += written;
  }
  close(client_fd);
}

}  // namespace http
}  // namespace dse
