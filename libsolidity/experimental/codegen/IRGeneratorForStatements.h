/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include <libsolidity/experimental/codegen/IRGenerationContext.h>
#include <libsolidity/experimental/codegen/IRVariable.h>
#include <libsolidity/experimental/ast/TypeSystemHelper.h>
#include <libsolidity/experimental/analysis/TypeInference.h>

#include <libsolidity/ast/ASTVisitor.h>

#include <functional>
#include <sstream>

namespace solidity::frontend::experimental
{
class Analysis;

class IRGeneratorForStatements: public ASTConstVisitor
{
public:
	IRGeneratorForStatements(IRGenerationContext& _context): m_context(_context) {}

	std::string generate(ASTNode const& _node);

	static std::size_t stackSize(IRGenerationContext const& _context, Type _type)
	{
		TypeSystemHelpers helper{_context.analysis.typeSystem()};
		_type = _context.env->resolve(_type);
		solAssert(std::holds_alternative<TypeConstant>(_type), "No monomorphized type.");

		// type -> # stack slots
		// unit, itself -> 0
		// void, literals(integer), typeFunction -> error (maybe generate a revert)
		// word, bool, function -> 1
		// pair -> sum(stackSize(args))
		// user-defined -> stackSize(underlying type)
		TypeConstant typeConstant = std::get<TypeConstant>(_type);
		if (helper.isPrimitiveType(_type, PrimitiveType::Unit) ||
			helper.isPrimitiveType(_type, PrimitiveType::Itself))
			return 0;
		else if (helper.isPrimitiveType(_type, PrimitiveType::Bool) || helper.isPrimitiveType(_type, PrimitiveType::Word))
		{
			solAssert(typeConstant.arguments.empty(), "Primitive type Bool or Word should have no arguments.");
			return 1;
		}
		else if (helper.isFunctionType(_type))
			return 1;
		else if (
			helper.isPrimitiveType(_type, PrimitiveType::Integer) ||
			helper.isPrimitiveType(_type, PrimitiveType::Void) ||
			helper.isPrimitiveType(_type, PrimitiveType::TypeFunction))
			solThrow(langutil::CompilerError, "Primitive type Void, Unit or TypeFunction do not have stack slot"); // FIXME: proper error
		else if (helper.isPrimitiveType(_type, PrimitiveType::Pair))
		{
			solAssert(typeConstant.arguments.size() == 2);
			return stackSize(_context, typeConstant.arguments.front()) + stackSize(_context, typeConstant.arguments.back());
		}
		else
		{
			Type underlyingType = _context.env->resolve(
				_context.analysis.annotation<TypeInference>().underlyingTypes.at(typeConstant.constructor));
			if (helper.isTypeConstant(underlyingType))
				return stackSize(_context, underlyingType);

			TypeEnvironment env = _context.env->clone();
			Type genericFunctionType = helper.typeFunctionType(
				helper.tupleType(typeConstant.arguments),
				env.typeSystem().freshTypeVariable({}));
			solAssert(env.unify(genericFunctionType, underlyingType).empty());

			Type resolvedType = env.resolveRecursive(genericFunctionType);
			auto [argumentType, resultType] = helper.destTypeFunctionType(resolvedType);
			return stackSize(_context, resultType);
		}

		//TODO: sum types
		return 0;
	}

private:
	bool visit(ExpressionStatement const& _expressionStatement) override;
	bool visit(Block const& _block) override;
	bool visit(IfStatement const& _ifStatement) override;
	bool visit(Assignment const& _assignment) override;
	bool visit(Identifier const& _identifier) override;
	bool visit(FunctionCall const& _functionCall) override;
	void endVisit(FunctionCall const& _functionCall) override;
	bool visit(ElementaryTypeNameExpression const& _elementaryTypeNameExpression) override;
	bool visit(MemberAccess const&) override { return true; }
	bool visit(TupleExpression const&) override;
	void endVisit(MemberAccess const& _memberAccess) override;
	bool visit(InlineAssembly const& _inlineAssembly) override;
	bool visit(BinaryOperation const&) override { return true; }
	void endVisit(BinaryOperation const& _binaryOperation) override;
	bool visit(VariableDeclarationStatement const& _variableDeclarationStatement) override;
	bool visit(Return const&) override { return true; }
	void endVisit(Return const& _return) override;
	/// Default visit will reject all AST nodes that are not explicitly supported.
	bool visitNode(ASTNode const& _node) override;

	/// Defines @a _var using the value of @a _value. It declares and assign the variable.
	void define(IRVariable const& _var, IRVariable const& _value) { assign(_var, _value, true); }
	/// Assigns @a _var to the value of @a _value. It does not declare the variable.
	void assign(IRVariable const& _var, IRVariable const& _value, bool _declare = false);
	/// Declares variable @a _var.
	void declare(IRVariable const& _var);

	IRGenerationContext& m_context;
	std::stringstream m_code;
	enum class Builtins
	{
		Identity,
		FromBool,
		ToBool
	};
	std::map<Expression const*, std::variant<Declaration const*, Builtins>> m_expressionDeclaration;

	std::string buildFunctionCall(FunctionDefinition const& _functionDefinition, Type _functionType, std::vector<ASTPointer<Expression const>> const& _arguments);

	template<typename IRVariableType>
	IRVariable var(IRVariableType const& _var) const
	{
		return IRVariable(_var, type(_var), stackSize(m_context, type(_var)));
	}

	Type type(ASTNode const& _node) const;

	FunctionDefinition const& resolveTypeClassFunction(TypeClass _class, std::string _name, Type _type);
};

}
