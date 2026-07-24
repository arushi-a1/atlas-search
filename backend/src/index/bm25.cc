// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "index/bm25.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "index/tokenizer.h"

namespace dse {
namespace index {

std::vector<QueryClause> QueryParser::Parse(const std::string& query) {
  std::vector<QueryClause> clauses;
  size_t i = 0;
  while (i < query.size()) {
    if (query[i] == '"') {
      size_t end = query.find('"', i + 1);
      std::string phrase = end == std::string::npos
                                ? query.substr(i + 1)
                                : query.substr(i + 1, end - i - 1);
      std::vector<std::string> terms = Tokenizer::Tokenize(phrase);
      if (!terms.empty()) {
        QueryClause clause;
        clause.terms = terms;
        clause.is_phrase = terms.size() > 1;
        clauses.push_back(std::move(clause));
      }
      i = (end == std::string::npos) ? query.size() : end + 1;
    } else if (std::isspace(static_cast<unsigned char>(query[i]))) {
      ++i;
    } else {
      size_t start = i;
      while (i < query.size() && query[i] != '"' &&
             !std::isspace(static_cast<unsigned char>(query[i]))) {
        ++i;
      }
      std::vector<std::string> terms = Tokenizer::Tokenize(query.substr(start, i - start));
      for (auto& t : terms) {
        clauses.push_back(QueryClause{{t}, false});
      }
    }
  }
  return clauses;
}

Bm25Ranker::Bm25Ranker(const InvertedIndex& index, Bm25Params params)
    : index_(index), params_(params) {}

double Bm25Ranker::ScoreTerm(const std::string& term,
                              const Posting& posting) const {
  size_t n = index_.DocumentCount();
  uint32_t df = index_.DocFrequency(term);
  if (df == 0 || n == 0) return 0.0;

  // Standard BM25 IDF with the +1 smoothing to keep it non-negative for
  // common terms.
  double idf = std::log(1.0 + (static_cast<double>(n) - df + 0.5) / (df + 0.5));

  const DocumentMeta* meta = index_.GetDocumentMeta(posting.doc_id);
  double avg_len = std::max(1.0, index_.AverageBodyLength());

  double body_score = 0.0;
  if (posting.body_freq > 0 && meta != nullptr) {
    double len_norm = params_.k1 * (1 - params_.b +
                                     params_.b * meta->body_length / avg_len);
    body_score = idf * (posting.body_freq * (params_.k1 + 1)) /
                 (posting.body_freq + len_norm);
  }

  double title_score = 0.0;
  if (posting.title_freq > 0 && meta != nullptr) {
    // Title field uses a much shorter average length; approximate with a
    // fixed small normalizer since titles rarely vary length enough to
    // need full BM25F field statistics.
    double len_norm = params_.k1 * (1 - params_.b + params_.b *
                                     std::max<uint32_t>(1, meta->title_length) / 8.0);
    title_score = idf * (posting.title_freq * (params_.k1 + 1)) /
                  (posting.title_freq + len_norm);
  }

  return body_score + params_.title_weight * title_score;
}

bool Bm25Ranker::PhraseMatches(const std::vector<QueryClause>& clauses,
                                DocId doc_id) const {
  for (const auto& clause : clauses) {
    if (!clause.is_phrase) continue;

    // Gather sorted body positions per term in the phrase for this doc.
    std::vector<const std::vector<uint32_t>*> position_lists;
    for (const auto& term : clause.terms) {
      const std::vector<Posting>* postings = index_.GetPostings(term);
      if (postings == nullptr) return false;
      auto it = std::find_if(postings->begin(), postings->end(),
                              [doc_id](const Posting& p) { return p.doc_id == doc_id; });
      if (it == postings->end() || it->body_positions.empty()) return false;
      position_lists.push_back(&it->body_positions);
    }

    // A phrase matches if there exists a starting position p such that
    // term[k] occurs at position p + k for every k.
    bool found = false;
    for (uint32_t start : *position_lists[0]) {
      bool all_match = true;
      for (size_t k = 1; k < position_lists.size(); ++k) {
        uint32_t want = start + static_cast<uint32_t>(k);
        const auto& positions = *position_lists[k];
        if (!std::binary_search(positions.begin(), positions.end(), want)) {
          all_match = false;
          break;
        }
      }
      if (all_match) { found = true; break; }
    }
    if (!found) return false;
  }
  return true;
}

std::vector<SearchResult> Bm25Ranker::Search(const std::string& query,
                                              size_t top_k) const {
  std::vector<QueryClause> clauses = QueryParser::Parse(query);

  std::unordered_map<DocId, double> scores;
  std::unordered_set<std::string> seen_terms;

  for (const auto& clause : clauses) {
    for (const auto& term : clause.terms) {
      if (Tokenizer::IsStopWord(term) && clauses.size() > 1) continue;
      if (!seen_terms.insert(term).second) continue;  // score each term once
      const std::vector<Posting>* postings = index_.GetPostings(term);
      if (postings == nullptr) continue;
      for (const auto& posting : *postings) {
        scores[posting.doc_id] += ScoreTerm(term, posting);
      }
    }
  }

  // Enforce phrase clauses: any doc that doesn't contain the exact phrase
  // is dropped, even if its individual term scores were high.
  bool has_phrase = std::any_of(clauses.begin(), clauses.end(),
                                 [](const QueryClause& c) { return c.is_phrase; });
  std::vector<std::pair<DocId, double>> ranked;
  for (auto& [doc_id, score] : scores) {
    if (has_phrase && !PhraseMatches(clauses, doc_id)) continue;
    ranked.emplace_back(doc_id, score);
  }

  std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  if (ranked.size() > top_k) ranked.resize(top_k);

  std::vector<SearchResult> results;
  results.reserve(ranked.size());
  for (const auto& [doc_id, score] : ranked) {
    const DocumentMeta* meta = index_.GetDocumentMeta(doc_id);
    if (meta == nullptr) continue;
    results.push_back({doc_id, score, meta->url, meta->title, meta->snippet});
  }
  return results;
}

}  // namespace index
}  // namespace dse
