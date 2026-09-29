#include "aiws/chunker.hpp"
#include "aiws/context_builder.hpp"
#include "aiws/corpus_index.hpp"
#include "aiws/processing_core.hpp"
#include "aiws/retrieval_engine.hpp"
#include "aiws/text_processor.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition,
           const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr
            << "FAIL: "
            << message
            << '\n';
    }
}

std::string make_words(
    int first,
    int last) {

    std::string text;

    for (int i = first;
         i < last;
         ++i) {

        if (!text.empty()) {
            text += ' ';
        }

        text +=
            "w" + std::to_string(i);
    }

    return text;
}

void test_text_processing() {
    using namespace aiws;

    const std::string text =
        "Hello,\r\n \t\r\nWORLD! R2-D2";

    const auto tokens =
        TextProcessor::tokenize(text);

    check(
        TextProcessor::normalize(text) ==
            "hello world r2 d2",
        "normalization keeps letters/digits "
        "and removes punctuation");

    check(
        tokens.size() == 4,
        "tokenizer finds four tokens");

    check(
        tokens.size() >= 2 &&
            tokens[0].paragraph == 0 &&
            tokens[1].paragraph == 1,
        "CRLF blank line creates a paragraph boundary");

    check(
        tokens.size() >= 2 &&
            text.substr(
                tokens[1].begin,
                tokens[1].end -
                    tokens[1].begin) ==
                "WORLD",
        "token source span points into original text");

    check(
        TextProcessor::normalize(
            "... !!!").empty(),
        "punctuation-only text normalizes to empty");
}

void test_chunking() {
    using namespace aiws;

    const std::string text =
        make_words(0, 105) +
        "\n\n" +
        make_words(105, 130);

    const Document document{
        "doc",
        "Paragraph Test",
        text
    };

    const std::string original =
        document.text();

    Chunker chunker;

    const auto chunks =
        chunker.chunk(
            document,
            3);

    check(
        chunks.size() == 2,
        "130 token document makes two chunks");

    check(
        chunks.size() >= 1 &&
            chunks[0].token_count == 105,
        "chunker prefers paragraph boundary "
        "inside 100-120 window");

    check(
        chunks.size() >= 2 &&
            chunks[1].token_count == 45,
        "second chunk includes 20 token overlap");

    check(
        chunks.size() >= 2 &&
            chunks[1].text.rfind(
                "w85 ",
                0) == 0,
        "overlap begins twenty tokens before "
        "first chunk end");

    check(
        chunks.size() >= 2 &&
            chunks[0].id == "doc#0" &&
            chunks[1].id == "doc#1",
        "chunk IDs and sequence numbers "
        "are deterministic");

    check(
        chunks.size() >= 1 &&
            chunks[0].document_order == 3,
        "chunk keeps source document insertion order");

    check(
        chunks.size() >= 1 &&
            text.substr(
                chunks[0].source_begin,
                chunks[0].source_end -
                    chunks[0].source_begin) ==
                make_words(0, 105),
        "chunk source span refers to original "
        "document text");

    check(
        document.text() == original,
        "chunking does not modify the document");

    const Document empty{
        "empty",
        "Empty",
        "...\t---"
    };

    check(
        chunker.chunk(
            empty,
            0).empty(),
        "effectively empty document produces no chunks");
}

void test_corpus_index() {
    using namespace aiws;

    Chunker chunker;

    auto chunks =
        chunker.chunk(
            Document{
                "a",
                "A",
                "alpha alpha beta"
            },
            0);

    auto second =
        chunker.chunk(
            Document{
                "b",
                "B",
                "alpha gamma"
            },
            1);

    chunks.insert(
        chunks.end(),
        second.begin(),
        second.end());

    CorpusIndex index(chunks);

    check(
        index.document_frequency(
            "alpha") == 2,
        "index document frequency counts "
        "chunks containing a term");

    check(
        index.term_frequency(
            "alpha",
            "a#0") == 2,
        "index stores term occurrence frequency");

    check(
        index.term_frequency(
            "beta",
            "missing") == 0,
        "missing chunk ID has zero term frequency");

    check(
        index.postings(
            "notthere") == nullptr,
        "unknown term has no posting list");

    auto replacement =
        chunker.chunk(
            Document{
                "c",
                "C",
                "delta delta"
            },
            0);

    index.build(replacement);

    check(
        index.document_frequency(
            "alpha") == 0,
        "index rebuild removes stale terms");

    check(
        index.document_frequency(
            "delta") == 1,
        "index rebuild stores replacement corpus");
}

void test_retrieval() {
    using namespace aiws;

    std::vector<Chunk> chunks = {
        {
            "b#0",
            "b",
            1,
            0,
            "alpha",
            1,
            0,
            5
        },
        {
            "a#1",
            "a",
            0,
            1,
            "alpha",
            1,
            0,
            5
        },
        {
            "a#0",
            "a",
            0,
            0,
            "alpha",
            1,
            0,
            5
        }
    };

    CorpusIndex index(chunks);
    RetrievalEngine engine;

    const auto results =
        engine.search(
            "ALPHA alpha",
            10,
            chunks,
            index);

    check(
        results.size() == 3,
        "retrieval returns candidate union");

    check(
        results.size() >= 3 &&
            results[0].chunk_id == "a#0" &&
            results[1].chunk_id == "a#1" &&
            results[2].chunk_id == "b#0",
        "score ties use document order and then "
        "chunk sequence");

    check(
        results.size() >= 1 &&
            std::fabs(
                results[0].score -
                1.1) < 1e-12,
        "TF-IDF and coverage score is "
        "calculated correctly");

    check(
        results.size() >= 1 &&
            results[0].matched_terms == 1,
        "repeated query terms count once");

    check(
        engine.search(
            "unknown",
            10,
            chunks,
            index).empty(),
        "unknown query term returns no candidates");

    check(
        engine.search(
            "alpha",
            0,
            chunks,
            index).empty(),
        "k equal to zero returns no results");

    bool threw = false;

    try {
        (void)engine.search(
            "alpha",
            -1,
            chunks,
            index);
    } catch (
        const std::invalid_argument&) {

        threw = true;
    }

    check(
        threw,
        "negative k throws invalid_argument");
}

void test_context_builder() {
    using namespace aiws;

    std::vector<SearchResult> ranked = {
        {
            "a#0",
            "a",
            0,
            "one two three",
            4.0,
            1
        },
        {
            "a#0",
            "a",
            0,
            "one two three",
            4.0,
            1
        },
        {
            "b#0",
            "b",
            0,
            "four five six",
            3.0,
            1
        }
    };

    ContextBuilder builder;

    const auto context =
        builder.build(
            ranked,
            5);

    check(
        context.size() == 2,
        "context skips duplicate chunk and "
        "keeps ranked order");

    check(
        context.size() >= 1 &&
            !context[0].truncated &&
            context[0].token_count == 3,
        "whole chunk is included when it fits");

    check(
        context.size() >= 2 &&
            context[1].truncated &&
            context[1].text ==
                "four five" &&
            context[1].token_count == 2,
        "last context item is truncated "
        "to remaining budget");

    check(
        builder.build(
            ranked,
            0).empty(),
        "zero token budget returns empty context");
}

void test_processing_core_integration() {
    using namespace aiws;

    ProcessingCore core;
    Workspace workspace;

    workspace.add_document(
        Document{
            "d1",
            "Colors",
            "red green blue"
        });

    workspace.add_document(
        Document{
            "d2",
            "More Green",
            "green green"
        });

    workspace.add_document(
        Document{
            "d3",
            "Empty",
            "... ---"
        });

    core.rebuild(workspace);

    check(
        core.chunk_count() == 2,
        "empty source document adds no chunk");

    check(
        core.document_frequency(
            "GREEN!") == 2,
        "public frequency call uses "
        "normal text processing");

    check(
        core.term_frequency(
            "green",
            "d2#0") == 2,
        "public term frequency reaches "
        "the built index");

    const auto results =
        core.search(
            "red green",
            10);

    check(
        results.size() == 2 &&
            results[0].document_id ==
                "d1",
        "end-to-end ranking rewards "
        "query coverage");

    const auto context =
        core.build_context(
            "red green",
            10,
            4);

    check(
        context.size() == 2 &&
            context[0].token_count == 3 &&
            context[1].token_count == 1 &&
            context[1].truncated,
        "end-to-end context respects "
        "total token budget");

    bool multi_term_threw = false;

    try {
        (void)core.term_frequency(
            "red green",
            "d1#0");
    } catch (
        const std::invalid_argument&) {

        multi_term_threw = true;
    }

    check(
        multi_term_threw,
        "multi-token term argument is invalid");

    check(
        core.term_frequency(
            "!!!",
            "d1#0") == 0,
        "effectively empty term argument "
        "returns zero");

    Workspace duplicate_ids;

    duplicate_ids.add_document(
        Document{
            "same",
            "One",
            "apple"
        });

    duplicate_ids.add_document(
        Document{
            "same",
            "Two",
            "banana"
        });

    bool duplicate_threw = false;

    try {
        core.rebuild(
            duplicate_ids);
    } catch (
        const std::invalid_argument&) {

        duplicate_threw = true;
    }

    check(
        duplicate_threw,
        "duplicate document IDs reject rebuild");

    check(
        core.chunk_count() == 2 &&
            core.document_frequency(
                "green") == 2,
        "failed rebuild leaves previous "
        "valid corpus unchanged");
}

}  // namespace

int main() {
    test_text_processing();
    test_chunking();
    test_corpus_index();
    test_retrieval();
    test_context_builder();
    test_processing_core_integration();

    if (failures == 0) {
        std::cout
            << "All student tests passed.\n";

        return 0;
    }

    std::cerr
        << failures
        << " student test(s) failed.\n";

    return 1;
}