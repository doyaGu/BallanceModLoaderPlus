#include "BML/Generated/bml_events_imc.hpp"

#include <gtest/gtest.h>

#include <array>

TEST(ImcHeadersTest, GeneratedEventCodecRoundTripsArrayFields) {
    namespace Events = BML::Imc::Generated::Bml::Events;
    Events::EventValue input{};
    input.Kind = 7;
    input.HasObjectIds = true;
    input.ObjectIds = {{1, 2, 3}, {4, 5, 6}};
    input.HasBallCenters = true;
    input.BallCenters = {{1.0f, 2.0f, 3.0f}, {-4.0f, 5.0f, 6.0f}};
    input.HasBallRadii = true;
    input.BallRadii = {0.5f, 1.25f, 2.0f};
    input.HasCommandArgs = true;
    input.CommandArgs = {"fast", "path"};

    const std::size_t size = Events::EncodedEventSize(input);
    ASSERT_GT(size, 0u);
    ASSERT_LE(size, BML_IMC_INLINE_PAYLOAD_SIZE);
    std::array<std::uint8_t, BML_IMC_INLINE_PAYLOAD_SIZE> storage{};
    ASSERT_EQ(Events::EncodeEvent(input, storage.data(), size), BML_OK);
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = storage.data();
    message.DataSize = size;
    Events::EventValue output{};
    ASSERT_EQ(Events::DecodeEvent(message, output), BML_OK);
    ASSERT_EQ(output.ObjectIds.size(), 2u);
    EXPECT_EQ(output.ObjectIds[1].Generation, 6u);
    ASSERT_EQ(output.BallCenters.size(), 2u);
    EXPECT_FLOAT_EQ(output.BallCenters[1].x, -4.0f);
    EXPECT_EQ(output.BallRadii, input.BallRadii);
    EXPECT_EQ(output.CommandArgs, input.CommandArgs);
}
