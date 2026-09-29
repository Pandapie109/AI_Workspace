#include "aiws/retrieval_engine.hpp"

#include "aiws/text_processor.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace aiws {

double RetrievalEngine::canonical_score(double value) {
    constexpr double scale = 1000000000000.0;
    return std::round(value * scale) / scale;
}

std::vector<SearchResult> RetrievalEngine::search(
    const std::string& query,
    int k,
    const std::vector<Chunk>& chunks,
    const CorpusIndex& index) const {

    if (k < 0) {
        throw std::invalid_argument("k must not be negative");
    }

    if (k == 0 || chunks.empty()) {
        return {};
    }

    const auto normalized_terms =
        TextProcessor::terms(query);

    std::vector<std::string> query_terms;
    std::unordered_set<std::string> seen_terms;

    for (const auto& term : normalized_terms) {
        if (seen_terms.insert(term).second) {
            query_terms.push_back(term);
        }
    }

    if (query_terms.empty()) {
        return {};
    }

    std::vector<double> base_scores(
        chunks.size(), 0.0);

    std::vector<std::size_t> matched_terms(
        chunks.size(), 0);

    std::vector<bool> is_candidate(
        chunks.size(), false);

    const double total_chunks =
        static_cast<double>(chunks.size());

    for (const auto& term : query_terms) {
        const auto* term_postings =
            index.postings(term);

        if (term_postings == nullptr) {
            continue;
        }

        const double document_frequency =
            static_cast<double>(
                index.document_frequency(term));

        const double idf =
            std::log(
                (total_chunks + 1.0) /
                (document_frequency + 1.0)) +
            1.0;

        for (const auto& posting : *term_postings) {
            if (posting.chunk_index >= chunks.size()) {
                continue;
            }

            const double tf =
                1.0 +
                std::log(
                    static_cast<double>(
                        posting.frequency));

            base_scores[posting.chunk_index] +=
                tf * idf;

            ++matched_terms[posting.chunk_index];

            is_candidate[posting.chunk_index] =
                true;
        }
    }

    std::vector<SearchResult> results;

    const double query_term_count =
        static_cast<double>(
            query_terms.size());

    for (std::size_t i = 0;
         i < chunks.size();
         ++i) {

        if (!is_candidate[i]) {
            continue;
        }

        const double coverage =
            1.0 +
            0.10 *
                static_cast<double>(
                    matched_terms[i]) /
                query_term_count;

        const double score =
            canonical_score(
                base_scores[i] * coverage);

        results.push_back(
            SearchResult{
                chunks[i].id,
                chunks[i].document_id,
                chunks[i].sequence,
                chunks[i].text,
                score,
                matched_terms[i]
            });
    }

    std::sort(
        results.begin(),
        results.end(),
        [&index, &chunks](
            const SearchResult& left,
            const SearchResult& right) {

            if (left.score != right.score) {
                return left.score >
                       right.score;
            }

            const Chunk* left_chunk =
                index.find_chunk(
                    chunks,
                    left.chunk_id);

            const Chunk* right_chunk =
                index.find_chunk(
                    chunks,
                    right.chunk_id);

            if (left_chunk != nullptr &&
                right_chunk != nullptr &&
                left_chunk->document_order !=
                    right_chunk->document_order) {

                return left_chunk->document_order <
                       right_chunk->document_order;
            }

            if (left.chunk_sequence !=
                right.chunk_sequence) {

                return left.chunk_sequence <
                       right.chunk_sequence;
            }

            return left.chunk_id <
                   right.chunk_id;
        });

    if (results.size() >
        static_cast<std::size_t>(k)) {

        results.resize(
            static_cast<std::size_t>(k));
    }

    return results;
}

}  // namespace aiws