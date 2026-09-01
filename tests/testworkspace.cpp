#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Document.hpp"
#include "Message.hpp"
#include "Prompt.hpp"
#include "Workspace.hpp"

void testPrompt() {
    Prompt emptyPrompt;
    assert(emptyPrompt.title().empty());
    assert(emptyPrompt.empty());

    Prompt p1("Reviewer", "Review this requirement.");
    Prompt p2("Reviewer", "Review this requirement.");

    assert(p1 == p2);
    assert(!(p1 != p2));

    p2.setText("Summarize this requirement.");
    assert(p1 != p2);
}

void testMessage() {
    Message defaultMessage;
    assert(defaultMessage.role() == MessageRole::User);
    assert(defaultMessage.empty());

    Message m1(MessageRole::System, "Follow the project rules.");
    Message m2(MessageRole::System, "Follow the project rules.");

    assert(m1 == m2);

    m2.setRole(MessageRole::Assistant);
    assert(m1 != m2);

    m2.setText("");
    assert(m2.empty());
}

void testDocument() {
    Document emptyDocument;
    assert(emptyDocument.title().empty());
    assert(emptyDocument.sourcePath().empty());
    assert(emptyDocument.empty());

    Document madeDocument("Notes", "abc");
    assert(madeDocument.title() == "Notes");
    assert(madeDocument.sourcePath().empty());
    assert(madeDocument.characterCount() == 3);

    const std::string fileName = "document_test_file.txt";
    const std::string fileText = "First line.\nSecond line.\n";

    {
        std::ofstream outputFile(fileName, std::ios::binary);
        outputFile << fileText;
    }

    Document loadedDocument;
    assert(loadedDocument.load(fileName));
    assert(loadedDocument.title() == fileName);
    assert(loadedDocument.sourcePath() == fileName);
    assert(loadedDocument.contents() == fileText);
    assert(loadedDocument.characterCount() == fileText.size());

    // A failed load should leave the old document unchanged.
    Document beforeFailedLoad = loadedDocument;
    assert(!loadedDocument.load("file_that_should_not_exist_12345.txt"));
    assert(loadedDocument == beforeFailedLoad);

    std::remove(fileName.c_str());
}

void testWorkspace() {
    Workspace w("School Project");

    Document d1("First", "Document one");
    Document d2("Second", "Document two");
    Prompt p1("Prompt One", "Do the first task.");
    Message m1(MessageRole::User, "Hello");

    w.addDocument(d1);
    w.addDocument(d2);
    w.addPrompt(p1);
    w.addMessage(m1);

    assert(w.name() == "School Project");
    assert(w.documentCount() == 2);
    assert(w.promptCount() == 1);
    assert(w.messageCount() == 1);

    // Check that insertion order is preserved.
    assert(w.documentAt(0).title() == "First");
    assert(w.documentAt(1).title() == "Second");

    // Mutable access should let us change an object in the workspace.
    w.documentAt(0).setTitle("Updated First");
    assert(w.documentAt(0).title() == "Updated First");

    // Also make sure the const overloads work.
    const Workspace& constWorkspace = w;
    assert(constWorkspace.documentAt(1).title() == "Second");
    assert(constWorkspace.promptAt(0).title() == "Prompt One");
    assert(constWorkspace.messageAt(0).text() == "Hello");
}

void testInvalidIndex() {
    Workspace w;
    bool threwException = false;

    try {
        w.documentAt(0);
    } catch (const std::out_of_range&) {
        threwException = true;
    }

    assert(threwException);
}

void testCopyIndependence() {
    Workspace original("Original");
    original.addDocument(Document("Doc", "Original document text"));
    original.addPrompt(Prompt("Prompt", "Original prompt text"));
    original.addMessage(Message(MessageRole::User, "Original message"));

    Workspace copy = original;
    assert(copy == original);

    original.setName("Changed");
    original.documentAt(0).setTitle("Changed Doc");
    original.promptAt(0).setText("Changed prompt text");
    original.messageAt(0).setText("Changed message");

    assert(copy.name() == "Original");
    assert(copy.documentAt(0).title() == "Doc");
    assert(copy.promptAt(0).text() == "Original prompt text");
    assert(copy.messageAt(0).text() == "Original message");
    assert(copy != original);
}

int main() {
    testPrompt();
    testMessage();
    testDocument();
    testWorkspace();
    testInvalidIndex();
    testCopyIndependence();

    std::cout << "All M0 tests passed successfully.\n";
    return 0;
}
