/*
 * Copyright (c) 2021, Tim Flynn <trflynn89@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtr.h>
#include <AK/NonnullRefPtrVector.h>
#include <AK/RefCounted.h>
#include <AK/RefPtr.h>
#include <AK/String.h>
#include <LibSQL/Forward.h>
#include <LibSQL/Token.h>

namespace SQL {

template<class T, class... Args>
static inline NonnullRefPtr<T>
create_ast_node(Args&&... args)
{
    return adopt_ref(*new T(forward<Args>(args)...));
}

class ASTNode : public RefCounted<ASTNode> {
public:
    virtual ~ASTNode() { }

protected:
    ASTNode() = default;
};

//==================================================================================================
// Language types
//==================================================================================================

class SignedNumber final : public ASTNode {
public:
    explicit SignedNumber(double value)
        : m_value(value)
    {
    }

    double value() const { return m_value; }

private:
    double m_value;
};

class TypeName : public ASTNode {
public:
    TypeName(String name, NonnullRefPtrVector<SignedNumber> signed_numbers)
        : m_name(move(name))
        , m_signed_numbers(move(signed_numbers))
    {
        VERIFY(m_signed_numbers.size() <= 2);
    }

    const String& name() const { return m_name; }
    const NonnullRefPtrVector<SignedNumber> signed_numbers() const { return m_signed_numbers; }

private:
    String m_name;
    NonnullRefPtrVector<SignedNumber> m_signed_numbers;
};

class ColumnDefinition : public ASTNode {
public:
    ColumnDefinition(String name, NonnullRefPtr<TypeName> type_name)
        : m_name(move(name))
        , m_type_name(move(type_name))
    {
    }

    const String& name() const { return m_name; }
    const NonnullRefPtr<TypeName>& type_name() const { return m_type_name; }

private:
    String m_name;
    NonnullRefPtr<TypeName> m_type_name;
};

class CommonTableExpression : public ASTNode {
public:
    CommonTableExpression(String table_name, Vector<String> column_names)
        : m_table_name(move(table_name))
        , m_column_names(move(column_names))
    {
    }

    const String& table_name() const { return m_table_name; }
    const Vector<String>& column_names() const { return m_column_names; }

private:
    String m_table_name;
    Vector<String> m_column_names;
};

class CommonTableExpressionList : public ASTNode {
public:
    CommonTableExpressionList(bool recursive, NonnullRefPtrVector<CommonTableExpression> common_table_expressions)
        : m_recursive(recursive)
        , m_common_table_expressions(move(common_table_expressions))
    {
        VERIFY(!m_common_table_expressions.is_empty());
    }

    bool recursive() const { return m_recursive; }
    const NonnullRefPtrVector<CommonTableExpression>& common_table_expressions() const { return m_common_table_expressions; }

private:
    bool m_recursive;
    NonnullRefPtrVector<CommonTableExpression> m_common_table_expressions;
};

class QualifiedTableName : public ASTNode {
public:
    QualifiedTableName(String schema_name, String table_name, String alias)
        : m_schema_name(move(schema_name))
        , m_table_name(move(table_name))
        , m_alias(move(alias))
    {
    }

    const String& schema_name() const { return m_schema_name; }
    const String& table_name() const { return m_table_name; }
    const String& alias() const { return m_alias; }

private:
    String m_schema_name;
    String m_table_name;
    String m_alias;
};

class ReturningClause : public ASTNode {
public:
    struct ColumnClause {
        NonnullRefPtr<Expression> expression;
        String column_alias;
    };

    ReturningClause() = default;

    explicit ReturningClause(Vector<ColumnClause> columns)
        : m_columns(move(columns))
    {
    }

    bool return_all_columns() const { return m_columns.is_empty(); };
    const Vector<ColumnClause>& columns() const { return m_columns; }

private:
    Vector<ColumnClause> m_columns;
};

enum class ResultType {
    All,
    Table,
    Expression,
};

class ResultColumn : public ASTNode {
public:
    ResultColumn() = default;

    explicit ResultColumn(String table_name)
        : m_type(ResultType::Table)
        , m_table_name(move(table_name))
    {
    }

    ResultColumn(NonnullRefPtr<Expression> expression, String column_alias)
        : m_type(ResultType::Expression)
        , m_expression(move(expression))
        , m_column_alias(move(column_alias))
    {
    }

    ResultType type() const { return m_type; }

    bool select_from_table() const { return !m_table_name.is_null(); }
    const String& table_name() const { return m_table_name; }

    bool select_from_expression() const { return !m_expression.is_null(); }
    const RefPtr<Expression>& expression() const { return m_expression; }
    const String& column_alias() const { return m_column_alias; }

private:
    ResultType m_type { ResultType::All };

    String m_table_name {};

    RefPtr<Expression> m_expression {};
    String m_column_alias {};
};

class GroupByClause : public ASTNode {
public:
    GroupByClause(NonnullRefPtrVector<Expression> group_by_list, RefPtr<Expression> having_clause)
        : m_group_by_list(move(group_by_list))
        , m_having_clause(move(having_clause))
    {
        VERIFY(!m_group_by_list.is_empty());
    }

    const NonnullRefPtrVector<Expression>& group_by_list() const { return m_group_by_list; }
    const RefPtr<Expression>& having_clause() const { return m_having_clause; }

private:
    NonnullRefPtrVector<Expression> m_group_by_list;
    RefPtr<Expression> m_having_clause;
};

class TableOrSubquery : public ASTNode {
public:
    TableOrSubquery() = default;

    TableOrSubquery(String schema_name, String table_name, String table_alias)
        : m_is_table(true)
        , m_schema_name(move(schema_name))
        , m_table_name(move(table_name))
        , m_table_alias(move(table_alias))
    {
    }

    explicit TableOrSubquery(NonnullRefPtrVector<TableOrSubquery> subqueries)
        : m_is_subquery(!subqueries.is_empty())
        , m_subqueries(move(subqueries))
    {
    }

    bool is_table() const { return m_is_table; }
    const String& schema_name() const { return m_schema_name; }
    const String& table_name() const { return m_table_name; }
    const String& table_alias() const { return m_table_alias; }

    bool is_subquery() const { return m_is_subquery; }
    const NonnullRefPtrVector<TableOrSubquery>& subqueries() const { return m_subqueries; }

private:
    bool m_is_table { false };
    String m_schema_name {};
    String m_table_name {};
    String m_table_alias {};

    bool m_is_subquery { false };
    NonnullRefPtrVector<TableOrSubquery> m_subqueries {};
};

enum class Order {
    Ascending,
    Descending,
};

enum class Nulls {
    First,
    Last,
};

class OrderingTerm : public ASTNode {
public:
    OrderingTerm(NonnullRefPtr<Expression> expression, String collation_name, Order order, Nulls nulls)
        : m_expression(move(expression))
        , m_collation_name(move(collation_name))
        , m_order(order)
        , m_nulls(nulls)
    {
    }

    const NonnullRefPtr<Expression>& expression() const { return m_expression; }
    const String& collation_name() const { return m_collation_name; }
    Order order() const { return m_order; }
    Nulls nulls() const { return m_nulls; }

private:
    NonnullRefPtr<Expression> m_expression;
    String m_collation_name;
    Order m_order;
    Nulls m_nulls;
};

class LimitClause : public ASTNode {
public:
    LimitClause(NonnullRefPtr<Expression> limit_expression, RefPtr<Expression> offset_expression)
        : m_limit_expression(move(limit_expression))
        , m_offset_expression(move(offset_expression))
    {
    }

    const NonnullRefPtr<Expression>& limit_expression() const { return m_limit_expression; }
    const RefPtr<Expression>& offset_expression() const { return m_offset_expression; }

private:
    NonnullRefPtr<Expression> m_limit_expression;
    RefPtr<Expression> m_offset_expression;
};

//==================================================================================================
// Expressions
//==================================================================================================

class Expression : public ASTNode {
};

class ErrorExpression final : public Expression {
};

class NumericLiteral : public Expression {
public:
    explicit NumericLiteral(double value)
        : m_value(value)
    {
    }

    double value() const { return m_value; }

private:
    double m_value;
};

class StringLiteral : public Expression {
public:
    explicit StringLiteral(String value)
        : m_value(move(value))
    {
    }

    const String& value() const { return m_value; }

private:
    String m_value;
};

class BlobLiteral : public Expression {
public:
    explicit BlobLiteral(String value)
        : m_value(move(value))
    {
    }

    const String& value() const { return m_value; }

private:
    String m_value;
};

class NullLiteral : public Expression {
};

class NestedExpression : public Expression {
public:
    const NonnullRefPtr<Expression>& expression() const { return m_expression; }

protected:
    explicit NestedExpression(NonnullRefPtr<Expression> expression)
        : m_expression(move(expression))
    {
    }

private:
    NonnullRefPtr<Expression> m_expression;
};

class NestedDoubleExpression : public Expression {
public:
    const NonnullRefPtr<Expression>& lhs() const { return m_lhs; }
    const NonnullRefPtr<Expression>& rhs() const { return m_rhs; }

protected:
    NestedDoubleExpression(NonnullRefPtr<Expression> lhs, NonnullRefPtr<Expression> rhs)
        : m_lhs(move(lhs))
        , m_rhs(move(rhs))
    {
    }

private:
    NonnullRefPtr<Expression> m_lhs;
    NonnullRefPtr<Expression> m_rhs;
};

class InvertibleNestedExpression : public NestedExpression {
public:
    bool invert_expression() const { return m_invert_expression; }

protected:
    InvertibleNestedExpression(NonnullRefPtr<Expression> expression, bool invert_expression)
        : NestedExpression(move(expression))
        , m_invert_expression(invert_expression)
    {
    }

private:
    bool m_invert_expression;
};

class InvertibleNestedDoubleExpression : public NestedDoubleExpression {
public:
    bool invert_expression() const { return m_invert_expression; }

protected:
    InvertibleNestedDoubleExpression(NonnullRefPtr<Expression> lhs, NonnullRefPtr<Expression> rhs, bool invert_expression)
        : NestedDoubleExpression(move(lhs), move(rhs))
        , m_invert_expression(invert_expression)
    {
    }

private:
    bool m_invert_expression;
};

class ColumnNameExpression : public Expression {
public:
    ColumnNameExpression(String schema_name, String table_name, String column_name)
        : m_schema_name(move(schema_name))
        , m_table_name(move(table_name))
        , m_column_name(move(column_name))
    {
    }

    const String& schema_name() const { return m_schema_name; }
    const String& table_name() const { return m_table_name; }
    const String& column_name() const { return m_column_name; }

private:
    String m_schema_name;
    String m_table_name;
    String m_column_name;
};

enum class UnaryOperator {
    Minus,
    Plus,
    BitwiseNot,
    Not,
};

class UnaryOperatorExpression : public NestedExpression {
public:
    UnaryOperatorExpression(UnaryOperator type, NonnullRefPtr<Expression> expression)
        : NestedExpression(move(expression))
        , m_type(type)
    {
    }

    UnaryOperator type() const { return m_type; }

private:
    UnaryOperator m_type;
};

enum class BinaryOperator {
    // Note: These are in order of highest-to-lowest operator precedence.
    Concatenate,
    Multiplication,
    Division,
    Modulo,
    Plus,
    Minus,
    ShiftLeft,
    ShiftRight,
    BitwiseAnd,
    BitwiseOr,
    LessThan,
    LessThanEquals,
    GreaterThan,
    GreaterThanEquals,
    Equals,
    NotEquals,
    And,
    Or,
};

class BinaryOperatorExpression : public NestedDoubleExpression {
public:
    BinaryOperatorExpression(BinaryOperator type, NonnullRefPtr<Expression> lhs, NonnullRefPtr<Expression> rhs)
        : NestedDoubleExpression(move(lhs), move(rhs))
        , m_type(type)
    {
    }

    BinaryOperator type() const { return m_type; }

private:
    BinaryOperator m_type;
};

class ChainedExpression : public Expression {
public:
    explicit ChainedExpression(NonnullRefPtrVector<Expression> expressions)
        : m_expressions(move(expressions))
    {
    }

    const NonnullRefPtrVector<Expression>& expressions() const { return m_expressions; }

private:
    NonnullRefPtrVector<Expression> m_expressions;
};

class CastExpression : public NestedExpression {
public:
    CastExpression(NonnullRefPtr<Expression> expression, NonnullRefPtr<TypeName> type_name)
        : NestedExpression(move(expression))
        , m_type_name(move(type_name))
    {
    }

    const NonnullRefPtr<TypeName>& type_name() const { return m_type_name; }

private:
    NonnullRefPtr<TypeName> m_type_name;
};

class CaseExpression : public Expression {
public:
    struct WhenThenClause {
        NonnullRefPtr<Expression> when;
        NonnullRefPtr<Expression> then;
    };

    CaseExpression(RefPtr<Expression> case_expression, Vector<WhenThenClause> when_then_clauses, RefPtr<Expression> else_expression)
        : m_case_expression(case_expression)
        , m_when_then_clauses(when_then_clauses)
        , m_else_expression(else_expression)
    {
        VERIFY(!m_when_then_clauses.is_empty());
    }

    const RefPtr<Expression>& case_expression() const { return m_case_expression; }
    const Vector<WhenThenClause>& when_then_clauses() const { return m_when_then_clauses; }
    const RefPtr<Expression>& else_expression() const { return m_else_expression; }

private:
    RefPtr<Expression> m_case_expression;
    Vector<WhenThenClause> m_when_then_clauses;
    RefPtr<Expression> m_else_expression;
};

class CollateExpression : public NestedExpression {
public:
    CollateExpression(NonnullRefPtr<Expression> expression, String collation_name)
        : NestedExpression(move(expression))
        , m_collation_name(move(collation_name))
    {
    }

    const String& collation_name() const { return m_collation_name; }

private:
    String m_collation_name;
};

enum class MatchOperator {
    Like,
    Glob,
    Match,
    Regexp,
};

class MatchExpression : public InvertibleNestedDoubleExpression {
public:
    MatchExpression(MatchOperator type, NonnullRefPtr<Expression> lhs, NonnullRefPtr<Expression> rhs, RefPtr<Expression> escape, bool invert_expression)
        : InvertibleNestedDoubleExpression(move(lhs), move(rhs), invert_expression)
        , m_type(type)
        , m_escape(move(escape))
    {
    }

    MatchOperator type() const { return m_type; }
    const RefPtr<Expression>& escape() const { return m_escape; }

private:
    MatchOperator m_type;
    RefPtr<Expression> m_escape;
};

class NullExpression : public InvertibleNestedExpression {
public:
    NullExpression(NonnullRefPtr<Expression> expression, bool invert_expression)
        : InvertibleNestedExpression(move(expression), invert_expression)
    {
    }
};

class IsExpression : public InvertibleNestedDoubleExpression {
public:
    IsExpression(NonnullRefPtr<Expression> lhs, NonnullRefPtr<Expression> rhs, bool invert_expression)
        : InvertibleNestedDoubleExpression(move(lhs), move(rhs), invert_expression)
    {
    }
};

class BetweenExpression : public InvertibleNestedDoubleExpression {
public:
    BetweenExpression(NonnullRefPtr<Expression> expression, NonnullRefPtr<Expression> lhs, NonnullRefPtr<Expression> rhs, bool invert_expression)
        : InvertibleNestedDoubleExpression(move(lhs), move(rhs), invert_expression)
        , m_expression(move(expression))
    {
    }

    const NonnullRefPtr<Expression>& expression() const { return m_expression; }

private:
    NonnullRefPtr<Expression> m_expression;
};

class InChainedExpression : public InvertibleNestedExpression {
public:
    InChainedExpression(NonnullRefPtr<Expression> expression, NonnullRefPtr<ChainedExpression> expression_chain, bool invert_expression)
        : InvertibleNestedExpression(move(expression), invert_expression)
        , m_expression_chain(move(expression_chain))
    {
    }

    const NonnullRefPtr<ChainedExpression>& expression_chain() const { return m_expression_chain; }

private:
    NonnullRefPtr<ChainedExpression> m_expression_chain;
};

class InTableExpression : public InvertibleNestedExpression {
public:
    InTableExpression(NonnullRefPtr<Expression> expression, String schema_name, String table_name, bool invert_expression)
        : InvertibleNestedExpression(move(expression), invert_expression)
        , m_schema_name(move(schema_name))
        , m_table_name(move(table_name))
    {
    }

    const String& schema_name() const { return m_schema_name; }
    const String& table_name() const { return m_table_name; }

private:
    String m_schema_name;
    String m_table_name;
};

//==================================================================================================
// Statements
//==================================================================================================

class Statement : public ASTNode {
};

class ErrorStatement final : public Statement {
};

class CreateTable : public Statement {
public:
    CreateTable(String schema_name, String table_name, NonnullRefPtrVector<ColumnDefinition> columns, bool is_temporary, bool is_error_if_table_exists)
        : m_schema_name(move(schema_name))
        , m_table_name(move(table_name))
        , m_columns(move(columns))
        , m_is_temporary(is_temporary)
        , m_is_error_if_table_exists(is_error_if_table_exists)
    {
    }

    const String& schema_name() const { return m_schema_name; }
    const String& table_name() const { return m_table_name; }
    const NonnullRefPtrVector<ColumnDefinition> columns() const { return m_columns; }
    bool is_temporary() const { return m_is_temporary; }
    bool is_error_if_table_exists() const { return m_is_error_if_table_exists; }

private:
    String m_schema_name;
    String m_table_name;
    NonnullRefPtrVector<ColumnDefinition> m_columns;
    bool m_is_temporary;
    bool m_is_error_if_table_exists;
};

class DropTable : public Statement {
public:
    DropTable(String schema_name, String table_name, bool is_error_if_table_does_not_exist)
        : m_schema_name(move(schema_name))
        , m_table_name(move(table_name))
        , m_is_error_if_table_does_not_exist(is_error_if_table_does_not_exist)
    {
    }

    const String& schema_name() const { return m_schema_name; }
    const String& table_name() const { return m_table_name; }
    bool is_error_if_table_does_not_exist() const { return m_is_error_if_table_does_not_exist; }

private:
    String m_schema_name;
    String m_table_name;
    bool m_is_error_if_table_does_not_exist;
};

class Delete : public Statement {
public:
    Delete(RefPtr<CommonTableExpressionList> common_table_expression_list, NonnullRefPtr<QualifiedTableName> qualified_table_name, RefPtr<Expression> where_clause, RefPtr<ReturningClause> returning_clause)
        : m_common_table_expression_list(move(common_table_expression_list))
        , m_qualified_table_name(move(qualified_table_name))
        , m_where_clause(move(where_clause))
        , m_returning_clause(move(returning_clause))
    {
    }

    const RefPtr<CommonTableExpressionList>& common_table_expression_list() const { return m_common_table_expression_list; }
    const NonnullRefPtr<QualifiedTableName>& qualified_table_name() const { return m_qualified_table_name; }
    const RefPtr<Expression>& where_clause() const { return m_where_clause; }
    const RefPtr<ReturningClause>& returning_clause() const { return m_returning_clause; }

private:
    RefPtr<CommonTableExpressionList> m_common_table_expression_list;
    NonnullRefPtr<QualifiedTableName> m_qualified_table_name;
    RefPtr<Expression> m_where_clause;
    RefPtr<ReturningClause> m_returning_clause;
};

class Select : public Statement {
public:
    Select(RefPtr<CommonTableExpressionList> common_table_expression_list, bool select_all, NonnullRefPtrVector<ResultColumn> result_column_list, NonnullRefPtrVector<TableOrSubquery> table_or_subquery_list, RefPtr<Expression> where_clause, RefPtr<GroupByClause> group_by_clause, NonnullRefPtrVector<OrderingTerm> ordering_term_list, RefPtr<LimitClause> limit_clause)
        : m_common_table_expression_list(move(common_table_expression_list))
        , m_select_all(move(select_all))
        , m_result_column_list(move(result_column_list))
        , m_table_or_subquery_list(move(table_or_subquery_list))
        , m_where_clause(move(where_clause))
        , m_group_by_clause(move(group_by_clause))
        , m_ordering_term_list(move(ordering_term_list))
        , m_limit_clause(move(limit_clause))
    {
    }

    const RefPtr<CommonTableExpressionList>& common_table_expression_list() const { return m_common_table_expression_list; }
    bool select_all() const { return m_select_all; }
    const NonnullRefPtrVector<ResultColumn>& result_column_list() const { return m_result_column_list; }
    const NonnullRefPtrVector<TableOrSubquery>& table_or_subquery_list() const { return m_table_or_subquery_list; }
    const RefPtr<Expression>& where_clause() const { return m_where_clause; }
    const RefPtr<GroupByClause>& group_by_clause() const { return m_group_by_clause; }
    const NonnullRefPtrVector<OrderingTerm>& ordering_term_list() const { return m_ordering_term_list; }
    const RefPtr<LimitClause>& limit_clause() const { return m_limit_clause; }

private:
    RefPtr<CommonTableExpressionList> m_common_table_expression_list;
    bool m_select_all;
    NonnullRefPtrVector<ResultColumn> m_result_column_list;
    NonnullRefPtrVector<TableOrSubquery> m_table_or_subquery_list;
    RefPtr<Expression> m_where_clause;
    RefPtr<GroupByClause> m_group_by_clause;
    NonnullRefPtrVector<OrderingTerm> m_ordering_term_list;
    RefPtr<LimitClause> m_limit_clause;
};

}
