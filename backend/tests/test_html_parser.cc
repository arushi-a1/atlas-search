// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "test_framework.h"

#include "parser/html_parser.h"

using dse::parser::HtmlParser;

TEST(ParsesTitleAndVisibleText) {
  std::string html =
      "<html><head><title>Hello World</title></head>"
      "<body><p>Some <b>bold</b> text.</p></body></html>";
  auto page = HtmlParser::Parse(html, "https://example.com/");
  ASSERT_EQ(page.title, "Hello World");
  ASSERT_TRUE(page.visible_text.find("Some bold text.") != std::string::npos);
}

TEST(ExtractsAndResolvesRelativeLinks) {
  std::string html =
      "<a href=\"/about\">About</a>"
      "<a href=\"contact.html\">Contact</a>"
      "<a href=\"https://other.com/x\">External</a>";
  auto page = HtmlParser::Parse(html, "https://example.com/blog/post");
  ASSERT_EQ(page.links.size(), 3u);
  ASSERT_EQ(page.links[0], "https://example.com/about");
  ASSERT_EQ(page.links[2], "https://other.com/x");
}

TEST(SkipsScriptAndStyleContent) {
  std::string html =
      "<html><body><script>var x = 'not indexed';</script>"
      "<style>.a{color:red}</style><p>Real content</p></body></html>";
  auto page = HtmlParser::Parse(html, "https://example.com/");
  ASSERT_TRUE(page.visible_text.find("not indexed") == std::string::npos);
  ASSERT_TRUE(page.visible_text.find("Real content") != std::string::npos);
}

TEST(DecodesHtmlEntities) {
  std::string html = "<title>Fish &amp; Chips</title>";
  auto page = HtmlParser::Parse(html, "https://example.com/");
  ASSERT_EQ(page.title, "Fish & Chips");
}

TEST(BlockTagsSeparateWords) {
  // Regression: adjacent block elements used to be glued together
  // ("rankingBM25"), creating bogus index terms.
  std::string html =
      "<html><head><title>Okapi BM25 ranking</title></head>"
      "<body><h1>Search</h1><p>A distributed engine.</p><div>Next</div>"
      "<ul><li>one</li><li>two</li></ul>line<br>break</body></html>";
  auto page = HtmlParser::Parse(html, "https://example.com/");
  ASSERT_EQ(page.visible_text,
            "Okapi BM25 ranking Search A distributed engine. Next one two line break");
}

TEST(InlineTagsDoNotSplitWords) {
  std::string html = "<p><b>W</b>ord and <span>more</span>text</p>";
  auto page = HtmlParser::Parse(html, "https://example.com/");
  ASSERT_EQ(page.visible_text, "Word and moretext");
}
