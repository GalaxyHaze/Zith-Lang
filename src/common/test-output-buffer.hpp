#pragma once

#include "common/test-log-protocol.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zith::common {

/// Captures output with fixed-capacity staging for framed test-log bodies.
class TestOutputBuffer {
public:
    void append(std::string_view text) {
        for (const char value : text) {
            switch (state_) {
            case State::Outside:
                appendOutside(value);
                break;
            case State::Context:
                appendContext(value);
                break;
            case State::Header:
                appendHeader(value);
                break;
            case State::Body:
                appendBody(value);
                break;
            }
        }
    }

    [[nodiscard]] std::string take() {
        if (state_ == State::Outside) {
            output_.append(outsidePending_);
        } else if (state_ == State::Context) {
            output_.append(kTestLogContextPrefix);
            output_.append(contextName_);
        } else if (state_ == State::Header) {
            output_.append(header_);
        } else {
            for (const char value : endPending_)
                appendBodyByte(value);
            flushLogBuffer();
        }
        return std::move(output_);
    }

private:
    enum class State { Outside, Context, Header, Body };

    std::string output_;
    std::string outsidePending_;
    std::string contextName_;
    std::string header_;
    std::string logName_;
    std::string endPending_;
    std::vector<std::pair<std::string, std::size_t>> logBytes_;
    std::array<char, kTestLogCapacity> logBuffer_{};
    std::size_t logBufferSize_ = 0;
    State state_ = State::Outside;

    [[nodiscard]] static bool startsWith(std::string_view text, std::string_view prefix) {
        return text.size() <= prefix.size() && prefix.substr(0, text.size()) == text;
    }

    [[nodiscard]] static bool isProtocolPrefix(std::string_view text) {
        return startsWith(text, kTestLogBeginPrefix) || startsWith(text, kTestLogContextPrefix);
    }

    void appendOutside(const char value) {
        outsidePending_.push_back(value);
        while (!isProtocolPrefix(outsidePending_)) {
            output_.push_back(outsidePending_.front());
            outsidePending_.erase(0U, 1U);
        }

        if (outsidePending_ == kTestLogBeginPrefix) {
            output_.append(kTestLogBeginPrefix);
            outsidePending_.clear();
            header_.clear();
            state_ = State::Header;
        } else if (outsidePending_ == kTestLogContextPrefix) {
            outsidePending_.clear();
            contextName_.clear();
            state_ = State::Context;
        }
    }

    void appendContext(const char value) {
        if (value == '\n') {
            activeTest_ = std::move(contextName_);
            contextName_.clear();
            state_ = State::Outside;
            return;
        }
        contextName_.push_back(value);
    }

    void appendHeader(const char value) {
        if (value != '\n') {
            header_.push_back(value);
            return;
        }
        logName_ = header_.empty() ? activeTest_ : std::move(header_);
        header_.clear();
        output_.append(logName_);
        output_.push_back('\n');
        endPending_.clear();
        logBufferSize_ = 0;
        state_ = State::Body;
    }

    void appendBody(const char value) {
        endPending_.push_back(value);
        while (!startsWith(endPending_, kTestLogEndMarker)) {
            appendBodyByte(endPending_.front());
            endPending_.erase(0U, 1U);
        }
        if (endPending_ == kTestLogEndMarker) {
            flushLogBuffer();
            output_.append(kTestLogEndMarker);
            endPending_.clear();
            logName_.clear();
            state_ = State::Outside;
        }
    }

    void appendBodyByte(const char value) {
        auto entry = logBytes_.begin();
        while (entry != logBytes_.end() && entry->first != logName_)
            ++entry;
        if (entry == logBytes_.end()) {
            logBytes_.emplace_back(logName_, 0U);
            entry = logBytes_.end() - 1;
        }
        if (entry->second < kTestLogCapacity && logBufferSize_ < logBuffer_.size()) {
            logBuffer_[logBufferSize_] = value;
            ++logBufferSize_;
            ++entry->second;
        }
    }

    void flushLogBuffer() {
        output_.append(logBuffer_.data(), logBufferSize_);
        logBufferSize_ = 0;
    }

    std::string activeTest_;
};

} // namespace zith::common
