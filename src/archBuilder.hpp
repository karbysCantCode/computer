#include <iostream>

#include "token.hpp"
#include "arch.hpp"

class ArchBuilder {
  public:
  ArchBuilder() {};
  Architecture::Architecture build(std::vector<ArchToken::ArchToken>& tks);

  std::string consumeError();
  std::string consumeWarning();
  bool isError() {return !errors.empty();}
  bool isWarning() {return !warnings.empty();}
  private:
  std::vector<ArchToken::ArchToken>* tokens = nullptr;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  size_t pos = 0;

  void logError(const std::string& err) {errors.push_back(err);}
  void logError(const char* err)  {errors.emplace_back(err);}
  void logWarning(const std::string& warn) {warnings.push_back(warn);}
  void logWarning(const char* warn) {warnings.emplace_back(warn);}

  bool isAtEnd() const {if (tokens == nullptr) return true; return !(pos < tokens->size());}
  const ArchToken::ArchToken peek(size_t t = 0) {if (tokens == nullptr) return ArchToken::ArchToken(); return (pos+t < tokens->size()) ? (*tokens)[pos+t] : ArchToken::ArchToken();}
  const ArchToken::ArchToken advance() {if (tokens == nullptr) return ArchToken::ArchToken(); const auto token = (*tokens)[pos]; pos++; return token;}

  Architecture::Instruction::Argument parseArgument();

  const std::string parseRange(const std::string& str, size_t* pos) const;
};