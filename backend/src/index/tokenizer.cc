// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "index/tokenizer.h"

#include <cctype>
#include <unordered_set>

namespace dse {
namespace index {

namespace {
// A small, standard stop-word list. Kept out of scoring for BM25's IDF
// term but *not* dropped from the token stream, so phrase search across
// stop words ("to be or not to be") still works.
const std::unordered_set<std::string>& StopWords() {
  static const std::unordered_set<std::string> kStopWords = {
      "a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
      "has", "he", "in", "is", "it", "its", "of", "on", "that", "the",
      "to", "was", "were", "will", "with"};
  return kStopWords;
}
}  // namespace

std::vector<std::string> Tokenizer::Tokenize(const std::string& text) {
  std::vector<std::string> tokens;
  std::string current;
  for (char c : text) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc)) {
      current += static_cast<char>(std::tolower(uc));
    } else {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    }
  }
  if (!current.empty()) tokens.push_back(current);
  return tokens;
}

bool Tokenizer::IsStopWord(const std::string& token) {
  return StopWords().count(token) > 0;
}

}  // namespace index
}  // namespace dse
