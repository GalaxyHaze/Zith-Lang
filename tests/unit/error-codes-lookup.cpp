#include "../test-common.hpp"
#include "diagnostics/error-codes.hpp"

#include <cstdio>

using namespace zith::diagnostics;

static void test_lookup_all_lexical() {
    auto r1 = lookupError(err::UnknownToken);
    CHECK(r1.has_value(), "UnknownToken found");
    if (r1) {
        CHECK_EQ(r1->prefix, 'E', "UnknownToken prefix");
        CHECK_EQ(r1->category, "lexical", "UnknownToken category");
    }

    auto r2 = lookupError(err::UnclosedString);
    CHECK(r2.has_value(), "UnclosedString found");
    if (r2)
        CHECK_EQ(r2->prefix, 'E', "UnclosedString prefix");

    auto r3 = lookupError(err::InvalidEscape);
    CHECK(r3.has_value(), "InvalidEscape found");
    auto r4 = lookupError(err::InvalidIntLiteral);
    CHECK(r4.has_value(), "InvalidIntLiteral found");
    auto r5 = lookupError(err::UnclosedComment);
    CHECK(r5.has_value(), "UnclosedComment found");
}

static void test_lookup_all_parse() {
    auto r1 = lookupError(err::ExpectedExpr);
    CHECK(r1.has_value(), "ExpectedExpr found");
    if (r1)
        CHECK_EQ(r1->category, "parse", "ExpectedExpr category");

    auto r2 = lookupError(err::ExpectedSemicolon);
    CHECK(r2.has_value(), "ExpectedSemicolon found");
    auto r3 = lookupError(err::UnclosedParen);
    CHECK(r3.has_value(), "UnclosedParen found");
    auto r4 = lookupError(err::ExpectedIdent);
    CHECK(r4.has_value(), "ExpectedIdent found");
    auto r5 = lookupError(err::InvalidImportDepth);
    CHECK(r5.has_value(), "InvalidImportDepth found");
    auto r6 = lookupError(err::ImportError);
    CHECK(r6.has_value(), "ImportError found");
}

static void test_lookup_all_semantic() {
    auto r1 = lookupError(err::UndefinedIdent);
    CHECK(r1.has_value(), "UndefinedIdent found");
    if (r1)
        CHECK_EQ(r1->category, "semantic", "UndefinedIdent category");

    auto r2 = lookupError(err::DuplicateDecl);
    CHECK(r2.has_value(), "DuplicateDecl found");
    auto r3 = lookupError(err::WrongArity);
    CHECK(r3.has_value(), "WrongArity found");
    auto r4 = lookupError(err::UnusedDecl);
    CHECK(r4.has_value(), "UnusedDecl found");
    auto r5 = lookupError(err::NotNamespace);
    CHECK(r5.has_value(), "NotNamespace found");
    auto r6 = lookupError(err::NoMember);
    CHECK(r6.has_value(), "NoMember found");
}

static void test_lookup_all_types() {
    auto r1 = lookupError(err::TypeMismatch);
    CHECK(r1.has_value(), "TypeMismatch found");
    if (r1)
        CHECK_EQ(r1->category, "types", "TypeMismatch category");

    auto r2 = lookupError(err::CannotInfer);
    CHECK(r2.has_value(), "CannotInfer found");
    auto r3 = lookupError(err::InvalidCast);
    CHECK(r3.has_value(), "InvalidCast found");
    auto r4 = lookupError(err::CyclicType);
    CHECK(r4.has_value(), "CyclicType found");
}

static void test_lookup_all_ownership() {
    auto r1 = lookupError(err::UseAfterMove);
    CHECK(r1.has_value(), "UseAfterMove found");
    if (r1)
        CHECK_EQ(r1->category, "ownership", "UseAfterMove category");

    auto r2 = lookupError(err::BorrowConflict);
    CHECK(r2.has_value(), "BorrowConflict found");
    auto r3 = lookupError(err::DoubleBorrow);
    CHECK(r3.has_value(), "DoubleBorrow found");
}

static void test_lookup_all_mir() {
    auto r1 = lookupError(err::InvalidIR);
    CHECK(r1.has_value(), "InvalidIR found");
    if (r1)
        CHECK_EQ(r1->category, "mir", "InvalidIR category");

    auto r2 = lookupError(err::Unreachable);
    CHECK(r2.has_value(), "Unreachable found");
}

static void test_lookup_all_runtime() {
    auto r1 = lookupError(err::IndexOutOfBounds);
    CHECK(r1.has_value(), "IndexOutOfBounds found");
    if (r1) {
        CHECK_EQ(r1->prefix, 'R', "IndexOutOfBounds prefix");
        CHECK_EQ(r1->category, "runtime", "IndexOutOfBounds category");
    }

    auto r2 = lookupError(err::DivisionByZero);
    CHECK(r2.has_value(), "DivisionByZero found");
    auto r3 = lookupError(err::NullDeref);
    CHECK(r3.has_value(), "NullDeref found");
    auto r4 = lookupError(err::Panic);
    CHECK(r4.has_value(), "Panic found");
}

static void test_lookup_unknown() {
    auto r = lookupError(99999);
    CHECK(!r.has_value(), "unknown code returns nullopt");
}

static void test_lookup_zero() {
    auto r = lookupError(0);
    CHECK(!r.has_value(), "code 0 returns nullopt");
}

int main() {
    std::printf("error-codes-lookup tests\n");
    std::printf("========================\n\n");

    test_lookup_all_lexical();
    test_lookup_all_parse();
    test_lookup_all_semantic();
    test_lookup_all_types();
    test_lookup_all_ownership();
    test_lookup_all_mir();
    test_lookup_all_runtime();
    test_lookup_unknown();
    test_lookup_zero();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
