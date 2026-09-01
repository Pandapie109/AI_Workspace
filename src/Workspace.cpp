#include "Workspace.hpp"

Workspace::Workspace(std::string name)
    : name_(name) {
}

bool Workspace::operator==(const Workspace& other) const {
    return name_ == other.name_ &&
           documents_ == other.documents_ &&
           prompts_ == other.prompts_ &&
           messages_ == other.messages_;
}

bool Workspace::operator!=(const Workspace& other) const {
    return !(*this == other);
}

const std::string& Workspace::name() const noexcept {
    return name_;
}

void Workspace::setName(std::string name) {
    name_ = name;
}

void Workspace::addDocument(Document document) {
    documents_.push_back(document);
}

void Workspace::addPrompt(Prompt prompt) {
    prompts_.push_back(prompt);
}

void Workspace::addMessage(Message message) {
    messages_.push_back(message);
}

std::size_t Workspace::documentCount() const noexcept {
    return documents_.size();
}

std::size_t Workspace::promptCount() const noexcept {
    return prompts_.size();
}

std::size_t Workspace::messageCount() const noexcept {
    return messages_.size();
}

Document& Workspace::documentAt(std::size_t index) {
    return documents_.at(index);
}

const Document& Workspace::documentAt(std::size_t index) const {
    return documents_.at(index);
}

Prompt& Workspace::promptAt(std::size_t index) {
    return prompts_.at(index);
}

const Prompt& Workspace::promptAt(std::size_t index) const {
    return prompts_.at(index);
}

Message& Workspace::messageAt(std::size_t index) {
    return messages_.at(index);
}

const Message& Workspace::messageAt(std::size_t index) const {
    return messages_.at(index);
}
