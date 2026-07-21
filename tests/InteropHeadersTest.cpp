#include "BML/Core.h"
#include "BML/Events.h"
#include "BML/Gameplay.h"
#include "BML/Generated/bml_runtime_api.h"
#include "BML/InteropApi.h"
#include "BML/Runtime.h"
#include "BML/Scene.h"
#include "BML/UI.h"

#include <gtest/gtest.h>

TEST(InteropHeadersTest, PublicFacadesRemainHeaderOnlyAndCAbiBacked) {
    static_assert(sizeof(BML_ObjectRef) == sizeof(uint32_t) * 3, "ObjectRef must remain fixed-layout");
    static_assert(sizeof(BML_Mat4) == sizeof(float) * 16, "Mat4 must remain fixed-layout");
    BML::Events::Event event{};
    BML::Interop::Generated::Bml::Runtime::RuntimeStateValue generated{};
    EXPECT_EQ(0, event.Kind);
    EXPECT_FALSE(event.LoadData.has_value());
    EXPECT_FALSE(generated.InGame);
}
