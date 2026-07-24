// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "http/router.h"

#include "http/http_server.h"

namespace dse {
namespace http {

void Router::AddRoute(const std::string& method, const std::string& path,
                       Handler handler) {
  routes_[{method, path}] = std::move(handler);
}

bool Router::Dispatch(const HttpRequest& request,
                       HttpResponse* response) const {
  auto it = routes_.find({request.method, request.path});
  if (it == routes_.end()) return false;
  *response = it->second(request);
  return true;
}

}  // namespace http
}  // namespace dse
