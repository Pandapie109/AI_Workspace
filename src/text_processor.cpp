#include "aiws/text_processor.hpp"

#include <algorithm>

namespace aiws {
namespace {

bool is_ascii_letter(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_ascii_digit(unsigned char c) {
    return c >= '0' && c <= '9';
}

bool is_token_character(unsigned char c) {
    return is_ascii_letter(c) || is_ascii_digit(c);
}

char normalized_character(unsigned char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return static_cast<char>(c);
}

// Checks whether the separator text between two tokens contains a blank line.
// CRLF is treated as one newline so LF and CRLF documents behave the same.
bool has_paragraph_boundary(const std::string& text,
                            std::size_t begin,
                            std::size_t end) {
    bool saw_newline = false;

    for (std::size_t i = begin; i < end; ++i) {
        const char c = text[i];

        if (c == '\r' || c == '\n') {
            if (c == '\r' && i + 1 < end && text[i + 1] == '\n') {
                ++i;
            }

            if (saw_newline) {
                return true;
            }
            saw_newline = true;
        } else if (c == ' ' || c == '\t') {
            // Spaces and tabs are allowed between the two newline characters.
        } else {
            // Any other byte breaks the blank-line pattern.
            saw_newline = false;
        }
    }

    return false;
}

template <typename TokenType, typename Getter>
std::string join_range(const std::vector<TokenType>& tokens,
                       std::size_t begin,
                       std::size_t end,
                       Getter getter) {
    if (begin >= end || begin >= tokens.size()) {
        return {};
    }

    end = std::min(end, tokens.size());
    std::string result;

    for (std::size_t i = begin; i < end; ++i) {
        if (!result.empty()) {
            result += ' ';
        }
        result += getter(tokens[i]);
    }

    return result;
}

}  // namespace

std::vector<TokenInfo> TextProcessor::tokenize(const std::string& text) {
    std::vector<TokenInfo> tokens;
    std::size_t i = 0;
    std::size_t paragraph = 0;
    std::size_t previous_end = 0;
    bool have_previous_token = false;

    while (i < text.size()) {
        while (i < text.size() &&
               !is_token_character(static_cast<unsigned char>(text[i]))) {
            ++i;
        }

        if (i == text.size()) {
            break;
        }

        const std::size_t begin = i;
        std::string token;

        while (i < text.size() &&
               is_token_character(static_cast<unsigned char>(text[i]))) {
            token += normalized_character(static_cast<unsigned char>(text[i]));
            ++i;
        }

        if (have_previous_token &&
            has_paragraph_boundary(text, previous_end, begin)) {
            ++paragraph;
        }

        tokens.push_back(TokenInfo{token, begin, i, paragraph});
        previous_end = i;
        have_previous_token = true;
    }

    return tokens;
}

std::vector<std::string> TextProcessor::terms(const std::string& text) {
    const auto token_info = tokenize(text);
    std::vector<std::string> result;
    result.reserve(token_info.size());

    for (const auto& token : token_info) {
        result.push_back(token.token);
    }

    return result;
}

std::string TextProcessor::normalize(const std::string& text) {
    const auto tokens = tokenize(text);
    return join(tokens, 0, tokens.size());
}

std::string TextProcessor::join(const std::vector<TokenInfo>& tokens,
                                std::size_t begin,
                                std::size_t end) {
    return join_range(tokens, begin, end,
                      [](const TokenInfo& token) -> const std::string& {
                          return token.token;
                      });
}

std::string TextProcessor::join(const std::vector<std::string>& tokens,
                                std::size_t begin,
                                std::size_t end) {
    return join_range(tokens, begin, end,
                      [](const std::string& token) -> const std::string& {
                          return token;
                      });
}

}  // namespace aiws