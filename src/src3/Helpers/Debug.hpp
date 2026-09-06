#pragma once
#include <stack>
#include <string>
#include <format>
#include <iostream>

namespace Debug
{


class MessageLogger {
  public:
  std::stack<std::string> m_messages;

  inline bool isEmpty() const {return m_messages.empty();}
  inline std::string consumeMessage() {auto msg = m_messages.top(); m_messages.pop(); return msg;}
  inline void logMessage(const std::string& message) {m_messages.push(message);}
  inline void dumpToCout() {while (!isEmpty()) std::cout << consumeMessage() << std::endl;}

};

class FullLogger {
  public:
  MessageLogger Debugs;
  MessageLogger Warnings;
  MessageLogger Errors;

  inline void dumpToCout() {Errors.dumpToCout();Warnings.dumpToCout();Debugs.dumpToCout();}
};

} // namespace Debug
