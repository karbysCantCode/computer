#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "token.hpp"

namespace smake {
struct Target {
  std::string name;

  std::unordered_set<std::string> includeDirectories;
  std::string workingDirectory;
  
  std::unordered_set<std::string> sourceFilepaths;
  std::unordered_set<PreprocessorReplacement> preprocessorReplacements;

  std::string entrySymbol;
  TargetFormats outputFormat;
  std::string outputDirectory;




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
  std::unordered_set<std::string> filepaths;
};


class SMakeProject {
  std::unordered_map<std::string, Target> targets;
  std::unordered_map<std::string, FList> flists;
  std::unordered_set<std::string> labels;


  SMakeProject();

  int build(const std::vector<SmakeToken::SmakeToken>& tokens);

  private:
};

class SmakeTokenParser {
  const std::vector<SmakeToken::SmakeToken>& tokens;
  const SMakeProject project;

  SmakeTokenParser(const std::vector<SmakeToken::SmakeToken>& tks, SMakeProject& targetProject);

  private:
  int pos = 0;
  template <size_t N>
  bool validateTokenTypes(const SmakeToken::SmakeTokenTypes (&tokenTypes)[N]) const;
  SmakeToken::SmakeToken advance();
  SmakeToken::SmakeToken peek(const size_t dist = 0) const;
  bool isAtEnd() const;

};
}
