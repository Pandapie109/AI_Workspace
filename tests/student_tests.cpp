#include "aiws/chunking_strategy.hpp"
#include "aiws/context_strategy.hpp"
#include "aiws/processing_core.hpp"
#include "aiws/retrieval_strategy.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAIL: " << name << '\n';
        ++failures;
    }
}

struct CallCounts {
    int chunk_calls = 0;
    int search_calls = 0;
    int context_calls = 0;
};

class MarkerChunker final
    : public aiws::ChunkingStrategy {

public:
    explicit MarkerChunker(
        std::shared_ptr<CallCounts> counts)
        : counts_(std::move(counts)) {}

    std::vector<aiws::Chunk> chunk(
        const aiws::Document& document,
        std::size_t order) const override {

        ++counts_->chunk_calls;

        return {{
            document.id() + "#marker",
            document.id(),
            order,
            0,
            "marker text",
            2,
            0,
            document.text().size()
        }};
    }

private:
    std::shared_ptr<CallCounts> counts_;
};

class MarkerRetrieval final
    : public aiws::RetrievalStrategy {

public:
    explicit MarkerRetrieval(
        std::shared_ptr<CallCounts> counts)
        : counts_(std::move(counts)) {}

    std::vector<aiws::SearchResult> search(
        const std::string&,
        int k,
        const std::vector<aiws::Chunk>& chunks,
        const aiws::CorpusIndex&) const override {

        ++counts_->search_calls;

        if (k <= 0 || chunks.empty()) {
            return {};
        }

        const auto& chunk = chunks.back();

        return {{
            chunk.id,
            chunk.document_id,
            chunk.sequence,
            "retrieval marker",
            123.0,
            1
        }};
    }

private:
    std::shared_ptr<CallCounts> counts_;
};

class MarkerContext final
    : public aiws::ContextStrategy {

public:
    explicit MarkerContext(
        std::shared_ptr<CallCounts> counts)
        : counts_(std::move(counts)) {}

    std::vector<aiws::ContextItem> build(
        const std::vector<aiws::SearchResult>& ranked,
        std::size_t token_budget) const override {

        ++counts_->context_calls;

        if (ranked.empty() || token_budget == 0) {
            return {};
        }

        const auto& result = ranked.front();

        return {{
            result.chunk_id,
            result.document_id,
            result.chunk_sequence,
            "context marker",
            2,
            result.score,
            false
        }};
    }

private:
    std::shared_ptr<CallCounts> counts_;
};

std::string make_words(int count) {
    std::string text;

    for (int i = 0; i < count; ++i) {
        if (!text.empty()) {
            text += ' ';
        }

        text += "word" + std::to_string(i);
    }

    return text;
}

void test_default_m1_chunking() {
    aiws::Workspace workspace;

    workspace.add_document(
        aiws::Document{
            "long",
            "",
            make_words(130)
        });

    aiws::ProcessingCore core;
    core.rebuild(workspace);

    check(
        core.chunk_count() == 2,
        "default configuration still uses M1 chunking");

    check(
        core.chunks()[0].token_count == 120,
        "default first chunk keeps 120-token M1 maximum");

    check(
        core.chunks()[1].token_count == 30,
        "default second chunk keeps 20-token M1 overlap");
}

void test_runtime_substitution_across_components() {
    auto counts =
        std::make_shared<CallCounts>();

    aiws::ProcessingCore core(
        std::make_unique<MarkerChunker>(counts),
        std::make_unique<MarkerRetrieval>(counts),
        std::make_unique<MarkerContext>(counts));

    aiws::Workspace workspace;

    workspace.add_document(
        aiws::Document{
            "a",
            "",
            "alpha beta"
        });

    workspace.add_document(
        aiws::Document{
            "b",
            "",
            "gamma delta"
        });

    core.rebuild(workspace);

    check(
        counts->chunk_calls == 2,
        "injected chunking strategy called once per document");

    check(
        core.chunks()[0].id == "a#marker",
        "custom chunk output stored by ProcessingCore");

    const auto ranked =
        core.search("ignored", 1);

    check(
        counts->search_calls == 1,
        "injected retrieval strategy used by search");

    check(
        ranked.size() == 1 &&
            ranked[0].score == 123.0,
        "custom retrieval result reaches caller");

    const auto context =
        core.build_context(
            "ignored",
            1,
            10);

    check(
        counts->search_calls == 2,
        "build_context obtains ranked results through configured retrieval");

    check(
        counts->context_calls == 1,
        "injected context strategy used by build_context");

    check(
        context.size() == 1 &&
            context[0].text == "context marker",
        "custom context result reaches caller");
}

void test_null_configuration_rejected() {
    auto counts =
        std::make_shared<CallCounts>();

    bool retrieval_threw = false;
    bool context_threw = false;

    try {
        aiws::ProcessingCore bad(
            std::make_unique<MarkerChunker>(counts),
            nullptr,
            std::make_unique<MarkerContext>(counts));
    }
    catch (const std::invalid_argument&) {
        retrieval_threw = true;
    }

    try {
        aiws::ProcessingCore bad(
            std::make_unique<MarkerChunker>(counts),
            std::make_unique<MarkerRetrieval>(counts),
            nullptr);
    }
    catch (const std::invalid_argument&) {
        context_threw = true;
    }

    check(
        retrieval_threw,
        "null retrieval strategy is rejected");

    check(
        context_threw,
        "null context strategy is rejected");
}

void test_failed_rebuild_preserves_old_corpus() {
    aiws::ProcessingCore core;

    aiws::Workspace valid;

    valid.add_document(
        aiws::Document{
            "good",
            "",
            "alpha beta gamma"
        });

    core.rebuild(valid);

    const std::size_t old_count =
        core.chunk_count();

    const std::string old_id =
        core.chunks().front().id;

    aiws::Workspace invalid;

    invalid.add_document(
        aiws::Document{
            "dup",
            "",
            "first"
        });

    invalid.add_document(
        aiws::Document{
            "dup",
            "",
            "second"
        });

    bool threw = false;

    try {
        core.rebuild(invalid);
    }
    catch (const std::invalid_argument&) {
        threw = true;
    }

    check(
        threw,
        "duplicate document ids throw invalid_argument");

    check(
        core.chunk_count() == old_count &&
            core.chunks().front().id == old_id,
        "failed rebuild leaves previous corpus unchanged");
}

void test_move_only_core_keeps_configuration() {
    static_assert(
        !std::is_copy_constructible_v<
            aiws::ProcessingCore>);

    static_assert(
        !std::is_copy_assignable_v<
            aiws::ProcessingCore>);

    static_assert(
        std::is_move_constructible_v<
            aiws::ProcessingCore>);

    static_assert(
        std::is_move_assignable_v<
            aiws::ProcessingCore>);

    auto counts =
        std::make_shared<CallCounts>();

    aiws::ProcessingCore original(
        std::make_unique<MarkerChunker>(counts),
        std::make_unique<MarkerRetrieval>(counts),
        std::make_unique<MarkerContext>(counts));

    aiws::Workspace workspace;

    workspace.add_document(
        aiws::Document{
            "move",
            "",
            "some text"
        });

    original.rebuild(workspace);

    aiws::ProcessingCore moved(
        std::move(original));

    const auto ranked =
        moved.search(
            "anything",
            1);

    check(
        ranked.size() == 1 &&
            ranked[0].score == 123.0,
        "moved ProcessingCore still owns and uses injected strategies");
}

}  // namespace

int main() {
    test_default_m1_chunking();
    test_runtime_substitution_across_components();
    test_null_configuration_rejected();
    test_failed_rebuild_preserves_old_corpus();
    test_move_only_core_keeps_configuration();

    if (failures != 0) {
        return 1;
    }

    std::cout << "Student M2 tests passed\n";

    return 0;
}