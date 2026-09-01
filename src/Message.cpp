#include "Message.hpp"

Message::Message(MessageRole role, std::string text)
    : role_(role), text_(text) {
}

bool Message::operator==(const Message& other) const {
    return role_ == other.role_ && text_ == other.text_;
}

bool Message::operator!=(const Message& other) const {
    return !(*this == other);
}

MessageRole Message::role() const noexcept {
    return role_;
}

const std::string& Message::text() const noexcept {
    return text_;
}

void Message::setRole(MessageRole role) noexcept {
    role_ = role;
}

void Message::setText(std::string text) {
    text_ = text;
}

bool Message::empty() const noexcept {
    return text_.empty();
}
