#include <gtest/gtest.h>

#include "AngelScript/ScriptModState.h"

namespace BML {
namespace Test {

TEST(ScriptModStateTest, ClearFailureClearsStaleDiagnostics) {
    ScriptModState state;
    ScriptDiagnostic diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Compile, "Compile failed");
    diagnostic.RawMessage = "old compiler output";

    state.Fail(diagnostic);
    ASSERT_TRUE(state.IsFailed());
    ASSERT_FALSE(state.GetLastDiagnosticText().empty());

    state.ClearFailure();

    EXPECT_FALSE(state.IsFailed());
    EXPECT_TRUE(state.GetLastDiagnosticText().empty());
    EXPECT_TRUE(state.GetLastDiagnostic().Message.empty());
    EXPECT_TRUE(state.GetLastDiagnostic().RawMessage.empty());
}

} // namespace Test
} // namespace BML
