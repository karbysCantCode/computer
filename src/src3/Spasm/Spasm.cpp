#include "Spasm/Spasm.hpp"

#include "Spasm/Lexer.hpp"
#include "Spasm/Parser.hpp"
#include "Spasm/Linker.hpp"
#include "Spasm/OutputGenerator.hpp"
#include "Spasm/Preprocessor.hpp"
#include "Helpers/FileHelper.hpp"

#include <queue>

namespace Spasm {

  void spasmPipeline(SMake::Project& project, Arch::Architecture& arch, CLIOptions& options) {
    for (auto& target : project.m_targets) {
      Debug::FullLogger logger;

      Program program;
      
      Linker linker;
      
      for (const auto& sourcePath : target.second.m_sourceFilepaths) {
        program.m_filePathsToCreateTranslationUnitsOfAndPreprocess.push(sourcePath);
      }
      
      // auto [it, inserted] = program.m_translationUnits.emplace("", std::make_unique<Program::TranslationUnit>());
      // it->second->m_statementVector.();
      // linker.addIndependentTranslationUnits(it->second.get());

      //parse trabskaton units
      while (!program.m_filePathsToCreateTranslationUnitsOfAndPreprocess.empty()) {
        auto transUnit = std::make_unique<Program::TranslationUnit>();
        auto transUnitPtr = transUnit.get();
        transUnitPtr->m_sourcePath = program.m_filePathsToCreateTranslationUnitsOfAndPreprocess.top();
        if (program.m_translationUnits.find(transUnit->m_sourcePath) == program.m_translationUnits.end()) {
          program.m_translationUnits.emplace(transUnit->m_sourcePath, std::move(transUnit));
        } else {
          program.m_translationUnits[transUnit->m_sourcePath] = std::move(transUnit);
        }
        
        program.m_filePathsToCreateTranslationUnitsOfAndPreprocess.pop();
        transUnitPtr->m_source = std::make_unique<std::string>(FileHelper::openFileToString(transUnitPtr->m_sourcePath));
        
        SpasmLexer lexer;
        auto preprocessedTokens = lexer.run(*transUnitPtr->m_source, transUnitPtr->m_sourcePath);
        
        
        Preprocessor preproc;
        transUnitPtr->processedTokens = preproc.run(preprocessedTokens, target.second, *transUnitPtr, program, linker.m_dependantTranslationUnitMap, &logger);
        
        program.m_translationUnitsToParseAndLink.push(transUnitPtr);
      }
      
      while (!program.m_translationUnitsToParseAndLink.empty()) {
        Program::TranslationUnit& translationUnit = *program.m_translationUnitsToParseAndLink.top();
        program.m_translationUnitsToParseAndLink.pop();
        
        Parser parser;
        parser.ParseTokens(translationUnit.processedTokens, arch, translationUnit, program, &logger);
        
        if (translationUnit.m_includedFiles.size() == 0) {
          linker.addIndependentTranslationUnits(&translationUnit);
        }
      }

      // update here and generator.run
      const size_t entrySymbolJumpByteLength = target.second.m_entrySymbol.empty() ? 0 : 14;
      
      Linker::LinkedResult linkedResult = linker.run(entrySymbolJumpByteLength, &logger);
      
      OutputGenerator generator;

      generator.run(target.second, linker, linkedResult, entrySymbolJumpByteLength, &logger);

      if (!logger.Errors.isEmpty()) {
        logger.Errors.logMessage("Compilation aborted after linking due to error(s) in compilation pipeline. (not exclusively linker)");
      } else {

      }

      if (!options.silent) {
        while (!logger.Errors.isEmpty()) {
          std::cout << logger.Errors.consumeMessage() << '\n';
        }
      }

      if (options.warns) {
        while (!logger.Warnings.isEmpty()) {
          std::cout << logger.Warnings.consumeMessage() << '\n';
        }
      }
      if (options.debugs) {
        while (!logger.Debugs.isEmpty()) {
          std::cout << logger.Debugs.consumeMessage() << '\n';
        }
      }
      
      program.debugPrint();
    }
  }

using EvaluatePairType = std::pair<int, std::string>;
EvaluatePairType Program::IdentifierExpr::evaluate(NonOwningIdentifierMapType& identifierMap) {
  std::string constructedName;
  Program::IdentifierObject* lastIdentifierObject;
  const auto it = identifierMap.find(identifierPath.front());
  constructedName.append(identifierPath.front());
  identifierPath.pop();
  if (it == identifierMap.end()) {
    return {0, std::format("Identifier \"{}\" is not declared before it is referenced here. (potentially partial identifier path)", constructedName)};
  }

  lastIdentifierObject = it->second;

  while (!identifierPath.empty()) {
    const auto it = lastIdentifierObject->children.find(identifierPath.front());
    constructedName.append(identifierPath.front());
    identifierPath.pop();
    if (it == lastIdentifierObject->children.end()) {
      return {0, std::format("Identifier \"{}\" is not declared before it is referenced here. (potentially partial identifier path)", constructedName)};
    }
  }

  if (!lastIdentifierObject->addressResolved) {
    return {0, std::format("Identifier \"{}\" is not resolved before it is referenced here. (potentially partial identifier path)", constructedName)};
  }

  value = lastIdentifierObject->address;
  setEvaluated();
  return {value, ""};
}

EvaluatePairType Program::UnaryExpr::evaluate(NonOwningIdentifierMapType& idenMap) {
  if (!right) {
    return {0, "Expression argument doesn't exist."};
  }

  const auto eval = right->evaluate(idenMap);
  switch (op) {
    case Token::Type::SUBTRACT:
      return {-eval.first, eval.second};
      break;
    case Token::Type::BITWISENOT:
      return {~eval.first, eval.second};
      break;
    default: break;

  }
  return {0, "Unknown unary operator type."};
}

EvaluatePairType Program::BinaryExpr::evaluate(NonOwningIdentifierMapType& idenMap) {
  if (!right) {
    return {0, "Expression right argument doesn't exist."};
  }
  if (!left) {
    return {0, "Expression left argument doesn't exist."};
  }

  const auto evalLeft = left->evaluate(idenMap);
  const auto evalRight = right->evaluate(idenMap);

  if (!evalLeft.second.empty()) {
    return {0, evalLeft.second};
  }
  if (!evalRight.second.empty()) {
    return {0, evalRight.second};
  }

  switch (op) {
    using TY = Token::Type;
    case TY::BITWISEOR:
      return {evalLeft.first | evalRight.first, ""};
      break;
    case TY::BITWISEXOR:
      return {evalLeft.first ^ evalRight.first, ""};
      break;
    case TY::BITWISEAND:
      return {evalLeft.first & evalRight.first, ""};
      break;
    case TY::LEFTSHIFT:
      return {evalLeft.first << evalRight.first, ""};
      break;
    case TY::RIGHTSHIFT:
      return {evalLeft.first >> evalRight.first, ""};
      break;
    case TY::SUBTRACT:
      return {evalLeft.first - evalRight.first, ""};
      break;
    case TY::ADD:
      return {evalLeft.first + evalRight.first, ""};
      break;
    case TY::MULTIPLY:
      return {evalLeft.first * evalRight.first, ""};
      break;
    case TY::DIVIDE:
      return {evalLeft.first / evalRight.first, ""};
      break;
    case TY::MOD:
      return {evalLeft.first % evalRight.first, ""};
      break;
    default:break;
  }

  return {0, "Unforseen error in binary expression evaluation!"};
}

std::string_view Program::IdentifierObject::fullName() const {
  const auto* start = nameSegments[0].data();
  const auto& backRef = nameSegments.back();
  const auto* end = backRef.data() + backRef.size();
  return std::string_view(start, end - start);
}

//depth 1 and 0 both return the global identifier
//depth N > 1 returns N segments 
std::string_view Program::IdentifierObject::getNDepthName(size_t depth) const {
  depth = depth == 0 ? 0 : depth-1;
  depth = depth > nameSegments.size() ? nameSegments.size() : depth;
  const auto* start = nameSegments[0].data();
  const auto& backRef = nameSegments[depth];
  const auto* end = backRef.data() + backRef.size();
  return std::string_view(start, end - start);
}

std::vector<std::string_view> Program::IdentifierObject::getNDepthNameVector(size_t depth) const {
  depth = depth == 0 ? 1 : depth;
  depth = depth > nameSegments.size() ? nameSegments.size() : depth;
  return std::vector<std::string_view>(nameSegments.begin(), nameSegments.begin() + depth);
}

std::string_view Program::IdentifierObject::name() const {
  return nameSegments.back();
}

void Spasm::Program::debugPrint() const {
  std::cout << "\n=========== PROGRAM DEBUG DUMP ===========\n";

  for (const auto& [path, tuPtr] : m_translationUnits) {
    std::cout << "\n-- Translation Unit: " << path.string() << " --\n";

    const TranslationUnit& tu = *tuPtr;

    std::cout << "\n  Statements:\n";
    for (const auto& stmt : tu.m_statementVector) {
      debugPrintStatement(stmt.get(), 2);
    }

    std::cout << "\n  Identifiers:\n";
    for (const auto& [name, obj] : tu.m_identifierMap) {
      indent(2);
      std::cout << "Identifier Key: " << name << "\n";
      debugPrintIdentifier(obj.get(), 3);
    }

    std::cout << "\nUnresolved Expressions Stack:\n";
    auto stackCopy = tu.m_unresolvedExpressions;
    while (!stackCopy.empty()) {
      indent(1);
      std::cout << "Unresolved Expr:\n";
      debugPrintExpr(stackCopy.front(), 2);
      stackCopy.pop();
    }
  }



  std::cout << "\n==========================================\n";
}

void Spasm::Program::debugPrintStatement(const StatementSymbol* stmt, int indentLevel) const {
  indent(indentLevel);

  if (auto lbl = dynamic_cast<const LabelSymbol*>(stmt)) {
    std::cout << "LabelSymbol: " << lbl->name << "\n";
  }
  else if (auto def = dynamic_cast<const DefinitionSymbol*>(stmt)) {
    std::cout << "DefinitionSymbol: " << def->name << "\n";
  }
  else if (auto instr = dynamic_cast<const InstructionSymbol*>(stmt)) {
    std::cout << "InstructionSymbol: opcode=" << instr->instruction.m_opcode
              << " name=" << instr->instruction.m_name << "\n";

    for (const auto& op : instr->operands) {
      debugPrintOperand(op.get(), indentLevel + 1);
    }
  }
  else {
    std::cout << "Unknown StatementSymbol\n";
  }
}

void Spasm::Program::debugPrintIdentifier(const IdentifierObject* obj, int indentLevel) const {
  indent(indentLevel);
  std::cout << "Name: " << obj->name()
            << " Address: " << obj->address
            << " Resolved: " << obj->addressResolved
            << "\n";

  if (auto lbl = dynamic_cast<const LabelObject*>(obj)) {
    indent(indentLevel);
    std::cout << "Type: LabelObject\n";

    for (const auto& [childName, child] : lbl->children) {
      indent(indentLevel + 1);
      std::cout << "Child Label: " << childName << "\n";
      debugPrintIdentifier(child.get(), indentLevel + 2);
    }
  }
  else if (auto data = dynamic_cast<const DataObject*>(obj)) {
    indent(indentLevel);
    std::cout << "Type: DataObject\n";

    indent(indentLevel);
    std::cout << "ElementSize: " << data->elementSize
              << " ElementCount: " << data->elementCount << "\n";

    indent(indentLevel);
    std::cout << "Raw Data: ";
    for (auto b : data->data) {
      std::cout << static_cast<int>(b) << " ";
    }
    std::cout << "\n";

    for (const auto& expr : data->exprData) {
      debugPrintExpr(expr.get(), indentLevel + 1);
    }
  }
}

void Spasm::Program::debugPrintExpr(const Expr* expr, int indentLevel) const {
  indent(indentLevel);

  if (!expr) {
    std::cout << "NULL Expr\n";
    return;
  }

  if (auto num = dynamic_cast<const NumberExpr*>(expr)) {
    std::cout << "NumberExpr: " << num->value << "\n";
  }
  else if (auto id = dynamic_cast<const IdentifierExpr*>(expr)) {
    std::cout << "IdentifierExpr: ";
    auto qCopy = std::queue(id->identifierPath);
    while (!qCopy.empty()) {
      std::cout << qCopy.front();
      qCopy.pop();
      if (!qCopy.empty()) {
        std::cout << '.';
      }
    }
    std::cout << "\n";
  }
  else if (auto unary = dynamic_cast<const UnaryExpr*>(expr)) {
    std::cout << "UnaryExpr: op=" << static_cast<int>(unary->op) << "\n";
    debugPrintExpr(unary->right.get(), indentLevel + 1);
  }
  else if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
    std::cout << "BinaryExpr: op=" << static_cast<int>(bin->op) << "\n";
    debugPrintExpr(bin->left.get(), indentLevel + 1);
    debugPrintExpr(bin->right.get(), indentLevel + 1);
  }
  else {
    std::cout << "Base Expr (value=" << expr->value
              << ", evaluated=" << expr->evaluated << ")\n";
  }
}

void Spasm::Program::debugPrintOperand(const Operand* op, int indentLevel) const {
  indent(indentLevel);

  if (auto reg = dynamic_cast<const RegisterOperand*>(op)) {
    std::cout << "RegisterOperand: " << reg->reg.m_registerName << "\n";
  }
  else if (auto expr = dynamic_cast<const ExpressionOperand*>(op)) {
    std::cout << "ExpressionOperand:\n";
    debugPrintExpr(expr->expression.get(), indentLevel + 1);
  }
  else if (auto c = dynamic_cast<const ConstantOperand*>(op)) {
    std::cout << "ConstantOperand: " << c->value << "\n";
  }
  else {
    std::cout << "Unknown Operand\n";
  }
}

void Spasm::Program::indent(int count) {
  for (int i = 0; i < count; ++i)
    std::cout << "  ";
}
}