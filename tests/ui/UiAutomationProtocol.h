#ifndef BML_TESTS_UI_AUTOMATION_PROTOCOL_H
#define BML_TESTS_UI_AUTOMATION_PROTOCOL_H

#include <filesystem>
#include <string>
#include <vector>

namespace UiTest {

enum class InputProfile {
    ModList,
    LevelOne,
    CustomMap,
};

struct Scenario {
    std::string Name;
    std::string TestName;
    std::string Surface;
    std::string CaptureName;
    InputProfile Input = InputProfile::ModList;
    std::vector<std::string> Fixtures;
    std::vector<std::string> Requirements;
    std::vector<std::string> RequiredLogs;
    std::filesystem::path DefinitionPath;
    std::filesystem::path SourcePath;
};

struct ScenarioResult {
    std::string ScenarioName;
    std::string TestName;
    bool Passed = false;
    int Failures = 0;
};

Scenario LoadScenario(const std::filesystem::path &definitionPath,
                      const std::filesystem::path &sourceRoot);
std::vector<Scenario> LoadScenarioCatalog(const std::filesystem::path &scenarioDirectory,
                                          const std::filesystem::path &sourceRoot);
ScenarioResult ReadScenarioResult(const std::filesystem::path &path);

} // namespace UiTest

#endif // BML_TESTS_UI_AUTOMATION_PROTOCOL_H
