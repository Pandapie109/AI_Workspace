#include "aiws/processing_core.hpp"

#include "aiws/chunker.hpp"
#include "aiws/context_builder.hpp"
#include "aiws/corpus_index.hpp"
#include "aiws/retrieval_engine.hpp"
#include "aiws/text_processor.hpp"

#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace aiws {

struct ProcessingCore::Impl {
    Chunker chunker{
        ChunkingPolicy{
            kMaxChunkTokens,
            kChunkOverlap,
            kParagraphPreferenceWindow
        }
    };

    std::vector<Chunk> chunks;
    CorpusIndex index;
    RetrievalEngine retrieval;
    ContextBuilder context_builder;
};

ProcessingCore::ProcessingCore()
    : impl_(std::make_unique<Impl>()) {
}

ProcessingCore::~ProcessingCore() =
    default;

ProcessingCore::ProcessingCore(
    ProcessingCore&&) noexcept =
    default;

ProcessingCore&
ProcessingCore::operator=(
    ProcessingCore&&) noexcept =
    default;

std::string ProcessingCore::normalize(
    const std::string& text) {

    return TextProcessor::normalize(text);
}

void ProcessingCore::rebuild(
    const Workspace& workspace) {

    std::unordered_set<std::string>
        document_ids;

    // Validate before changing the current
    // corpus so a failed rebuild leaves the
    // previous valid state untouched.
    for (const auto& document :
         workspace.documents()) {

        if (!document_ids
                 .insert(document.id())
                 .second) {

            throw std::invalid_argument(
                "duplicate document id");
        }
    }

    std::vector<Chunk> new_chunks;

    for (std::size_t document_order = 0;
         document_order <
             workspace.documents().size();
         ++document_order) {

        auto document_chunks =
            impl_->chunker.chunk(
                workspace.documents()
                    [document_order],
                document_order);

        new_chunks.insert(
            new_chunks.end(),
            std::make_move_iterator(
                document_chunks.begin()),
            std::make_move_iterator(
                document_chunks.end()));
    }

    CorpusIndex new_index(
        new_chunks);

    impl_->chunks =
        std::move(new_chunks);

    impl_->index =
        std::move(new_index);
}

const std::vector<Chunk>&
ProcessingCore::chunks() const noexcept {

    return impl_->chunks;
}

std::size_t
ProcessingCore::chunk_count() const noexcept {

    return impl_->chunks.size();
}

std::size_t
ProcessingCore::document_frequency(
    const std::string& term) const {

    const auto normalized_terms =
        TextProcessor::terms(term);

    if (normalized_terms.empty()) {
        return 0;
    }

    if (normalized_terms.size() > 1) {
        throw std::invalid_argument(
            "term must normalize to one token");
    }

    return impl_->index
        .document_frequency(
            normalized_terms.front());
}

std::size_t
ProcessingCore::term_frequency(
    const std::string& term,
    const std::string& chunk_id) const {

    const auto normalized_terms =
        TextProcessor::terms(term);

    if (normalized_terms.empty()) {
        return 0;
    }

    if (normalized_terms.size() > 1) {
        throw std::invalid_argument(
            "term must normalize to one token");
    }

    return impl_->index
        .term_frequency(
            normalized_terms.front(),
            chunk_id);
}

std::vector<SearchResult>
ProcessingCore::search(
    const std::string& query,
    int k) const {

    return impl_->retrieval.search(
        query,
        k,
        impl_->chunks,
        impl_->index);
}

std::vector<ContextItem>
ProcessingCore::build_context(
    const std::string& query,
    int k,
    std::size_t token_budget) const {

    const auto ranked =
        search(query, k);

    return impl_->context_builder.build(
        ranked,
        token_budget);
}

}  // namespace aiws