/*
 * Copyright (c) Meta Platforms, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "axiom/optimizer/tests/ExprPrinters.h"
#include <gtest/gtest.h>
#include "velox/common/memory/Memory.h"
#include "velox/functions/prestosql/registration/RegistrationFunctions.h"
#include "velox/parse/ExpressionsParser.h"
#include "velox/parse/TypeResolver.h"

namespace facebook::velox::core {
namespace {

class ExprPrintersTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    memory::MemoryManager::initialize({});
    functions::prestosql::registerAllScalarFunctions();
  }

  void SetUp() override {
    parse::registerTypeResolver();
    pool_ = memory::memoryManager()->addLeafPool();

    // Create RowType with test columns:
    // bigint_column BIGINT
    // varchar_column VARCHAR
    // bigint_array_column ARRAY(BIGINT)
    // varchar_array_column ARRAY(VARCHAR)
    rowType_ = ROW(
        {{"bigint_column", BIGINT()},
         {"varchar_column", VARCHAR()},
         {"bigint_array_column", ARRAY(BIGINT())},
         {"varchar_array_column", ARRAY(VARCHAR())}});
  }

  // Parse expression string to IExpr (untyped)
  ExprPtr parseToUntyped(const std::string& exprStr) {
    parse::ParseOptions options;
    return parse::parseExpr(exprStr, options);
  }

  // Convert IExpr to ITypedExpr
  TypedExprPtr convertToTyped(const ExprPtr& untypedExpr) {
    return Expressions::inferTypes(untypedExpr, rowType_, pool_.get());
  }

  // Test that parsing, converting to typed, and printing produces valid output
  void testExpressionRoundTrip(const std::string& exprStr) {
    // Parse to untyped expression
    auto untypedExpr = parseToUntyped(exprStr);
    ASSERT_NE(untypedExpr, nullptr) << "Failed to parse: " << exprStr;

    // Convert to typed expression
    auto typedExpr = convertToTyped(untypedExpr);
    ASSERT_NE(typedExpr, nullptr) << "Failed to convert to typed: " << exprStr;

    // Print both using ExprPrinters
    std::string untypedText = IExprPrinter::toText(*untypedExpr);
    std::string typedText = ITypedExprPrinter::toText(*typedExpr);

    // Verify the untyped text is non-empty
    EXPECT_FALSE(untypedText.empty())
        << "Untyped printer produced empty text for: " << exprStr;

    // Verify the typed text is non-empty
    EXPECT_FALSE(typedText.empty())
        << "Typed printer produced empty text for: " << exprStr;

    // Parse the printed untyped text back to untyped expression
    auto reparsedUntyped = parseToUntyped(untypedText);
    ASSERT_NE(reparsedUntyped, nullptr)
        << "Failed to reparse untyped printed text: " << untypedText;

    // Print the reparsed expression
    std::string reparsedUntypedText = IExprPrinter::toText(*reparsedUntyped);

    // The reparsed text should match the original printed text
    EXPECT_EQ(untypedText, reparsedUntypedText)
        << "Reparsed untyped text doesn't match original for: " << exprStr
        << "\nOriginal: " << untypedText
        << "\nReparsed: " << reparsedUntypedText;
  }

  std::shared_ptr<memory::MemoryPool> pool_;
  RowTypePtr rowType_;
};

TEST_F(ExprPrintersTest, orExpression) {
  testExpressionRoundTrip("bigint_column < 11 or bigint_column > 22");
}

TEST_F(ExprPrintersTest, inWithNull) {
  testExpressionRoundTrip("varchar_column in ('s1', 's2', null)");
}

TEST_F(ExprPrintersTest, inWithNumbers) {
  testExpressionRoundTrip("bigint_column in (1, 2, 3)");
}

TEST_F(ExprPrintersTest, arrayWithNull) {
  testExpressionRoundTrip("varchar_array_column = array['s1', null]");
}

} // namespace
} // namespace facebook::velox::core
