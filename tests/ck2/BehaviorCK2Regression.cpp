#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "CKAll.h"
#include "Behavior/CKEdit.h"
#include "Behavior/Graph.h"
#include "Behavior/Runtime.h"
#include "Behavior/Sessions.h"

namespace {

using namespace BML::Behavior::Internal;

const CKGUID kPrototype(0x70102341, 0x31004177);
bool g_SwapPins = false;
std::function<void(CKBehavior *)> g_OnCreate;

void Require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}

void Check(const Status &status) {
    Require(static_cast<bool>(status), status.Message.c_str());
}

int Read(CKParameter *parameter) {
    int value = 0;
    Require(parameter && parameter->GetValue(&value, FALSE) == CK_OK, "read int");
    return value;
}

void Write(CKParameter *parameter, int value) {
    Require(parameter && parameter->SetValue(&value, sizeof(value)) == CK_OK, "write int");
}

int Run(const CKBehaviorContext &) { return CKBR_OK; }

int Callback(const CKBehaviorContext &context) {
    if (g_OnCreate && context.CallbackMessage == CKM_BEHAVIORCREATE &&
        context.Behavior->GetInputParameter(0)->GetRealSource()) {
        auto callback = std::move(g_OnCreate);
        g_OnCreate = {};
        callback(context.Behavior);
    }
    if (g_SwapPins && context.CallbackMessage == CKM_BEHAVIOREDITED) {
        g_SwapPins = false;
        auto *pin = context.Behavior->RemoveInputParameter(0);
        context.Behavior->AddInputParameter(pin);
    }
    return CK_OK;
}

CKERROR Prototype(CKBehaviorPrototype **out) {
    *out = CreateCKBehaviorPrototype("Behavior regression fixture");
    (*out)->DeclareInput("In");
    (*out)->DeclareInParameter("Value", CKPGUID_INT, "7");
    (*out)->DeclareInParameter("Other", CKPGUID_INT, "9");
    (*out)->SetBehaviorFlags(static_cast<CK_BEHAVIOR_FLAGS>(
        CKBEHAVIOR_VARIABLEPARAMETERINPUTS | CKBEHAVIOR_VARIABLEINPUTS));
    (*out)->SetFunction(Run);
    (*out)->SetBehaviorCallbackFct(Callback, CKCB_BEHAVIORCREATE | CKCB_BEHAVIOREDITED);
    return CK_OK;
}

void Register(XObjectDeclarationArray *registry) {
    auto *declaration = CreateCKObjectDeclaration("Behavior regression fixture");
    declaration->SetGuid(kPrototype);
    declaration->SetType(CKDLL_BEHAVIORPROTOTYPE);
    declaration->SetCompatibleClassId(CKCID_BEOBJECT);
    declaration->SetCreationFunction(Prototype);
    CKStoreDeclaration(registry, declaration);
}

int Count() { return 1; }

CKPluginInfo *Info(int) {
    static CKPluginInfo info{};
    info.m_Type = CKPLUGIN_BEHAVIOR_DLL;
    info.m_GUID = CKGUID(0x70102341, 0x31004178);
    info.m_Summary = "Behavior regression fixture";
    info.m_Description = "Behavior regression fixture";
    return &info;
}

ObjectRef Issue(const void *object) {
    return object ? ObjectRef{1, static_cast<CKObject *>(const_cast<void *>(object))->GetID(), 1}
                  : ObjectRef{};
}

struct Fixture {
    CKContext *Context;
    Runtime BehaviorRuntime;
    CKBehavior *Graph;
    std::unique_ptr<GraphSource> Graphs;
    CKEdit Editor;

    explicit Fixture(CKContext *context)
        : Context(context), BehaviorRuntime(context, Issue),
          Graph(CKBehavior::Cast(context->CreateObject(CKCID_BEHAVIOR, "Test graph"))),
          Graphs(MakeCKGraphSource(context, BehaviorRuntime, Issue)),
          Editor(context, BehaviorRuntime, nullptr, *Graphs) {
        Require(Graph != nullptr, "create graph");
        Graph->UseGraph();
    }
};

void Values(Fixture &fixture, bool conflict, bool overlap) {
    auto *source = fixture.Graph->CreateOutputParameter("Source", CKPGUID_INT);
    auto *relay = fixture.Graph->CreateOutputParameter("Relay", CKPGUID_INT);
    auto *destination = fixture.Graph->CreateLocalParameter("Destination", CKPGUID_INT);
    Write(source, 10);
    Write(relay, 30);
    Write(destination, 99);
    Require(source->AddDestination(relay) == CK_OK, "source destination");
    Require(relay->AddDestination(destination) == CK_OK, "relay destination");
    // A diamond must snapshot the destination only once.
    Require(source->AddDestination(destination) == CK_OK, "diamond destination");
    Ops edit;
    Check(fixture.Editor.Begin(fixture.Graph, {"regression", "set"}, edit));
    edit.Set(edit.Graph().Pout(0), Value::From(CKPGUID_INT, 20));
    if (overlap)
        edit.Set(edit.Graph().Local(0), Value::From(CKPGUID_INT, 40));
    Patch patch;
    const Status applied = fixture.Editor.Apply(edit, patch);
    if (overlap) {
        Require(applied.Code == Error::SourceConflict, "overlapping propagated writes must conflict");
        Require(Read(source) == 10 && Read(relay) == 30 && Read(destination) == 99,
                "failed overlapping Set changed values");
        return;
    }
    Check(applied);
    Require(Read(source) == 20 && Read(relay) == 20 && Read(destination) == 20,
            "Set did not propagate");
    auto *later = fixture.Graph->CreateLocalParameter("Later destination", CKPGUID_INT);
    Write(later, 123);
    Require(source->AddDestination(later) == CK_OK, "late destination");
    if (conflict)
        Write(destination, 77);
    const Status closed = fixture.Editor.Close(patch);
    if (conflict) {
        Require(closed.Code == Error::RevertConflict, "changed destination must conflict");
        Require(Read(destination) == 77, "Close overwrote an external destination value");
    } else {
        Check(closed);
        Require(Read(destination) == 99, "Close lost original destination value");
    }
    Require(Read(source) == 10 && Read(relay) == 30, "Close lost original Pout values");
    Require(Read(later) == 123, "Close propagated into a later destination");
    Require(source->GetDestinationCount() == 3 && source->GetDestination(0) == relay &&
            source->GetDestination(1) == destination && source->GetDestination(2) == later &&
            relay->GetDestinationCount() == 1 && relay->GetDestination(0) == destination,
            "Close changed destination relations or their order");
    if (conflict) {
        Write(destination, 20);
        Check(fixture.Editor.Close(patch));
        Require(Read(destination) == 99 && Read(source) == 10 && Read(relay) == 30,
                "retry did not restore the conflicted destination independently");
    }
}

void StaleSlot(Fixture &fixture, bool bind) {
    Sessions sessions(fixture.BehaviorRuntime, nullptr,
        MakeCKGraphSource(fixture.Context, fixture.BehaviorRuntime, Issue));
    Require(sessions.RegisterOwner("regression") != 0, "register owner");
    std::uintptr_t session = 0;
    Check(sessions.OpenSession("regression", session));
    auto run = sessions.Attach(session, fixture.Graph, BlockSpec(kPrototype));
    Check(run.Result);
    Layout before;
    Check(sessions.ReadLiveLayout(run.Id, before));
    auto *native = sessions.Block(run.Id);
    Ops edit;
    Check(fixture.Editor.Begin(fixture.Graph, {"regression", "layout"}, edit));
    Node node;
    Check(fixture.Editor.Use(edit, native, node));
    edit.AppendIn(node, "Extra");
    g_SwapPins = true;
    Patch patch;
    Check(fixture.Editor.Apply(edit, patch));
    auto *source = fixture.Graph->CreateLocalParameter("Bind source", CKPGUID_INT);
    Write(source, 55);
    std::uint64_t generation = 0;
    const auto set = [&](std::uint64_t expected) {
        return bind
            ? sessions.Bind(run.Id, expected, Slot::At(SlotKind::InputParameter, 0),
                fixture.Graph, 0, Slot::At(SlotKind::Local, 0),
                Parameter::BindingKind::Direct, generation)
            : sessions.Set(run.Id, expected, Slot::At(SlotKind::InputParameter, 0),
                Value::From(CKPGUID_INT, 55), generation);
    };
    Require(set(before.Generation).Code == Error::StaleLayout,
            "old run Slot accepted after graph EDITED rearranged Pins");
    Require(Read(native->GetInputParameter(0)->GetRealSource()) == 9,
            "stale Slot retargeted another Pin");
    Layout after;
    Check(sessions.ReadLiveLayout(run.Id, after));
    Require(after.Generation != before.Generation, "live layout generation did not advance");
    Check(set(after.Generation));
    Require(Read(native->GetInputParameter(0)->GetRealSource()) == 55,
            "fresh Slot failed to address current Pin");
    Layout stable;
    Check(sessions.ReadLiveLayout(run.Id, stable));
    Require(stable.Generation == after.Generation, "value or binding changed layout generation");
    Check(fixture.Editor.Close(patch));
    Require(set(stable.Generation).Code == Error::StaleLayout,
            "Slot survived Patch teardown EDITED");
}

void PinType(Fixture &fixture, bool shared) {
    BlockSpec spec(kPrototype);
    spec.PinType(Slot::At(SlotKind::InputParameter, 0), CKPGUID_FLOAT);
    auto detached = fixture.BehaviorRuntime.Instantiate(nullptr, spec);
    Check(detached.Detail);
    CKParameter *previous = nullptr;
    CreateResult consumer;
    if (shared) {
        consumer = fixture.BehaviorRuntime.Instantiate(nullptr, BlockSpec(kPrototype));
        Check(consumer.Detail);
        g_OnCreate = [&](CKBehavior *behavior) {
            previous = behavior->GetInputParameter(0)->GetRealSource();
            Check(fixture.BehaviorRuntime.SetInput(consumer.Handle, Slot::At(SlotKind::InputParameter, 0),
                                         Parameter::Binding::Direct(previous)));
        };
    }
    auto attached = fixture.BehaviorRuntime.AttachToGraph(fixture.Graph, spec);
    Check(attached.Detail);
    auto *pin = attached.Handle.Get()->GetInputParameter(0);
    if (shared) {
        Require(previous && previous->GetGUID() == CKPGUID_INT && Read(previous) == 7,
                "PinType changed a source still used by another Run");
    }
    Require(detached.Handle.Get()->GetInputParameter(0)->GetRealSource()->GetGUID() == CKPGUID_FLOAT,
            "detached default source type differs from Pin");
    Require(pin->GetGUID() == CKPGUID_FLOAT && pin->GetRealSource() &&
            pin->GetRealSource()->GetGUID() == CKPGUID_FLOAT,
            "graph default source type differs from Pin");
}

void ValueOnlyEdit(Fixture &fixture) {
    auto run = fixture.BehaviorRuntime.AttachToGraph(fixture.Graph, BlockSpec(kPrototype));
    Check(run.Detail);
    CKBehavior *block = run.Handle.Get();
    const auto generation = run.Handle.LayoutGeneration();
    NativeRef reference;
    Check(fixture.Graphs->Refer(block, reference));
    Layout before;
    Check(fixture.Graphs->ReadLayout(reference, before));
    Ops edit;
    Check(fixture.Editor.Begin(fixture.Graph, {"regression", "value-only"}, edit));
    Node node;
    Check(fixture.Editor.Use(edit, block, node));
    edit.Bind(node.Pin(0), Value::From(CKPGUID_INT, 23));
    Patch patch;
    Check(fixture.Editor.Apply(edit, patch));
    Layout after;
    Check(fixture.Graphs->ReadLayout(reference, after));
    Require(run.Handle.LayoutGeneration() == generation && after.Generation == before.Generation,
            "value-only EDITED invalidated Run Slots or graph Ports");
    Require(Read(block->GetInputParameter(0)->GetRealSource()) == 23, "Bind did not change value");
    Check(fixture.Editor.Close(patch));
    Check(fixture.Graphs->ReadLayout(reference, after));
    Require(run.Handle.LayoutGeneration() == generation && after.Generation == before.Generation,
            "value-only teardown invalidated Run Slots or graph Ports");
    Require(Read(block->GetInputParameter(0)->GetRealSource()) == 7, "Close did not restore source");
}

void ForeignPinSource(Fixture &fixture) {
    auto *source = fixture.Graph->CreateLocalParameter("Foreign source", CKPGUID_INT);
    Write(source, 42);
    g_OnCreate = [&](CKBehavior *behavior) {
        Require(behavior->GetInputParameter(0)->SetDirectSource(source) == CK_OK,
                "bind foreign source in CREATE");
    };
    BlockSpec spec(kPrototype);
    spec.PinType(Slot::At(SlotKind::InputParameter, 0), CKPGUID_FLOAT);
    auto attached = fixture.BehaviorRuntime.AttachToGraph(fixture.Graph, spec);
    Require(attached.Detail.Code == Error::TypeMismatch,
            "PinType accepted an incompatible foreign source");
    Require(source->GetGUID() == CKPGUID_INT && Read(source) == 42,
            "PinType mutated a foreign source");
}

} // namespace

int main(int argc, char **argv) {
    try {
        Require(argc == 2, "pass a regression case name");
        Require(CKStartUp() == CK_OK, "startup");
        Require(CKGetPluginManager()->RegisterStaticPlugin(
            "BehaviorRegression", Count, Info, nullptr, Register) == CK_OK, "register plugin");
        CKContext *context = nullptr;
        Require(CKCreateContext(&context, nullptr, 0, 0) == CK_OK, "create context");
        {
            Fixture fixture(context);
            const std::string_view test(argv[1]);
            if (test == "values" || test == "value-conflict" || test == "value-overlap")
                Values(fixture, test == "value-conflict", test == "value-overlap");
            else if (test == "slot-set" || test == "slot-bind")
                StaleSlot(fixture, test == "slot-bind");
            else if (test == "slot-value-only")
                ValueOnlyEdit(fixture);
            else if (test == "pin-type" || test == "pin-type-shared")
                PinType(fixture, test == "pin-type-shared");
            else if (test == "pin-type-foreign")
                ForeignPinSource(fixture);
            else
                Require(false, "unknown regression case");
        }
        CKCloseContext(context);
        CKShutdown();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
