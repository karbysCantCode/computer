#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#include "token.hpp"

namespace smake {
struct Target {
  std::string name;

  std::unordered_set<std::filesystem::path> includeDirectories;
  std::string workingDirectory;
  
  std::unordered_set<std::filesystem::path> sourceFilepaths;
  std::unordered_set<PreprocessorReplacement> preprocessorReplacements;

  std::string entrySymbol;
  TargetFormats outputFormat;
  std::string outputDirectory;

  Target(const std::string& nm) : name(nm) {}




};

enum class TargetFormats {
  BIN,
  HEX,
  ELF
};

struct PreprocessorReplacement {
  std::string replacementTarget;
  std::string replacement;
};

struct FList {
  std::unordered_set<std::filesystem::path> filepaths;
};


class SMakeProject {
  public:
  std::unordered_map<std::string, Target> targets;
  std::unordered_map<std::string, FList> flists;
  std::unordered_set<std::string> labels;


  SMakeProject();

  int build(const std::vector<SmakeToken::SmakeToken>& tokens);

  private:
};

class SmakeTokenParser {
  public:
  const std::vector<SmakeToken::SmakeToken>& tokens;
  SMakeProject project;

  SmakeTokenParser(const std::vector<SmakeToken::SmakeToken>& tks, SMakeProject& targetProject, std::filesystem::path makefilePath);

  bool isErrors() {return !errors.empty();}
  bool isWarnings() {return !warnings.empty();}
  bool isDebugs() {return !debugs.empty();}

  std::string consumeError();
  std::string consumeWarning();
  std::string consumeDebug();
  private:
  int pos = 0;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::vector<std::string> debugs;
  const std::filesystem::path makeFilePath;

  enum class SearchType {
    SHALLOW,
    CDEPTH,
    ALL
  };

  void logError(const std::string& message) {errors.emplace_back(message);}
  void logWarning(const std::string& message) {warnings.emplace_back(message);}
  void logDebug(const std::string& message) {debugs.emplace_back(message);}
 
  int validateTokenTypes(std::initializer_list<SmakeToken::SmakeTokenTypes> allowed) const;
  SmakeToken::SmakeToken advance();
  SmakeToken::SmakeToken peek(const size_t dist = 0) const;
  void advanceUntilAfterType(SmakeToken::SmakeTokenTypes type);
  bool isAtEnd() const;

  bool searchHandler(FList& targetList, std::unordered_set<std::string>& fileExtentions, std::filesystem::path pathToSearch, SmakeTokenParser::SearchType type, int searchDepth = -1);

  void parseKeywordTarget();
  void parseKeywordInclude_Directory();
  void parseKeywordWorking_Directory();
  void parseKeywordFlist();
  void parseKeywordSearchSet();
  void parseKeywordSearchAdd();
  void parseKeywordAddTarget();
  void parseKeywordDefine();
  void parseKeywordEntry();
  void parseKeywordOutput();
  void parseKeywordFormat();
  void parseKeywordDepends();
  void parseKeywordLabel();
  void parseKeywordIfdef();
  void parseKeywordIfndef();

};
}
