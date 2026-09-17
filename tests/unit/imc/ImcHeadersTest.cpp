// The generated codecs are the only place the array wire encoding is exercised
// end to end, so this drives every array element type the generator supports
// through one record. test.sample is the interface the generator tests own.
#include "test_sample_imc.hpp"

#include <gtest/gtest.h>

#include <array>

TEST(ImcHeadersTest, GeneratedCodecRoundTripsArrayFields) {
    namespace Sample = BML::Imc::Generated::Test::Sample;
    Sample::BundleValue input{};
    input.HasObjects = true;
    input.Objects = {{1, 2, 3}, {4, 5, 6}};
    input.HasPoints = true;
    input.Points = {{1.0f, 2.0f, 3.0f}, {-4.0f, 5.0f, 6.0f}};
    input.HasWeights = true;
    input.Weights = {0.5f, 1.25f, 2.0f};
    input.HasLabels = true;
    input.Labels = {"fast", "path"};

    const std::size_t size = Sample::EncodedBundleSize(input);
    ASSERT_GT(size, 0u);
    ASSERT_LE(size, BML_IMC_INLINE_PAYLOAD_SIZE);
    std::array<std::uint8_t, BML_IMC_INLINE_PAYLOAD_SIZE> storage{};
    ASSERT_EQ(Sample::EncodeBundle(input, storage.data(), size), BML_OK);
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = storage.data();
    message.DataSize = size;
    Sample::BundleValue output{};
    ASSERT_EQ(Sample::DecodeBundle(message, output), BML_OK);
    ASSERT_EQ(output.Objects.size(), 2u);
    EXPECT_EQ(output.Objects[1].Generation, 6u);
    ASSERT_EQ(output.Points.size(), 2u);
    EXPECT_FLOAT_EQ(output.Points[1].x, -4.0f);
    EXPECT_EQ(output.Weights, input.Weights);
    EXPECT_EQ(output.Labels, input.Labels);
}
