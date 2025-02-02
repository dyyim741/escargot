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

#ifndef VariableDeclaratorNode_h
#define VariableDeclaratorNode_h

#include "ExpressionNode.h"
#include "IdentifierNode.h"
#include "Node.h"

#include "parser/Lexer.h"

namespace Escargot {

class VariableDeclaratorNode : public Node {
public:
    VariableDeclaratorNode(EscargotLexer::KeywordKind kind, Node* id, Node* init = nullptr)
        : Node()
        , m_kind(kind)
        , m_id(id)
        , m_init(init)
    {
    }

    virtual ~VariableDeclaratorNode()
    {
    }

    virtual ASTNodeType type() override { return ASTNodeType::VariableDeclarator; }
    Node* id() { return m_id; }
    Node* init() { return m_init; }
    virtual void generateStatementByteCode(ByteCodeBlock* codeBlock, ByteCodeGenerateContext* context) override
    {
        bool addFakeUndefinedLiteralNode = false;
        if (m_kind != EscargotLexer::KeywordKind::VarKeyword && !m_init && !context->m_forInOfVarBinding) {
            addFakeUndefinedLiteralNode = true;
            m_init = new (alloca(sizeof(LiteralNode))) LiteralNode(Value());
        }

        if (m_init) {
            context->getRegister();
            context->m_isLexicallyDeclaredBindingInitialization = m_kind != EscargotLexer::KeywordKind::VarKeyword;
            if (m_id->isIdentifier() && !m_id->asIdentifier()->name().string()->equals("arguments")) {
                // check canUseIndexedVariableStorage for give right value to generateStoreByteCode(isInit..) with eval
                AssignmentExpressionSimpleNode* assign = new (alloca(sizeof(AssignmentExpressionSimpleNode))) AssignmentExpressionSimpleNode(m_id, m_init);
                assign->m_loc = m_loc;
                assign->generateResultNotRequiredExpressionByteCode(codeBlock, context);
            } else {
                auto r = m_init->getRegister(codeBlock, context);
                m_init->generateExpressionByteCode(codeBlock, context, r);
                m_id->generateStoreByteCode(codeBlock, context, r, true);
                context->giveUpRegister();
            }
            ASSERT(!context->m_isLexicallyDeclaredBindingInitialization);
            context->giveUpRegister();
        }

        if (addFakeUndefinedLiteralNode) {
            m_init = nullptr;
        }
    }

private:
    EscargotLexer::KeywordKind m_kind;
    Node* m_id; // id: Pattern;
    Node* m_init; // init: Expression | null;
};
}

#endif
