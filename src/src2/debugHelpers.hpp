#pragma once

#include <vector>
#include <string>

namespace Debug {
  class MessageLogger {
    public:
    std::vector<std::string> m_messages;

    inline bool isNotEmpty() const {return !m_messages.empty();}
    inline void logMessage(const std::string& message) {
      if (p_currentAuthor.empty()) {
        m_messages.push_back(message);
      } else {
        m_messages.push_back(p_currentAuthor + ' ' + message);
      }
    }
    inline std::string consumeMessage() {const auto msg = m_messages.back(); m_messages.pop_back(); return msg;}
    
    void setAuthor(const std::string& newAuthor) {p_currentAuthor = newAuthor;}
    void resetAuthor() {p_currentAuthor.clear();}
    private:
    std::string p_currentAuthor;
  };

  struct FullLogger {
    MessageLogger Debugs;
    MessageLogger Warnings;
    MessageLogger Errors;

    void setAuthor(const std::string& newAuthor) {
      Errors.setAuthor(newAuthor);
      Warnings.setAuthor(newAuthor);
      Debugs.setAuthor(newAuthor);
    }
    void resetAuthor() {
      Errors.resetAuthor();
      Warnings.resetAuthor();
      Debugs.resetAuthor();
    }
  };
}

// example defines
#define logError(message)   if (logger != nullptr) logger->Errors.logMessage(message);
#define logWarning(message) if (logger != nullptr) logger->Warnings.logMessage(message);
#define logDebug(message)   if (logger != nullptr) logger->Debugs.logMessage(message);

#undef logError
#undef logWarning
#undef logDebug