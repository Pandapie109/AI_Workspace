#include "Document.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

Document::Document(std::string title, std::string contents)
    : title_(title), contents_(contents) {
}

bool Document::operator==(const Document& other) const {
    return title_ == other.title_ &&
           sourcePath_ == other.sourcePath_ &&
           contents_ == other.contents_;
}

bool Document::operator!=(const Document& other) const {
    return !(*this == other);
}

bool Document::load(const std::string& path) {
    std::ifstream inputFile(path, std::ios::binary);

    if (!inputFile) {
        return false;
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();

    if (inputFile.bad()) {
        return false;
    }

    std::string newContents = buffer.str();
    std::string newTitle = std::filesystem::path(path).filename().string();

    // Change the document only after the file was read successfully.
    title_ = newTitle;
    sourcePath_ = path;
    contents_ = newContents;

    return true;
}

const std::string& Document::title() const noexcept {
    return title_;
}

const std::string& Document::sourcePath() const noexcept {
    return sourcePath_;
}

const std::string& Document::contents() const noexcept {
    return contents_;
}

void Document::setTitle(std::string title) {
    title_ = title;
}

std::size_t Document::characterCount() const noexcept {
    return contents_.size();
}

bool Document::empty() const noexcept {
    return contents_.empty();
}
