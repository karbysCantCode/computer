#pragma once

#include "Spasm/Spasm.hpp"
#include "Spasm/Lexer.hpp"
#include <filesystem>

namespace Spasm {
class Linker {
public:
  std::queue<Program::TranslationUnit*> run(
    
  );

  void linkTU(
    Program::TranslationUnit&, 
    std::queue<Program::TranslationUnit*>&
  );

  inline void addIndependentTranslationUnits(Program::TranslationUnit* tuptr) {m_independentTranslationUnits.push(tuptr);}
  inline bool areUnlinkedIndependentTranslationUnits() const {return !m_independentTranslationUnits.empty();}
  inline auto consumeIndependentTranslationUnitFromStack() {auto& TU = m_independentTranslationUnits.top(); m_independentTranslationUnits.pop(); return TU;}
  std::stack<Program::TranslationUnit*> m_independentTranslationUnits;
  std::unordered_map<std::filesystem::path, std::vector<Program::TranslationUnit*>> m_dependantTranslationUnitMap;
  std::unordered_map<std::string_view, Program::IdentifierObject*> m_fullNameCollatedIdentifierMap;
  std::unordered_map<std::string_view, Program::IdentifierObject*> m_globalIdentifierMap;

private:
  Debug::FullLogger* p_logger;
  inline void logError(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(errToken.location.toString() + message);}}
  inline void logWarning(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(errToken.location.toString() + message);}}
  inline void logDebug(const Token& errToken, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(errToken.location.toString() + message);}}
  inline void logError(const SourceLocation& sLoc, const std::string& message) const{if (p_logger != nullptr) {p_logger->Errors.logMessage(sLoc.toString() + message);}}
  inline void logWarning(const SourceLocation& sLoc, const std::string& message) const{if (p_logger != nullptr) {p_logger->Warnings.logMessage(sLoc.toString() + message);}}
  inline void logDebug(const SourceLocation& sLoc, const std::string& message) const{if (p_logger != nullptr) {p_logger->Debugs.logMessage(sLoc.toString() + message);}}

  bool iteratorAlreadyInFullNameMap(Spasm::Program::NonOwningIdentifierMapType::iterator iterator) const {return iterator != m_fullNameCollatedIdentifierMap.end();}
  bool nameAlreadyInGlobalMap(std::string_view& fullname) const {return m_globalIdentifierMap.find(fullname) != m_globalIdentifierMap.end();}
};

}