#include "BML/ImcWire.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {
using BML::Imc::Wire::AddBoolFieldSize;
using BML::Imc::Wire::AddFixed32FieldSize;
using BML::Imc::Wire::AddFixed64FieldSize;
using BML::Imc::Wire::AddLengthDelimitedFieldSize;
using BML::Imc::Wire::FieldView;
using BML::Imc::Wire::Kind;
using BML::Imc::Wire::Reader;
using BML::Imc::Wire::Writer;

TEST(ImcWireTest, RoundTripsKnownFieldsAndSkipsUnknownFields) {
    std::size_t size = 0;
    ASSERT_TRUE(AddBoolFieldSize(size, 1));
    ASSERT_TRUE(AddLengthDelimitedFieldSize(size, 99, 3));
    ASSERT_TRUE(AddFixed32FieldSize(size, 2));
    std::array<std::uint8_t, 128> bytes{};
    Writer writer(bytes.data(), size);
    ASSERT_EQ(writer.Begin(), BML_OK);
    ASSERT_EQ(writer.WriteBool(1, true), BML_OK);
    ASSERT_EQ(writer.WriteString(99, "new"), BML_OK);
    ASSERT_EQ(writer.WriteInt(2, -17), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);

    Reader reader(bytes.data(), size);
    ASSERT_EQ(reader.Begin(), BML_OK);
    bool enabled = false;
    int value = 0;
    unsigned int known = 0;
    FieldView field;
    while (reader.Next(field) == BML_OK) {
        if (field.Id == 1) {
            ASSERT_EQ(Reader::ReadBool(field, enabled), BML_OK);
            known |= 1;
        } else if (field.Id == 2) {
            ASSERT_EQ(Reader::ReadInt(field, value), BML_OK);
            known |= 2;
        }
    }
    EXPECT_EQ(known, 3u);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(value, -17);
    EXPECT_EQ(reader.Finish(), BML_OK);
}

TEST(ImcWireTest, RejectsIncompatiblePhysicalWireKind) {
    std::size_t size = 0;
    ASSERT_TRUE(AddLengthDelimitedFieldSize(size, 1, 2));
    std::vector<std::uint8_t> bytes(size);
    Writer writer(bytes.data(), bytes.size());
    ASSERT_EQ(writer.Begin(), BML_OK);
    ASSERT_EQ(writer.WriteString(1, "42"), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);

    Reader reader(bytes.data(), bytes.size());
    ASSERT_EQ(reader.Begin(), BML_OK);
    FieldView field;
    ASSERT_EQ(reader.Next(field), BML_OK);
    int value = 0;
    EXPECT_EQ(Reader::ReadInt(field, value), BML_ERROR_TYPE_MISMATCH);
}

TEST(ImcWireTest, RejectsTruncationTrailingBytesAndIncompleteWrites) {
    std::size_t encodedSize = 0;
    ASSERT_TRUE(AddBoolFieldSize(encodedSize, 1));
    std::vector<std::uint8_t> bytes(encodedSize + 1);
    Writer writer(bytes.data(), encodedSize);
    ASSERT_EQ(writer.Begin(), BML_OK);
    ASSERT_EQ(writer.WriteBool(1, true), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);

    Reader trailing(bytes.data(), bytes.size());
    ASSERT_EQ(trailing.Begin(), BML_OK);
    FieldView field;
    ASSERT_EQ(trailing.Next(field), BML_OK);
    EXPECT_EQ(trailing.Next(field), BML_ERROR_MALFORMED_MESSAGE);

    Reader truncated(bytes.data(), encodedSize - 1);
    ASSERT_EQ(truncated.Begin(), BML_OK);
    EXPECT_EQ(truncated.Next(field), BML_ERROR_MALFORMED_MESSAGE);

    Writer incomplete(bytes.data(), bytes.size());
    ASSERT_EQ(incomplete.Begin(), BML_OK);
    ASSERT_EQ(incomplete.WriteBool(1, true), BML_OK);
    EXPECT_EQ(incomplete.Finish(), BML_ERROR_MALFORMED_MESSAGE);
}

TEST(ImcWireTest, PacksFieldIdAndWireKindIntoAProtobufStyleTag) {
    std::size_t size = 0;
    ASSERT_TRUE(AddFixed32FieldSize(size, 1));
    ASSERT_TRUE(AddFixed32FieldSize(size, 2));
    ASSERT_TRUE(AddFixed32FieldSize(size, 3));
    EXPECT_EQ(size, 15u); // Three one-byte tags plus three fixed32 values.

    std::vector<std::uint8_t> bytes(size);
    Writer writer(bytes.data(), bytes.size());
    ASSERT_EQ(writer.Begin(), BML_OK);
    ASSERT_EQ(writer.WriteInt(1, 7), BML_OK);
    ASSERT_EQ(writer.WriteInt(2, 8), BML_OK);
    ASSERT_EQ(writer.WriteInt(3, 9), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);
    EXPECT_EQ(bytes[0], (1u << 3) | static_cast<std::uint8_t>(Kind::Fixed32));
    EXPECT_EQ(bytes[5], (2u << 3) | static_cast<std::uint8_t>(Kind::Fixed32));
}

TEST(ImcWireTest, RoundTripsWideScalarsAndBinaryPayloads) {
    const std::vector<std::uint8_t> payload{0, 1, 0xff, 0, 42};
    std::size_t size = 0;
    ASSERT_TRUE(AddFixed64FieldSize(size, 1));
    ASSERT_TRUE(AddFixed64FieldSize(size, 2));
    ASSERT_TRUE(AddFixed64FieldSize(size, 3));
    ASSERT_TRUE(AddLengthDelimitedFieldSize(size, 4, payload.size()));

    std::vector<std::uint8_t> bytes(size);
    Writer writer(bytes.data(), bytes.size());
    const std::int64_t signedValue =
        (std::numeric_limits<std::int64_t>::min)() + 7;
    constexpr std::uint64_t UnsignedValue = UINT64_C(0xfedcba9876543210);
    constexpr double DoubleValue = -1234.5;
    ASSERT_EQ(writer.Begin(), BML_OK);
    ASSERT_EQ(writer.WriteInt64(1, signedValue), BML_OK);
    ASSERT_EQ(writer.WriteUInt64(2, UnsignedValue), BML_OK);
    ASSERT_EQ(writer.WriteDouble(3, DoubleValue), BML_OK);
    ASSERT_EQ(writer.WriteBytes(4, payload), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);

    std::int64_t decodedSigned = 0;
    std::uint64_t decodedUnsigned = 0;
    double decodedDouble = 0;
    std::vector<std::uint8_t> decodedPayload;
    Reader reader(bytes.data(), bytes.size());
    ASSERT_EQ(reader.Begin(), BML_OK);
    FieldView field;
    while (reader.Next(field) == BML_OK) {
        if (field.Id == 1)
            ASSERT_EQ(Reader::ReadInt64(field, decodedSigned), BML_OK);
        else if (field.Id == 2)
            ASSERT_EQ(Reader::ReadUInt64(field, decodedUnsigned), BML_OK);
        else if (field.Id == 3)
            ASSERT_EQ(Reader::ReadDouble(field, decodedDouble), BML_OK);
        else if (field.Id == 4)
            ASSERT_EQ(Reader::ReadBytes(field, decodedPayload), BML_OK);
    }
    EXPECT_EQ(reader.Finish(), BML_OK);
    EXPECT_EQ(decodedSigned, signedValue);
    EXPECT_EQ(decodedUnsigned, UnsignedValue);
    EXPECT_EQ(decodedDouble, DoubleValue);
    EXPECT_EQ(decodedPayload, payload);
}

TEST(ImcWireTest, RoundTripsPackedNumericArrays) {
    const std::vector<std::int64_t> signedValues{
        -1, 0, INT64_C(0x1020304050607080)};
    const std::vector<std::uint64_t> unsignedValues{0, UINT64_MAX};
    const std::vector<double> doubleValues{-0.0, 1.25, 1.0 / 3.0};
    std::size_t size = 0;
    ASSERT_TRUE(BML::Imc::Wire::AddFixedArrayFieldSize(
        size, 1, signedValues.size(), 8));
    ASSERT_TRUE(BML::Imc::Wire::AddFixedArrayFieldSize(
        size, 2, unsignedValues.size(), 8));
    ASSERT_TRUE(BML::Imc::Wire::AddFixedArrayFieldSize(
        size, 3, doubleValues.size(), 8));

    std::vector<std::uint8_t> bytes(size);
    Writer writer(bytes.data(), bytes.size());
    ASSERT_EQ(writer.Begin(), BML_OK);
    ASSERT_EQ(writer.WriteInt64Array(1, signedValues), BML_OK);
    ASSERT_EQ(writer.WriteUInt64Array(2, unsignedValues), BML_OK);
    ASSERT_EQ(writer.WriteDoubleArray(3, doubleValues), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);

    std::vector<std::int64_t> decodedSigned;
    std::vector<std::uint64_t> decodedUnsigned;
    std::vector<double> decodedDouble;
    Reader reader(bytes.data(), bytes.size());
    ASSERT_EQ(reader.Begin(), BML_OK);
    FieldView field;
    while (reader.Next(field) == BML_OK) {
        if (field.Id == 1)
            ASSERT_EQ(Reader::ReadInt64Array(field, decodedSigned), BML_OK);
        else if (field.Id == 2)
            ASSERT_EQ(Reader::ReadUInt64Array(field, decodedUnsigned), BML_OK);
        else if (field.Id == 3)
            ASSERT_EQ(Reader::ReadDoubleArray(field, decodedDouble), BML_OK);
    }
    EXPECT_EQ(reader.Finish(), BML_OK);
    EXPECT_EQ(decodedSigned, signedValues);
    EXPECT_EQ(decodedUnsigned, unsignedValues);
    EXPECT_EQ(decodedDouble, doubleValues);
}

TEST(ImcWireTest, RoundTripsCompositeMathValuesElementByElement) {
    const BML_Vec2 vector{1.5f, -2.25f};
    const std::vector<BML_Mat4> matrices{{
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11,
        12, 13, 14, 15,
    }};
    std::size_t size = 0;
    ASSERT_TRUE(AddLengthDelimitedFieldSize(size, 1, 8));
    ASSERT_TRUE(BML::Imc::Wire::AddFixedArrayFieldSize(
        size, 2, matrices.size(), 64));

    std::vector<std::uint8_t> bytes(size);
    Writer writer(bytes.data(), bytes.size());
    ASSERT_EQ(writer.Begin(), BML_OK);
    ASSERT_EQ(writer.WriteVec2(1, vector), BML_OK);
    ASSERT_EQ(writer.WriteMat4Array(2, matrices), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);

    BML_Vec2 decodedVector{};
    std::vector<BML_Mat4> decodedMatrices;
    Reader reader(bytes.data(), bytes.size());
    ASSERT_EQ(reader.Begin(), BML_OK);
    FieldView field;
    while (reader.Next(field) == BML_OK) {
        if (field.Id == 1)
            ASSERT_EQ(Reader::ReadVec2(field, decodedVector), BML_OK);
        else if (field.Id == 2)
            ASSERT_EQ(Reader::ReadMat4Array(field, decodedMatrices), BML_OK);
    }
    EXPECT_EQ(reader.Finish(), BML_OK);
    EXPECT_FLOAT_EQ(decodedVector.x, vector.x);
    EXPECT_FLOAT_EQ(decodedVector.y, vector.y);
    ASSERT_EQ(decodedMatrices.size(), 1u);
    EXPECT_FLOAT_EQ(decodedMatrices[0].m00, 0.0f);
    EXPECT_FLOAT_EQ(decodedMatrices[0].m13, 7.0f);
    EXPECT_FLOAT_EQ(decodedMatrices[0].m32, 14.0f);
}

TEST(ImcWireTest, RejectsMalformedArrayPayloadsBeforeUpdatingOutput) {
    const std::array<std::uint8_t, 5> truncatedInts{{42, 0, 0, 0, 1}};
    FieldView intField{
        1, Kind::LengthDelimited, truncatedInts.data(), truncatedInts.size()};
    std::vector<int> ints{99};
    EXPECT_EQ(Reader::ReadIntArray(intField, ints),
              BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(ints, std::vector<int>({99}));

    const std::array<std::uint8_t, 2> truncatedString{{5, 'x'}};
    FieldView stringField{
        2, Kind::LengthDelimited, truncatedString.data(),
        truncatedString.size()};
    std::vector<std::string> strings{"unchanged"};
    EXPECT_EQ(Reader::ReadStringArray(stringField, strings),
              BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(strings, std::vector<std::string>({"unchanged"}));

    const std::array<std::uint8_t, 1> invalidBool{{2}};
    FieldView boolField{
        3, Kind::LengthDelimited, invalidBool.data(), invalidBool.size()};
    std::vector<bool> booleans{true};
    EXPECT_EQ(Reader::ReadBoolArray(boolField, booleans),
              BML_ERROR_MALFORMED_MESSAGE);
    ASSERT_EQ(booleans.size(), 1u);
    EXPECT_TRUE(booleans[0]);
}

} // namespace
