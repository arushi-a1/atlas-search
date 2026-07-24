// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A pragmatic, dependency-free HTML parser: not a full DOM implementation,
// but enough to pull structured signal out of a crawled page -- title,
// visible text, outbound links, and meta description -- for indexing.

#ifndef DISTRIBUTED_SEARCH_PARSER_HTML_PARSER_H_
#define DISTRIBUTED_SEARCH_PARSER_HTML_PARSER_H_

#include <string>
#include <vector>

namespace dse {
namespace parser {

struct ParsedPage {
  std::string title;
  std::string meta_description;
  std::string visible_text;
  std::vector<std::string> links;      // resolved absolute URLs
  std::vector<std::string> headings;   // h1-h3 text, boosted in ranking
};

class HtmlParser {
 public:
  // base_url is used to resolve relative hrefs into absolute URLs.
  static ParsedPage Parse(const std::string& html, const std::string& base_url);

 private:
  static std::string ResolveUrl(const std::string& base, const std::string& href);
  static std::string StripTags(const std::string& fragment);
  static std::string DecodeEntities(const std::string& text);
};

}  // namespace parser
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_PARSER_HTML_PARSER_H_
