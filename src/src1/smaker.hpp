#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <sstream>

#include "token.hpp"

namespace smake {

enum class TargetFormats {
  BIN,
  HEX,
  ELF
};

constexpr const char* targetFormatToString(TargetFormats format) {
  switch (format)
  {
  case TargetFormats::BIN:
    return "BIN";
    break;
  case TargetFormats::HEX:
    return "HEX";
    break;
  case TargetFormats::ELF:
    return "ELF";
    break;
  default:
    return "INVALID";
    break;
  }
}

struct PreprocessorReplacement {
  std::string replacementTarget;
  std::string replacement;

  std::string toString() const {
    return "PreProcRep, target: \"" + replacementTarget + "\", replacment: \"" + replacement + "\"\n";
  }

  PreprocessorReplacement(const std::string& repTarget, const std::string& rep) : replacementTarget(repTarget), replacement(rep) {}
};

struct Target {
  std::string name;
  bool isBuilt = false;

  std::unordered_set<std::filesystem::path> includeDirectories;
  std::string workingDirectory;
  
  std::unordered_set<std::filesystem::path> sourceFilepaths;
  std::unordered_map<std::string, PreprocessorReplacement> preprocessorReplacements;

  std::string entrySymbol;
  TargetFormats outputFormat;
  std::string outputDirectory;
  std::string outputName;

  std::unordered_set<Target*> dependantTargets;

  Target(const std::string& nm) : name(nm) {}




};

struct FList {
  std::unordered_set<std::filesystem::path> filepaths;
  std::string name;

  std::string toString() const {
    std::ostringstream oss;
    oss << name << '\n';
    for (const auto& path : filepaths) {
      oss << "    \"" << path.u8string() << "\"\n";
    }
    return oss.str();
  }
};


class SMakeProject {
  public:
  std::unordered_map<std::string, Target> targets;
  std::unordered_map<std::string, FList> flists;
  std::unordered_set<std::string> labels;


  SMakeProject();

  int build(const std::vector<SmakeToken::SmakeToken>& tokens);

  std::string toString() const {
    return "";
  }
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
  void parseSearch(bool clear = false);
  std::filesystem::path parsePath(std::string p);

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
