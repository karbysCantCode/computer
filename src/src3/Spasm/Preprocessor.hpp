#pragma once

#include "Helpers/Debug.hpp"
#include "Spasm/Spasm.hpp"
#include "Spasm/Lexer.hpp"
#include "SMake/SMake.hpp"
#include "Spasm/Linker.hpp"

#include <variant>
#include <map>

namespace Spasm {
class Preprocessor {
  public:
  struct FunctionMacro {
    std::map<std::string_view, size_t> arguments;
    std::vector<Token> contents;
    std::string_view name;
    Token definitionToken;

    bool fillWithReplacedContents(TokenHolder& tokenHolder, std::vector<std::vector<Token>> replacementArgs);
    void addArgument(const std::string_view arg) {arguments.emplace(arg, arguments.size());}
    
    FunctionMacro(const Token& defTok) : definitionToken(defTok) {}
  };

  struct ReplacementMacro {
    std::vector<Token> contents;
    std::string_view name;
    Token definitionToken;

    ReplacementMacro(const Token& defTok) : definitionToken(defTok) {}
  };

  TokenHolder run(TokenHolder&, SMake::Target&, Spasm::Program::TranslationUnit&, Spasm::Program&, std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>>&, Debug::FullLogger* logger = nullptr);
  private:
  using MacroMapType = std::unordered_map<
    std::filesystem::path, 
    std::unordered_map<
      std::string_view, 
      std::variant<
        Preprocessor::FunctionMacro, 
        Preprocessor::ReplacementMacro
      >
    >
  >;

  using InnerMacroMapType = std::unordered_map<
      std::string_view, 
      std::variant<
        Preprocessor::FunctionMacro, 
        Preprocessor::ReplacementMacro
      >
    >;

  Debug::FullLogger* p_logger;
  MacroMapType p_macroMap;


  //std::unordered_map<std::string_view, std::variant<FunctionMacro, ReplacementMacro>>* p_macroMap;

  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

  void processDefine(TokenHolder&, TokenHolder&, InnerMacroMapType&);
  void processInclude(TokenHolder& tokenHolder, 
    SMake::Target& target, 
    Spasm::Program::TranslationUnit& translationUnit, 
    Spasm::Program& targetProgram, 
    InnerMacroMapType& myMacroMap,
    std::unordered_map<
      std::filesystem::path, 
      std::vector<Program::TranslationUnit*>>& dependantTranslationUnitMap
    );
      
  void processEntry(TokenHolder&, SMake::Target&);

  FunctionMacro parseFunctionMacroDefinition(TokenHolder&, TokenHolder&);
  ReplacementMacro parseReplacementMacroDefinition(TokenHolder&, TokenHolder&);
  TokenHolder processMacroInvocation(std::variant<FunctionMacro, ReplacementMacro>&, TokenHolder&);
};

}