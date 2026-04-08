#include "Spasm/Linker.hpp"

namespace Spasm {

Linker::LinkedResult Linker::run(
  size_t entrySymbolSetupByteLength,
  Debug::FullLogger* logger
) {
  p_logger = logger;
  LinkedResult linked;

  // JUMP TO ENTRY SYM BYTES SETUP
  linked.maxAddress = entrySymbolSetupByteLength;

  std::stack<Program::TranslationUnit*> definitionResolvingTranslationUnitStack = m_independentTranslationUnits;

  while (!definitionResolvingTranslationUnitStack.empty()) {
    auto& translationUnit = *definitionResolvingTranslationUnitStack.top();
    definitionResolvingTranslationUnitStack.pop();

    linkDefinitionSymbols(translationUnit, linked);
  }

  linked.programDataStartAddress = linked.maxAddress;

  while (areUnlinkedIndependentTranslationUnits()) {
    auto& translationUnit = *consumeIndependentTranslationUnitFromStack();

    linkTU(translationUnit, linked);

    for (auto dependantUnit : m_dependantTranslationUnitMap[translationUnit.m_sourcePath]) {
      dependantUnit->m_dependenciesResolved++;
      if (dependantUnit->m_dependenciesResolved == dependantUnit->m_includedFiles.size()) {
        addIndependentTranslationUnits(dependantUnit);
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
  LinkedResult& linkedResult
) {
  //resolve addresses
  placeDefinitionSymbols(linkedResult, translationUnit);
  placeOtherSymbols(linkedResult, translationUnit);
  //check all identifiers have definitions
  checkForUndefinedIdentifiers();
  //resolve expressions
  resolveExpressions(translationUnit);

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
    
    definitionSymbol->address = linkedResult.maxAddress;
    // definitionSymbol->addressResolved = true;
    
    linkedResult.maxAddress += 
      definitionObjectPtr->elementCount * 
      definitionObjectPtr->elementSize;
    linkedResult.maxAddress += linkedResult.maxAddress % 2 == 1; //pad for 2byte alignment of instructions
  }
}

void Linker::placeOtherSymbols(LinkedResult& linkedResult, Program::TranslationUnit& translationUnit) {
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
      
      labelSymbol.address = linkedResult.maxAddress;
      //labelSymbol.addressResolved = true;
      break;
    }
    case KIND::DEFINITION:
      assert(false);
      break;
    case KIND::INSTRUCTION:
    {
      auto& instructionSymbol = *static_cast<Program::InstructionSymbol*>(statementSymbol.get());
      instructionSymbol.address = linkedResult.maxAddress;
      linkedResult.maxAddress += instructionSymbol.instruction.m_byteLength;
      linkedResult.maxAddress += linkedResult.maxAddress % 2 == 1; //pad for 2byte alignment of instructions
      break;
    }
    case KIND::RELAXOR:
    {
      auto& relaxorSymbol = *static_cast<Program::RelaxorSymbol*>(statementSymbol.get());
      relaxorSymbol.address = linkedResult.maxAddress;
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

void Linker::resolveExpressions(Program::TranslationUnit& translationUnit) {
  while (!translationUnit.m_unresolvedExpressions.empty()) {
    auto& expr = translationUnit.m_unresolvedExpressions.front();
    translationUnit.m_unresolvedExpressions.pop();

    std::cout << expr << '\n';
    auto eval = expr->evaluate(m_globalIdentifierMap);

    if (!eval.second.empty()) {
      logError(expr->location, eval.second);
      continue;
    }
  }
}
}