// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A minimal blocking HTTP/1.1 GET client (plain TCP; TLS is out of scope
// for this from-scratch client -- in production, swap this for libcurl
// with HTTPS enabled, see docs/ARCHITECTURE.md).

#ifndef DISTRIBUTED_SEARCH_CRAWLER_HTTP_CLIENT_H_
#define DISTRIBUTED_SEARCH_CRAWLER_HTTP_CLIENT_H_

#include <chrono>
#include <string>

namespace dse {
namespace crawler {

struct FetchResult {
  bool success = false;
  int status_code = 0;
  std::string body;
  std::string content_type;
  std::string error;
};

class HttpClient {
 public:
  static FetchResult Get(const std::string& host, int port,
                          const std::string& path, const std::string& user_agent,
                          std::chrono::milliseconds timeout);
};

}  // namespace crawler
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_CRAWLER_HTTP_CLIENT_H_
