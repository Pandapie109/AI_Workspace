# M1 Design

## 1. System structure

I kept the processing work split into the components that were already set up in the starter. `TextProcessor` is responsible for turning normal document/query text into the same lowercase ASCII tokens. `Chunker` uses those tokens and paragraph information to split each `Document` into source-attributed chunks. `CorpusIndex` builds the searchable term information for the current chunks. `RetrievalEngine` uses that index to calculate the required TF-IDF-style score and sort the results. `ContextBuilder` takes the ranked results and fills a token budget. Finally, `ProcessingCore` owns the current corpus state and connects all of those parts together for the public API.

The main data flow is:

`Workspace -> Chunker/TextProcessor -> vector<Chunk> -> CorpusIndex`

and then for a query:

`query -> TextProcessor -> RetrievalEngine -> ranked SearchResult objects -> ContextBuilder`

This keeps the normalization rules in one place instead of having slightly different parsing code in indexing and searching.

## 2. Design decisions

Most of the state is stored by value. The current chunks are kept in a `std::vector<Chunk>` because chunk order matters and the vector also gives a simple stable index number while a corpus is active. `CorpusIndex` uses an `unordered_map` from each term to a vector of postings. Each posting stores the chunk vector index and the frequency of that term in the chunk. A second map connects a chunk ID to its vector index.

`ProcessingCore` uses the starter's `Impl` pointer to own the `Chunker`, chunk vector, index, retrieval engine, and context builder. I did not add raw owning pointers. The `Document` objects stay owned by the `Workspace`; processing creates derived `Chunk` values and does not change the original document text.

For source attribution, each token remembers its original `[begin, end)` character positions. A chunk's source span comes from its first and last included token, while the chunk text itself is the normalized tokens joined with spaces.

## 3. Correctness and consistency

The biggest consistency rule is that documents, frequency lookups, and queries all go through `TextProcessor`. This makes a value such as `"ALPHA!"` refer to the same normalized term as `"alpha"` everywhere in the program.

A rebuild is made with temporary state first. `ProcessingCore` checks all document IDs for duplicates before replacing anything. It then makes a new chunk vector and a new `CorpusIndex`. Only after those steps succeed are they moved into the active core. This means a failed rebuild does not leave half of the old corpus and half of a new one.

The index `build()` method also constructs temporary maps and swaps them in at the end, so rebuilding clears stale terms instead of accumulating old postings. Search results are sorted by score and then by document insertion order and chunk sequence, which makes ties deterministic.

For chunking, the normal hard limit is 120 tokens. When more text remains, the chunker searches backward for the latest paragraph boundary from positions 100 through 120. The next chunk starts 20 tokens before the selected end. Empty normalized documents produce no chunks.

For context construction, chunk IDs are tracked so the same chunk is not added twice. Whole chunks are added while they fit. If the next unique chunk is too large, only the largest prefix that fits is included and marked as truncated.

## 4. Testing strategy

I added `tests/student_tests.cpp` in addition to the supplied public tests. The student tests call the lower-level components directly instead of checking everything only through `ProcessingCore`.

The tests cover LF/CRLF-style paragraph handling, punctuation and case normalization, original source spans, the 120-token chunk rule, the paragraph preference window, the 20-token overlap, empty documents, index frequency information, clearing stale state on rebuild, deterministic tie ordering, repeated query terms, unknown terms, negative `k`, context truncation, duplicate context results, and a multi-document end-to-end case.

I also test the important failed-rebuild case by first building a valid corpus, then trying a workspace with duplicate document IDs and checking that the old corpus is still present afterward.

## 5. Alternatives considered

One alternative was to put most of the processing inside `ProcessingCore`. That would have been shorter at first, but it would mix tokenization, chunking, indexing, ranking, and context logic in one file. I kept the separate components because they match different responsibilities and can be tested individually.

Another alternative was to scan every chunk or full document again during every search. I did not use that approach because M1 specifically has a corpus index, and repeated scanning would make the index mostly pointless. The inverted posting lists let retrieval start from only the chunks that contain at least one query term.

I also considered storing only normalized chunk text and trying to calculate source positions later. I kept source positions during tokenization instead because normalization changes punctuation and spacing, so reconstructing exact positions afterward would be less reliable.