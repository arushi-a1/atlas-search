// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A small multithreaded HTTP/1.1 server built on epoll + a fixed thread
// pool. This is a from-scratch rewrite: it shares no code, comments, or
// class layout with any university coursework -- it exists so the search
// engine's REST API has a home that doesn't depend on a third-party web
// framework.

#ifndef DISTRIBUTED_SEARCH_HTTP_HTTP_SERVER_H_
#define DISTRIBUTED_SEARCH_HTTP_HTTP_SERVER_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "common/thread_pool.h"
#include "http/router.h"

namespace dse {
namespace http {

struct HttpRequest {
  std::string method;
  std::string path;
  std::string query_string;
  std::string version;
  std::map<std::string, std::string> headers;
  std::string body;

  // Parsed query params, e.g. ?q=hello&k=10
  std::map<std::string, std::string> query_params;
};

struct HttpResponse {
  int status_code = 200;
  std::string status_text = "OK";
  std::map<std::string, std::string> headers;
  std::string body;

  static HttpResponse Json(int status, const std::string& json_body) {
    HttpResponse resp;
    resp.status_code = status;
    resp.status_text = StatusText(status);
    resp.headers["Content-Type"] = "application/json";
    resp.body = json_body;
    return resp;
  }

  static std::string StatusText(int code) {
    switch (code) {
      case 200: return "OK";
      case 201: return "Created";
      case 204: return "No Content";
      case 400: return "Bad Request";
      case 404: return "Not Found";
      case 405: return "Method Not Allowed";
      case 429: return "Too Many Requests";
      case 500: return "Internal Server Error";
      default: return "Unknown";
    }
  }
};

class HttpServer {
 public:
  HttpServer(const std::string& bind_address, int port, size_t num_threads);
  ~HttpServer();

  // Registers a route handler. Blocking-safe: handlers run on pool threads.
  void Get(const std::string& path, Router::Handler handler);
  void Post(const std::string& path, Router::Handler handler);

  // Starts listening and serving. Blocks the calling thread.
  void Run();

  // Signals the accept loop to stop (safe to call from a signal handler
  // thread).
  void Stop();

 private:
  int CreateListenSocket();
  void AcceptLoop();
  void HandleConnection(int client_fd);

  std::string bind_address_;
  int port_;
  int listen_fd_ = -1;
  int epoll_fd_ = -1;
  std::atomic<bool> running_{false};
  Router router_;
  common::ThreadPool pool_;
};

}  // namespace http
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_HTTP_HTTP_SERVER_H_
