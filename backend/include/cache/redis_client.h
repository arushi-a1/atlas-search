// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A minimal Redis client speaking RESP directly over a TCP socket --
// enough for GET/SET/EXPIRE/DEL, which is all the query-result cache
// needs. For a production system you'd likely swap this for hiredis or
// redis-plus-plus; this exists to keep the project dependency-free and
// easy to read end-to-end.

#ifndef DISTRIBUTED_SEARCH_CACHE_REDIS_CLIENT_H_
#define DISTRIBUTED_SEARCH_CACHE_REDIS_CLIENT_H_

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace dse {
namespace cache {

class RedisClient {
 public:
  RedisClient(const std::string& host, int port);
  ~RedisClient();

  bool Connect();
  bool IsConnected() const { return fd_ >= 0; }

  std::optional<std::string> Get(const std::string& key);
  bool Set(const std::string& key, const std::string& value, int ttl_seconds = 0);
  bool Del(const std::string& key);

 private:
  std::string SendCommand(const std::vector<std::string>& args);
  bool Reconnect();

  std::string host_;
  int port_;
  int fd_ = -1;
  std::mutex mutex_;
};

}  // namespace cache
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_CACHE_REDIS_CLIENT_H_
