#ifndef BML_API_INTEROPAPI_H
#define BML_API_INTEROPAPI_H
#include "BML/Engine.h"
#include "BML/ImGui.h"
namespace BML::Api {
const BML_EngineInterface &EngineInterface();
const BML_ImGuiInterface &ImGuiInterface();
} // namespace BML::Api
#endif
