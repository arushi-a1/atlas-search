// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "parser/html_parser.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>

namespace dse {
namespace parser {

namespace {

// Extracts an attribute value (e.g. href="...") from a tag's inner text.
std::string ExtractAttr(const std::string& tag, const std::string& attr) {
  std::regex attr_re(attr + R"(\s*=\s*["']([^"']*)["'])", std::regex::icase);
  std::smatch m;
  if (std::regex_search(tag, m, attr_re)) return m[1].str();
  return "";
}

}  // namespace

std::string HtmlParser::DecodeEntities(const std::string& text) {
  static const std::unordered_map<std::string, std::string> kEntities = {
      {"&amp;", "&"}, {"&lt;", "<"},   {"&gt;", ">"},  {"&quot;", "\""},
      {"&#39;", "'"}, {"&apos;", "'"}, {"&nbsp;", " "}};
  std::string out = text;
  for (const auto& [entity, replacement] : kEntities) {
    size_t pos = 0;
    while ((pos = out.find(entity, pos)) != std::string::npos) {
      out.replace(pos, entity.size(), replacement);
      pos += replacement.size();
    }
  }
  return out;
}

std::string HtmlParser::ResolveUrl(const std::string& base,
                                    const std::string& href) {
  if (href.empty()) return "";
  if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0) {
    return href;
  }
  if (href.rfind("//", 0) == 0) {
    auto scheme_end = base.find("://");
    std::string scheme = scheme_end == std::string::npos
                              ? "https"
                              : base.substr(0, scheme_end);
    return scheme + ":" + href;
  }
  // Strip scheme://host to get the origin.
  auto scheme_end = base.find("://");
  if (scheme_end == std::string::npos) return href;
  auto host_start = scheme_end + 3;
  auto path_start = base.find('/', host_start);
  std::string origin = path_start == std::string::npos
                            ? base
                            : base.substr(0, path_start);
  if (href[0] == '/') return origin + href;
  // Relative to the current directory of base's path.
  std::string base_dir = base;
  auto last_slash = base.rfind('/');
  if (last_slash != std::string::npos && last_slash > host_start) {
    base_dir = base.substr(0, last_slash + 1);
  } else {
    base_dir = origin + "/";
  }
  return base_dir + href;
}

namespace {

// Tags that end a run of text in a browser (block-level elements, line
// breaks, table cells, and document-structure tags). When one of these is
// stripped we emit a space so "<p>foo</p><p>bar</p>" becomes "foo bar"
// rather than "foobar". Inline tags (<b>, <span>, <a>, ...) are removed
// without a space so "<b>W</b>ord" stays "Word".
bool IsBlockTag(const std::string& tag_body) {
  static const char* kBlockTags[] = {
      "address", "article", "aside", "blockquote", "body", "br", "dd",
      "div", "dl", "dt", "fieldset", "figcaption", "figure", "footer",
      "form", "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hr",
      "html", "li", "main", "meta", "nav", "ol", "p", "pre", "section",
      "table", "tbody", "td", "tfoot", "th", "thead", "title", "tr", "ul"};
  // tag_body is everything between '<' and '>', e.g. "/p", "br/",
  // "div class=x". Pull out the bare lowercase tag name.
  size_t i = 0;
  if (i < tag_body.size() && tag_body[i] == '/') ++i;
  std::string name;
  while (i < tag_body.size() &&
         std::isalnum(static_cast<unsigned char>(tag_body[i]))) {
    name += static_cast<char>(
        std::tolower(static_cast<unsigned char>(tag_body[i])));
    ++i;
  }
  for (const char* t : kBlockTags) {
    if (name == t) return true;
  }
  return false;
}

}  // namespace

std::string HtmlParser::StripTags(const std::string& fragment) {
  std::string out;
  std::string tag_body;
  bool in_tag = false;
  for (char c : fragment) {
    if (c == '<') {
      in_tag = true;
      tag_body.clear();
      continue;
    }
    if (c == '>' && in_tag) {
      in_tag = false;
      if (IsBlockTag(tag_body)) out += ' ';
      continue;
    }
    if (in_tag) {
      tag_body += c;
    } else {
      out += c;
    }
  }
  return out;
}

ParsedPage HtmlParser::Parse(const std::string& html,
                              const std::string& base_url) {
  ParsedPage page;

  // Drop script/style bodies entirely -- their contents aren't visible text
  // and often aren't valid to naively strip-tag.
  std::string cleaned = html;
  static const std::regex kScriptStyle(
      R"(<(script|style)[^>]*>[\s\S]*?</\1>)", std::regex::icase);
  cleaned = std::regex_replace(cleaned, kScriptStyle, " ");

  static const std::regex kTitleRe(R"(<title[^>]*>([\s\S]*?)</title>)",
                                    std::regex::icase);
  std::smatch m;
  if (std::regex_search(cleaned, m, kTitleRe)) {
    page.title = DecodeEntities(StripTags(m[1].str()));
  }

  static const std::regex kMetaDescRe(
      R"(<meta\s+[^>]*name\s*=\s*["']description["'][^>]*>)",
      std::regex::icase);
  if (std::regex_search(cleaned, m, kMetaDescRe)) {
    page.meta_description = DecodeEntities(ExtractAttr(m[0].str(), "content"));
  }

  static const std::regex kAnchorRe(R"(<a\s+[^>]*href\s*=\s*["'][^"']*["'][^>]*>)",
                                     std::regex::icase);
  for (auto it = std::sregex_iterator(cleaned.begin(), cleaned.end(), kAnchorRe);
       it != std::sregex_iterator(); ++it) {
    std::string href = ExtractAttr(it->str(), "href");
    if (href.empty() || href[0] == '#') continue;
    if (href.rfind("mailto:", 0) == 0 || href.rfind("javascript:", 0) == 0) {
      continue;
    }
    std::string resolved = ResolveUrl(base_url, href);
    if (!resolved.empty()) page.links.push_back(resolved);
  }

  static const std::regex kHeadingRe(
      R"(<h[1-3][^>]*>([\s\S]*?)</h[1-3]>)", std::regex::icase);
  for (auto it = std::sregex_iterator(cleaned.begin(), cleaned.end(), kHeadingRe);
       it != std::sregex_iterator(); ++it) {
    std::string h = DecodeEntities(StripTags((*it)[1].str()));
    if (!h.empty()) page.headings.push_back(h);
  }

  std::string text = DecodeEntities(StripTags(cleaned));
  // Collapse whitespace.
  std::string collapsed;
  bool last_was_space = true;
  for (char c : text) {
    bool is_space = std::isspace(static_cast<unsigned char>(c));
    if (is_space) {
      if (!last_was_space) collapsed += ' ';
    } else {
      collapsed += c;
    }
    last_was_space = is_space;
  }
  if (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
  page.visible_text = collapsed;

  return page;
}

}  // namespace parser
}  // namespace dse
