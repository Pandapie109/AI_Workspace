#include "Prompt.hpp"

Prompt::Prompt(std::string title, std::string text)
    : title_(title), text_(text) {
}

bool Prompt::operator==(const Prompt& other) const {
    return title_ == other.title_ && text_ == other.text_;
}

bool Prompt::operator!=(const Prompt& other) const {
    return !(*this == other);
}

const std::string& Prompt::title() const noexcept {
    return title_;
}

const std::string& Prompt::text() const noexcept {
    return text_;
}

void Prompt::setTitle(std::string title) {
    title_ = title;
}

void Prompt::setText(std::string text) {
    text_ = text;
}

bool Prompt::empty() const noexcept {
    return text_.empty();
}
