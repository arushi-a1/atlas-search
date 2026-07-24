// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#ifndef DISTRIBUTED_SEARCH_INDEX_TOKENIZER_H_
#define DISTRIBUTED_SEARCH_INDEX_TOKENIZER_H_

#include <string>
#include <vector>

namespace dse {
namespace index {

class Tokenizer {
 public:
  // Lowercases, strips punctuation, and splits on whitespace. Position
  // (0-based, in tokens) is preserved by the caller's index into the
  // returned vector -- callers needing phrase search rely on this
  // property, so tokenization must not drop or merge tokens.
  static std::vector<std::string> Tokenize(const std::string& text);

  static bool IsStopWord(const std::string& token);
};

}  // namespace index
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_INDEX_TOKENIZER_H_
