// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#ifndef DISTRIBUTED_SEARCH_HTTP_ROUTER_H_
#define DISTRIBUTED_SEARCH_HTTP_ROUTER_H_

#include <functional>
#include <map>
#include <string>
#include <utility>

namespace dse {
namespace http {

struct HttpRequest;
struct HttpResponse;

class Router {
 public:
  using Handler = std::function<HttpResponse(const HttpRequest&)>;

  void AddRoute(const std::string& method, const std::string& path,
                Handler handler);

  // Returns true and fills response if a route matched; otherwise returns
  // false and the caller should respond 404.
  bool Dispatch(const HttpRequest& request, HttpResponse* response) const;

 private:
  std::map<std::pair<std::string, std::string>, Handler> routes_;
};

}  // namespace http
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_HTTP_ROUTER_H_
