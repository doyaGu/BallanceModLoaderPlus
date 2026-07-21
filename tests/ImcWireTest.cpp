#include "BML/ImcWire.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {
using BML::Imc::Wire::AddFieldSize;
using BML::Imc::Wire::FieldView;
using BML::Imc::Wire::HeaderSize;
using BML::Imc::Wire::Reader;
using BML::Imc::Wire::Type;
using BML::Imc::Wire::Writer;

TEST(ImcWireTest, RoundTripsKnownFieldsAndSkipsUnknownFields) {
    std::size_t size = HeaderSize;
    ASSERT_TRUE(AddFieldSize(size, 1));
    ASSERT_TRUE(AddFieldSize(size, 3));
    ASSERT_TRUE(AddFieldSize(size, 4));
    std::array<std::uint8_t, 128> bytes{};
    Writer writer(bytes.data(), size);
    ASSERT_EQ(writer.Begin(7, 0x123456789abcdef0ull, 3), BML_OK);
    ASSERT_EQ(writer.WriteBool(1, true), BML_OK);
    ASSERT_EQ(writer.WriteString(99, "new"), BML_OK);
    ASSERT_EQ(writer.WriteInt(2, -17), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);

    Reader reader(bytes.data(), size);
    ASSERT_EQ(reader.Begin(7), BML_OK);
    EXPECT_EQ(reader.DescriptorHash(), 0x123456789abcdef0ull);
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

TEST(ImcWireTest, RejectsSchemaAndFieldTypeMismatch) {
    constexpr std::size_t Size = HeaderSize + BML::Imc::Wire::FieldHeaderSize + 4;
    std::array<std::uint8_t, Size> bytes{};
    Writer writer(bytes.data(), bytes.size());
    ASSERT_EQ(writer.Begin(3, 1, 1), BML_OK);
    ASSERT_EQ(writer.WriteInt(1, 42), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);
    Reader wrongSchema(bytes.data(), bytes.size());
    EXPECT_EQ(wrongSchema.Begin(4), BML_ERROR_INTEROP_SCHEMA_MISMATCH);
    Reader reader(bytes.data(), bytes.size());
    ASSERT_EQ(reader.Begin(3), BML_OK);
    FieldView field;
    ASSERT_EQ(reader.Next(field), BML_OK);
    bool boolean = false;
    EXPECT_EQ(Reader::ReadBool(field, boolean), BML_ERROR_TYPE_MISMATCH);
}

TEST(ImcWireTest, RejectsTruncationTrailingBytesAndIncompleteWrites) {
    std::array<std::uint8_t, HeaderSize + BML::Imc::Wire::FieldHeaderSize + 2> bytes{};
    Writer writer(bytes.data(), bytes.size() - 1);
    ASSERT_EQ(writer.Begin(1, 1, 1), BML_OK);
    EXPECT_EQ(writer.WriteBool(1, true), BML_OK);
    EXPECT_EQ(writer.Finish(), BML_OK);
    Reader trailing(bytes.data(), bytes.size());
    ASSERT_EQ(trailing.Begin(1), BML_OK);
    FieldView field;
    ASSERT_EQ(trailing.Next(field), BML_OK);
    EXPECT_EQ(trailing.Finish(), BML_ERROR_MALFORMED_MESSAGE);
    Reader truncated(bytes.data(), HeaderSize + BML::Imc::Wire::FieldHeaderSize);
    ASSERT_EQ(truncated.Begin(1), BML_OK);
    EXPECT_EQ(truncated.Next(field), BML_ERROR_MALFORMED_MESSAGE);

    std::array<std::uint8_t, HeaderSize> header{};
    Writer incomplete(header.data(), header.size());
    ASSERT_EQ(incomplete.Begin(1, 1, 1), BML_OK);
    EXPECT_EQ(incomplete.Finish(), BML_ERROR_MALFORMED_MESSAGE);
}

TEST(ImcWireTest, UsesExactLittleEndianHeaderLayout) {
    std::array<std::uint8_t, HeaderSize> bytes{};
    Writer writer(bytes.data(), bytes.size());
    ASSERT_EQ(writer.Begin(0x11223344u, 0x0102030405060708ull, 0), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);
    EXPECT_EQ(bytes[0], 0x49);
    EXPECT_EQ(bytes[1], 0x4d);
    EXPECT_EQ(bytes[2], 0x43);
    EXPECT_EQ(bytes[3], 0x32);
    EXPECT_EQ(bytes[8], 0x44);
    EXPECT_EQ(bytes[11], 0x11);
    EXPECT_EQ(bytes[16], 0x08);
    EXPECT_EQ(bytes[23], 0x01);
}

TEST(ImcWireTest, RoundTripsWideScalarsAndBinaryPayloads) {
    const std::vector<std::uint8_t> payload{0, 1, 0xff, 0, 42};
    std::size_t size = HeaderSize;
    ASSERT_TRUE(AddFieldSize(size, 8));
    ASSERT_TRUE(AddFieldSize(size, 8));
    ASSERT_TRUE(AddFieldSize(size, 8));
    ASSERT_TRUE(AddFieldSize(size, payload.size()));

    std::vector<std::uint8_t> bytes(size);
    Writer writer(bytes.data(), bytes.size());
    const std::int64_t signedValue = (std::numeric_limits<std::int64_t>::min)() + 7;
    constexpr std::uint64_t UnsignedValue = UINT64_C(0xfedcba9876543210);
    constexpr double DoubleValue = -1234.5;
    ASSERT_EQ(writer.Begin(9, 1, 4), BML_OK);
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
    ASSERT_EQ(reader.Begin(9), BML_OK);
    FieldView field;
    while (reader.Next(field) == BML_OK) {
        if (field.Id == 1) ASSERT_EQ(Reader::ReadInt64(field, decodedSigned), BML_OK);
        else if (field.Id == 2) ASSERT_EQ(Reader::ReadUInt64(field, decodedUnsigned), BML_OK);
        else if (field.Id == 3) ASSERT_EQ(Reader::ReadDouble(field, decodedDouble), BML_OK);
        else if (field.Id == 4) ASSERT_EQ(Reader::ReadBytes(field, decodedPayload), BML_OK);
    }
    EXPECT_EQ(reader.Finish(), BML_OK);
    EXPECT_EQ(decodedSigned, signedValue);
    EXPECT_EQ(decodedUnsigned, UnsignedValue);
    EXPECT_EQ(decodedDouble, DoubleValue);
    EXPECT_EQ(decodedPayload, payload);
}

TEST(ImcWireTest, RoundTripsWideNumericArrays) {
    const std::vector<std::int64_t> signedValues{-1, 0, INT64_C(0x1020304050607080)};
    const std::vector<std::uint64_t> unsignedValues{0, UINT64_MAX};
    const std::vector<double> doubleValues{-0.0, 1.25, 1.0 / 3.0};
    std::size_t size = HeaderSize;
    ASSERT_TRUE(BML::Imc::Wire::AddFixedArrayFieldSize(size, signedValues.size(), 8));
    ASSERT_TRUE(BML::Imc::Wire::AddFixedArrayFieldSize(size, unsignedValues.size(), 8));
    ASSERT_TRUE(BML::Imc::Wire::AddFixedArrayFieldSize(size, doubleValues.size(), 8));

    std::vector<std::uint8_t> bytes(size);
    Writer writer(bytes.data(), bytes.size());
    ASSERT_EQ(writer.Begin(10, 1, 3), BML_OK);
    ASSERT_EQ(writer.WriteInt64Array(1, signedValues), BML_OK);
    ASSERT_EQ(writer.WriteUInt64Array(2, unsignedValues), BML_OK);
    ASSERT_EQ(writer.WriteDoubleArray(3, doubleValues), BML_OK);
    ASSERT_EQ(writer.Finish(), BML_OK);

    std::vector<std::int64_t> decodedSigned;
    std::vector<std::uint64_t> decodedUnsigned;
    std::vector<double> decodedDouble;
    Reader reader(bytes.data(), bytes.size());
    ASSERT_EQ(reader.Begin(10), BML_OK);
    FieldView field;
    while (reader.Next(field) == BML_OK) {
        if (field.Id == 1) ASSERT_EQ(Reader::ReadInt64Array(field, decodedSigned), BML_OK);
        else if (field.Id == 2) ASSERT_EQ(Reader::ReadUInt64Array(field, decodedUnsigned), BML_OK);
        else if (field.Id == 3) ASSERT_EQ(Reader::ReadDoubleArray(field, decodedDouble), BML_OK);
    }
    EXPECT_EQ(reader.Finish(), BML_OK);
    EXPECT_EQ(decodedSigned, signedValues);
    EXPECT_EQ(decodedUnsigned, unsignedValues);
    EXPECT_EQ(decodedDouble, doubleValues);
}

TEST(ImcWireTest, RejectsMalformedArrayPayloadsBeforeUpdatingOutput) {
    const std::array<std::uint8_t, 8> truncatedInts{{2, 0, 0, 0, 42, 0, 0, 0}};
    FieldView intField{1, Type::IntArray, 0, truncatedInts.data(), truncatedInts.size()};
    std::vector<int> ints{99};
    EXPECT_EQ(Reader::ReadIntArray(intField, ints), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(ints, std::vector<int>({99}));

    const std::array<std::uint8_t, 9> truncatedString{{1, 0, 0, 0, 5, 0, 0, 0, 'x'}};
    FieldView stringField{2, Type::StringArray, 0, truncatedString.data(), truncatedString.size()};
    std::vector<std::string> strings{"unchanged"};
    EXPECT_EQ(Reader::ReadStringArray(stringField, strings), BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_EQ(strings, std::vector<std::string>({"unchanged"}));

    const std::array<std::uint8_t, 5> invalidBool{{1, 0, 0, 0, 2}};
    FieldView boolField{3, Type::BoolArray, 0, invalidBool.data(), invalidBool.size()};
    std::vector<bool> booleans{true};
    EXPECT_EQ(Reader::ReadBoolArray(boolField, booleans), BML_ERROR_TYPE_MISMATCH);
    ASSERT_EQ(booleans.size(), 1u);
    EXPECT_TRUE(booleans[0]);
}

} // namespace
