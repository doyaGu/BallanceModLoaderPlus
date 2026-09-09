#include "UiAutomationProtocol.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void Require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}

std::string ReadText(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    Require(stream.good(), "Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

class TemporaryFile {
  public:
    explicit TemporaryFile(const char *stem)
        : m_Path(fs::temp_directory_path() /
                 (std::string(stem) + "-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                  ".tmp")) {}

    ~TemporaryFile() {
        std::error_code ignored;
        fs::remove(m_Path, ignored);
    }

    const fs::path &Path() const { return m_Path; }

  private:
    fs::path m_Path;
};

} // namespace

int main(int argc, char **argv) {
    try {
        Require(argc == 4, "Usage: UiFrameworkTest <scenario-dir> <source-root> <ui-test-dir>");
        const fs::path scenarioDirectory = fs::absolute(argv[1]);
        const fs::path sourceRoot = fs::absolute(argv[2]);
        const fs::path uiTestDirectory = fs::absolute(argv[3]);
        const auto scenarios = UiTest::LoadScenarioCatalog(scenarioDirectory, sourceRoot);

        const std::set<std::string> required = {"console", "custom-maps", "hud", "mod-menu",
                                                "script-tools"};
        std::set<std::string> names;
        for (const auto &scenario : scenarios) {
            names.insert(scenario.Name);
            const std::string source = ReadText(scenario.SourcePath);
            const std::string quotedName = "\"" + scenario.Name + "\"";
            const std::string quotedTestName = "\"" + scenario.TestName + "\"";
            Require(source.find("BML_REGISTER_UI_SCENARIO") != std::string::npos,
                    "Scenario source is not registered: " + scenario.Name);
            Require(source.find(quotedName) != std::string::npos,
                    "Scenario source name mismatch: " + scenario.Name);
            Require(source.find(quotedTestName) != std::string::npos,
                    "Scenario source test mismatch: " + scenario.Name);
        }
        for (const auto &name : required)
            Require(names.contains(name), "Missing baseline scenario: " + name);
        const auto scriptTools =
            std::find_if(scenarios.begin(), scenarios.end(),
                         [](const auto &scenario) { return scenario.Name == "script-tools"; });
        Require(scriptTools != scenarios.end() &&
                    scriptTools->Requirements == std::vector<std::string>{"angelscript"},
                "Script tools must declare its runtime requirement");
        const auto customMaps =
            std::find_if(scenarios.begin(), scenarios.end(),
                         [](const auto &scenario) { return scenario.Name == "custom-maps"; });
        Require(customMaps != scenarios.end() &&
                    customMaps->Fixtures == std::vector<std::string>{"custom-map"},
                "Custom maps must declare its test fixture");

        for (const auto &entry : fs::recursive_directory_iterator(uiTestDirectory)) {
            if (!entry.is_regular_file())
                continue;
            const auto extension = entry.path().extension().string();
            Require(extension != ".ps1" && extension != ".psm1" && extension != ".psd1",
                    "PowerShell is not allowed in the UI automation framework: " +
                        entry.path().string());
        }

        TemporaryFile sample("bml-ui-result-contract");
        {
            std::ofstream output(sample.Path(), std::ios::binary | std::ios::trunc);
            output << "format=bml-ui-result-v1\n"
                   << "scenario=hud\n"
                   << "test=hud_overlay\n"
                   << "status=passed\n"
                   << "failures=0\n";
        }
        const auto result = UiTest::ReadScenarioResult(sample.Path());
        Require(result.ScenarioName == "hud", "Result scenario was not parsed");
        Require(result.TestName == "hud_overlay", "Result test was not parsed");
        Require(result.Passed && result.Failures == 0, "Passing result was not parsed");

        {
            std::ofstream output(sample.Path(), std::ios::binary | std::ios::trunc);
            output << "format=bml-ui-result-v1\n"
                   << "scenario=hud\n"
                   << "test=hud_overlay\n"
                   << "status=passed\n"
                   << "failures=1\n";
        }
        bool inconsistentResultRejected = false;
        try {
            (void)UiTest::ReadScenarioResult(sample.Path());
        } catch (const std::exception &) {
            inconsistentResultRejected = true;
        }
        Require(inconsistentResultRejected, "Result status and failure count must agree");

        std::cout << "Validated " << scenarios.size() << " independently runnable UI scenarios\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
