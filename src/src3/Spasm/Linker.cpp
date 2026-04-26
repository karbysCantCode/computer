#include "Spasm/Linker.hpp"

namespace Spasm {

Linker::LinkedResult Linker::run(
  size_t entrySymbolSetupByteLength,
  Program &program,
  Debug::FullLogger* logger
) {
  p_logger = logger;
  LinkedResult linked;

  ExpressionsByLabelHelper expressionHelper;

  // JUMP TO ENTRY SYM BYTES SETUP
  linked.maxAddress = entrySymbolSetupByteLength;

  //std::stack<Program::TranslationUnit*> definitionResolvingTranslationUnitStack = m_independentTranslationUnits;
  // ok so like you kinda need to go through everything to do definitions not just the indeepnendtnt?
  
  for (auto& translationUnitElement : program.m_translationUnits) {
    auto& translationUnit = *translationUnitElement.second.get();
    //address holder overhead
    linked.addressHolder.resize(
      linked.addressHolder.size() 
      + translationUnit.m_statementVector.size() 
      + translationUnit.m_definitionVector.size()
    );
    std::cout << translationUnit.m_statementVector.size() << '\n';
    std::cout << translationUnit.m_definitionVector.size() << '\n';
    linked.statementHolder.resize(linked.addressHolder.size());
  
    linkDefinitionSymbols(translationUnit, linked);
  }
  
  // while (!definitionResolvingTranslationUnitStack.empty()) {
  //   auto& translationUnit = *definitionResolvingTranslationUnitStack.top();
  //   definitionResolvingTranslationUnitStack.pop();

  // }

  linked.programDataStartAddress = linked.maxAddress;

  while (areUnlinkedIndependentTranslationUnits()) {
    auto& translationUnit = *consumeIndependentTranslationUnitFromStack();

    linkTU(translationUnit, linked, expressionHelper);

    for (auto dependantUnit : m_dependantTranslationUnitMap[translationUnit.m_sourcePath]) {
      dependantUnit->m_dependenciesResolved++;
      if (dependantUnit->m_dependenciesResolved == dependantUnit->m_includedFiles.size()) {
        addIndependentTranslationUnits(dependantUnit);
      }
    }
  }

  //lets go relaxors!
  std::unordered_set<Program::RelaxorSymbol*> relaxorQueueSet;
  std::deque<Program::RelaxorSymbol*> relaxorQueue;
  relaxorQueue.insert(relaxorQueue.end(), program.m_relaxorPointerVector.begin(), program.m_relaxorPointerVector.end());
  relaxorQueueSet.insert(program.m_relaxorPointerVector.begin(), program.m_relaxorPointerVector.end());

  while (!relaxorQueue.empty()) {
    const auto relaxorPtr = relaxorQueue.front();
    auto& thisRelaxor = *relaxorPtr;
    relaxorQueue.pop_front();
    relaxorQueueSet.erase(relaxorPtr);

    int original = thisRelaxor.optionIndex;
    thisRelaxor.optionIndex = 0;
    while (
      thisRelaxor.relaxor.options.size() < thisRelaxor.optionIndex &&
      thisRelaxor.relaxor.options[thisRelaxor.optionIndex].conditionExpr->value == 0) {
      thisRelaxor.optionIndex++;
    }

    const size_t nextAddressIndex = thisRelaxor.addressIndex + 1;

    if (thisRelaxor.relaxor.options.size() >= thisRelaxor.optionIndex) {
      thisRelaxor.optionIndex = -1;
      continue;
    } else if (thisRelaxor.optionIndex == original) {
      continue;
    } else if (linked.addressHolder.size() <= nextAddressIndex) {
      continue;
    }

    const int sizeChange = thisRelaxor.relaxor.options[original].sumByteSizeOfOption() - thisRelaxor.relaxor.options[thisRelaxor.optionIndex].sumByteSizeOfOption();

    //did change, updating required
    auto it = std::lower_bound(linked.addressHolder.begin(), linked.addressHolder.end(), linked.addressHolder[nextAddressIndex]);

    if (it != linked.addressHolder.end()) {
      size_t index = it - linked.addressHolder.begin();

      // get all labels beyond the address of relaxor, 
      auto labelIt = std::lower_bound(
        linked.addressLabelHolder.begin(), 
        linked.addressLabelHolder.end(), 
        linked.addressHolder[nextAddressIndex],
        [](const addressLabelPointerPair& item, int value) {
          return *item.addressPtr < value;
        }
      );

      // mass increment
      if (sizeChange != 0) {
        for (size_t i = index; i < linked.addressHolder.size(); i++) {
          linked.addressHolder[i] -= sizeChange;
        }
      }

      std::vector<std::string> labelNames;
      labelNames.resize(labelIt - linked.addressLabelHolder.begin());

      size_t labelIndex = 0;
      for (auto it2 = labelIt; it2 != linked.addressLabelHolder.end(); ++it2) {
        labelNames[labelIndex++] = it2->labelPtr->labelObject->fullName();
      }
      //then evaluate dependant exprs
      auto exprs = expressionHelper.getExpressionsReferencingTheseLabels(labelNames);
      for (auto expr : exprs) {
        expr->evaluate(m_globalIdentifierMap, linked.addressHolder, false);
      }
      // and add relaxors dependant to changed labels back to the start of the q
      auto relaxors = expressionHelper.getRelaxorsReferencingTheseLabels(labelNames);
      for (const auto relaxor : relaxors) {
        if (relaxorQueueSet.find(relaxor) == relaxorQueueSet.end()) {
          relaxorQueueSet.emplace(relaxor);
          relaxorQueue.push_front(relaxor);
        }
      }
    }

  }
  return linked;
}

void Linker::linkDefinitionSymbols(
  Program::TranslationUnit& translationUnit, 
  LinkedResult& linkedResult
) {
  placeDefinitionSymbols(linkedResult, translationUnit);
}
void Linker::linkTU(
  Program::TranslationUnit& translationUnit, 
  LinkedResult& linkedResult,
  ExpressionsByLabelHelper& labelHelper
) {
  //resolve addresses

  //already done?
  //placeDefinitionSymbols(linkedResult, translationUnit);
  placeOtherSymbols(linkedResult, translationUnit, labelHelper);
  //check all identifiers have definitions
  checkForUndefinedIdentifiers();
  //resolve expressions
  resolveExpressions(translationUnit, linkedResult, labelHelper);

  //all linked!
  linkedResult.translationUnitQueue.push(&translationUnit);
}

void Linker::createTemporaryLabelObjectsToConstructSymbolFamilyTree(Program::IdentifierObject* identifierObject) {
  size_t segCount = 0;
  Program::LabelObject* parent = nullptr;
  auto& identifierSourceLoc = identifierObject->isIdentifier() 
    ? static_cast<Program::DataObject*>(identifierObject)->symbolObject->location 
    : static_cast<Program::LabelObject*>(identifierObject)->symbolObject->location;
  while (segCount < identifierObject->nameSegments.size() - 1) {
    segCount++;
    const auto currentName = identifierObject->getNDepthName(segCount);
    const auto& it = m_fullNameCollatedIdentifierMap.find(currentName);
    if (!iteratorAlreadyInFullNameMap(it)) {
      auto ref = std::make_unique<Program::LabelObject>(identifierObject->sourcePath, parent);
      if (!parent) {
        m_globalIdentifierMap.emplace(currentName, ref.get());
      }
      parent = ref.get();
      ref->nameSegments = identifierObject->getNDepthNameVector(segCount);
      m_fullNameCollatedIdentifierMap.emplace(currentName, ref.get());
      m_temporaryIdentifierOwner.emplace(currentName, std::move(ref));
    } else {
      if (auto newParent = dynamic_cast<Program::LabelObject*>(it->second)) {
        parent = newParent;
      } else {
        logError(identifierSourceLoc, std::format("Identifier \"{}\" is a data declaration and cannot have children.", it->second->fullName()));
      }
    }
  }
}

void Linker::placeDefinitionSymbols(LinkedResult& linkedResult, Program::TranslationUnit& translationUnit) {
  for (auto& definitionSymbol : translationUnit.m_definitionVector) {
    assert(definitionSymbol->getKind() == Program::StatementSymbol::Kind::DEFINITION);
  
    auto& definitionObject = *definitionSymbol->dataObject;
    auto& definitionObjectPtr = definitionSymbol->dataObject;
    const auto it = m_fullNameCollatedIdentifierMap.find(definitionObject.fullName());

    if (iteratorAlreadyInFullNameMap(it)) {
      if (auto ptr = dynamic_cast<Spasm::Program::LabelObject*>(it->second)) {
        logError(definitionSymbol->location, std::format("Identifier is already defined at \"{}\"", ptr->symbolObject->location.toString()));
      } else if (auto ptr = dynamic_cast<Spasm::Program::DataObject*>(it->second)) {
        logError(definitionSymbol->location, std::format("Identifier is already defined at \"{}\"", ptr->symbolObject->location.toString()));
      } else {
        logError(definitionSymbol->location, "Identifier is already defined with unknown type so no location is provided.");
      }

      continue;
    }
    //if it doesnt already exist ie, no fullname found
    if (!definitionObject.parent ) {
      if (!nameAlreadyInGlobalMap(definitionObject.name())) {
        m_globalIdentifierMap.emplace(definitionObject.name(), &definitionObject); 
      } else {
        logError(definitionSymbol->location, "somehow there is a parent-less identifier object in the linkers map, without an entry in the fullname, in the globalmap, this just shouldnt happen...");
        continue;
      }
    } else {
      const auto parentIt = m_fullNameCollatedIdentifierMap.find(definitionObject.parent->fullName());
      if (iteratorAlreadyInFullNameMap(parentIt)) {
        if (parentIt->second->isIdentifier()) {
          //illegal bc self is definiton and parent is def
          logError(definitionSymbol->location, std::format("Parent of data object \"{}\" is defined as a data object at \"{}\" (data objects cannot be parents.)", definitionObject.fullName(), static_cast<Program::DataObject*>(it->second)->symbolObject->location.toString()));
          continue;
        } else {
          const auto& defSharedPtr = parentIt->second->children.find(definitionObject.name())->second;
          parentIt->second->children.emplace(definitionObject.name(), defSharedPtr);
        }
      } else {
        createTemporaryLabelObjectsToConstructSymbolFamilyTree(definitionObjectPtr);
      }
    }

    m_fullNameCollatedIdentifierMap.emplace(definitionObject.fullName(), definitionObjectPtr);
    definitionSymbol->addressIndex = linkedResult.nAddressesEntered++; 
    linkedResult.addressHolder[definitionSymbol->addressIndex] = linkedResult.maxAddress;
    linkedResult.statementHolder[definitionSymbol->addressIndex] = definitionSymbol.get();
    // cant reallty do this because it gets reallocated every TU
    //definitionSymbol->address = linkedResult.addressHolder.data() + definitionSymbol->addressIndex * sizeof(size_t);
    // definitionSymbol->addressResolved = true;
    
    linkedResult.maxAddress += 
      definitionObjectPtr->elementCount * 
      definitionObjectPtr->elementSize;
    linkedResult.maxAddress += linkedResult.maxAddress % 2 == 1; //pad for 2byte alignment of instructions
  }
}

void Linker::placeOtherSymbols(LinkedResult& linkedResult, Program::TranslationUnit& translationUnit, ExpressionsByLabelHelper& labelHelper) {
  for (auto& statementSymbol : translationUnit.m_statementVector) {
    switch (statementSymbol.get()->getKind())
    {
    using KIND = Program::StatementSymbol::Kind;
    case KIND::LABEL:
    {
      auto& labelSymbol = *static_cast<Program::LabelSymbol*>(statementSymbol.get());
      auto& labelObject = *labelSymbol.labelObject;
      auto& labelObjectPtr = labelSymbol.labelObject;
      if (!labelObjectPtr) return; //should already habve an error?


      
      const auto it = m_fullNameCollatedIdentifierMap.find(labelObject.fullName());

      if (iteratorAlreadyInFullNameMap(it)) {
        if (auto ptr = dynamic_cast<Spasm::Program::LabelObject*>(it->second)) {
          if (ptr->symbolObject) {
            logError(labelSymbol.location, std::format("Identifier is already defined at \"{}\"", ptr->symbolObject->location.toString()));
          } else {
            //is a temporary, is okay!! just like switch urself into it
            labelObject.children.merge(ptr->children);
            //labelObject.parent = ptr->parent; // not sure???
            m_fullNameCollatedIdentifierMap[labelObject.fullName()] = labelObjectPtr;
            if (ptr->parent && labelObjectPtr->parent) {
              ptr->parent->children[ptr->fullName()] = std::move(labelObject.parent->children[labelObject.fullName()]);
            }
            if (labelObject.nameSegments.size() == 1) {
              m_globalIdentifierMap[labelObject.fullName()] = labelObjectPtr;
            }
            m_temporaryIdentifierOwner.erase(ptr->fullName());
          }
        } else if (auto ptr = dynamic_cast<Spasm::Program::DataObject*>(it->second)) {
          logError(labelSymbol.location, std::format("Identifier is already defined at \"{}\"", ptr->symbolObject->location.toString()));
        } else {
          logError(labelSymbol.location, "Identifier is already defined with unknown type so no location is provided.");
        }

        continue;
      }
      //if it doesnt already exist ie, no fullname found
      if (!labelObject.parent) {
        if (!nameAlreadyInGlobalMap(labelObject.fullName())) {
          m_globalIdentifierMap.emplace(labelObject.name(), &labelObject); 
        } else {
          logError(labelSymbol.location, "somehow there is a parent-less identifier object in the linkers map, without an entry in the fullname, in the globalmap, this just shouldnt happen...");
          continue;
        }
      } else {
        const auto parentIt = m_fullNameCollatedIdentifierMap.find(labelObject.parent->fullName());
        if (iteratorAlreadyInFullNameMap(parentIt)) {
          const auto& lblSharedPtr = parentIt->second->children.find(labelObject.name())->second;
          parentIt->second->children.emplace(labelObject.name(), lblSharedPtr);
        }
      }

      m_fullNameCollatedIdentifierMap.emplace(labelObject.fullName(), labelObjectPtr);
      
      labelSymbol.addressIndex = linkedResult.nAddressesEntered; 
      linkedResult.nAddressesEntered++;
      linkedResult.addressHolder[labelSymbol.addressIndex] = linkedResult.maxAddress;
      linkedResult.statementHolder[labelSymbol.addressIndex] = &labelSymbol;
      // cant reallty do this because it gets reallocated every TU
      //labelSymbol.address = linkedResult.addressHolder.data() + labelSymbol.addressIndex * sizeof(size_t);

      //labelSymbol.addressResolved = true;
      break;
    }
    case KIND::DEFINITION:
      assert(false);
      break;
    case KIND::INSTRUCTION:
    {
      auto& instructionSymbol = *static_cast<Program::InstructionSymbol*>(statementSymbol.get());
      instructionSymbol.addressIndex = linkedResult.nAddressesEntered++; 
      linkedResult.addressHolder[instructionSymbol.addressIndex] = linkedResult.maxAddress;
      linkedResult.statementHolder[instructionSymbol.addressIndex] = &instructionSymbol;
      // cant reallty do this because it gets reallocated every TU
      //instructionSymbol.address = linkedResult.addressHolder.data() + instructionSymbol.addressIndex * sizeof(size_t);
      linkedResult.maxAddress += instructionSymbol.instruction.m_byteLength;
      instructionSymbol.byteSize = instructionSymbol.instruction.m_byteLength;
      linkedResult.maxAddress += linkedResult.maxAddress % 2 == 1; //pad for 2byte alignment of instructions
      break;
    }
    case KIND::RELAXOR:
    {
      auto& relaxorSymbol = *static_cast<Program::RelaxorSymbol*>(statementSymbol.get());
      labelHelper.registerRelaxor(&relaxorSymbol, relaxorSymbol.relaxor.getLabelsReferencedFromConditionsInAllOptions()); //expression mentioned set, add KEEPING that to the expr class (base)
      
      relaxorSymbol.addressIndex = linkedResult.nAddressesEntered++; 
      linkedResult.addressHolder[relaxorSymbol.addressIndex] = linkedResult.maxAddress;
      linkedResult.statementHolder[relaxorSymbol.addressIndex] = &relaxorSymbol;

      // relaxorSymbol.relaxor.worstCaseSize = 0;
      // for (auto& option : relaxorSymbol.relaxor.options) {
      //   relaxorSymbol.relaxor.worstCaseSize = std::max(option.sumByteSizeOfOption(), largest);
      // }
      // cant reallty do this because it gets reallocated every TU
      //relaxorSymbol.address = linkedResult.addressHolder.data() + relaxorSymbol.addressIndex * sizeof(size_t);
      linkedResult.maxAddress += relaxorSymbol.relaxor.worstCaseSize;
      linkedResult.maxAddress += linkedResult.maxAddress % 2 == 1; //pad for 2byte alignment of instructions
      break;
    }
    default:
      assert(false);
      //just shouldnt happen and i raely cba to implement this error
      break;
    }
  }
}

void Linker::checkForUndefinedIdentifiers() {
  for (const auto& it : m_fullNameCollatedIdentifierMap) {
    if (it.second->isIdentifier()) {
      if (!static_cast<Program::DataObject*>(it.second)->symbolObject) {
        logError(SourceLocation("LINKER", 0,0), std::format("Data identifier \"{}\" is uninitialised.", it.first));
      }
    } else if (it.second->isLabel()) {
      if (!static_cast<Program::LabelObject*>(it.second)->symbolObject) {
        logError(SourceLocation("LINKER", 0,0), std::format("Label identifier \"{}\" is uninitialised.", it.first));
      }
    }
  }
}

void Linker::resolveExpressions(Program::TranslationUnit& translationUnit, LinkedResult& linked, ExpressionsByLabelHelper& labelHelper) {
  while (!translationUnit.m_unresolvedExpressions.empty()) {
    auto& expr = *translationUnit.m_unresolvedExpressions.front();
    translationUnit.m_unresolvedExpressions.pop();

    // std::cout << expr << '\n';
    auto eval = expr.evaluate(m_globalIdentifierMap, linked.addressHolder, true);

    labelHelper.registerExpression(&expr, eval.mentionedLabels);
    expr.mentionedLabels = std::move(eval.mentionedLabels);

    if (!eval.error.empty()) {
      logError(expr.location, eval.error);
      continue;
    }
  }
}

void Linker::ExpressionsByLabelHelper::registerExpression(Program::Expr* expr, const std::unordered_set<std::string>& mentionedLabels) {
  for (const auto& fullName : mentionedLabels) {
    p_labelByExpressionMap[fullName].insert(expr);
  }
}
std::unordered_set<Program::Expr*> Linker::ExpressionsByLabelHelper::getExpressionsReferencingTheseLabels(const std::vector<std::string>& fullNameList) const {
  std::unordered_set<Program::Expr*> finalSet;

  for (const auto& fullName : fullNameList) {
    const auto it = p_labelByExpressionMap.find(fullName);
    if (it == p_labelByExpressionMap.end()) continue;

    finalSet.insert(it->second.begin(), it->second.end());
  }

  return finalSet;
}


void Linker::ExpressionsByLabelHelper::registerRelaxor(Program::RelaxorSymbol* expr, const std::unordered_set<std::string>& mentionedLabels) {
  for (const auto& fullName : mentionedLabels) {
    p_relaxorByExpressionMap[fullName].insert(expr);
  }
}
std::set<Program::RelaxorSymbol*> Linker::ExpressionsByLabelHelper::getRelaxorsReferencingTheseLabels(const std::vector<std::string>& fullNameList) const {
  std::set<Program::RelaxorSymbol*> finalSet;

  for (const auto& fullName : fullNameList) {
    const auto it = p_relaxorByExpressionMap.find(fullName);
    if (it == p_relaxorByExpressionMap.end()) continue;

    finalSet.insert(it->second.begin(), it->second.end());
  }

  return finalSet;
}

}