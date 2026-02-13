#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <sstream>

#include "debugHelpers.hpp"



namespace SMake {
struct Token {
  enum class Type {
    UNASSIGNED,
    KEYWORD,
    OPENPAREN,
    CLOSEPAREN,
    OPENBLOCK,
    CLOSEBLOCK,
    STRING,
    IDENTIFIER,
    COMMA,
    INVALID
  };
  
  std::string m_value;
  Type m_type = Type::UNASSIGNED;
  int m_line = -1;
  int m_column = -1;

  inline void setString(const std::string& newString) {m_value=newString;}
  inline void setType(const Type& newType) {m_type=newType;}
  std::string toString(size_t padding) const;
  constexpr const char* typeToString() const {
  switch (m_type) {
    case Type::UNASSIGNED: return "UNASSIGNED";
    case Type::KEYWORD:    return "KEYWORD";
    case Type::OPENPAREN:  return "OPENPAREN";
    case Type::CLOSEPAREN: return "CLOSEPAREN";
    case Type::OPENBLOCK:  return "OPENBLOCK";
    case Type::CLOSEBLOCK: return "CLOSEBLOCK";
    case Type::STRING:     return "STRING";
    case Type::IDENTIFIER: return "IDENTIFIER";
    case Type::COMMA:      return "COMMA";
    case Type::INVALID:    return "INVALID";
    default: return "UNKNOWN";
  }
}
  std::string positionToString() const {return m_line < 0 || m_column < 0 ? std::string("END OF FILE") : "line " + std::to_string(m_line) + ", column " + std::to_string(m_column);}

  Token(const std::string& val, const Type t, int ln, int col) :
    m_value(val), m_type(t), m_line(ln), m_column(col) {}
  
  Token(const char val, const Type t, int ln, int col) :
    m_value{val}, m_type(t), m_line(ln), m_column(col) {}
};

struct Target {
  enum class Format {
    BIN,
    HEX,
    ELF
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

  std::unordered_set<Target*> m_dependantTargets;

  Target(std::string name) : m_name(name) {}

  std::string toString(size_t padding = 0, size_t indnt = 0) const;

  constexpr const char* formatToString() const {
    switch (m_outputFormat)
    {
    case Format::BIN:
      return "BIN";
      break;
    case Format::HEX:
      return "HEX";
      break;
    case Format::ELF:
      return "ELF";
      break;
    default:
      return "INVALID";
      break;
    }
  }

  
};
struct FList {
  std::unordered_set<std::filesystem::path> m_filepaths;
  std::string m_name;

  std::string toString(size_t padding = 0, size_t indnt = 0) const {
    std::ostringstream oss;
    const std::string indent(padding + indnt, ' ');
    const std::string indent2(padding * 2 + indnt, ' ');
    const std::string indent3(padding * 3 + indnt, ' ');
    oss << indent << "Name:" << m_name << '\n'
        << indent2 << "Paths:" << '\n';

    for (const auto& path : m_filepaths) {
      oss << indent3 << '"' << path.generic_string() << "\"\n";
    }
    return oss.str();
  }
};

class SMakeProject {
  public:
  std::unordered_map<std::string, Target> m_targets;
  std::unordered_map<std::string, FList> m_flists;
  std::unordered_set<std::string> m_labels;
  std::filesystem::path m_makefilePath;



  SMakeProject();

  int build(const std::vector<Token>& tokens);

  std::string toString() const;
  
  private:
};

void parseTokensToProject(const std::vector<Token>& tokens, SMakeProject& targetProject, std::filesystem::path makefilePath, Debug::FullLogger* logger);
std::vector<Token> lex(std::filesystem::path path, Debug::FullLogger* logger = nullptr);
}