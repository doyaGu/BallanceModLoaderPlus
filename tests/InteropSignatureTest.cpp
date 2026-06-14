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
