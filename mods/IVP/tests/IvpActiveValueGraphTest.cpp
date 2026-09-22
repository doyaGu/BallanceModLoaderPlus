#include "IvpTestAdapter.h"

#include "BML/IVP/ActiveValue.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

char *DuplicateString(const char *value) {
    const std::size_t size = std::strlen(value) + 1;
    auto *copy = static_cast<char *>(std::malloc(size));
    if (copy)
        std::memcpy(copy, value, size);
    return copy;
}

void *Allocate(unsigned int size) { return std::malloc(size); }
void Free(void *memory) { std::free(memory); }

int gFloatConstructions = 0;
int gIntConstructions = 0;
int gTerminalDoubleConstructions = 0;
int gTerminalIntConstructions = 0;
int gValueConstructions = 0;

template <class T>
void WriteField(void *object, std::size_t offset, const T &value) {
    std::memcpy(static_cast<std::byte *>(object) + offset, &value, sizeof(value));
}

void __fastcall ConstructActiveValue(
    IVP_U_Active_Value *value, void *, const char *name) {
    ++gValueConstructions;
    WriteField(value, 0x04, DuplicateString(name));
    WriteField(value, 0x08, 0);
}

void __fastcall ConstructActiveFloat(
    IVP_U_Active_Float *value, void *, const char *name) {
    ++gFloatConstructions;
    WriteField(value, 0x04, DuplicateString(name));
    WriteField(value, 0x08, 0);
    WriteField(value, 0x0C, std::uint16_t{0});
    WriteField(value, 0x0E, std::uint16_t{0});
    WriteField(value, 0x10, static_cast<void **>(nullptr));
    WriteField(value, 0x14, static_cast<IVP_U_Active_Value_Manager *>(nullptr));
    WriteField(value, 0x18, 0);
    WriteField(value, 0x20, IVP_DOUBLE{0.0});
}

void __fastcall ConstructActiveInt(
    IVP_U_Active_Int *value, void *, const char *name) {
    ++gIntConstructions;
    WriteField(value, 0x04, DuplicateString(name));
    WriteField(value, 0x08, 0);
    WriteField(value, 0x0C, std::uint16_t{0});
    WriteField(value, 0x0E, std::uint16_t{0});
    WriteField(value, 0x10, static_cast<void **>(nullptr));
    WriteField(value, 0x14, static_cast<IVP_U_Active_Value_Manager *>(nullptr));
    WriteField(value, 0x18, 0);
    WriteField(value, 0x1C, 0);
}

void __fastcall ConstructTerminalDouble(
    IVP_U_Active_Terminal_Double *value, void *, const char *name,
    IVP_DOUBLE initialValue) {
    ++gTerminalDoubleConstructions;
    ConstructActiveFloat(value, nullptr, name);
    WriteField(value, 0x20, initialValue);
    WriteField(value, 0x30, initialValue);
}

void __fastcall ConstructTerminalInt(
    IVP_U_Active_Terminal_Int *value, void *, const char *name,
    int initialValue) {
    ++gTerminalIntConstructions;
    ConstructActiveInt(value, nullptr, name);
    WriteField(value, 0x1C, initialValue);
    WriteField(value, 0x24, initialValue);
}

// InvokeThis uses x86 __thiscall. A free-function test shim therefore receives
// ECX through __fastcall's first register argument; EDX is intentionally unused.
void __fastcall IncrementVector(IVP_U_Vector_Base *vector, void *) {
    const std::uint16_t capacity =
        vector->memsize == 0 ? 4 : static_cast<std::uint16_t>(vector->memsize * 2);
    void **elements = static_cast<void **>(
        std::realloc(vector->elems, capacity * sizeof(void *)));
    ASSERT_NE(elements, nullptr);
    vector->elems = elements;
    vector->memsize = capacity;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::DuplicateString, &DuplicateString),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Allocate, &Allocate),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::Free, &Free),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::VectorIncrementMemory, &IncrementVector),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActiveValueConstruct, &ConstructActiveValue),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActiveFloatConstruct, &ConstructActiveFloat),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActiveIntConstruct, &ConstructActiveInt),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActiveTerminalDoubleConstruct,
        &ConstructTerminalDouble),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::ActiveTerminalIntConstruct,
        &ConstructTerminalInt),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpActiveValueGraph, CompleteTerminalConstructorsOwnBaseConstruction) {
    gFloatConstructions = 0;
    gIntConstructions = 0;
    gTerminalDoubleConstructions = 0;
    gTerminalIntConstructions = 0;

    IVP_U_Active_Terminal_Double floating("gravity_scale", 0.625);
    IVP_U_Active_Terminal_Int enabled("gravity_enabled", 1);

    EXPECT_EQ(gTerminalDoubleConstructions, 1);
    EXPECT_EQ(gTerminalIntConstructions, 1);
    EXPECT_EQ(gFloatConstructions, 1);
    EXPECT_EQ(gIntConstructions, 1);
    EXPECT_STREQ(floating.get_name(), "gravity_scale");
    EXPECT_STREQ(enabled.get_name(), "gravity_enabled");
    EXPECT_DOUBLE_EQ(floating.give_double_value(), 0.625);
    EXPECT_EQ(enabled.give_int_value(), 1);
}

TEST(IvpActiveValueGraph, DirectValueConstructionUsesRetailBody) {
    gValueConstructions = 0;
    IVP_U_Active_Value value("controller_parameter");
    EXPECT_EQ(gValueConstructions, 1);
    EXPECT_STREQ(value.get_name(), "controller_parameter");
}

TEST(IvpActiveValueGraph, ClampsAndSwitchesActuatorDemand) {
    IVP_U_Active_Terminal_Double command("command", 0.25);
    IVP_U_Active_Terminal_Double bias("bias", 5.0);
    IVP_U_Active_Terminal_Double zero("zero", 0.0);
    IVP_U_Active_Terminal_Double rangeLow("range_low", 0.0);
    IVP_U_Active_Terminal_Double rangeHigh("range_high", 100.0);
    IVP_U_Active_Terminal_Int enabled("enabled", 0);
    command.add_reference();
    bias.add_reference();
    zero.add_reference();
    rangeLow.add_reference();
    rangeHigh.add_reference();
    enabled.add_reference();

    IVP_U_Active_Add_Multiple requested(
        "requested", &bias, &command, 120.0);
    requested.add_reference();
    IVP_U_Active_Limit limited("limited", &requested, 0.0, 100.0);
    limited.add_reference();
    IVP_U_Active_Test_Range safe(
        "safe", &requested, &rangeLow, &rangeHigh);
    safe.add_reference();
    IVP_U_Active_Switch applied("applied", &enabled, &limited, &zero);
    applied.add_reference();

    EXPECT_DOUBLE_EQ(requested.give_double_value(), 35.0);
    EXPECT_DOUBLE_EQ(limited.give_double_value(), 35.0);
    EXPECT_EQ(safe.give_int_value(), 1);
    EXPECT_DOUBLE_EQ(applied.give_double_value(), 0.0);

    enabled.set_int(1);
    EXPECT_DOUBLE_EQ(applied.give_double_value(), 35.0);

    command.set_double(1.25);
    EXPECT_DOUBLE_EQ(requested.give_double_value(), 155.0);
    EXPECT_DOUBLE_EQ(limited.give_double_value(), 100.0);
    EXPECT_EQ(safe.give_int_value(), 0);
    EXPECT_DOUBLE_EQ(applied.give_double_value(), 100.0);

    command.set_double(-1.0);
    EXPECT_DOUBLE_EQ(requested.give_double_value(), -115.0);
    EXPECT_DOUBLE_EQ(limited.give_double_value(), 0.0);
    EXPECT_EQ(safe.give_int_value(), 0);
    EXPECT_DOUBLE_EQ(applied.give_double_value(), 0.0);
}

TEST(IvpActiveValueGraph, PropagatesTimeDrivenControlWaveforms) {
    IVP_U_Active_Terminal_Double time("time", 0.0);
    time.add_reference();
    IVP_U_Active_Sine sine("sine", &time, 1.0, 2.0, 3.0, 0.0);
    sine.add_reference();
    IVP_U_Active_Square square("square", &time, 1.0, -2.0, 2.0);
    square.add_reference();
    IVP_U_Active_Pulse pulse("pulse", &time, 1.0, 1, 4, -1.0, 1.0);
    pulse.add_reference();

    time.set_double(0.5);
    EXPECT_NEAR(sine.give_double_value(), std::sin(0.5) * 2.0 + 3.0, 1.0e-12);
    EXPECT_DOUBLE_EQ(square.give_double_value(), -2.0);
    EXPECT_DOUBLE_EQ(pulse.give_double_value(), -1.0);

    time.set_double(1.0);
    EXPECT_NEAR(sine.give_double_value(), std::sin(1.0) * 2.0 + 3.0, 1.0e-12);
    EXPECT_DOUBLE_EQ(square.give_double_value(), 2.0);
    EXPECT_DOUBLE_EQ(pulse.give_double_value(), 1.0);
}

} // namespace
