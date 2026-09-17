#include "UiAutomationProtocol.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;

namespace UiTest {
namespace {

std::string Trim(std::string value) {
    const auto notSpace = [](unsigned char character) { return !std::isspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

using Properties = std::map<std::string, std::vector<std::string>>;

Properties ReadProperties(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read " + path.string());

    Properties properties;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = Trim(line);
        if (line.empty() || line.front() == '#')
            continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos)
            throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) +
                                     ": expected key=value");
        const std::string key = Trim(line.substr(0, separator));
        const std::string value = Trim(line.substr(separator + 1));
        if (key.empty() || value.empty())
            throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) +
                                     ": key and value must not be empty");
        properties[key].push_back(value);
    }
    return properties;
}

std::string RequireOne(const Properties &properties, const std::string &key, const fs::path &path) {
    const auto found = properties.find(key);
    if (found == properties.end() || found->second.size() != 1)
        throw std::runtime_error(path.string() + ": expected exactly one '" + key + "'");
    return found->second.front();
}

void RejectUnknownKeys(const Properties &properties, const std::set<std::string> &allowed,
                       const fs::path &path) {
    for (const auto &property : properties) {
        if (!allowed.contains(property.first))
            throw std::runtime_error(path.string() + ": unknown key '" + property.first + "'");
    }
}

bool IsDelimitedLowercase(const std::string &value, char delimiter) {
    if (value.empty() || value.front() == delimiter || value.back() == delimiter)
        return false;
    if (!std::islower(static_cast<unsigned char>(value.front())))
        return false;
    bool previousWasDelimiter = false;
    for (const unsigned char character : value) {
        if (character == delimiter) {
            if (previousWasDelimiter)
                return false;
            previousWasDelimiter = true;
        } else {
            if (!(std::islower(character) || std::isdigit(character)))
                return false;
            previousWasDelimiter = false;
        }
    }
    return true;
}

bool IsCaptureName(const std::string &value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) != 0;
    });
}

void RequireFormat(bool valid, const fs::path &path, const char *field) {
    if (!valid)
        throw std::runtime_error(path.string() + ": invalid '" + field + "' value");
}

std::vector<std::string> ReadKnownList(const Properties &properties, const char *key,
                                       const std::set<std::string> &supported,
                                       const fs::path &path) {
    const auto found = properties.find(key);
    if (found == properties.end())
        return {};
    std::set<std::string> unique;
    for (const auto &value : found->second) {
        if (!supported.contains(value))
            throw std::runtime_error(path.string() + ": unsupported " + key + " '" + value + "'");
        if (!unique.insert(value).second)
            throw std::runtime_error(path.string() + ": duplicate " + key + " '" + value + "'");
    }
    return found->second;
}

std::vector<std::string> ReadUniqueList(const Properties &properties, const char *key,
                                        const fs::path &path) {
    const auto found = properties.find(key);
    if (found == properties.end())
        return {};
    const std::set<std::string> unique(found->second.begin(), found->second.end());
    if (unique.size() != found->second.size())
        throw std::runtime_error(path.string() + ": duplicate '" + key + "'");
    return found->second;
}

} // namespace

Scenario LoadScenario(const fs::path &definitionPath, const fs::path &sourceRoot) {
    const fs::path absoluteDefinition = fs::absolute(definitionPath);
    const Properties properties = ReadProperties(absoluteDefinition);
    RejectUnknownKeys(properties,
                      {"name", "test", "surface", "capture", "input", "fixture", "required_log",
                       "requires", "source"},
                      absoluteDefinition);

    Scenario scenario;
    scenario.Name = RequireOne(properties, "name", absoluteDefinition);
    scenario.TestName = RequireOne(properties, "test", absoluteDefinition);
    scenario.Surface = RequireOne(properties, "surface", absoluteDefinition);
    scenario.CaptureName = RequireOne(properties, "capture", absoluteDefinition);
    RequireFormat(IsDelimitedLowercase(scenario.Name, '-'), absoluteDefinition, "name");
    RequireFormat(scenario.Name != "all", absoluteDefinition, "name");
    RequireFormat(IsDelimitedLowercase(scenario.TestName, '_'), absoluteDefinition, "test");
    RequireFormat(IsDelimitedLowercase(scenario.Surface, '-'), absoluteDefinition, "surface");
    RequireFormat(IsCaptureName(scenario.CaptureName), absoluteDefinition, "capture");
    const std::string input = RequireOne(properties, "input", absoluteDefinition);
    if (input == "mod-list")
        scenario.Input = InputProfile::ModList;
    else if (input == "level-one")
        scenario.Input = InputProfile::LevelOne;
    else if (input == "custom-map")
        scenario.Input = InputProfile::CustomMap;
    else
        throw std::runtime_error(absoluteDefinition.string() + ": unsupported input profile '" +
                                 input + "'");

    scenario.Fixtures = ReadKnownList(properties, "fixture", {"custom-map"}, absoluteDefinition);
    scenario.RequiredLogs = ReadUniqueList(properties, "required_log", absoluteDefinition);
    scenario.Requirements =
        ReadKnownList(properties, "requires", {"angelscript"}, absoluteDefinition);
    if (scenario.Name != absoluteDefinition.stem().string())
        throw std::runtime_error(absoluteDefinition.string() +
                                 ": file name and scenario name differ");
    const std::string source = RequireOne(properties, "source", absoluteDefinition);
    static const std::regex sourceFormat(
        R"(^player/journeys/[a-z][a-z0-9-]*/[A-Za-z][A-Za-z0-9_]*Scenario[.]cpp$)");
    RequireFormat(std::regex_match(source, sourceFormat), absoluteDefinition, "source");
    scenario.DefinitionPath = absoluteDefinition.lexically_normal();
    scenario.SourcePath = fs::absolute(sourceRoot / source).lexically_normal();
    if (!fs::is_regular_file(scenario.SourcePath))
        throw std::runtime_error("Missing scenario source: " + scenario.SourcePath.string());
    return scenario;
}

std::vector<Scenario> LoadScenarioCatalog(const fs::path &scenarioDirectory,
                                          const fs::path &sourceRoot) {
    if (!fs::is_directory(scenarioDirectory))
        throw std::runtime_error("Scenario directory does not exist: " +
                                 scenarioDirectory.string());
    std::vector<Scenario> scenarios;
    for (const auto &entry : fs::directory_iterator(scenarioDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".scenario")
            scenarios.push_back(LoadScenario(entry.path(), sourceRoot));
    }
    std::sort(scenarios.begin(), scenarios.end(),
              [](const Scenario &left, const Scenario &right) { return left.Name < right.Name; });
    if (scenarios.empty())
        throw std::runtime_error("No UI scenarios found in " + scenarioDirectory.string());
    std::set<std::string> names;
    std::set<std::string> tests;
    std::set<std::string> captures;
    std::set<fs::path> sources;
    for (const auto &scenario : scenarios) {
        if (!names.insert(scenario.Name).second)
            throw std::runtime_error("Duplicate scenario name: " + scenario.Name);
        if (!tests.insert(scenario.TestName).second)
            throw std::runtime_error("Duplicate scenario test: " + scenario.TestName);
        if (!captures.insert(scenario.CaptureName).second)
            throw std::runtime_error("Duplicate scenario capture: " + scenario.CaptureName);
        if (!sources.insert(scenario.SourcePath).second)
            throw std::runtime_error("Duplicate scenario source: " + scenario.SourcePath.string());
    }
    return scenarios;
}

ScenarioResult ReadScenarioResult(const fs::path &path) {
    const Properties properties = ReadProperties(path);
    RejectUnknownKeys(properties, {"format", "scenario", "test", "status", "failures"}, path);
    if (RequireOne(properties, "format", path) != "bml-ui-result-v1")
        throw std::runtime_error(path.string() + ": unsupported result format");

    ScenarioResult result;
    result.ScenarioName = RequireOne(properties, "scenario", path);
    result.TestName = RequireOne(properties, "test", path);
    const std::string status = RequireOne(properties, "status", path);
    if (status != "passed" && status != "failed")
        throw std::runtime_error(path.string() + ": invalid status '" + status + "'");
    result.Passed = status == "passed";
    try {
        const std::string failures = RequireOne(properties, "failures", path);
        std::size_t consumed = 0;
        result.Failures = std::stoi(failures, &consumed);
        if (consumed != failures.size() || result.Failures < 0)
            throw std::invalid_argument("invalid count");
    } catch (const std::exception &) {
        throw std::runtime_error(path.string() + ": invalid failure count");
    }
    if (result.Passed != (result.Failures == 0))
        throw std::runtime_error(path.string() + ": status and failure count disagree");
    return result;
}

} // namespace UiTest
