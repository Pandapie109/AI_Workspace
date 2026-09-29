# M2 Technical & Design Understanding

## 1. Polymorphism and dynamic dispatch

One place where runtime polymorphism happens in my code is in ProcessingCore::search(). The base interface is RetrievalStrategy, and the default class that implements it is RetrievalEngine. Inside ProcessingCore, the retrieval strategy is stored using a std::unique_ptr<RetrievalStrategy>.

When ProcessingCore::search() is called, it uses:
return impl_->retrieval->search(
    query,
    k,
    impl_->chunks,
    impl_->index);

Even though impl_->retrieval is a pointer to the base class, the actual object can be a RetrievalEngine or some other class derived from RetrievalStrategy. Since search() is virtual, C++ calls the version of search() that belongs to the actual object being stored. For the default setup, that means RetrievalEngine::search() gets called.

If search() was not virtual, then the program would not be able to choose the derived version at runtime. That would basically defeat the purpose of using the strategy interface because custom retrieval classes would not work the way they are supposed to.

## 2. Ownership and lifetime

One example of ownership in my code is the retrieval strategy. In the default ProcessingCore constructor, the object is created with:
std::make_unique<RetrievalEngine>()

That unique_ptr is passed into the other ProcessingCore constructor, and then moved into the Impl object.
impl_ = std::make_unique<Impl>(
    std::move(chunking),
    std::move(retrieval),
    std::move(context));

Inside Impl, the retrieval strategy is stored as:
std::unique_ptr<RetrievalStrategy> retrieval;


This means that ProcessingCore, through its Impl object, owns the strategy. I used unique_ptr because only one object should own each strategy at a time. The pointer gets moved instead of copied, so ownership is transferred safely.

When the ProcessingCore object is destroyed, its Impl object is also destroyed. That automatically destroys the strategy objects too, so I do not have to manually use delete.

ProcessingCore is move-only because it owns unique_ptr objects. Copying it would mean trying to copy exclusive ownership, which is not allowed. Moving works because it transfers ownership from one ProcessingCore object to another.

The strategy base classes also need virtual destructors because the derived objects are being stored through base-class pointers. For example, a RetrievalEngine can be stored inside a unique_ptr<RetrievalStrategy>. The virtual destructor makes sure the correct derived destructor is called when the object is destroyed.

## 3. Architecture, extensibility, and M1 compatibility

One main design choice I made was having ProcessingCore work with strategy interfaces instead of directly depending on only the original M1 classes. The three interfaces are ChunkingStrategy, RetrievalStrategy, and ContextStrategy.

The default classes are still:

- Chunker
- RetrievalEngine
- ContextBuilder

The default ProcessingCore constructor creates those classes:

ProcessingCore::ProcessingCore()
    : ProcessingCore(
          std::make_unique<Chunker>(
              ChunkingPolicy{
                  kMaxChunkTokens,
                  kChunkOverlap,
                  kParagraphPreferenceWindow}),
          std::make_unique<RetrievalEngine>(),
          std::make_unique<ContextBuilder>()) {}

Because of this, the normal behavior still matches M1. The same chunking rules, retrieval system, and context builder are still being used when someone creates a normal ProcessingCore.

At the same time, the second constructor lets a different strategy be passed in. For example, I could make a new class that inherits from RetrievalStrategy and pass it into ProcessingCore. The rest of the program can still just call core.search() like normal. It does not need to know which retrieval strategy is being used.

Another way this could have been designed would be to put a bunch of if statements or a switch inside ProcessingCore to choose between different algorithms. I think the strategy approach works better for M2 because a new implementation can be added without changing the main ProcessingCore code. It also keeps the different responsibilities more separated.

## 4. Testing and defect reasoning

One test I wrote in student_tests.cpp is test_runtime_substitution_across_components().

This test creates custom versions of all three strategies:

- MarkerChunker
- MarkerRetrieval
- MarkerContext

These custom classes return easy-to-recognize results instead of doing the normal M1 behavior. For example, MarkerRetrieval returns a search result with a score of: 123.0
The test then creates a ProcessingCore using these custom strategies and calls the normal rebuild(), search(), and build_context() functions.

This test checks that the custom strategies are actually being used. If ProcessingCore was still hard-coded to directly call Chunker, RetrievalEngine, or ContextBuilder, then the marker values would never show up and the test would fail.

The test also keeps track of how many times each custom strategy gets called. This gives more evidence that the calls are actually going through the strategy interfaces.

I think this test is useful beyond just rerunning the public tests because it checks the main new idea in M2, which is runtime substitution. The normal public tests can check that the default behavior still works, while this test checks that a completely different implementation can be plugged in and actually affect the results.