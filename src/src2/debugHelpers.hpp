#pragma once

#include <vector>
#include <string>

namespace Debug {
  class MessageLogger {
    public:
    std::vector<std::string> m_messages;

    inline bool isNotEmpty() const {return !m_messages.empty();}
    inline void logMessage(const std::string& message) {m_messages.push_back(message);}
    inline std::string consumeMessage() {const auto msg = m_messages.back(); m_messages.pop_back(); return msg;}
  };

  struct FullLogger {
    MessageLogger Debugs;
    MessageLogger Warnings;
    MessageLogger Errors;
  };
}

// example defines
#define logError(message)   if (logger != nullptr) logger->Errors.logMessage(message);
#define logWarning(message) if (logger != nullptr) logger->Warnings.logMessage(message);
#define logDebug(message)   if (logger != nullptr) logger->Debugs.logMessage(message);

#undef logError
#undef logWarning
#undef logDebug