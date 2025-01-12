/*
 * Copyright (c) 2021, Tim Flynn <trflynn89@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/TestSuite.h>

#include <AK/Optional.h>
#include <AK/Result.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/TypeCasts.h>
#include <AK/Vector.h>
#include <LibSQL/Lexer.h>
#include <LibSQL/Parser.h>

namespace {

using ParseResult = AK::Result<NonnullRefPtr<SQL::Statement>, String>;

ParseResult parse(StringView sql)
{
    auto parser = SQL::Parser(SQL::Lexer(sql));
    auto statement = parser.next_statement();

    if (parser.has_errors()) {
        return parser.errors()[0].to_string();
    }

    return statement;
}

}

TEST_CASE(create_table)
{
    EXPECT(parse("").is_error());
    EXPECT(parse("CREATE").is_error());
    EXPECT(parse("CREATE TABLE").is_error());
    EXPECT(parse("CREATE TABLE test").is_error());
    EXPECT(parse("CREATE TABLE test ()").is_error());
    EXPECT(parse("CREATE TABLE test ();").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 ").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 )").is_error());
    EXPECT(parse("CREATE TABLE IF test ( column1 );").is_error());
    EXPECT(parse("CREATE TABLE IF NOT test ( column1 );").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar()").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar(abc)").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar(123 )").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar(123,  )").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar(123, ) )").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar(.) )").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar(.abc) )").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar(0x) )").is_error());
    EXPECT(parse("CREATE TABLE test ( column1 varchar(0xzzz) )").is_error());
    EXPECT(parse("WITH table AS () CREATE TABLE test ( column1 );").is_error());

    struct Column {
        StringView name;
        StringView type;
        Vector<double> signed_numbers {};
    };

    auto validate = [](StringView sql, StringView expected_schema, StringView expected_table, Vector<Column> expected_columns, bool expected_is_temporary = false, bool expected_is_error_if_table_exists = true) {
        auto result = parse(sql);
        EXPECT(!result.is_error());

        auto statement = result.release_value();
        EXPECT(is<SQL::CreateTable>(*statement));

        const auto& table = static_cast<const SQL::CreateTable&>(*statement);
        EXPECT_EQ(table.schema_name(), expected_schema);
        EXPECT_EQ(table.table_name(), expected_table);
        EXPECT_EQ(table.is_temporary(), expected_is_temporary);
        EXPECT_EQ(table.is_error_if_table_exists(), expected_is_error_if_table_exists);

        const auto& columns = table.columns();
        EXPECT_EQ(columns.size(), expected_columns.size());

        for (size_t i = 0; i < columns.size(); ++i) {
            const auto& column = columns[i];
            const auto& expected_column = expected_columns[i];
            EXPECT_EQ(column.name(), expected_column.name);

            const auto& type_name = column.type_name();
            EXPECT_EQ(type_name->name(), expected_column.type);

            const auto& signed_numbers = type_name->signed_numbers();
            EXPECT_EQ(signed_numbers.size(), expected_column.signed_numbers.size());

            for (size_t j = 0; j < signed_numbers.size(); ++j) {
                double signed_number = signed_numbers[j].value();
                double expected_signed_number = expected_column.signed_numbers[j];
                EXPECT_EQ(signed_number, expected_signed_number);
            }
        }
    };

    validate("CREATE TABLE test ( column1 );", {}, "test", { { "column1", "BLOB" } });
    validate("CREATE TABLE schema.test ( column1 );", "schema", "test", { { "column1", "BLOB" } });
    validate("CREATE TEMP TABLE test ( column1 );", {}, "test", { { "column1", "BLOB" } }, true, true);
    validate("CREATE TEMPORARY TABLE test ( column1 );", {}, "test", { { "column1", "BLOB" } }, true, true);
    validate("CREATE TABLE IF NOT EXISTS test ( column1 );", {}, "test", { { "column1", "BLOB" } }, false, false);

    validate("CREATE TABLE test ( column1 int );", {}, "test", { { "column1", "int" } });
    validate("CREATE TABLE test ( column1 varchar );", {}, "test", { { "column1", "varchar" } });
    validate("CREATE TABLE test ( column1 varchar(255) );", {}, "test", { { "column1", "varchar", { 255 } } });
    validate("CREATE TABLE test ( column1 varchar(255, 123) );", {}, "test", { { "column1", "varchar", { 255, 123 } } });
    validate("CREATE TABLE test ( column1 varchar(255, -123) );", {}, "test", { { "column1", "varchar", { 255, -123 } } });
    validate("CREATE TABLE test ( column1 varchar(0xff) );", {}, "test", { { "column1", "varchar", { 255 } } });
    validate("CREATE TABLE test ( column1 varchar(3.14) );", {}, "test", { { "column1", "varchar", { 3.14 } } });
    validate("CREATE TABLE test ( column1 varchar(1e3) );", {}, "test", { { "column1", "varchar", { 1000 } } });
}

TEST_CASE(drop_table)
{
    EXPECT(parse("DROP").is_error());
    EXPECT(parse("DROP TABLE").is_error());
    EXPECT(parse("DROP TABLE test").is_error());
    EXPECT(parse("DROP TABLE IF test;").is_error());
    EXPECT(parse("WITH table AS () DROP TABLE test;").is_error());

    auto validate = [](StringView sql, StringView expected_schema, StringView expected_table, bool expected_is_error_if_table_does_not_exist = true) {
        auto result = parse(sql);
        EXPECT(!result.is_error());

        auto statement = result.release_value();
        EXPECT(is<SQL::DropTable>(*statement));

        const auto& table = static_cast<const SQL::DropTable&>(*statement);
        EXPECT_EQ(table.schema_name(), expected_schema);
        EXPECT_EQ(table.table_name(), expected_table);
        EXPECT_EQ(table.is_error_if_table_does_not_exist(), expected_is_error_if_table_does_not_exist);
    };

    validate("DROP TABLE test;", {}, "test");
    validate("DROP TABLE schema.test;", "schema", "test");
    validate("DROP TABLE IF EXISTS test;", {}, "test", false);
}

TEST_CASE(delete_)
{
    EXPECT(parse("DELETE").is_error());
    EXPECT(parse("DELETE FROM").is_error());
    EXPECT(parse("DELETE FROM table").is_error());
    EXPECT(parse("DELETE FROM table WHERE").is_error());
    EXPECT(parse("DELETE FROM table WHERE 15").is_error());
    EXPECT(parse("DELETE FROM table WHERE 15 RETURNING").is_error());
    EXPECT(parse("DELETE FROM table WHERE 15 RETURNING *").is_error());
    EXPECT(parse("DELETE FROM table WHERE (');").is_error());
    EXPECT(parse("WITH DELETE FROM table;").is_error());
    EXPECT(parse("WITH table DELETE FROM table;").is_error());
    EXPECT(parse("WITH table AS DELETE FROM table;").is_error());
    EXPECT(parse("WITH RECURSIVE table DELETE FROM table;").is_error());
    EXPECT(parse("WITH RECURSIVE table AS DELETE FROM table;").is_error());

    struct SelectedTableList {
        struct SelectedTable {
            StringView table_name {};
            Vector<StringView> column_names {};
        };

        bool recursive { false };
        Vector<SelectedTable> selected_tables {};
    };

    auto validate = [](StringView sql, SelectedTableList expected_selected_tables, StringView expected_schema, StringView expected_table, StringView expected_alias, bool expect_where_clause, bool expect_returning_clause, Vector<StringView> expected_returned_column_aliases) {
        auto result = parse(sql);
        EXPECT(!result.is_error());

        auto statement = result.release_value();
        EXPECT(is<SQL::Delete>(*statement));

        const auto& delete_ = static_cast<const SQL::Delete&>(*statement);

        const auto& common_table_expression_list = delete_.common_table_expression_list();
        EXPECT_EQ(common_table_expression_list.is_null(), expected_selected_tables.selected_tables.is_empty());
        if (common_table_expression_list) {
            EXPECT_EQ(common_table_expression_list->recursive(), expected_selected_tables.recursive);

            const auto& common_table_expressions = common_table_expression_list->common_table_expressions();
            EXPECT_EQ(common_table_expressions.size(), expected_selected_tables.selected_tables.size());

            for (size_t i = 0; i < common_table_expressions.size(); ++i) {
                const auto& common_table_expression = common_table_expressions[i];
                const auto& expected_common_table_expression = expected_selected_tables.selected_tables[i];
                EXPECT_EQ(common_table_expression.table_name(), expected_common_table_expression.table_name);
                EXPECT_EQ(common_table_expression.column_names().size(), expected_common_table_expression.column_names.size());

                for (size_t j = 0; j < common_table_expression.column_names().size(); ++j)
                    EXPECT_EQ(common_table_expression.column_names()[j], expected_common_table_expression.column_names[j]);
            }
        }

        const auto& qualified_table_name = delete_.qualified_table_name();
        EXPECT_EQ(qualified_table_name->schema_name(), expected_schema);
        EXPECT_EQ(qualified_table_name->table_name(), expected_table);
        EXPECT_EQ(qualified_table_name->alias(), expected_alias);

        const auto& where_clause = delete_.where_clause();
        EXPECT_EQ(where_clause.is_null(), !expect_where_clause);
        if (where_clause)
            EXPECT(!is<SQL::ErrorExpression>(*where_clause));

        const auto& returning_clause = delete_.returning_clause();
        EXPECT_EQ(returning_clause.is_null(), !expect_returning_clause);
        if (returning_clause) {
            EXPECT_EQ(returning_clause->columns().size(), expected_returned_column_aliases.size());

            for (size_t i = 0; i < returning_clause->columns().size(); ++i) {
                const auto& column = returning_clause->columns()[i];
                const auto& expected_column_alias = expected_returned_column_aliases[i];

                EXPECT(!is<SQL::ErrorExpression>(*column.expression));
                EXPECT_EQ(column.column_alias, expected_column_alias);
            }
        }
    };

    validate("DELETE FROM table;", {}, {}, "table", {}, false, false, {});
    validate("DELETE FROM schema.table;", {}, "schema", "table", {}, false, false, {});
    validate("DELETE FROM schema.table AS alias;", {}, "schema", "table", "alias", false, false, {});
    validate("DELETE FROM table WHERE (1 == 1);", {}, {}, "table", {}, true, false, {});
    validate("DELETE FROM table RETURNING *;", {}, {}, "table", {}, false, true, {});
    validate("DELETE FROM table RETURNING column;", {}, {}, "table", {}, false, true, { {} });
    validate("DELETE FROM table RETURNING column AS alias;", {}, {}, "table", {}, false, true, { "alias" });
    validate("DELETE FROM table RETURNING column1 AS alias1, column2 AS alias2;", {}, {}, "table", {}, false, true, { "alias1", "alias2" });

    // FIXME: When parsing of SELECT statements are supported, the common-table-expressions below will become invalid due to the empty "AS ()" clause.
    validate("WITH table AS () DELETE FROM table;", { false, { { "table" } } }, {}, "table", {}, false, false, {});
    validate("WITH table (column) AS () DELETE FROM table;", { false, { { "table", { "column" } } } }, {}, "table", {}, false, false, {});
    validate("WITH table (column1, column2) AS () DELETE FROM table;", { false, { { "table", { "column1", "column2" } } } }, {}, "table", {}, false, false, {});
    validate("WITH RECURSIVE table AS () DELETE FROM table;", { true, { { "table", {} } } }, {}, "table", {}, false, false, {});
}

TEST_CASE(select)
{
    EXPECT(parse("SELECT").is_error());
    EXPECT(parse("SELECT;").is_error());
    EXPECT(parse("SELECT DISTINCT;").is_error());
    EXPECT(parse("SELECT ALL;").is_error());
    EXPECT(parse("SELECT *").is_error());
    EXPECT(parse("SELECT * FROM;").is_error());
    EXPECT(parse("SELECT table. FROM table;").is_error());
    EXPECT(parse("SELECT * FROM (").is_error());
    EXPECT(parse("SELECT * FROM ()").is_error());
    EXPECT(parse("SELECT * FROM ();").is_error());
    EXPECT(parse("SELECT * FROM (table1)").is_error());
    EXPECT(parse("SELECT * FROM (table1, )").is_error());
    EXPECT(parse("SELECT * FROM (table1, table2)").is_error());
    EXPECT(parse("SELECT * FROM table").is_error());
    EXPECT(parse("SELECT * FROM table WHERE;").is_error());
    EXPECT(parse("SELECT * FROM table WHERE 1 ==1").is_error());
    EXPECT(parse("SELECT * FROM table GROUP;").is_error());
    EXPECT(parse("SELECT * FROM table GROUP BY;").is_error());
    EXPECT(parse("SELECT * FROM table GROUP BY column").is_error());
    EXPECT(parse("SELECT * FROM table ORDER:").is_error());
    EXPECT(parse("SELECT * FROM table ORDER BY column").is_error());
    EXPECT(parse("SELECT * FROM table ORDER BY column COLLATE:").is_error());
    EXPECT(parse("SELECT * FROM table ORDER BY column COLLATE collation").is_error());
    EXPECT(parse("SELECT * FROM table ORDER BY column NULLS;").is_error());
    EXPECT(parse("SELECT * FROM table ORDER BY column NULLS SECOND;").is_error());
    EXPECT(parse("SELECT * FROM table LIMIT;").is_error());
    EXPECT(parse("SELECT * FROM table LIMIT 12").is_error());
    EXPECT(parse("SELECT * FROM table LIMIT 12 OFFSET;").is_error());
    EXPECT(parse("SELECT * FROM table LIMIT 12 OFFSET 15").is_error());

    struct Type {
        SQL::ResultType type;
        StringView table_name_or_column_alias {};
    };

    struct From {
        StringView schema_name;
        StringView table_name;
        StringView table_alias;
    };

    struct Ordering {
        String collation_name;
        SQL::Order order;
        SQL::Nulls nulls;
    };

    auto validate = [](StringView sql, Vector<Type> expected_columns, Vector<From> expected_from_list, bool expect_where_clause, size_t expected_group_by_size, bool expect_having_clause, Vector<Ordering> expected_ordering, bool expect_limit_clause, bool expect_offset_clause) {
        auto result = parse(sql);
        EXPECT(!result.is_error());

        auto statement = result.release_value();
        EXPECT(is<SQL::Select>(*statement));

        const auto& select = static_cast<const SQL::Select&>(*statement);

        const auto& result_column_list = select.result_column_list();
        EXPECT_EQ(result_column_list.size(), expected_columns.size());
        for (size_t i = 0; i < result_column_list.size(); ++i) {
            const auto& result_column = result_column_list[i];
            const auto& expected_column = expected_columns[i];
            EXPECT_EQ(result_column.type(), expected_column.type);

            switch (result_column.type()) {
            case SQL::ResultType::All:
                EXPECT(expected_column.table_name_or_column_alias.is_null());
                break;
            case SQL::ResultType::Table:
                EXPECT_EQ(result_column.table_name(), expected_column.table_name_or_column_alias);
                break;
            case SQL::ResultType::Expression:
                EXPECT_EQ(result_column.column_alias(), expected_column.table_name_or_column_alias);
                break;
            }
        }

        const auto& table_or_subquery_list = select.table_or_subquery_list();
        EXPECT_EQ(table_or_subquery_list.size(), expected_from_list.size());
        for (size_t i = 0; i < table_or_subquery_list.size(); ++i) {
            const auto& result_from = table_or_subquery_list[i];
            const auto& expected_from = expected_from_list[i];
            EXPECT_EQ(result_from.schema_name(), expected_from.schema_name);
            EXPECT_EQ(result_from.table_name(), expected_from.table_name);
            EXPECT_EQ(result_from.table_alias(), expected_from.table_alias);
        }

        const auto& where_clause = select.where_clause();
        EXPECT_EQ(where_clause.is_null(), !expect_where_clause);
        if (where_clause)
            EXPECT(!is<SQL::ErrorExpression>(*where_clause));

        const auto& group_by_clause = select.group_by_clause();
        EXPECT_EQ(group_by_clause.is_null(), (expected_group_by_size == 0));
        if (group_by_clause) {
            const auto& group_by_list = group_by_clause->group_by_list();
            EXPECT_EQ(group_by_list.size(), expected_group_by_size);
            for (size_t i = 0; i < group_by_list.size(); ++i)
                EXPECT(!is<SQL::ErrorExpression>(group_by_list[i]));

            const auto& having_clause = group_by_clause->having_clause();
            EXPECT_EQ(having_clause.is_null(), !expect_having_clause);
            if (having_clause)
                EXPECT(!is<SQL::ErrorExpression>(*having_clause));
        }

        const auto& ordering_term_list = select.ordering_term_list();
        EXPECT_EQ(ordering_term_list.size(), expected_ordering.size());
        for (size_t i = 0; i < ordering_term_list.size(); ++i) {
            const auto& result_order = ordering_term_list[i];
            const auto& expected_order = expected_ordering[i];
            EXPECT(!is<SQL::ErrorExpression>(*result_order.expression()));
            EXPECT_EQ(result_order.collation_name(), expected_order.collation_name);
            EXPECT_EQ(result_order.order(), expected_order.order);
            EXPECT_EQ(result_order.nulls(), expected_order.nulls);
        }

        const auto& limit_clause = select.limit_clause();
        EXPECT_EQ(limit_clause.is_null(), !expect_limit_clause);
        if (limit_clause) {
            const auto& limit_expression = limit_clause->limit_expression();
            EXPECT(!is<SQL::ErrorExpression>(*limit_expression));

            const auto& offset_expression = limit_clause->offset_expression();
            EXPECT_EQ(offset_expression.is_null(), !expect_offset_clause);
            if (offset_expression)
                EXPECT(!is<SQL::ErrorExpression>(*offset_expression));
        }
    };

    Vector<Type> all { { SQL::ResultType::All } };
    Vector<From> from { { {}, "table", {} } };

    validate("SELECT * FROM table;", { { SQL::ResultType::All } }, from, false, 0, false, {}, false, false);
    validate("SELECT table.* FROM table;", { { SQL::ResultType::Table, "table" } }, from, false, 0, false, {}, false, false);
    validate("SELECT column AS alias FROM table;", { { SQL::ResultType::Expression, "alias" } }, from, false, 0, false, {}, false, false);
    validate("SELECT table.column AS alias FROM table;", { { SQL::ResultType::Expression, "alias" } }, from, false, 0, false, {}, false, false);
    validate("SELECT schema.table.column AS alias FROM table;", { { SQL::ResultType::Expression, "alias" } }, from, false, 0, false, {}, false, false);
    validate("SELECT column AS alias, *, table.* FROM table;", { { SQL::ResultType::Expression, "alias" }, { SQL::ResultType::All }, { SQL::ResultType::Table, "table" } }, from, false, 0, false, {}, false, false);

    validate("SELECT * FROM table;", all, { { {}, "table", {} } }, false, 0, false, {}, false, false);
    validate("SELECT * FROM schema.table;", all, { { "schema", "table", {} } }, false, 0, false, {}, false, false);
    validate("SELECT * FROM schema.table AS alias;", all, { { "schema", "table", "alias" } }, false, 0, false, {}, false, false);
    validate("SELECT * FROM schema.table AS alias, table2, table3 AS table4;", all, { { "schema", "table", "alias" }, { {}, "table2", {} }, { {}, "table3", "table4" } }, false, 0, false, {}, false, false);

    validate("SELECT * FROM table WHERE column IS NOT NULL;", all, from, true, 0, false, {}, false, false);

    validate("SELECT * FROM table GROUP BY column;", all, from, false, 1, false, {}, false, false);
    validate("SELECT * FROM table GROUP BY column1, column2, column3;", all, from, false, 3, false, {}, false, false);
    validate("SELECT * FROM table GROUP BY column HAVING 'abc';", all, from, false, 1, true, {}, false, false);

    validate("SELECT * FROM table ORDER BY column;", all, from, false, 0, false, { { {}, SQL::Order::Ascending, SQL::Nulls::First } }, false, false);
    validate("SELECT * FROM table ORDER BY column COLLATE collation;", all, from, false, 0, false, { { "collation", SQL::Order::Ascending, SQL::Nulls::First } }, false, false);
    validate("SELECT * FROM table ORDER BY column ASC;", all, from, false, 0, false, { { {}, SQL::Order::Ascending, SQL::Nulls::First } }, false, false);
    validate("SELECT * FROM table ORDER BY column DESC;", all, from, false, 0, false, { { {}, SQL::Order::Descending, SQL::Nulls::Last } }, false, false);
    validate("SELECT * FROM table ORDER BY column ASC NULLS LAST;", all, from, false, 0, false, { { {}, SQL::Order::Ascending, SQL::Nulls::Last } }, false, false);
    validate("SELECT * FROM table ORDER BY column DESC NULLS FIRST;", all, from, false, 0, false, { { {}, SQL::Order::Descending, SQL::Nulls::First } }, false, false);
    validate("SELECT * FROM table ORDER BY column1, column2 DESC, column3 NULLS LAST;", all, from, false, 0, false, { { {}, SQL::Order::Ascending, SQL::Nulls::First }, { {}, SQL::Order::Descending, SQL::Nulls::Last }, { {}, SQL::Order::Ascending, SQL::Nulls::Last } }, false, false);

    validate("SELECT * FROM table LIMIT 15;", all, from, false, 0, false, {}, true, false);
    validate("SELECT * FROM table LIMIT 15 OFFSET 16;", all, from, false, 0, false, {}, true, true);
}

TEST_MAIN(SqlStatementParser)
