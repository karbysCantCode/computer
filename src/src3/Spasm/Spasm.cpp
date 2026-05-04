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
      
      Linker::LinkedResult linkedResult = linker.run(entrySymbolJumpByteLength, program, &logger);
      
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
        program.debugPrint();
      } 
      
    }
  }

Program::EvaluateTriple Program::IdentifierExpr::evaluate(std::vector<size_t>& addressHolder, bool getMentionedLabels) {
  std::string constructedName;
  Program::IdentifierObject* lastIdentifierObject;
  auto pathCopy = identifierPath;
  const auto it = identifierMap->find(pathCopy.front());
  constructedName.append(pathCopy.front());
  pathCopy.pop();
  if (it == identifierMap->end()) {
    return {0, std::format("Identifier \"{}\" is not declared before it is referenced here. (potentially partial identifier path)", constructedName), {}};
  }

  lastIdentifierObject = *it->second;

  while (!pathCopy.empty()) {
    const auto it2 = lastIdentifierObject->children.find(pathCopy.front());
    constructedName.append(pathCopy.front());
    pathCopy.pop();
    if (it2 == lastIdentifierObject->children.end()) {
      return {0, std::format("Identifier \"{}\" is not declared before it is referenced here. (potentially partial identifier path)", constructedName), {}};
    }

    lastIdentifierObject = *it2->second;
  }

  // if (!lastIdentifierObject->addressResolved) {
  //   return {0, std::format("Identifier \"{}\" is not resolved before it is referenced here. (potentially partial identifier path)", constructedName)};
  // }

  value = addressHolder[lastIdentifierObject->addressIndex];
  setEvaluated();
  return {value, "", {constructedName}};
}

Program::EvaluateTriple Program::UnaryExpr::evaluate(std::vector<size_t>& addressHolder, bool getMentionedLabels) {
  if (!right) {
    return {0, "Expression argument doesn't exist.", {}};
  }

  EvaluateTriple triple;

  const auto eval = right->evaluate(addressHolder, getMentionedLabels);
  switch (op) {
    case Token::Type::SUBTRACT:
      triple = {-eval.value, eval.error, eval.mentionedLabels};
      break;
    case Token::Type::BITWISENOT:
      triple = {~eval.value, eval.error, eval.mentionedLabels};
      break;
    case Token::Type::RELATIVEOPERATOR:
      triple = {eval.value - ((int)addressHolder[*addressIndexPtr] + (int)relativeAddressOffset), eval.error, eval.mentionedLabels};
      break;
    case Token::Type::ABSOLUTE:
      triple = {std::abs(eval.value), eval.error, eval.mentionedLabels};
      break;
    default: 
      return {0, "Unknown unary operator type.", eval.mentionedLabels};
      break;

  }
  value = triple.value;
  return triple;
}

Program::EvaluateTriple Program::BinaryExpr::evaluate(std::vector<size_t>& addressHolder, bool getMentionedLabels) {
  if (!right) {
    return {0, "Expression right argument doesn't exist.", {}};
  }
  if (!left) {
    return {0, "Expression left argument doesn't exist.", {}};
  }
  
  EvaluateTriple triple;

  const auto evalLeft = left->evaluate(addressHolder, getMentionedLabels);
  const auto evalRight = right->evaluate(addressHolder, getMentionedLabels);

  //merge label set
  auto labels = std::move(evalLeft.mentionedLabels);
  labels.reserve(labels.size() + evalRight.mentionedLabels.size());
  labels.insert(
    evalRight.mentionedLabels.begin(),
    evalRight.mentionedLabels.end()
  );

  if (!evalLeft.error.empty()) {
    return {0, evalLeft.error, labels};
  }
  if (!evalRight.error.empty()) {
    return {0, evalRight.error, labels};
  }

  switch (op) {
    using TY = Token::Type;
    case TY::COMPARISONOR:
      triple =  {evalLeft.value || evalRight.value, "", labels};
      break;
    case TY::COMPARISONAND:
      triple =  {evalLeft.value && evalRight.value, "", labels};
      break;
    case TY::BITWISEOR:
      triple =  {evalLeft.value | evalRight.value, "", labels};
      break;
    case TY::BITWISEXOR:
      triple =  {evalLeft.value ^ evalRight.value, "", labels};
      break;
    case TY::BITWISEAND:
      triple =  {evalLeft.value & evalRight.value, "", labels};
      break;
    case TY::EQUAL:
      triple =  {evalLeft.value == evalRight.value, "", labels};
      break;
    case TY::NOTEQUAL:
      triple =  {evalLeft.value != evalRight.value, "", labels};
      break;
    case TY::LESSTHAN:
      triple =  {evalLeft.value < evalRight.value, "", labels};
      break;
    case TY::LESSTHANOREQUAL:
      triple =  {evalLeft.value <= evalRight.value, "", labels};
      break;
    case TY::GREATERTHAN:
      triple =  {evalLeft.value > evalRight.value, "", labels};
      break;
    case TY::GREATERTHANOREQUAL:
      triple =  {evalLeft.value >= evalRight.value, "", labels};
      break;
    case TY::LEFTSHIFT:
      triple =  {evalLeft.value << evalRight.value, "", labels};
      break;
    case TY::RIGHTSHIFT:
      triple =  {evalLeft.value >> evalRight.value, "", labels};
      break;
    case TY::SUBTRACT:
      triple =  {evalLeft.value - evalRight.value, "", labels};
      break;
    case TY::ADD:
      triple =  {evalLeft.value + evalRight.value, "", labels};
      break;
    case TY::MULTIPLY:
      triple =  {evalLeft.value * evalRight.value, "", labels};
      break;
    case TY::DIVIDE:
      triple =  {evalLeft.value / evalRight.value, "", labels};
      break;
    case TY::MOD:
      triple =  {evalLeft.value % evalRight.value, "", labels};
      break;
    default:
      return {0, "Unforseen error in binary expression evaluation!", labels};
      break;
  }

  value = triple.value;
  return triple;

}

std::string Program::IdentifierObject::fullName() const {
  std::string name;
  for (const auto seg : nameSegments) {
    name.append(seg);
    name.push_back('.');
  }
  return name;

  //old 
  // const auto* start = nameSegments[0].data();
  // const auto& backRef = nameSegments.back();
  // const auto* end = backRef.data() + backRef.size();
  // const auto size = end - start;
  // return std::string_view(start, size);
}

//depth 1 and 0 both return the global identifier
//depth N > 1 returns N segments 
std::string Program::IdentifierObject::getNDepthName(size_t depth) const {
  std::string name;
  depth = depth == 0 ? 1 : depth-1;
  for (size_t i = 0; i < depth; i++) {
    name.append(nameSegments[i]);
    name.push_back('.');
  }
  return name;

  //old
  // depth = depth > nameSegments.size() ? nameSegments.size() : depth;
  // const auto* start = nameSegments[0].data();
  // const auto& backRef = nameSegments[depth];
  // const auto* end = backRef.data() + backRef.size();
  // return std::string_view(start, end - start);
}

std::vector<std::string_view> Program::IdentifierObject::getNDepthNameVector(size_t depth) const {
  depth = depth == 0 ? 1 : depth;
  depth = depth > nameSegments.size() ? nameSegments.size() : depth;
  return std::vector<std::string_view>(nameSegments.begin(), nameSegments.begin() + depth);
}

std::string_view Program::IdentifierObject::name() const {
  return nameSegments.back();
}

void Program::IdentifierObject::assimilate(IdentifierObject& identifier) {
  addressIndex = identifier.addressIndex;
  nameSegments = identifier.nameSegments;
  addressIndex = identifier.addressIndex;
  parent       = identifier.parent;
  children.merge(identifier.children);
}

std::unordered_set<std::string> Program::RelaxorDefinition::getLabelsReferencedFromConditionsInAllOptions() {
  std::unordered_set<std::string> mentionedLabels;
  for (const auto& option : options) {
    mentionedLabels.insert(option.conditionExpr->mentionedLabels.begin(), option.conditionExpr->mentionedLabels.end());
  }
  return std::move(mentionedLabels);
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
      debugPrintIdentifier(*obj, 3);
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
            << " Address: " << obj->addressIndex
            //<< " Resolved: " << obj->addressResolved
            << "\n";

  if (auto lbl = dynamic_cast<const LabelObject*>(obj)) {
    indent(indentLevel);
    std::cout << "Type: LabelObject\n";

    for (const auto& [childName, child] : lbl->children) {
      indent(indentLevel + 1);
      std::cout << "Child Label: " << childName << "\n";
      debugPrintIdentifier(*child, indentLevel + 2);
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