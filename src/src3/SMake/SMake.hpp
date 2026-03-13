#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <utility>

#include "SMake/Lexer.hpp"
#include "Helpers/Debug.hpp"

namespace SMake {

struct FList {
  std::unordered_set<std::filesystem::path> m_filepaths;
  std::string m_name;
};

struct Target {
  enum class Format {
    BIN, HEX, ELF
  };

  std::string m_name;
  bool m_isBuilt = false;

  std::unordered_set<std::filesystem::path> m_includeDirectories;
  std::string m_workingDirectory;
  
  std::unordered_set<std::filesystem::path> m_sourceFilepaths;
  //std::unordered_map<std::string, PreprocessorReplacement> m_preprocessorReplacements;

  std::string m_entrySymbol;
  Format m_outputFormat;
  std::string m_outputDirectory;
  std::string m_outputName;
  // std::unordered_map<
  //       std::filesystem::path, 
  //       std::unique_ptr<Spasm::Program::ProgramForm>
  //   > m_filePathProgramMap;

  std::unordered_set<Target*> m_dependantTargets;
};


class Project {
public:
  std::unordered_map<std::string, Target> m_targets;
  std::unordered_map<std::string, FList> m_flists;
  std::unordered_set<std::string> m_labels;
  std::filesystem::path m_makefilePath;

  
private:

// parsing helpers
enum class SearchType {
  SHALLOW,
  CDEPTH,
  ALL
};

Debug::FullLogger *p_logger = nullptr;

std::pair<bool, int> validateTokenTypes(TokenHolder& tokenHolder, std::initializer_list<Token::Type> allowed);
bool searchHandler(FList& targetList, std::unordered_set<std::string>& fileExtentions , std::filesystem::path pathToSearch, SearchType type, int searchDepth);
void parseSearch(TokenHolder& tokenHolder, bool clear);

void parseKeywordTarget(TokenHolder&);
void parseKeywordInclude_Directory(TokenHolder&);
void parseKeywordWorking_Directory(TokenHolder&);
void parseKeywordFlist(TokenHolder&);
void parseKeywordSearchSet(TokenHolder&);
void parseKeywordSearchAdd(TokenHolder&);
void parseKeywordAddTarget(TokenHolder&);
void parseKeywordDefine(TokenHolder&);
void parseKeywordEntry(TokenHolder&);
void parseKeywordOutput(TokenHolder&);
void parseKeywordFormat(TokenHolder&);
void parseKeywordDepends(TokenHolder&);
void parseKeywordLabel(TokenHolder&);
void parseKeywordIfdef(TokenHolder&);
void parseKeywordIfndef(TokenHolder&);

void logError(const Token&, const std::string&);
};
}