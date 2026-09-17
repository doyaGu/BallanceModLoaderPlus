#include <gtest/gtest.h>

#include "AngelScript/ScriptDevEvents.h"

namespace BML {
namespace Test {

namespace {

ScriptDevEvent MakeEvent(std::string code, std::string modId, std::string message) {
    ScriptDevEvent event;
    event.Code = std::move(code);
    event.ModId = std::move(modId);
    event.Message = std::move(message);
    return event;
}

} // namespace

TEST(ScriptDevEventRingBufferTest, DropsOldestEventsAndKeepsMonotonicSequence) {
    ScriptDevEventRingBuffer buffer(3);

    EXPECT_EQ(1u, buffer.Append(MakeEvent("A", "mod", "one")));
    EXPECT_EQ(2u, buffer.Append(MakeEvent("B", "mod", "two")));
    EXPECT_EQ(3u, buffer.Append(MakeEvent("C", "mod", "three")));
    EXPECT_EQ(4u, buffer.Append(MakeEvent("D", "mod", "four")));
    EXPECT_EQ(5u, buffer.Append(MakeEvent("E", "mod", "five")));

    std::vector<ScriptDevEvent> snapshot = buffer.Snapshot();
    ASSERT_EQ(3u, snapshot.size());
    EXPECT_EQ(3u, snapshot[0].Sequence);
    EXPECT_EQ("three", snapshot[0].Message);
    EXPECT_EQ(4u, snapshot[1].Sequence);
    EXPECT_EQ("four", snapshot[1].Message);
    EXPECT_EQ(5u, snapshot[2].Sequence);
    EXPECT_EQ("five", snapshot[2].Message);
    EXPECT_EQ(2u, buffer.GetDroppedCount());
    EXPECT_EQ(5u, buffer.GetGeneration());

    buffer.Clear();
    EXPECT_TRUE(buffer.Snapshot().empty());
    EXPECT_EQ(0u, buffer.GetDroppedCount());
    EXPECT_EQ(6u, buffer.GetGeneration());
    EXPECT_EQ(6u, buffer.Append(MakeEvent("F", "mod", "six")));
}

TEST(ScriptDevEventFilterTest, MatchesReloadAttemptSelectedModSourceAndText) {
    std::vector<ScriptDevEvent> events;

    ScriptDevEvent log = MakeEvent("LogLine", "BML", "regular log");
    log.SourcePath = "ModLoader.log";
    events.push_back(log);

    ScriptDevEvent reload = MakeEvent("ScriptReloadDiagnostic", "hello.script", "compile failed");
    reload.Severity = ScriptDevEventSeverity::Error;
    reload.Phase = "compile";
    reload.SourcePath = "Mods/HelloMod.mod.as";
    reload.ReloadAttemptId = 7;
    reload.Fields.push_back({"stack", "line 12"});
    events.push_back(reload);

    ScriptDevLogFilters filters;
    filters.Severity = "error";
    filters.Source = "HelloMod";
    filters.Attempt = "7";
    filters.Text = "line 12";
    filters.SelectedModId = "hello.script";
    filters.SelectedModOnly = true;

    std::vector<ScriptDevEvent> filtered = FilterScriptDevEvents(events, filters);
    ASSERT_EQ(1u, filtered.size());
    EXPECT_EQ("ScriptReloadDiagnostic", filtered[0].Code);
    EXPECT_EQ(7u, filtered[0].ReloadAttemptId);
}

TEST(ScriptDevEventFilterTest, ReloadOnlyIncludesReloadPhaseAndScriptReloadCodes) {
    std::vector<ScriptDevEvent> events;
    events.push_back(MakeEvent("LogLine", "hello.script", "regular log"));

    ScriptDevEvent byPhase = MakeEvent("Custom", "hello.script", "phase reload");
    byPhase.Phase = "reload";
    events.push_back(byPhase);

    ScriptDevEvent byCode = MakeEvent("ScriptReloadCommitted", "hello.script", "code reload");
    byCode.Phase = "other";
    events.push_back(byCode);

    ScriptDevLogFilters filters;
    filters.ReloadOnly = true;
    std::vector<ScriptDevEvent> filtered = FilterScriptDevEvents(events, filters);

    ASSERT_EQ(2u, filtered.size());
    EXPECT_EQ("Custom", filtered[0].Code);
    EXPECT_EQ("ScriptReloadCommitted", filtered[1].Code);
}

TEST(ScriptDevEventFilterTest, LimitKeepsNewestMatchingEvents) {
    std::vector<ScriptDevEvent> events;
    for (int i = 0; i < 5; ++i) {
        ScriptDevEvent event = MakeEvent("LogLine", "hello.script", "message " + std::to_string(i));
        event.Sequence = static_cast<uint64_t>(i + 1);
        events.push_back(event);
    }

    ScriptDevLogFilters filters;
    filters.SelectedModId = "hello.script";
    filters.SelectedModOnly = true;
    std::vector<ScriptDevEvent> filtered = FilterScriptDevEvents(events, filters, 2);

    ASSERT_EQ(2u, filtered.size());
    EXPECT_EQ(4u, filtered[0].Sequence);
    EXPECT_EQ(5u, filtered[1].Sequence);
}

} // namespace Test
} // namespace BML
