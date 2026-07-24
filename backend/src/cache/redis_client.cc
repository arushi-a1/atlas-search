// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "cache/redis_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sstream>

#include "common/logger.h"

namespace dse {
namespace cache {

RedisClient::RedisClient(const std::string& host, int port)
    : host_(host), port_(port) {}

RedisClient::~RedisClient() {
  if (fd_ >= 0) close(fd_);
}

bool RedisClient::Connect() {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  std::string port_str = std::to_string(port_);
  if (getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &res) != 0) {
    return false;
  }
  for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
    fd_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd_ < 0) continue;
    if (connect(fd_, p->ai_addr, p->ai_addrlen) == 0) break;
    close(fd_);
    fd_ = -1;
  }
  freeaddrinfo(res);
  if (fd_ < 0) {
    DSE_LOG_WARN("redis_client", "failed to connect to " + host_ + ":" + port_str);
  }
  return fd_ >= 0;
}

bool RedisClient::Reconnect() {
  if (fd_ >= 0) { close(fd_); fd_ = -1; }
  return Connect();
}

// Encodes args as a RESP array of bulk strings, e.g.
// *2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n
std::string RedisClient::SendCommand(const std::vector<std::string>& args) {
  if (fd_ < 0 && !Reconnect()) return "";

  std::ostringstream cmd;
  cmd << "*" << args.size() << "\r\n";
  for (const auto& arg : args) {
    cmd << "$" << arg.size() << "\r\n" << arg << "\r\n";
  }
  std::string cmd_str = cmd.str();

  size_t sent = 0;
  while (sent < cmd_str.size()) {
    ssize_t n = send(fd_, cmd_str.data() + sent, cmd_str.size() - sent, 0);
    if (n <= 0) {
      if (!Reconnect()) return "";
      sent = 0;
      continue;
    }
    sent += n;
  }

  std::string response;
  char buf[4096];
  ssize_t n = recv(fd_, buf, sizeof(buf), 0);
  if (n <= 0) return "";
  response.append(buf, n);

  // For bulk strings, keep reading until we have the full declared length.
  if (!response.empty() && response[0] == '$') {
    size_t line_end = response.find("\r\n");
    if (line_end != std::string::npos) {
      long declared = std::stol(response.substr(1, line_end - 1));
      if (declared >= 0) {
        size_t needed = line_end + 2 + declared + 2;
        while (response.size() < needed) {
          n = recv(fd_, buf, sizeof(buf), 0);
          if (n <= 0) break;
          response.append(buf, n);
        }
      }
    }
  }
  return response;
}

std::optional<std::string> RedisClient::Get(const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string resp = SendCommand({"GET", key});
  if (resp.empty() || resp[0] != '$') return std::nullopt;
  if (resp.rfind("$-1", 0) == 0) return std::nullopt;  // key miss

  size_t line_end = resp.find("\r\n");
  long declared = std::stol(resp.substr(1, line_end - 1));
  if (declared < 0) return std::nullopt;
  return resp.substr(line_end + 2, declared);
}

bool RedisClient::Set(const std::string& key, const std::string& value,
                       int ttl_seconds) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> args = {"SET", key, value};
  if (ttl_seconds > 0) {
    args.push_back("EX");
    args.push_back(std::to_string(ttl_seconds));
  }
  std::string resp = SendCommand(args);
  return resp.rfind("+OK", 0) == 0;
}

bool RedisClient::Del(const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string resp = SendCommand({"DEL", key});
  return !resp.empty() && resp[0] == ':';
}

}  // namespace cache
}  // namespace dse
