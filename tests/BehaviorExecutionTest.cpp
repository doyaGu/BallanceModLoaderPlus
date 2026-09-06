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

using namespace BML::Behavior::Internal;

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
        ++ResolveCalls;
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
            resolved = {input.Index};
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
        resolved = {index};
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
                     ExecutionFault &) override {
        ++OutputReads;
        for (int index = 0; index < static_cast<int>(Outputs.size()); ++index) {
            if (Outputs[index].Active)
                activeOutputs.push_back({index, Outputs[index].Name, 0});
        }
        return true;
    }

    bool ReadPouts(std::vector<Pout> &pouts,
                   ExecutionFault &fault) override {
        ++PoutReads;
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
                      ExecutionFault &fault) override {
        if (OnClear)
            OnClear(outputs);
        if (ClearFailure) {
            fault = *ClearFailure;
            return false;
        }
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
    int ResolveCalls = 0;
    int OutputReads = 0;
    int PoutReads = 0;
    std::vector<Input> Inputs;
    std::vector<Input> Outputs;
    std::vector<int> Activated;
    std::deque<NativeExecution> Native;
    std::function<void()> OnExecute;
    std::vector<Pout> Pouts;
    std::optional<ExecutionFault> ReadFailure;
    std::optional<ExecutionFault> ClearFailure;
    std::function<void(std::vector<Pout> &)> OnRead;
    std::function<void(const std::vector<ExecutionOutput> &)> OnClear;
};

TEST(BehaviorExecution, ResolvesImmediateInputOnceAndQueuedInputAgainAtExecution) {
    Execution execution(FrameRetention::Ignore());
    FakeExecutionAdapter adapter({"A", "B"});

    ASSERT_TRUE(execution.Pulse(ExecutionInput::Named("A"), 1, adapter));
    EXPECT_EQ(adapter.ResolveCalls, 1);

    ASSERT_EQ(execution.Pulse(ExecutionInput::Named("B"), 1, adapter).State,
              AdmissionState::Queued);
    EXPECT_EQ(adapter.ResolveCalls, 2);

    ASSERT_TRUE(execution.Step(2, adapter));
    EXPECT_EQ(adapter.ResolveCalls, 3);
}

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
    EXPECT_EQ(completed.Frame->ActiveOutputs[0], 0);
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
    EXPECT_EQ(retryResult.Fault.Code, ExecutionError::NativeFailed);
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
    EXPECT_EQ(breakResult.Fault.Code, ExecutionError::UnsupportedBreak);
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
    EXPECT_EQ(signalsAdapter.PoutReads, 0);

    Execution each(FrameRetention::EachFrame(4));
    FakeExecutionAdapter eachAdapter;
    eachAdapter.Native.push_back(FunctionResult(1, true));
    eachAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(each.Pulse(ExecutionInput::At(0, 1), 1, eachAdapter));
    ASSERT_TRUE(each.Step(2, eachAdapter));
    EXPECT_EQ(each.Take().size(), 2u);
    EXPECT_EQ(eachAdapter.PoutReads, 0);

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
    EXPECT_EQ(latestAdapter.PoutReads, 0);

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
    EXPECT_EQ(ignoreAdapter.OutputReads, 2);
    EXPECT_EQ(ignoreAdapter.PoutReads, 0);
}

TEST(BehaviorExecution, DoesNotReadPoutsUnlessThePolicyRequestsThem) {
    Execution execution(FrameRetention::Signals());
    FakeExecutionAdapter adapter;
    adapter.ReadFailure = {ExecutionError::UnsupportedPout, 7,
                           "unsupported Pout"};

    const ExecutionResult result =
        execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
    ASSERT_TRUE(result);
    EXPECT_EQ(adapter.PoutReads, 0);
    EXPECT_NE(execution.State(), ExecutionState::Failed);
    ASSERT_TRUE(result.Frame);
    EXPECT_FALSE(result.Fault);
}

TEST(BehaviorExecution, FullFrameQueueUsesIndependentFailureSlot) {
    Execution execution(FrameRetention::EachFrame(1));
    FakeExecutionAdapter adapter;
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.Native.push_back(FunctionResult(1, true));

    ASSERT_TRUE(execution.Pulse(ExecutionInput::At(0, 1), 1, adapter));
    const ExecutionResult overflowed = execution.Step(2, adapter);
    ASSERT_TRUE(overflowed);
    EXPECT_EQ(execution.State(), ExecutionState::Failed);
    EXPECT_EQ(execution.Failure().Code, ExecutionError::FrameQueueFull);
    // The caller sees the same Frame the store kept, not a success that the
    // Run's Failed state contradicts.
    EXPECT_EQ(overflowed.Fault.Code, ExecutionError::FrameQueueFull);
    ASSERT_TRUE(overflowed.Frame);
    EXPECT_FALSE(overflowed.Frame->NativeContinuation);
    ASSERT_TRUE(overflowed.Overflow);
    EXPECT_EQ(overflowed.Overflow->Capacity, 1u);

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

TEST(BehaviorExecution, FrameQueueFullTerminalOwnsTheCapturedPayload) {
    class PayloadBatch final : public FrameBatch {
    public:
        bool Measure(const RunFrame &) override { return true; }
        FrameBatchResult Ready() override { return FrameBatchResult::Complete; }
        bool Write(const RunFrame &frame) override {
            if (frame.Sequence != 2)
                return true;
            OutArray = reinterpret_cast<std::uintptr_t>(
                frame.ActiveOutputs.data());
            OutName = reinterpret_cast<std::uintptr_t>(
                frame.ActiveOutputs.front().Name.data());
            PoutArray = reinterpret_cast<std::uintptr_t>(frame.Pouts.data());
            PoutText = reinterpret_cast<std::uintptr_t>(
                frame.Pouts.front().Text.data());
            return true;
        }

        std::uintptr_t OutArray = 0;
        std::uintptr_t OutName = 0;
        std::uintptr_t PoutArray = 0;
        std::uintptr_t PoutText = 0;
    };

    FrameStore frames(FrameRetention::EachFrame(1));
    RunFrame first;
    first.Sequence = 1;
    ASSERT_FALSE(frames.Retain(std::move(first)).Overflowed);

    RunFrame terminal;
    terminal.Sequence = 2;
    terminal.NativeContinuation = true;
    terminal.ActiveOutputs.push_back(
        {0, std::string(128, 'o'), 0});
    Pout pout;
    pout.Index = 0;
    pout.Name = std::string(128, 'n');
    pout.Kind = PoutKind::Utf8;
    pout.Text = std::string(1024, 'v');
    terminal.Pouts.push_back(std::move(pout));

    const auto outArray = reinterpret_cast<std::uintptr_t>(
        terminal.ActiveOutputs.data());
    const auto outName = reinterpret_cast<std::uintptr_t>(
        terminal.ActiveOutputs.front().Name.data());
    const auto poutArray = reinterpret_cast<std::uintptr_t>(
        terminal.Pouts.data());
    const auto poutText = reinterpret_cast<std::uintptr_t>(
        terminal.Pouts.front().Text.data());

    ASSERT_TRUE(frames.Retain(std::move(terminal)).Overflowed);
    PayloadBatch batch;
    ASSERT_EQ(frames.Take(batch), FrameBatchResult::Complete);
    EXPECT_EQ(batch.OutArray, outArray);
    EXPECT_EQ(batch.OutName, outName);
    EXPECT_EQ(batch.PoutArray, poutArray);
    EXPECT_EQ(batch.PoutText, poutText);
}

TEST(BehaviorExecution, FrameTakeTransfersTheCapturedPayload) {
    FrameStore frames(FrameRetention::EachFrame(4));
    RunFrame captured;
    captured.Sequence = 1;
    captured.ActiveOutputs.push_back(
        {0, std::string(128, 'o'), 0});
    Pout pout;
    pout.Index = 0;
    pout.Name = std::string(128, 'n');
    pout.Kind = PoutKind::Utf8;
    pout.Text = std::string(1024, 'v');
    captured.Pouts.push_back(std::move(pout));

    const auto outArray = reinterpret_cast<std::uintptr_t>(
        captured.ActiveOutputs.data());
    const auto outName = reinterpret_cast<std::uintptr_t>(
        captured.ActiveOutputs.front().Name.data());
    const auto poutArray = reinterpret_cast<std::uintptr_t>(
        captured.Pouts.data());
    const auto poutText = reinterpret_cast<std::uintptr_t>(
        captured.Pouts.front().Text.data());

    ASSERT_FALSE(frames.Retain(std::move(captured)).Overflowed);
    const std::vector<RunFrame> taken = frames.Take();
    ASSERT_EQ(taken.size(), 1u);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(
                  taken[0].ActiveOutputs.data()),
              outArray);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(
                  taken[0].ActiveOutputs.front().Name.data()),
              outName);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(taken[0].Pouts.data()),
              poutArray);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(
                  taken[0].Pouts.front().Text.data()),
              poutText);
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
    EXPECT_EQ(result.Fault.Code, ExecutionError::Cancelled);
    EXPECT_EQ(execution.State(), ExecutionState::Closing);
    execution.MarkClosed();
    EXPECT_EQ(execution.State(), ExecutionState::Closed);
}

TEST(BehaviorExecution, ReadsActiveOutAndPoutBeforeClearingTheOut) {
    Execution execution(FrameRetention::Signals().Pouts());
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
    bool poutsRead = false;
    adapter.OnRead = [&](std::vector<Pout> &) { poutsRead = true; };
    adapter.OnClear = [&](const std::vector<ExecutionOutput> &) {
        EXPECT_TRUE(poutsRead);
        ASSERT_EQ(adapter.Pouts.size(), 1u);
        EXPECT_EQ(adapter.Pouts[0].Int32, 42);
    };

    ExecutionResult result =
        execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
    ASSERT_TRUE(result.Frame);
    ASSERT_EQ(result.Frame->ActiveOutputs.size(), 1u);
    EXPECT_EQ(result.Frame->ActiveOutputs[0], 0);
    const auto frames = execution.Take();
    ASSERT_EQ(frames.size(), 1u);
    ASSERT_EQ(frames[0].Pouts.size(), 1u);
    EXPECT_EQ(frames[0].Pouts[0].Int32, 42);
    EXPECT_FALSE(adapter.Outputs[0].Active);
}

TEST(BehaviorExecution, PoutValuesAreOwnedAndKeepNameOccurrences) {
    Execution execution(FrameRetention::Signals().Pouts());
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

TEST(BehaviorExecution, PoutFailureStopsTheExecutionAfterNativeCapture) {
    const std::array<ExecutionFault, 3> faults{{
        {ExecutionError::UnsupportedPout, 1, "unsupported Pout format"},
        {ExecutionError::PoutReadFailed, 2, "Pout read failed"},
        {ExecutionError::PoutReadFailed, 3,
         "ObjectRefs rejected an object Pout"},
    }};
    for (const ExecutionFault &fault : faults) {
        SCOPED_TRACE(fault.Message);
        Execution execution(FrameRetention::Signals().Pouts());
        FakeExecutionAdapter adapter;
        adapter.Outputs[0].Active = true;
        adapter.ReadFailure = fault;

        ExecutionResult result =
            execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
        EXPECT_EQ(result.State, AdmissionState::Executed);
        EXPECT_EQ(result.Fault.Code, fault.Code);
        EXPECT_EQ(adapter.Calls, 1);
        EXPECT_EQ(adapter.PoutReads, 1);
        EXPECT_EQ(execution.State(), ExecutionState::Failed);
        EXPECT_EQ(execution.Failure().Code, fault.Code);
        EXPECT_EQ(execution.NextSequence(), 2u);
        auto frames = execution.Take();
        ASSERT_EQ(frames.size(), 1u);
        EXPECT_EQ(frames[0].Sequence, 1u);
        EXPECT_EQ(frames[0].Fault.Code, fault.Code);
        EXPECT_EQ(frames[0].Fault.NativeCode, fault.NativeCode);
        EXPECT_EQ(frames[0].Fault.Message, fault.Message);
        ASSERT_EQ(frames[0].ActiveOutputs.size(), 1u);
        EXPECT_EQ(frames[0].ActiveOutputs[0].Name, "Out");
        EXPECT_FALSE(adapter.Outputs[0].Active);
    }
}

TEST(BehaviorExecution, DynamicPoutFailureKeepsOutAndUsesNativeSequence) {
    Execution execution(FrameRetention::Signals().Pouts());
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
    EXPECT_EQ(result.Fault.Code, ExecutionError::UnsupportedPout);
    ASSERT_EQ(result.Frame->ActiveOutputs.size(), 1u);
    EXPECT_EQ(result.Frame->ActiveOutputs[0], 0);
    const auto frames = execution.Take();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_TRUE(frames[0].Pouts.empty());
    EXPECT_EQ(execution.NextSequence(), 2u);
    EXPECT_EQ(execution.State(), ExecutionState::Failed);
    EXPECT_EQ(execution.Failure().Code, ExecutionError::UnsupportedPout);
}

TEST(BehaviorExecution, PoutFailureRemainsTheTerminalCaptureDiagnostic) {
    Execution execution(FrameRetention::Signals().Pouts());
    FakeExecutionAdapter adapter;
    adapter.Outputs[0].Active = true;
    adapter.ReadFailure = {ExecutionError::PoutReadFailed, 2,
                           "Pout read failed"};
    adapter.ClearFailure = {ExecutionError::OutUnavailable, 3,
                            "Out clear failed"};

    const ExecutionResult result =
        execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
    ASSERT_TRUE(result.Frame);
    EXPECT_EQ(result.Frame->Sequence, 1u);
    EXPECT_EQ(result.Fault.Code, ExecutionError::PoutReadFailed);
    EXPECT_EQ(result.Fault.NativeCode, 2);
    ASSERT_EQ(result.Frame->ActiveOutputs.size(), 1u);
    EXPECT_EQ(execution.State(), ExecutionState::Failed);
    EXPECT_EQ(execution.Failure().Code, ExecutionError::PoutReadFailed);
}

TEST(BehaviorExecution, PoutFailureDoesNotReplaceAFatalNativeDiagnostic) {
    struct Case {
        NativeExecution Native;
        ExecutionError Expected;
    };
    const std::array<Case, 2> cases{{
        {FunctionResult(10, false, true), ExecutionError::NativeFailed},
        {FunctionResult(11, false, false, true),
         ExecutionError::UnsupportedBreak},
    }};

    for (const Case &test : cases) {
        SCOPED_TRACE(static_cast<int>(test.Expected));
        Execution execution(FrameRetention::Signals().Pouts());
        FakeExecutionAdapter adapter;
        adapter.Native.push_back(test.Native);
        adapter.ReadFailure = {ExecutionError::PoutReadFailed, 23,
                               "Pout read failed after native execution"};

        const ExecutionResult result =
            execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
        ASSERT_TRUE(result.Frame);
        EXPECT_EQ(adapter.PoutReads, 0);
        EXPECT_EQ(result.Fault.Code, test.Expected);
        EXPECT_EQ(result.Fault.NativeCode, test.Native.ReturnCode);
        EXPECT_EQ(execution.Failure().Code, test.Expected);
        EXPECT_EQ(execution.Failure().NativeCode, test.Native.ReturnCode);
    }
}

TEST(BehaviorExecution, LatestAndIgnoreKeepSeparateFailedAndFinalFrames) {
    for (const FrameRetention retention : {
             FrameRetention::Latest(), FrameRetention::Ignore()}) {
        FrameStore frames(retention);
        RunFrame failed;
        failed.Sequence = 1;
        failed.Fault = {ExecutionError::PoutReadFailed, 7,
                        "Pout read failed"};
        ASSERT_FALSE(frames.Retain(std::move(failed)).Overflowed);

        RunFrame completed;
        completed.Sequence = 2;
        ASSERT_FALSE(frames.Retain(std::move(completed)).Overflowed);

        const std::vector<RunFrame> retained = frames.Read();
        ASSERT_EQ(retained.size(), 2u);
        EXPECT_EQ(retained[0].Sequence, 1u);
        EXPECT_EQ(retained[0].Fault.Code, ExecutionError::PoutReadFailed);
        EXPECT_EQ(retained[1].Sequence, 2u);
        EXPECT_FALSE(retained[1].Fault);
    }
}

TEST(BehaviorExecution, ConsumeClearsEveryRoleHeldByOneFrame) {
    FrameStore frames(FrameRetention::Latest());
    RunFrame failedAndFinal;
    failedAndFinal.Sequence = 1;
    failedAndFinal.Fault = {ExecutionError::PoutReadFailed, 7,
                            "Pout read failed"};
    ASSERT_FALSE(frames.Retain(std::move(failedAndFinal)).Overflowed);
    ASSERT_EQ(frames.Read().size(), 1u);

    const std::array<std::uint64_t, 1> batch{1};
    EXPECT_TRUE(frames.Consume(batch));
    EXPECT_TRUE(frames.Read().empty());
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

TEST(BehaviorExecution, ConsumeRejectsAReadWhenTheVisibleBatchChanged) {
    FrameStore frames(FrameRetention::EachFrame(4));
    RunFrame first;
    first.Sequence = 1;
    ASSERT_FALSE(frames.Retain(std::move(first)).Overflowed);
    const std::vector<RunFrame> read = frames.Read();
    ASSERT_EQ(read.size(), 1u);

    RunFrame second;
    second.Sequence = 2;
    ASSERT_FALSE(frames.Retain(std::move(second)).Overflowed);
    const std::array<std::uint64_t, 1> staleBatch{1};
    EXPECT_FALSE(frames.Consume(staleBatch));

    const std::vector<RunFrame> retained = frames.Read();
    ASSERT_EQ(retained.size(), 2u);
    EXPECT_EQ(retained[0].Sequence, 1u);
    EXPECT_EQ(retained[1].Sequence, 2u);
}

TEST(BehaviorExecution, FrameStoreWritesOneLockedBatchAndConsumesOnSuccess) {
    class Batch final : public FrameBatch {
    public:
        bool Measure(const RunFrame &frame) override {
            Measured.push_back(&frame);
            return true;
        }
        FrameBatchResult Ready() override {
            return Available ? FrameBatchResult::Complete
                             : FrameBatchResult::Insufficient;
        }
        bool Write(const RunFrame &frame) override {
            Written.push_back(&frame);
            Sequences.push_back(frame.Sequence);
            return true;
        }

        bool Available = false;
        std::vector<const RunFrame *> Measured;
        std::vector<const RunFrame *> Written;
        std::vector<std::uint64_t> Sequences;
    };

    FrameStore frames(FrameRetention::Latest());
    RunFrame first;
    first.Sequence = 1;
    first.NativeContinuation = true;
    ASSERT_FALSE(frames.Retain(std::move(first)).Overflowed);
    RunFrame final;
    final.Sequence = 2;
    ASSERT_FALSE(frames.Retain(std::move(final)).Overflowed);

    Batch small;
    EXPECT_EQ(frames.Take(small), FrameBatchResult::Insufficient);
    EXPECT_EQ(small.Measured.size(), 2u);
    EXPECT_TRUE(small.Written.empty());
    EXPECT_EQ(frames.Read().size(), 2u);

    Batch accepted;
    accepted.Available = true;
    EXPECT_EQ(frames.Take(accepted), FrameBatchResult::Complete);
    ASSERT_EQ(accepted.Measured.size(), 2u);
    ASSERT_EQ(accepted.Written.size(), 2u);
    EXPECT_EQ(accepted.Measured[0], accepted.Written[0]);
    EXPECT_EQ(accepted.Measured[1], accepted.Written[1]);
    EXPECT_LT(accepted.Sequences[0], accepted.Sequences[1]);
    EXPECT_TRUE(frames.Read().empty());
}

} // namespace
