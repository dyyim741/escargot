/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#ifndef IdentifierNode_h
#define IdentifierNode_h

#include "ExpressionNode.h"
#include "Node.h"

#include "runtime/Context.h"

namespace Escargot {

// interface Identifier <: Node, Expression, Pattern {
class IdentifierNode : public Node {
public:
    IdentifierNode()
        : Node()
        , m_name()
    {
    }

    explicit IdentifierNode(const AtomicString& name)
        : Node()
        , m_name(name)
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::Identifier; }
    AtomicString name()
    {
        return m_name;
    }

    void addLexicalVariableErrorsIfNeeds(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, InterpretedCodeBlock::IndexedIdentifierInfo info, bool isLexicallyDeclaredBindingInitialization, bool isVariableChainging = false)
    {
        // <temporal dead zone error>
        // only stack allocated lexical variables needs check (heap variables are checked on runtime)
        if (!isLexicallyDeclaredBindingInitialization && info.m_isResultSaved && info.m_isStackAllocated && info.m_type == InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared) {
            bool finded = false;
            auto iter = context->m_lexicallyDeclaredNames->begin();
            while (iter != context->m_lexicallyDeclaredNames->end()) {
                if (iter->first == info.m_blockIndex && iter->second == m_name) {
                    finded = true;
                    break;
                }
                iter++;
            }

            if (!finded) {
                codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::ReferenceError, errorMessage_IsNotInitialized, m_name), context, this);
            }
        }

        // <const variable check>
        // every indexed variables are checked on bytecode generation time
        if (!isLexicallyDeclaredBindingInitialization && isVariableChainging && info.m_isResultSaved && !info.m_isMutable && info.m_type == InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared) {
            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, errorMessage_AssignmentToConstantVariable, m_name), context, this);
        }
    }

    bool isPointsArgumentsObject(ByteCodeGenerateContext* context)
    {
        if (context->m_codeBlock->context()->staticStrings().arguments == m_name && context->m_codeBlock->usesArgumentsObject() && !context->m_codeBlock->isArrowFunctionExpression()) {
            return true;
        }
        return false;
    }

    bool mayNeedsResolveAddress(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context)
    {
        if (context->m_codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
            if (!info.m_isResultSaved) {
                if (codeBlock->m_codeBlock->asInterpretedCodeBlock()->hasAncestorUsesNonIndexedVariableStorage()) {
                    if (context->m_isWithScope) {
                        return true;
                    }
                }

                if (context->m_isLeftBindingAffectedByRightExpression) {
                    return true;
                }
            }
        } else {
            if (context->m_isWithScope) {
                return true;
            }
            if (context->m_isLeftBindingAffectedByRightExpression) {
                return true;
            }
        }
        return false;
    }

    virtual void generateStoreByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex srcRegister, bool needToReferenceSelf) override
    {
        bool isLexicallyDeclaredBindingInitialization = context->m_isLexicallyDeclaredBindingInitialization;
        bool isFunctionDeclarationBindingInitialization = context->m_isFunctionDeclarationBindingInitialization;
        context->m_isLexicallyDeclaredBindingInitialization = false;
        context->m_isFunctionDeclarationBindingInitialization = false;

        if (isLexicallyDeclaredBindingInitialization) {
            context->addLexicallyDeclaredNames(m_name);
        }

        if (isPointsArgumentsObject(context)) {
            codeBlock->pushCode(EnsureArgumentsObject(ByteCodeLOC(m_loc.index)), context, this);
        }

        if (context->m_codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
            addLexicalVariableErrorsIfNeeds(codeBlock, context, info, isLexicallyDeclaredBindingInitialization, true);

            if (!info.m_isResultSaved) {
                if (codeBlock->m_codeBlock->asInterpretedCodeBlock()->hasAncestorUsesNonIndexedVariableStorage()) {
                    size_t addressRegisterIndex = SIZE_MAX;
                    if (mayNeedsResolveAddress(codeBlock, context) && !needToReferenceSelf) {
                        addressRegisterIndex = context->getLastRegisterIndex();
                        context->giveUpRegister();
                    }
                    if (isLexicallyDeclaredBindingInitialization || isFunctionDeclarationBindingInitialization) {
                        codeBlock->pushCode(InitializeByName(ByteCodeLOC(m_loc.index), srcRegister, m_name, isLexicallyDeclaredBindingInitialization), context, this);
                    } else {
                        if (addressRegisterIndex != SIZE_MAX) {
                            codeBlock->pushCode(StoreByNameWithAddress(ByteCodeLOC(m_loc.index), addressRegisterIndex, srcRegister, m_name), context, this);
                        } else {
                            codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                        }
                    }
                } else {
                    if (isLexicallyDeclaredBindingInitialization) {
                        codeBlock->pushCode(InitializeGlobalVariable(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                    } else {
                        codeBlock->pushCode(SetGlobalVariable(ByteCodeLOC(m_loc.index), srcRegister, codeBlock->m_codeBlock->context()->ensureGlobalVariableAccessCacheSlot(m_name)), context, this);
                    }
                }
            } else {
                if (info.m_type != InterpretedCodeBlock::IndexedIdentifierInfo::LexicallyDeclared) {
                    if (!info.m_isMutable) {
                        if (codeBlock->m_codeBlock->isStrict())
                            codeBlock->pushCode(ThrowStaticErrorOperation(ByteCodeLOC(m_loc.index), ErrorObject::TypeError, errorMessage_AssignmentToConstantVariable, m_name), context, this);
                        return;
                    }
                }

                if (info.m_isStackAllocated) {
                    if (srcRegister != REGULAR_REGISTER_LIMIT + info.m_index) {
                        codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), srcRegister, REGULAR_REGISTER_LIMIT + info.m_index), context, this);
                    }
                } else {
                    if (info.m_isGlobalLexicalVariable) {
                        if (isLexicallyDeclaredBindingInitialization) {
                            codeBlock->pushCode(InitializeGlobalVariable(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                        } else {
                            codeBlock->pushCode(SetGlobalVariable(ByteCodeLOC(m_loc.index), srcRegister, codeBlock->m_codeBlock->context()->ensureGlobalVariableAccessCacheSlot(m_name)), context, this);
                        }
                    } else {
                        if (isLexicallyDeclaredBindingInitialization) {
                            ASSERT(info.m_upperIndex == 0);
                            codeBlock->pushCode(InitializeByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_index), context, this);
                        } else {
                            codeBlock->pushCode(StoreByHeapIndex(ByteCodeLOC(m_loc.index), srcRegister, info.m_upperIndex, info.m_index), context, this);
                        }
                    }
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->asInterpretedCodeBlock()->canAllocateEnvironmentOnStack());
            size_t addressRegisterIndex = SIZE_MAX;
            if (mayNeedsResolveAddress(codeBlock, context) && !needToReferenceSelf) {
                addressRegisterIndex = context->getLastRegisterIndex();
                context->giveUpRegister();
            }
            if (isLexicallyDeclaredBindingInitialization || isFunctionDeclarationBindingInitialization) {
                codeBlock->pushCode(InitializeByName(ByteCodeLOC(m_loc.index), srcRegister, m_name, isLexicallyDeclaredBindingInitialization), context, this);
            } else {
                if (addressRegisterIndex != SIZE_MAX) {
                    codeBlock->pushCode(StoreByNameWithAddress(ByteCodeLOC(m_loc.index), addressRegisterIndex, srcRegister, m_name), context, this);
                } else {
                    codeBlock->pushCode(StoreByName(ByteCodeLOC(m_loc.index), srcRegister, m_name), context, this);
                }
            }
        }
    }

    virtual void generateExpressionByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context, ByteCodeRegisterIndex dstRegister) override
    {
        if (isPointsArgumentsObject(context)) {
            codeBlock->pushCode(EnsureArgumentsObject(ByteCodeLOC(m_loc.index)), context, this);
        }

        if (context->m_codeBlock->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
            addLexicalVariableErrorsIfNeeds(codeBlock, context, info, false);

            if (!info.m_isResultSaved) {
                if (codeBlock->m_codeBlock->asInterpretedCodeBlock()->hasAncestorUsesNonIndexedVariableStorage()) {
                    codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), dstRegister, m_name), context, this);
                } else {
                    codeBlock->pushCode(GetGlobalVariable(ByteCodeLOC(m_loc.index), dstRegister, codeBlock->m_codeBlock->context()->ensureGlobalVariableAccessCacheSlot(m_name)), context, this);
                }
            } else {
                if (info.m_isStackAllocated) {
                    if (context->m_canSkipCopyToRegister) {
                        if (dstRegister != (REGULAR_REGISTER_LIMIT + info.m_index)) {
                            codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + info.m_index, dstRegister), context, this);
                        }
                    } else
                        codeBlock->pushCode(Move(ByteCodeLOC(m_loc.index), REGULAR_REGISTER_LIMIT + info.m_index, dstRegister), context, this);
                } else {
                    if (info.m_isGlobalLexicalVariable) {
                        codeBlock->pushCode(GetGlobalVariable(ByteCodeLOC(m_loc.index), dstRegister, codeBlock->m_codeBlock->context()->ensureGlobalVariableAccessCacheSlot(m_name)), context, this);
                    } else {
                        codeBlock->pushCode(LoadByHeapIndex(ByteCodeLOC(m_loc.index), dstRegister, info.m_upperIndex, info.m_index), context, this);
                    }
                }
            }
        } else {
            ASSERT(!context->m_codeBlock->asInterpretedCodeBlock()->canAllocateEnvironmentOnStack());
            codeBlock->pushCode(LoadByName(ByteCodeLOC(m_loc.index), dstRegister, m_name), context, this);
        }
    }

    virtual void generateReferenceResolvedAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        generateExpressionByteCode(codeBlock, context, getRegister(codeBlock, context));
    }

    virtual void generateResolveAddressByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        if (mayNeedsResolveAddress(codeBlock, context)) {
            auto r = context->getRegister();
            codeBlock->pushCode(ResolveNameAddress(ByteCodeLOC(m_loc.index), m_name, r), context, this);
        }
    }

    std::tuple<bool, ByteCodeRegisterIndex, InterpretedCodeBlock::IndexedIdentifierInfo> isAllocatedOnStack(ByteCodeGenerateContext* context)
    {
        if (isPointsArgumentsObject(context)) {
            return std::make_tuple(false, REGISTER_LIMIT, InterpretedCodeBlock::IndexedIdentifierInfo());
        }

        if (context->m_codeBlock->asInterpretedCodeBlock()->canUseIndexedVariableStorage()) {
            InterpretedCodeBlock::IndexedIdentifierInfo info = context->m_codeBlock->asInterpretedCodeBlock()->indexedIdentifierInfo(m_name, context->m_lexicalBlockIndex);
            if (!info.m_isResultSaved) {
                return std::make_tuple(false, REGISTER_LIMIT, info);
            } else {
                if (info.m_isStackAllocated && info.m_isMutable) {
                    if (context->m_canSkipCopyToRegister)
                        return std::make_tuple(true, REGULAR_REGISTER_LIMIT + info.m_index, info);
                    else
                        return std::make_tuple(false, REGISTER_LIMIT, info);
                } else {
                    return std::make_tuple(false, REGISTER_LIMIT, info);
                }
            }
        } else {
            return std::make_tuple(false, REGISTER_LIMIT, InterpretedCodeBlock::IndexedIdentifierInfo());
        }
    }

    virtual ByteCodeRegisterIndex getRegister(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        auto ret = isAllocatedOnStack(context);
        if (std::get<0>(ret)) {
            context->pushRegister(std::get<1>(ret));
            return std::get<1>(ret);
        } else {
            return context->getRegister();
        }
    }


    virtual void iterateChildrenIdentifier(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        fn(m_name, false);
    }

    virtual void iterateChildrenIdentifierAssigmentCase(const std::function<void(AtomicString name, bool isAssignment)>& fn) override
    {
        fn(m_name, true);
    }

    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        generateExpressionByteCode(codeBlock, context, context->getRegister());
        context->giveUpRegister();
    }

private:
    AtomicString m_name;
};
}

#endif
