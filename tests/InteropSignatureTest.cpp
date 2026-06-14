#include "InteropSignature.h"

#include "BML/Defines.h"

#include <gtest/gtest.h>

TEST(InteropSignatureTest, ValidatesSupportedV1Signatures) {
    BML::InteropSignatureInfo info;
    EXPECT_EQ(BML::InteropSignature::Validate("int AddOne(int value)", "AddOne", &info), BML_OK);
    EXPECT_EQ(info.Name, "AddOne");
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::Int);
    ASSERT_EQ(info.ParameterTypes.size(), 1u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::Int);

    EXPECT_EQ(BML::InteropSignature::Validate("string Echo(const string &in value)", "Echo", &info), BML_OK);
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::String);
    ASSERT_EQ(info.ParameterTypes.size(), 1u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::String);

    EXPECT_EQ(BML::InteropSignature::Validate("void SetText(string &in value)", "SetText", &info), BML_OK);
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::Void);
    ASSERT_EQ(info.ParameterTypes.size(), 1u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::String);

    EXPECT_EQ(BML::InteropSignature::Validate("void Ping()", "Ping", &info), BML_OK);
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::Void);
    EXPECT_TRUE(info.ParameterTypes.empty());
}

TEST(InteropSignatureTest, ValidatesSupportedV2Signatures) {
    BML::InteropSignatureInfo info;
    EXPECT_EQ(BML::InteropSignature::Validate(
                  "array<int>@ Values(const array<int> &in input)",
                  "Values",
                  &info),
              BML_OK);
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::IntArray);
    EXPECT_EQ(info.ReturnDescriptor.ElementType, BML::InteropValueType::Int);
    ASSERT_EQ(info.ParameterDescriptors.size(), 1u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::IntArray);
    EXPECT_EQ(info.ParameterDescriptors[0].ElementType, BML::InteropValueType::Int);
    EXPECT_EQ(info.CanonicalSignature, "array<int> Values(array<int>)");

    EXPECT_EQ(BML::InteropSignature::Validate(
                  "array<uint8>@ Digest(const array<uint8> &in bytes)",
                  "Digest",
                  &info),
              BML_OK);
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::Buffer);
    ASSERT_EQ(info.ParameterTypes.size(), 1u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::Buffer);

    EXPECT_EQ(BML::InteropSignature::Validate(
                  "int[]@ ReflectedValues(const int[]&in input)",
                  "ReflectedValues",
                  &info),
              BML_OK);
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::IntArray);
    ASSERT_EQ(info.ParameterTypes.size(), 1u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::IntArray);
    EXPECT_EQ(info.CanonicalSignature, "array<int> ReflectedValues(array<int>)");

    EXPECT_EQ(BML::InteropSignature::Validate(
                  "uint8[]@ ReflectedDigest(const uint8[]&in bytes)",
                  "ReflectedDigest",
                  &info),
              BML_OK);
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::Buffer);
    ASSERT_EQ(info.ParameterTypes.size(), 1u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::Buffer);

    EXPECT_EQ(BML::InteropSignature::Validate("CKObject@ Resolve(CKObject@ object)", "Resolve", &info),
              BML_OK);
    EXPECT_EQ(info.ReturnType, BML::InteropValueType::ObjectId);
    ASSERT_EQ(info.ParameterTypes.size(), 1u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::ObjectId);

    EXPECT_EQ(BML::InteropSignature::Validate(
                  "void Consume(const array<bool> &in flags, const array<float> &in weights, const array<string> &in names)",
                  "Consume",
                  &info),
              BML_OK);
    ASSERT_EQ(info.ParameterTypes.size(), 3u);
    EXPECT_EQ(info.ParameterTypes[0], BML::InteropValueType::BoolArray);
    EXPECT_EQ(info.ParameterTypes[1], BML::InteropValueType::FloatArray);
    EXPECT_EQ(info.ParameterTypes[2], BML::InteropValueType::StringArray);
}

TEST(InteropSignatureTest, RejectsBadOrMismatchedSignatures) {
    EXPECT_EQ(BML::InteropSignature::Validate("int MissingClose(", "MissingClose"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("double Unsupported()", "Unsupported"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("int AddOne(int value)", "OtherName"),
              BML_ERROR_INTEROP_SIGNATURE_MISMATCH);
    EXPECT_EQ(BML::InteropSignature::Validate("int Foo() trailing", "Foo"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("int Foo(void)", "Foo"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("void Foo(string &out value)", "Foo"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("void Foo(string &inout value)", "Foo"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("int Foo(int,)", "Foo"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("int Foo(, int)", "Foo"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
}

TEST(InteropSignatureTest, RejectsUnsupportedV2Shapes) {
    EXPECT_EQ(BML::InteropSignature::Validate("array<array<int>>@ Nested()", "Nested"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate(
                  "void NestedParam(const array<array<int>> &in value)",
                  "NestedParam"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate(
                  "void ObjectArray(const array<CKObject@> &in value)",
                  "ObjectArray"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("void ArrayHandle(array<int>@ value)", "ArrayHandle"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("void ArrayHandle(int[]@ value)", "ArrayHandle"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("int[][]@ NestedShorthand()", "NestedShorthand"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("const array<int> &in BadReturn()", "BadReturn"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("dictionary@ Dict()", "Dict"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("void OutArray(array<int> &out value)", "OutArray"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
    EXPECT_EQ(BML::InteropSignature::Validate("void InOutArray(array<int> &inout value)", "InOutArray"),
              BML_ERROR_INTEROP_BAD_SIGNATURE);
}

TEST(InteropSignatureTest, ComparesDescriptorEquivalentSignatures) {
    EXPECT_TRUE(BML::InteropSignature::EquivalentSignatures(
        "array<int>@ Values(const array<int> &in input)",
        "int[]@ Values(const int[]&in values)",
        "Values"));
    EXPECT_TRUE(BML::InteropSignature::EquivalentSignatures(
        "string Echo(const string &in value)",
        "string Echo(string &in text)",
        "Echo"));
    EXPECT_FALSE(BML::InteropSignature::EquivalentSignatures(
        "array<int>@ Values(const array<int> &in input)",
        "array<float>@ Values(const array<float> &in input)",
        "Values"));
}
