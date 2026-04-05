#pragma once

#include "Helpers/Debug.hpp"
#include "Spasm/Spasm.hpp"
#include "Spasm/Lexer.hpp"
#include "SMake/SMake.hpp"

#include <variant>
#include <map>

namespace Spasm {
class Preprocessor {
  public:

  TokenHolder run(TokenHolder&, SMake::Target&, Spasm::Program::TranslationUnit&, Spasm::Program&, std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>>&, Debug::FullLogger* logger = nullptr);
  private:
  Debug::FullLogger* p_logger;

  struct FunctionMacro {
    std::map<std::string_view, size_t> arguments;
    std::vector<Token> contents;
    std::string_view name;

    bool fillWithReplacedContents(TokenHolder& tokenHolder, std::vector<std::vector<Token>> replacementArgs);
    void addArgument(const std::string_view arg) {arguments.emplace(arg, arguments.size());}
  };

  struct ReplacementMacro {
    std::vector<Token> contents;
    std::string_view name;
  };

  std::unordered_map<std::string_view, std::variant<FunctionMacro, ReplacementMacro>> p_macroMap;

  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

  void processDefine(TokenHolder&, TokenHolder&);
  void processInclude(TokenHolder&, SMake::Target&, Spasm::Program::TranslationUnit&, Spasm::Program&, std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>>&);
  void processEntry(TokenHolder&, SMake::Target&);

  FunctionMacro parseFunctionMacroDefinition(TokenHolder&, TokenHolder&);
  ReplacementMacro parseReplacementMacroDefinition(TokenHolder&, TokenHolder&);
  TokenHolder processMacroInvocation(std::variant<FunctionMacro, ReplacementMacro>&, TokenHolder&);
};
}