#include "Behavior/Execution.h"

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
                return {BehaviorKind::Function, 0, false, false, false, false, {}};
            }
            return {BehaviorKind::Function, 1, true, false, false, false, {}};
        }

        if (Native.empty())
            return {Kind, 0, false, false, false, false, {}};
        NativeExecution result = std::move(Native.front());
        Native.pop_front();
        return result;
    }

    bool CaptureOutputs(std::vector<ExecutionOutput> &outputs,
                        ExecutionFault &) override {
        for (int index = 0; index < static_cast<int>(Outputs.size()); ++index) {
            if (Outputs[index].Active)
                outputs.push_back({index, Outputs[index].Name, 0});
        }
        return true;
    }

    bool ClearOutputs(const std::vector<ExecutionOutput> &outputs,
                      ExecutionFault &) override {
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
    BehaviorKind Kind = BehaviorKind::Function;
    bool WaitForAll = false;
    int Calls = 0;
    std::vector<Input> Inputs;
    std::vector<Input> Outputs;
    std::vector<int> Activated;
    std::deque<NativeExecution> Native;
    std::function<void()> OnExecute;
};

NativeExecution FunctionResult(int code, bool retry = false,
                               bool error = false, bool breakpoint = false) {
    return {BehaviorKind::Function, code, retry, false, error, breakpoint, {}};
}

NativeExecution GraphResult(int code, bool active, bool error = false) {
    return {BehaviorKind::Graph, code, false, active, error, false, {}};
}

TEST(BehaviorExecution, WaitForAllKeepsInputsAndQueuesSameFramePulse) {
    Execution execution;
    FakeExecutionAdapter adapter({"A", "B"});
    adapter.WaitForAll = true;

    ExecutionResult first = execution.Pulse(ExecutionInput::Named("A"), 10, adapter);
    ASSERT_EQ(first.State, AdmissionState::Executed);
    ASSERT_TRUE(first.Outcome);
    EXPECT_TRUE(first.Outcome->NativeContinuation);
    EXPECT_FALSE(first.Outcome->QueuedInput);
    EXPECT_TRUE(adapter.Inputs[0].Active);
    EXPECT_FALSE(adapter.Inputs[1].Active);

    ExecutionResult second = execution.Pulse(ExecutionInput::Named("B"), 10, adapter);
    EXPECT_EQ(second.State, AdmissionState::Queued);
    EXPECT_EQ(adapter.Calls, 1);
    EXPECT_FALSE(adapter.Inputs[1].Active);

    ExecutionResult completed = execution.Step(11, adapter);
    ASSERT_EQ(completed.State, AdmissionState::Executed);
    ASSERT_TRUE(completed.Outcome);
    EXPECT_TRUE(completed.Outcome->Terminal);
    ASSERT_EQ(completed.Outcome->ActiveOutputs.size(), 1u);
    EXPECT_EQ(completed.Outcome->ActiveOutputs[0].Name, "Out");
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
    ASSERT_TRUE(first.Outcome);
    EXPECT_FALSE(first.Outcome->NativeContinuation);
    EXPECT_TRUE(first.Outcome->QueuedInput);
    EXPECT_EQ(execution.State(), ExecutionState::Pending);
    EXPECT_EQ(adapter.Calls, 1);

    EXPECT_EQ(execution.Step(4, adapter).State, AdmissionState::Queued);
    EXPECT_EQ(adapter.Calls, 1);
    EXPECT_EQ(execution.Step(5, adapter).State, AdmissionState::Executed);
    EXPECT_EQ(adapter.Calls, 2);
}

TEST(BehaviorExecution, FunctionUsesRetryAndGraphUsesNativeActivity) {
    Execution function;
    FakeExecutionAdapter functionAdapter;
    functionAdapter.Native.push_back(FunctionResult(7, true));
    functionAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(function.Pulse(ExecutionInput::At(0, 1), 1, functionAdapter));
    EXPECT_EQ(function.State(), ExecutionState::Pending);
    ASSERT_TRUE(function.Step(2, functionAdapter));
    EXPECT_EQ(function.State(), ExecutionState::Idle);

    Execution graph;
    FakeExecutionAdapter graphAdapter;
    graphAdapter.Kind = BehaviorKind::Graph;
    graphAdapter.Native.push_back(GraphResult(0, true));
    graphAdapter.Native.push_back(GraphResult(0, false));
    ASSERT_TRUE(graph.Pulse(ExecutionInput::At(0, 1), 1, graphAdapter));
    EXPECT_EQ(graph.State(), ExecutionState::Pending);
    ASSERT_TRUE(graph.Step(2, graphAdapter));
    EXPECT_EQ(graph.State(), ExecutionState::Idle);
}

TEST(BehaviorExecution, RetryErrorContinuesButFatalAndBreakClose) {
    Execution retry;
    FakeExecutionAdapter retryAdapter;
    retryAdapter.Native.push_back(FunctionResult(9, true, true));
    ExecutionResult retryResult = retry.Pulse(ExecutionInput::At(0, 1), 1,
                                              retryAdapter);
    ASSERT_TRUE(retryResult.Outcome);
    EXPECT_EQ(retryResult.Outcome->Fault.Code, ExecutionError::NativeFailed);
    EXPECT_TRUE(retryResult.Outcome->NativeContinuation);
    EXPECT_EQ(retry.State(), ExecutionState::Pending);

    Execution fatal;
    FakeExecutionAdapter fatalAdapter;
    fatalAdapter.Native.push_back(FunctionResult(10, false, true));
    ExecutionResult fatalResult = fatal.Pulse(ExecutionInput::At(0, 1), 1,
                                              fatalAdapter);
    ASSERT_TRUE(fatalResult.Outcome);
    EXPECT_TRUE(fatalResult.Outcome->Terminal);
    EXPECT_EQ(fatal.State(), ExecutionState::Failed);

    Execution breakpoint;
    FakeExecutionAdapter breakAdapter;
    breakAdapter.Native.push_back(FunctionResult(11, false, false, true));
    ExecutionResult breakResult = breakpoint.Pulse(ExecutionInput::At(0, 1), 1,
                                                   breakAdapter);
    ASSERT_TRUE(breakResult.Outcome);
    EXPECT_EQ(breakResult.Outcome->Fault.Code, ExecutionError::UnsupportedBreak);
    EXPECT_TRUE(breakResult.Outcome->Terminal);
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

TEST(BehaviorExecution, RetentionPoliciesExposeSignalsGapsAndTerminalState) {
    Execution signals(OutcomeRetention::Signals(4));
    FakeExecutionAdapter signalsAdapter;
    signalsAdapter.Native.push_back(FunctionResult(1, true));
    signalsAdapter.Native.push_back(FunctionResult(1, true));
    signalsAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(signals.Pulse(ExecutionInput::At(0, 1), 1, signalsAdapter));
    ASSERT_TRUE(signals.Step(2, signalsAdapter));
    ASSERT_TRUE(signals.Step(3, signalsAdapter));
    auto signalOutcomes = signals.Drain();
    ASSERT_EQ(signalOutcomes.size(), 2u);
    EXPECT_EQ(signalOutcomes[0].Sequence, 1u);
    EXPECT_EQ(signalOutcomes[1].Sequence, 3u);

    Execution each(OutcomeRetention::EachFrame(4));
    FakeExecutionAdapter eachAdapter;
    eachAdapter.Native.push_back(FunctionResult(1, true));
    eachAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(each.Pulse(ExecutionInput::At(0, 1), 1, eachAdapter));
    ASSERT_TRUE(each.Step(2, eachAdapter));
    EXPECT_EQ(each.Drain().size(), 2u);

    Execution latest(OutcomeRetention::Latest());
    FakeExecutionAdapter latestAdapter;
    latestAdapter.Native.push_back(FunctionResult(1, true));
    latestAdapter.Native.push_back(FunctionResult(1, true));
    latestAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(latest.Pulse(ExecutionInput::At(0, 1), 1, latestAdapter));
    ASSERT_TRUE(latest.Step(2, latestAdapter));
    ASSERT_TRUE(latest.Step(3, latestAdapter));
    auto latestOutcomes = latest.Drain();
    ASSERT_EQ(latestOutcomes.size(), 2u);
    EXPECT_EQ(latestOutcomes[0].Sequence, 2u);
    EXPECT_EQ(latestOutcomes[1].Sequence, 3u);

    Execution ignore(OutcomeRetention::Ignore());
    FakeExecutionAdapter ignoreAdapter;
    ignoreAdapter.Native.push_back(FunctionResult(1, true));
    ignoreAdapter.Native.push_back(FunctionResult(0));
    ASSERT_TRUE(ignore.Pulse(ExecutionInput::At(0, 1), 1, ignoreAdapter));
    ASSERT_TRUE(ignore.Step(2, ignoreAdapter));
    auto ignored = ignore.Drain();
    ASSERT_EQ(ignored.size(), 1u);
    EXPECT_TRUE(ignored[0].Terminal);
    EXPECT_EQ(ignored[0].Sequence, 2u);
}

TEST(BehaviorExecution, FullOutcomeQueueUsesIndependentTerminalSlot) {
    Execution execution(OutcomeRetention::EachFrame(1));
    FakeExecutionAdapter adapter;
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.Native.push_back(FunctionResult(1, true));

    ASSERT_TRUE(execution.Pulse(ExecutionInput::At(0, 1), 1, adapter));
    ASSERT_TRUE(execution.Step(2, adapter));
    EXPECT_EQ(execution.State(), ExecutionState::Closing);
    EXPECT_EQ(execution.TerminalError().Code, ExecutionError::OutcomeQueueFull);

    auto outcomes = execution.Drain();
    ASSERT_EQ(outcomes.size(), 2u);
    EXPECT_EQ(outcomes[0].Sequence, 1u);
    EXPECT_EQ(outcomes[1].Sequence, 2u);
    EXPECT_EQ(outcomes[1].Fault.Code, ExecutionError::OutcomeQueueFull);
    ASSERT_TRUE(outcomes[1].Overflow);
    EXPECT_EQ(outcomes[1].Overflow->Capacity, 1u);
}

TEST(BehaviorExecution, CloseDuringExecuteDefersStateTransition) {
    Execution execution;
    FakeExecutionAdapter adapter;
    adapter.Native.push_back(FunctionResult(1, true));
    adapter.OnExecute = [&] { execution.RequestClose(); };

    ExecutionResult result = execution.Pulse(ExecutionInput::At(0, 1), 1, adapter);
    ASSERT_TRUE(result.Outcome);
    EXPECT_TRUE(result.Outcome->Terminal);
    EXPECT_EQ(result.Outcome->Fault.Code, ExecutionError::Cancelled);
    EXPECT_EQ(execution.State(), ExecutionState::Closing);
    execution.MarkClosed();
    EXPECT_EQ(execution.State(), ExecutionState::Closed);
}

} // namespace
