#include "Behavior/Execution.h"
#include "Behavior/FrameStore.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

class FakeExecutionAdapter final : public ExecutionAdapter {
public:
    struct Input {
        std::string Name;
        bool Active = false;
    };

    explicit FakeExecutionAdapter(std::vector<std::string> inputs = {"In"}) {
        for (std::string &name : inputs)
            Inputs.push_back({std::move(name), false});
        Outputs.push_back({"Out", false});
    }

    bool Resolve(const ExecutionInput &input, ResolvedInput &resolved,
                 ExecutionFault &fault) override {
        if (input.Selector == InputSelector::Index) {
            if (input.LayoutGeneration != LayoutGeneration) {
                fault = {ExecutionError::LayoutStale, 0,
                         "The indexed input belongs to an older layout."};
                return false;
            }
            if (input.Index < 0 || input.Index >= static_cast<int>(Inputs.size())) {
                fault = {ExecutionError::SelectorNotFound, 0,
                         "The indexed input does not exist."};
                return false;
            }
            resolved = {input.Index, Inputs[input.Index].Name,
                        Occurrence(input.Index)};
            return true;
        }

        std::vector<int> matches;
        for (int index = 0; index < static_cast<int>(Inputs.size()); ++index) {
            if (Inputs[index].Name == input.Name)
                matches.push_back(index);
        }
        if (matches.empty()) {
            fault = {ExecutionError::SelectorNotFound, 0,
                     "The named input does not exist."};
            return false;
        }
        if (input.RequireUnique && matches.size() != 1) {
            fault = {ExecutionError::SelectorAmbiguous, 0,
                     "The named input is ambiguous."};
            return false;
        }
        if (input.Occurrence < 0 ||
            input.Occurrence >= static_cast<int>(matches.size())) {
            fault = {ExecutionError::SelectorNotFound, 0,
                     "The named input occurrence does not exist."};
            return false;
        }
        const int index = matches[input.Occurrence];
        resolved = {index, Inputs[index].Name, input.Occurrence};
        return true;
    }

    bool Activate(const ResolvedInput &input, ExecutionFault &fault) override {
        if (input.Index < 0 || input.Index >= static_cast<int>(Inputs.size())) {
            fault = {ExecutionError::ActivationFailed, 0,
                     "Resolved input is no longer valid."};
            return false;
        }
        Inputs[input.Index].Active = true;
        Activated.push_back(input.Index);
        return true;
    }

    NativeExecution Execute() override {
        ++Calls;
        if (OnExecute)
            OnExecute();

        if (WaitForAll) {
            const bool all = !Inputs.empty() &&
                std::all_of(Inputs.begin(), Inputs.end(),
                            [](const Input &input) { return input.Active; });
            if (all) {
                for (Input &input : Inputs)
                    input.Active = false;
                Outputs[0].Active = true;
                return {0, false, false, false, false, {}};
            }
            return {1, true, true, false, false, {}};
        }

        if (Native.empty())
            return {0, false, false, false, false, {}};
        NativeExecution result = std::move(Native.front());
        Native.pop_front();
        return result;
    }

    bool ReadOutputs(std::vector<ExecutionOutput> &activeOutputs,
                     std::vector<Pout> &pouts,
                     ExecutionFault &fault) override {
        for (int index = 0; index < static_cast<int>(Outputs.size()); ++index) {
            if (Outputs[index].Active)
                activeOutputs.push_back({index, Outputs[index].Name, 0});
        }
        pouts = Pouts;
        if (OnRead)
            OnRead(pouts);
        if (ReadFailure) {
            fault = *ReadFailure;
            return false;
        }
        return true;
    }

    bool ClearOutputs(const std::vector<ExecutionOutput> &outputs,
                      ExecutionFault &) override {
        if (OnClear)
            OnClear(outputs);
        for (const ExecutionOutput &output : outputs)
            Outputs[output.Index].Active = false;
        return true;
    }

    int Occurrence(int index) const {
        int occurrence = 0;
        for (int current = 0; current < index; ++current) {
            if (Inputs[current].Name == Inputs[index].Name)
                ++occurrence;
        }
        return occurrence;
    }

    std::uint64_t LayoutGeneration = 1;
    bool WaitForAll = false;
    int Calls = 0;
    std::vector<Input> Inputs;
    std::vector<Input> Outputs;
    std::vector<int> Activated;
    std::deque<NativeExecution> Native;
    std::function<void()> OnExecute;
    std::vector<Pout> Pouts;
    std::optional<ExecutionFault> ReadFailure;
    std::function<void(std::vector<Pout> &)> OnRead;
    std::function<void(const std::vector<ExecutionOutput> &)> OnClear;
};

NativeExecution FunctionResult(int code, bool retry = false,
                               bool error = false, bool breakpoint = false) {
    return {code, retry, retry, error, breakpoint, {}};
}

NativeExecution GraphResult(int code, bool active, bool error = false) {
    return {code, false, active, error, false, {}};
}

TEST(BehaviorExecution, WaitForAllKeepsInputsAndQueuesSameFramePulse) {
    Execution execution;
    FakeExecutionAdapter adapter({"A", "B"});
    adapter.WaitForAll = true;

    ExecutionResult first = execution.Pulse(ExecutionInput::Named("A"), 10, adapter);
    ASSERT_EQ(first.State, AdmissionState::Executed);
    ASSERT_TRUE(first.Frame);
    EXPECT_TRUE(first.Frame->NativeContinuation);
    EXPECT_FALSE(first.Frame->QueuedInput);
    EXPECT_TRUE(adapter.Inputs[0].Active);
    EXPECT_FALSE(adapter.Inputs[1].Active);

    ExecutionResult second = execution.Pulse(ExecutionInput::Named("B"), 10, adapter);
    EXPECT_EQ(second.State, AdmissionState::Queued);
    EXPECT_EQ(adapter.Calls, 1);
    EXPECT_FALSE(adapter.Inputs[1].Active);

    ExecutionResult completed = execution.Step(11, adapter);
    ASSERT_EQ(completed.State, AdmissionState::Executed);
    ASSERT_TRUE(completed.Frame);
    EXPECT_FALSE(completed.Frame->NativeContinuation);
    EXPECT_FALSE(completed.Frame->QueuedInput);
    ASSERT_EQ(completed.Frame->ActiveOutputs.size(), 1u);
    EXPECT_EQ(completed.Frame->ActiveOutputs[0].Name, "Out");
    EXPECT_FALSE(adapter.Inputs[0].Active);
    EXPECT_FALSE(adapter.Inputs[1].Active);
    EXPECT_FALSE(adapter.Outputs[0].Active);
    EXPECT_EQ(adapter.Calls, 2);
}

TEST(BehaviorExecution, CoalescesSameInputAndPreservesFirstAdmissionOrder) {
    Execution execution;
    FakeExecutionAdapter adapter({"A", "B", "C"});
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.Native.push_back(FunctionResult(0));

    ASSERT_EQ(execution.Pulse(ExecutionInput::Named("A"), 1, adapter).State,
              AdmissionState::Executed);
    EXPECT_EQ(execution.Pulse(ExecutionInput::Named("C"), 1, adapter).State,
              AdmissionState::Queued);
    EXPECT_EQ(execution.Pulse(ExecutionInput::Named("B"), 1, adapter).State,
              AdmissionState::Queued);
    EXPECT_EQ(execution.Pulse(ExecutionInput::Named("C"), 1, adapter).State,
              AdmissionState::Queued);

    ASSERT_EQ(execution.Step(2, adapter).State, AdmissionState::Executed);
    ASSERT_EQ(adapter.Activated.size(), 3u);
    EXPECT_EQ(adapter.Activated[0], 0);
    EXPECT_EQ(adapter.Activated[1], 2);
    EXPECT_EQ(adapter.Activated[2], 1);
}

TEST(BehaviorExecution, ReentrantPulseRunsOnTheNextFrameOnly) {
    Execution execution;
    FakeExecutionAdapter adapter({"A", "B"});
    adapter.Native.push_back(FunctionResult(0));
    adapter.Native.push_back(FunctionResult(0));
    adapter.OnExecute = [&] {
        if (adapter.Calls == 1) {
            EXPECT_EQ(execution.Pulse(ExecutionInput::Named("B"), 4, adapter).State,
                      AdmissionState::Queued);
        }
    };

    ExecutionResult first = execution.Pulse(ExecutionInput::Named("A"), 4, adapter);
    ASSERT_TRUE(first.Frame);
    EXPECT_FALSE(first.Frame->NativeContinuation);
    EXPECT_TRUE(first.Frame->QueuedInput);
    EXPECT_EQ(execution.State(), ExecutionState::Pending);
    EXPECT_EQ(adapter.Calls, 1);

    EXPECT_EQ(execution.Step(4, adapter).State, AdmissionState::Queued);
    EXPECT_EQ(adapter.Calls, 1);
    EXPECT_EQ(execution.Step(5, adapter).State, AdmissionState::Executed);
    EXPECT_EQ(adapter.Calls, 2);
}

TEST(BehaviorExecution, UsesPostExecuteNativeActivityForFunctionsAndGraphs) {
    Execution function;
    FakeExecutionAdapter functionAdapter;
    functionAdapter.Native.push_back(FunctionResult(7, true));
    functionAdapter.Native.push_back(FunctionResult(0));
    const ExecutionResult functionFirst =
        function.Pulse(ExecutionInput::At(0, 1), 1, functionAdapter);
    ASSERT_TRUE(functionFirst);
    ASSERT_TRUE(functionFirst.Frame);
    EXPECT_TRUE(functionFirst.Frame->NativeContinuation);
    EXPECT_EQ(function.State(), ExecutionState::Pending);
    ASSERT_TRUE(function.Step(2, functionAdapter));
    EXPECT_EQ(function.State(), ExecutionState::Idle);

    Execution graph;
    FakeExecutionAdapter graphAdapter;
    graphAdapter.Native.push_back(GraphResult(0, true));
    graphAdapter.Native.push_back(GraphResult(0, false));
    const ExecutionResult graphFirst =
        graph.Pulse(ExecutionInput::At(0, 1), 1, graphAdapter);
    ASSERT_TRUE(graphFirst);
    ASSERT_TRUE(graphFirst.Frame);
    EXPECT_TRUE(graphFirst.Frame->NativeContinuation);
    EXPECT_EQ(graph.State(), ExecutionState::Pending);
    ASSERT_TRUE(graph.Step(2, graphAdapter));
    EXPECT_EQ(graph.State(), ExecutionState::Idle);
}

TEST(BehaviorExecution, CallRequiresExplicitContinueWithoutRepeatingFirstInput) {
    Execution execution;
    FakeExecutionAdapter adapter;
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.Native.push_back(FunctionResult(0));

    ExecutionResult first = execution.Call(
        ExecutionInput::At(0, 1), 7, adapter);
    ASSERT_EQ(first.State, AdmissionState::Executed);
    EXPECT_EQ(execution.State(), ExecutionState::Pending);
    EXPECT_FALSE(execution.Managed());
    EXPECT_FALSE(execution.NeedsFrame());
    ASSERT_EQ(adapter.Activated.size(), 1u);

    execution.Continue();
    EXPECT_TRUE(execution.NeedsFrame());
    ASSERT_TRUE(execution.Step(8, adapter));
    EXPECT_EQ(execution.State(), ExecutionState::Idle);
    EXPECT_EQ(adapter.Calls, 2);
    EXPECT_EQ(adapter.Activated.size(), 1u);
}

TEST(BehaviorExecution, RetryErrorContinuesButFatalAndBreakClose) {
    Execution retry;
    FakeExecutionAdapter retryAdapter;
    retryAdapter.Native.push_back(FunctionResult(9, true, true));
    ExecutionResult retryResult = retry.Pulse(ExecutionInput::At(0, 1), 1,
                                              retryAdapter);
    ASSERT_TRUE(retryResult.Frame);
    EXPECT_EQ(retryResult.Frame->Fault.Code, ExecutionError::NativeFailed);
    EXPECT_TRUE(retryResult.Frame->NativeContinuation);
    EXPECT_EQ(retry.State(), ExecutionState::Pending);

    Execution fatal;
    FakeExecutionAdapter fatalAdapter;
    fatalAdapter.Native.push_back(FunctionResult(10, false, true));
    ExecutionResult fatalResult = fatal.Pulse(ExecutionInput::At(0, 1), 1,
                                              fatalAdapter);
    ASSERT_TRUE(fatalResult.Frame);
    EXPECT_FALSE(fatalResult.Frame->NativeContinuation);
    EXPECT_FALSE(fatalResult.Frame->QueuedInput);
    EXPECT_EQ(fatal.State(), ExecutionState::Failed);

    Execution breakpoint;
    FakeExecutionAdapter breakAdapter;
    breakAdapter.Native.push_back(FunctionResult(11, false, false, true));
    ExecutionResult breakResult = breakpoint.Pulse(ExecutionInput::At(0, 1), 1,
                                                   breakAdapter);
    ASSERT_TRUE(breakResult.Frame);
    EXPECT_EQ(breakResult.Frame->Fault.Code, ExecutionError::UnsupportedBreak);
    EXPECT_FALSE(breakResult.Frame->NativeContinuation);
    EXPECT_FALSE(breakResult.Frame->QueuedInput);
    EXPECT_EQ(breakpoint.State(), ExecutionState::Failed);
    EXPECT_FALSE(breakpoint.NeedsFrame());
}

TEST(BehaviorExecution, IndexedQueuedInputFailsClosedAfterLayoutChange) {
    Execution execution;
    FakeExecutionAdapter adapter({"A", "B"});
    adapter.Native.push_back(FunctionResult(1, true));

    ASSERT_TRUE(execution.Pulse(ExecutionInput::At(0, 1), 1, adapter));
    ASSERT_EQ(execution.Pulse(ExecutionInput::At(1, 1), 1, adapter).State,
              AdmissionState::Queued);
    adapter.LayoutGeneration = 2;

    ExecutionResult drift = execution.Step(2, adapter);
    EXPECT_EQ(drift.State, AdmissionState::Failed);
    EXPECT_EQ(drift.Fault.Code, ExecutionError::LayoutStale);
    EXPECT_EQ(execution.State(), ExecutionState::Failed);
    EXPECT_EQ(execution.NextSequence(), 2u);
    EXPECT_EQ(adapter.Calls, 1);
}

TEST(BehaviorExecution, NamedQueuedInputReresolvesAfterLayoutChange) {
    Execution execution;
    FakeExecutionAdapter adapter({"A", "B"});
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.Native.push_back(FunctionResult(0));

    ASSERT_TRUE(execution.Pulse(ExecutionInput::Named("A"), 1, adapter));
    ASSERT_EQ(execution.Pulse(ExecutionInput::Named("B"), 1, adapter).State,
              AdmissionState::Queued);
    adapter.LayoutGeneration = 2;
    std::swap(adapter.Inputs[0], adapter.Inputs[1]);

    ASSERT_TRUE(execution.Step(2, adapter));
    ASSERT_GE(adapter.Activated.size(), 2u);
    EXPECT_EQ(adapter.Activated.back(), 0);
}

TEST(BehaviorExecution, RetentionPoliciesKeepTheLastNonContinuingFrame) {
    Execution signals(FrameRetention::Signals(4));
    FakeExecutionAdapter signalsAdapter;
    signalsAdapter.Native.push_back(FunctionResult(1, true));
    signalsAdapter.Native.push_back(FunctionResult(1, true));
    signalsAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(signals.Pulse(ExecutionInput::At(0, 1), 1, signalsAdapter));
    ASSERT_TRUE(signals.Step(2, signalsAdapter));
    ASSERT_TRUE(signals.Step(3, signalsAdapter));
    auto signalFrames = signals.Take();
    ASSERT_EQ(signalFrames.size(), 2u);
    EXPECT_EQ(signalFrames[0].Sequence, 1u);
    EXPECT_EQ(signalFrames[1].Sequence, 3u);

    Execution each(FrameRetention::EachFrame(4));
    FakeExecutionAdapter eachAdapter;
    eachAdapter.Native.push_back(FunctionResult(1, true));
    eachAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(each.Pulse(ExecutionInput::At(0, 1), 1, eachAdapter));
    ASSERT_TRUE(each.Step(2, eachAdapter));
    EXPECT_EQ(each.Take().size(), 2u);

    Execution latest(FrameRetention::Latest());
    FakeExecutionAdapter latestAdapter;
    latestAdapter.Native.push_back(FunctionResult(1, true));
    latestAdapter.Native.push_back(FunctionResult(1, true));
    latestAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(latest.Pulse(ExecutionInput::At(0, 1), 1, latestAdapter));
    ASSERT_TRUE(latest.Step(2, latestAdapter));
    ASSERT_TRUE(latest.Step(3, latestAdapter));
    auto latestFrames = latest.Take();
    ASSERT_EQ(latestFrames.size(), 2u);
    EXPECT_EQ(latestFrames[0].Sequence, 2u);
    EXPECT_EQ(latestFrames[1].Sequence, 3u);

    Execution ignore(FrameRetention::Ignore());
    FakeExecutionAdapter ignoreAdapter;
    ignoreAdapter.Native.push_back(FunctionResult(1, true));
    ignoreAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(ignore.Pulse(ExecutionInput::At(0, 1), 1, ignoreAdapter));
    ASSERT_TRUE(ignore.Step(2, ignoreAdapter));
    auto ignored = ignore.Take();
    ASSERT_EQ(ignored.size(), 1u);
    EXPECT_FALSE(ignored[0].NativeContinuation);
    EXPECT_FALSE(ignored[0].QueuedInput);
    EXPECT_EQ(ignored[0].Sequence, 2u);
}

TEST(BehaviorExecution, FullFrameQueueUsesIndependentFailureSlot) {
    Execution execution(FrameRetention::EachFrame(1));
    FakeExecutionAdapter adapter;
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.Native.push_back(FunctionResult(1, true));

    ASSERT_TRUE(execution.Pulse(ExecutionInput::At(0, 1), 1, adapter));
    ASSERT_TRUE(execution.Step(2, adapter));
    EXPECT_EQ(execution.State(), ExecutionState::Failed);
    EXPECT_EQ(execution.Failure().Code, ExecutionError::FrameQueueFull);

    auto frames = execution.Take();
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].Sequence, 1u);
    EXPECT_EQ(frames[1].Sequence, 2u);
    EXPECT_EQ(frames[1].Fault.Code, ExecutionError::FrameQueueFull);
    ASSERT_TRUE(frames[1].Overflow);
    EXPECT_EQ(frames[1].Overflow->Capacity, 1u);
}

TEST(BehaviorExecution, QueueFullPreservesTheDroppedNativeFault) {
    Execution execution(FrameRetention::EachFrame(0));
    FakeExecutionAdapter adapter;
    adapter.Native.push_back(FunctionResult(10, false, true));

    ASSERT_TRUE(execution.Pulse(ExecutionInput::At(0, 1), 1, adapter));
    auto frames = execution.Take();
    ASSERT_EQ(frames.size(), 1u);
    ASSERT_TRUE(frames[0].Overflow);
    EXPECT_EQ(frames[0].Fault.Code, ExecutionError::FrameQueueFull);
    EXPECT_EQ(frames[0].Overflow->Cause.Code, ExecutionError::NativeFailed);
    EXPECT_EQ(frames[0].Overflow->Cause.NativeCode, 10);
}

TEST(BehaviorExecution, CloseDuringExecuteDefersStateTransition) {
    Execution execution;
    FakeExecutionAdapter adapter;
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.OnExecute = [&] { execution.RequestClose(); };

    ExecutionResult result = execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
    ASSERT_TRUE(result.Frame);
    EXPECT_FALSE(result.Frame->NativeContinuation);
    EXPECT_FALSE(result.Frame->QueuedInput);
    EXPECT_EQ(result.Frame->Fault.Code, ExecutionError::Cancelled);
    EXPECT_EQ(execution.State(), ExecutionState::Closing);
    execution.MarkClosed();
    EXPECT_EQ(execution.State(), ExecutionState::Closed);
}

TEST(BehaviorExecution, ReadsActiveOutAndPoutBeforeClearingTheOut) {
    Execution execution;
    FakeExecutionAdapter adapter;
    adapter.Outputs[0].Active = true;
    Pout value;
    value.Index = 0;
    value.Name = "Value";
    value.TypeGuid1 = 0x5a5716fd;
    value.TypeGuid2 = 0x44e276d7;
    value.Kind = PoutKind::Int32;
    value.Int32 = 42;
    adapter.Pouts.push_back(value);
    adapter.OnClear = [&](const std::vector<ExecutionOutput> &) {
        ASSERT_EQ(adapter.Pouts.size(), 1u);
        EXPECT_EQ(adapter.Pouts[0].Int32, 42);
    };

    ExecutionResult result =
        execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
    ASSERT_TRUE(result.Frame);
    ASSERT_EQ(result.Frame->ActiveOutputs.size(), 1u);
    ASSERT_EQ(result.Frame->Pouts.size(), 1u);
    EXPECT_EQ(result.Frame->Pouts[0].Int32, 42);
    EXPECT_FALSE(adapter.Outputs[0].Active);
}

TEST(BehaviorExecution, PoutValuesAreOwnedAndKeepNameOccurrences) {
    Execution execution;
    FakeExecutionAdapter adapter;
    Pout first;
    first.Index = 0;
    first.Name = "Value";
    first.Occurrence = 0;
    first.Kind = PoutKind::Utf8;
    first.Text = "first";
    Pout second = first;
    second.Index = 1;
    second.Occurrence = 1;
    second.Text = "second";
    adapter.Pouts = {first, second};

    ASSERT_TRUE(execution.Pulse(ExecutionInput::At(0, 1), 1, adapter));
    adapter.Pouts[0].Text = "mutated";
    auto frames = execution.Take();
    ASSERT_EQ(frames.size(), 1u);
    ASSERT_EQ(frames[0].Pouts.size(), 2u);
    EXPECT_EQ(frames[0].Pouts[0].Text, "first");
    EXPECT_EQ(frames[0].Pouts[1].Occurrence, 1);
    EXPECT_EQ(frames[0].Pouts[1].Text, "second");
}

TEST(BehaviorExecution, UnsupportedPoutBeforeExecuteCreatesNoFrame) {
    Execution execution;
    FakeExecutionAdapter adapter;
    NativeExecution rejected;
    rejected.Executed = false;
    rejected.Fault = {ExecutionError::UnsupportedPout, 1,
                      "unsupported Pout"};
    adapter.Native.push_back(rejected);

    ExecutionResult result =
        execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
    EXPECT_EQ(result.State, AdmissionState::Failed);
    EXPECT_EQ(result.Fault.Code, ExecutionError::UnsupportedPout);
    EXPECT_EQ(execution.NextSequence(), 1u);
    EXPECT_TRUE(execution.Take().empty());
}

TEST(BehaviorExecution, DynamicPoutFailureKeepsOutAndUsesNativeSequence) {
    Execution execution;
    FakeExecutionAdapter adapter;
    adapter.Outputs[0].Active = true;
    adapter.Pouts.push_back(Pout{});
    adapter.ReadFailure = {ExecutionError::UnsupportedPout, 2,
                           "dynamic Pout type"};

    ExecutionResult result =
        execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
    ASSERT_TRUE(result.Frame);
    EXPECT_EQ(result.Frame->Sequence, 1u);
    EXPECT_FALSE(result.Frame->NativeContinuation);
    EXPECT_FALSE(result.Frame->QueuedInput);
    EXPECT_EQ(result.Frame->Fault.Code, ExecutionError::UnsupportedPout);
    ASSERT_EQ(result.Frame->ActiveOutputs.size(), 1u);
    EXPECT_TRUE(result.Frame->Pouts.empty());
    EXPECT_EQ(execution.NextSequence(), 2u);
}

TEST(BehaviorExecution, FrameStoreReadsWithoutConsumingThenConsumesExactly) {
    auto frames =
        std::make_shared<FrameStore>(FrameRetention::EachFrame(4));
    Execution execution(frames);
    FakeExecutionAdapter adapter;
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.Native.push_back(FunctionResult(0));

    ASSERT_TRUE(execution.Pulse(ExecutionInput::At(0, 1), 1, adapter));
    ASSERT_TRUE(execution.Step(2, adapter));
    const auto firstRead = frames->Read();
    const auto secondRead = frames->Read();
    ASSERT_EQ(firstRead.size(), 2u);
    EXPECT_EQ(secondRead.size(), firstRead.size());
    EXPECT_EQ(secondRead[0].Sequence, firstRead[0].Sequence);

    const std::array<std::uint64_t, 2> wrong{1, 3};
    EXPECT_FALSE(frames->Consume(wrong));
    EXPECT_EQ(frames->Read().size(), 2u);
    const std::array<std::uint64_t, 2> exact{1, 2};
    EXPECT_TRUE(frames->Consume(exact));
    EXPECT_TRUE(frames->Read().empty());
}

} // namespace
