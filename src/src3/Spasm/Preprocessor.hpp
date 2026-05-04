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
  struct AbstractMacro {
    TokenHolder contents;
    std::string_view name;
    Token definitionToken;
    size_t invokeCount = 0;

    enum class Kind {
      FUNCTION,
      REPLACEMENT,
      NONE
    };

    virtual Kind getKind() const {assert(false); return Kind::NONE;}

    inline std::string getMangledName() const {return std::to_string(invokeCount) + std::string(name);}


    AbstractMacro(const Token& defTok) : definitionToken(defTok) {}
  };

  using MacroMapType = std::unordered_map<
    std::filesystem::path, 
    std::unordered_map<
      std::string_view, 
      std::shared_ptr<AbstractMacro>
    >
  >;

  using InnerMacroMapType = std::unordered_map<
    std::string_view, 
    std::shared_ptr<AbstractMacro>
  >;

  struct FunctionMacro : AbstractMacro{
    std::map<std::string_view, size_t> arguments;

    virtual Kind getKind() const override {return Kind::FUNCTION;}
    bool fillWithReplacedContents(Debug::FullLogger* logger, Program::TranslationUnit&, TokenHolder& tokenHolder, InnerMacroMapType& macroMap, std::vector<std::vector<Token>> replacementArgs);
    void addArgument(const std::string_view arg) {arguments.emplace(arg, arguments.size());}
    
    FunctionMacro(const Token& defTok) : AbstractMacro(defTok) {}
  };

  struct ReplacementMacro : AbstractMacro {
    virtual Kind getKind() const override {return Kind::REPLACEMENT;}
    ReplacementMacro(const Token& defTok) : AbstractMacro(defTok) {}
  };

  TokenHolder run(TokenHolder&, SMake::Target&, Spasm::Program::TranslationUnit&, Spasm::Program&, std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>>&, Debug::FullLogger* logger = nullptr);
  private:

  Debug::FullLogger* p_logger;
  MacroMapType p_macroMap;


  //std::unordered_map<std::string_view, std::variant<FunctionMacro, ReplacementMacro>>* p_macroMap;

  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  static inline void logError(Debug::FullLogger* logger, const Token& errToken, const std::string& message) {if (logger != nullptr) {logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}

  void processDefine(
    TokenHolder&, 
    TokenHolder&, 
    InnerMacroMapType&, 
    SMake::Target&, 
    Spasm::Program::TranslationUnit&, 
    Spasm::Program&, 
    std::unordered_map<
      std::filesystem::path, 
      std::vector<Program::TranslationUnit*>
    >&
  );
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
  static TokenHolder processMacroInvocation(Debug::FullLogger* logger, Program::TranslationUnit&, AbstractMacro*, TokenHolder&, InnerMacroMapType&);
  TokenHolder recurseDefineContents(
    AbstractMacro* macro,
    InnerMacroMapType& macroMap, 
    SMake::Target& target, 
    Spasm::Program::TranslationUnit& translationUnit, 
    Spasm::Program& targetProgram, 
    std::unordered_map<
      std::filesystem::path, 
      std::vector<Program::TranslationUnit*>
      >& dependantTranslationUnitMap
  );
};

}