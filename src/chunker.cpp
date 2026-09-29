#include "aiws/chunker.hpp"

#include "aiws/text_processor.hpp"

#include <algorithm>
#include <stdexcept>

namespace aiws {

Chunker::Chunker(ChunkingPolicy policy) : policy_(policy) {
    if (policy_.max_tokens == 0 ||
        policy_.overlap >= policy_.max_tokens ||
        policy_.paragraph_window > policy_.max_tokens) {
        throw std::invalid_argument("invalid chunking policy");
    }
}

std::vector<Chunk> Chunker::chunk(const Document& document,
                                  std::size_t document_order) const {
    const auto tokens = TextProcessor::tokenize(document.text());
    std::vector<Chunk> chunks;

    if (tokens.empty()) {
        return chunks;
    }

    std::size_t start = 0;
    std::size_t sequence = 0;

    while (start < tokens.size()) {
        const std::size_t remaining = tokens.size() - start;
        std::size_t end = tokens.size();

        if (remaining > policy_.max_tokens) {
            const std::size_t hard_end = start + policy_.max_tokens;
            end = hard_end;

            const std::size_t min_local_boundary =
                policy_.max_tokens - policy_.paragraph_window;

            // Search backward so the latest qualifying paragraph boundary wins.
            for (std::size_t local_boundary = policy_.max_tokens;
                 local_boundary >= min_local_boundary;
                 --local_boundary) {

                if (local_boundary > policy_.overlap &&
                    local_boundary > 0) {

                    const std::size_t boundary =
                        start + local_boundary;

                    if (boundary < tokens.size() &&
                        tokens[boundary - 1].paragraph !=
                            tokens[boundary].paragraph) {
                        end = boundary;
                        break;
                    }
                }

                if (local_boundary == 0) {
                    break;
                }
            }
        }

        const std::size_t token_count = end - start;

        Chunk chunk;
        chunk.id = document.id() + "#" + std::to_string(sequence);
        chunk.document_id = document.id();
        chunk.document_order = document_order;
        chunk.sequence = sequence;
        chunk.text = TextProcessor::join(tokens, start, end);
        chunk.token_count = token_count;
        chunk.source_begin = tokens[start].begin;
        chunk.source_end = tokens[end - 1].end;

        chunks.push_back(std::move(chunk));

        if (end == tokens.size()) {
            break;
        }

        start = end - policy_.overlap;
        ++sequence;
    }

    return chunks;
}

}  // namespace aiws