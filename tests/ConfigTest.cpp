#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>

#include <windows.h>

#include "BML/IMod.h"

#include "Config.h"
#include "Logger.h"

Logger *Logger::m_DefaultLogger = nullptr;

Logger *Logger::GetDefault() {
    return m_DefaultLogger;
}

void Logger::SetDefault(Logger *logger) {
    m_DefaultLogger = logger;
}

Logger::Logger(const char *modName) : m_ModName(modName) {}

void Logger::Info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("INFO", fmt, args);
    va_end(args);
}

void Logger::Warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("WARN", fmt, args);
    va_end(args);
}

void Logger::Error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("ERROR", fmt, args);
    va_end(args);
}

void Logger::Log(const char *level, const char *fmt, va_list args) {
    SYSTEMTIME sys;
    GetLocalTime(&sys);

    FILE *out_files[] = {
        stdout,
    };

    for (FILE *file: out_files) {
        fprintf(file, "[%02d/%02d/%d %02d:%02d:%02d.%03d] ", sys.wMonth, sys.wDay,
                sys.wYear, sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds);
        fprintf(file, "[%s/%s]: ", m_ModName, level);
        vfprintf(file, fmt, args);
        fputc('\n', file);
        fflush(file);
    }
}

ILogger *IMod::GetLogger() {
    if (m_Logger == nullptr)
        m_Logger = new Logger(GetID());
    return m_Logger;
}

IConfig *IMod::GetConfig() {
    if (m_Config == nullptr) {
        auto *config = new Config(this);
        m_Config = config;
    }
    return m_Config;
}

IMod::~IMod() {
    if (m_Logger)
        delete m_Logger;
    if (m_Config)
        delete m_Config;
}

// Mock implementation of IMod for testing
class MockMod : public IMod {
public:
    explicit MockMod(IBML *bml) : IMod(bml) {}

    const char* GetID() override { return "MockMod"; }
    const char* GetName() override { return "MockModName"; }
    const char* GetVersion() override { return "1.0"; }
    const char* GetAuthor() override { return "Tester"; }
    const char *GetDescription() override {
        return "Test description for the mock mod.";
    }
    DECLARE_BML_VERSION;

    void OnModifyConfig(const char* category, const char* key, IProperty* prop) override {
        modifiedCount++;
        lastCategory = category ? category : "";
        lastKey = key ? key : "";
        lastProp = prop;
    }

    std::atomic<int> modifiedCount{0};
    std::string lastCategory;
    std::string lastKey;
    IProperty* lastProp = nullptr;
};

// Test fixture for Config tests
class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockMod = new MockMod(nullptr);
        config = new Config(mockMod);
    }

    void TearDown() override {
        delete config;
        delete mockMod;
    }

    MockMod* mockMod;
    Config* config;
};

// Basic creation and destruction
TEST_F(ConfigTest, ConstructionDestruction) {
    ASSERT_NE(nullptr, config);
    EXPECT_EQ(mockMod, config->GetMod());

    // Test with null mod
    Config* nullModConfig = new Config(nullptr);
    EXPECT_EQ(nullptr, nullModConfig->GetMod());
    delete nullModConfig;
}

// Category management
TEST_F(ConfigTest, CategoryManagement) {
    // Test HasCategory with non-existent category
    EXPECT_FALSE(config->HasCategory("TestCategory"));
    EXPECT_FALSE(config->HasCategory(nullptr));

    // Get non-existent category should create it
    Category* cat = config->GetCategory("TestCategory");
    ASSERT_NE(nullptr, cat);
    EXPECT_STREQ("TestCategory", cat->GetName());

    // Now HasCategory should return true
    EXPECT_TRUE(config->HasCategory("TestCategory"));

    // Get existing category should return same instance
    Category* cat2 = config->GetCategory("TestCategory");
    EXPECT_EQ(cat, cat2);

    // Test category comment
    config->SetCategoryComment("TestCategory", "Test Comment");
    EXPECT_STREQ("Test Comment", config->GetCategoryComment("TestCategory"));

    // Test null comment
    config->SetCategoryComment("TestCategory", nullptr);
    EXPECT_STREQ("", config->GetCategoryComment("TestCategory"));

    // Test getting category by index
    EXPECT_EQ(cat, config->GetCategory((size_t)0));
    EXPECT_EQ(nullptr, config->GetCategory(99)); // Out of bounds
}

// Property management
TEST_F(ConfigTest, PropertyManagement) {
    // Test HasKey with non-existent property
    EXPECT_FALSE(config->HasKey("TestCategory", "TestKey"));

    // Test null parameters
    EXPECT_FALSE(config->HasKey(nullptr, "TestKey"));
    EXPECT_FALSE(config->HasKey("TestCategory", nullptr));
    EXPECT_FALSE(config->HasKey(nullptr, nullptr));

    // Get non-existent property should create it
    IProperty* prop = config->GetProperty("TestCategory", "TestKey");
    ASSERT_NE(nullptr, prop);

    // Now HasKey should return true
    EXPECT_TRUE(config->HasKey("TestCategory", "TestKey"));

    // Get existing property should return same instance
    IProperty* prop2 = config->GetProperty("TestCategory", "TestKey");
    EXPECT_EQ(prop, prop2);

    // Test null parameters for GetProperty
    EXPECT_EQ(nullptr, config->GetProperty(nullptr, "TestKey"));
    EXPECT_EQ(nullptr, config->GetProperty("TestCategory", nullptr));
    EXPECT_EQ(nullptr, config->GetProperty(nullptr, nullptr));
}

// Property types and values
TEST_F(ConfigTest, PropertyValues) {
    // String property
    IProperty* strProp = config->GetProperty("TestCategory", "StringProp");
    strProp->SetString("Test String");
    EXPECT_EQ(IProperty::STRING, strProp->GetType());
    EXPECT_STREQ("Test String", strProp->GetString());

    // Test null string
    strProp->SetString(nullptr);
    EXPECT_STREQ("", strProp->GetString());

    // Boolean property
    IProperty* boolProp = config->GetProperty("TestCategory", "BoolProp");
    boolProp->SetBoolean(true);
    EXPECT_EQ(IProperty::BOOLEAN, boolProp->GetType());
    EXPECT_TRUE(boolProp->GetBoolean());

    // Integer property
    IProperty* intProp = config->GetProperty("TestCategory", "IntProp");
    intProp->SetInteger(42);
    EXPECT_EQ(IProperty::INTEGER, intProp->GetType());
    EXPECT_EQ(42, intProp->GetInteger());

    // Float property
    IProperty* floatProp = config->GetProperty("TestCategory", "FloatProp");
    floatProp->SetFloat(3.14f);
    EXPECT_EQ(IProperty::FLOAT, floatProp->GetType());
    EXPECT_FLOAT_EQ(3.14f, floatProp->GetFloat());

    // Key property
    IProperty* keyProp = config->GetProperty("TestCategory", "KeyProp");
    keyProp->SetKey(static_cast<CKKEYBOARD>(123));
    EXPECT_EQ(IProperty::KEY, keyProp->GetType());
    EXPECT_EQ(static_cast<CKKEYBOARD>(123), keyProp->GetKey());

    // Test cross-type access returns default values
    EXPECT_STREQ("", intProp->GetString());
    EXPECT_FALSE(strProp->GetBoolean());
    EXPECT_EQ(0, strProp->GetInteger());
    EXPECT_FLOAT_EQ(0.0f, strProp->GetFloat());
    EXPECT_EQ(static_cast<CKKEYBOARD>(0), strProp->GetKey());
}

// Default values
TEST_F(ConfigTest, DefaultValues) {
    // String default
    IProperty* strProp = config->GetProperty("DefaultCategory", "StringProp");
    strProp->SetDefaultString("Default String");
    EXPECT_EQ(IProperty::STRING, strProp->GetType());
    EXPECT_STREQ("Default String", strProp->GetString());

    // Test null default string
    IProperty* nullStrProp = config->GetProperty("DefaultCategory", "NullStringProp");
    nullStrProp->SetDefaultString(nullptr);
    EXPECT_EQ(IProperty::STRING, nullStrProp->GetType());
    EXPECT_STREQ("", nullStrProp->GetString());

    // Boolean default
    IProperty* boolProp = config->GetProperty("DefaultCategory", "BoolProp");
    boolProp->SetDefaultBoolean(true);
    EXPECT_EQ(IProperty::BOOLEAN, boolProp->GetType());
    EXPECT_TRUE(boolProp->GetBoolean());

    // Integer default
    IProperty* intProp = config->GetProperty("DefaultCategory", "IntProp");
    intProp->SetDefaultInteger(42);
    EXPECT_EQ(IProperty::INTEGER, intProp->GetType());
    EXPECT_EQ(42, intProp->GetInteger());

    // Float default
    IProperty* floatProp = config->GetProperty("DefaultCategory", "FloatProp");
    floatProp->SetDefaultFloat(3.14f);
    EXPECT_EQ(IProperty::FLOAT, floatProp->GetType());
    EXPECT_FLOAT_EQ(3.14f, floatProp->GetFloat());

    // Key default
    IProperty* keyProp = config->GetProperty("DefaultCategory", "KeyProp");
    keyProp->SetDefaultKey(static_cast<CKKEYBOARD>(123));
    EXPECT_EQ(IProperty::KEY, keyProp->GetType());
    EXPECT_EQ(static_cast<CKKEYBOARD>(123), keyProp->GetKey());

    // Test setting value after default
    strProp->SetString("New Value");
    EXPECT_STREQ("New Value", strProp->GetString());
}

// Modification notification
TEST_F(ConfigTest, ModificationNotification) {
    mockMod->modifiedCount = 0;
    IProperty* prop = config->GetProperty("TestCategory", "TestProp");

    // Set initial value
    prop->SetString("Initial");
    EXPECT_EQ(1, mockMod->modifiedCount);
    EXPECT_EQ("TestCategory", mockMod->lastCategory);
    EXPECT_EQ("TestProp", mockMod->lastKey);
    EXPECT_EQ(prop, mockMod->lastProp);

    // Setting same value shouldn't trigger notification
    prop->SetString("Initial");
    EXPECT_EQ(1, mockMod->modifiedCount);

    // Setting different value should trigger notification
    prop->SetString("Changed");
    EXPECT_EQ(2, mockMod->modifiedCount);

    // Test with null mod (shouldn't crash)
    Config* nullModConfig = new Config(nullptr);
    IProperty* nullModProp = nullModConfig->GetProperty("TestCategory", "TestProp");
    nullModProp->SetString("Test"); // Shouldn't crash
    delete nullModConfig;
}

// Property utility functions
TEST_F(ConfigTest, PropertyUtilityFunctions) {
    // Test GetStringSize
    Property* strProp = static_cast<Property*>(config->GetProperty("TestCategory", "StringProp"));
    strProp->SetString("Test String");
    EXPECT_EQ(11u, strProp->GetStringSize());

    strProp->SetString("");
    EXPECT_EQ(0u, strProp->GetStringSize());

    // Test GetHash
    strProp->SetString("Test String");
    size_t hash1 = strProp->GetHash();

    // Same string should have same hash
    strProp->SetString("Test String");
    EXPECT_EQ(hash1, strProp->GetHash());

    // Different string should have different hash
    strProp->SetString("Different String");
    EXPECT_NE(hash1, strProp->GetHash());

    // Test GetXXXPtr functions
    Property* boolProp = static_cast<Property*>(config->GetProperty("TestCategory", "BoolProp"));
    boolProp->SetBoolean(true);
    bool* boolPtr = boolProp->GetBooleanPtr();
    ASSERT_NE(nullptr, boolPtr);
    EXPECT_TRUE(*boolPtr);
    *boolPtr = false;
    EXPECT_FALSE(boolProp->GetBoolean());

    Property* intProp = static_cast<Property*>(config->GetProperty("TestCategory", "IntProp"));
    intProp->SetInteger(42);
    int* intPtr = intProp->GetIntegerPtr();
    ASSERT_NE(nullptr, intPtr);
    EXPECT_EQ(42, *intPtr);
    *intPtr = 24;
    EXPECT_EQ(24, intProp->GetInteger());

    // Test wrong type returns nullptr
    EXPECT_EQ(nullptr, boolProp->GetIntegerPtr());
    EXPECT_EQ(nullptr, intProp->GetBooleanPtr());
}

// Property value copying
TEST_F(ConfigTest, PropertyCopy) {
    Property* srcProp = static_cast<Property*>(config->GetProperty("SourceCategory", "SourceProp"));
    Property* destProp = static_cast<Property*>(config->GetProperty("DestCategory", "DestProp"));

    // Test string copy
    srcProp->SetString("Test String");
    destProp->CopyValue(srcProp);
    EXPECT_EQ(IProperty::STRING, destProp->GetType());
    EXPECT_STREQ("Test String", destProp->GetString());

    // Test boolean copy
    srcProp->SetBoolean(true);
    destProp->CopyValue(srcProp);
    EXPECT_EQ(IProperty::BOOLEAN, destProp->GetType());
    EXPECT_TRUE(destProp->GetBoolean());

    // Test null copy (should not crash)
    destProp->CopyValue(nullptr);
    EXPECT_EQ(IProperty::BOOLEAN, destProp->GetType()); // Should remain unchanged
}

// File I/O
TEST_F(ConfigTest, FileIO) {
    // Create test config file
    const wchar_t* filename = L"test_config.cfg";

    // Set up some properties
    IProperty* strProp = config->GetProperty("TestCategory", "StringProp");
    strProp->SetString("Test String");
    strProp->SetComment("String Property Comment");

    IProperty* boolProp = config->GetProperty("TestCategory", "BoolProp");
    boolProp->SetBoolean(true);

    config->SetCategoryComment("TestCategory", "Test Category Comment");

    // Save the config
    EXPECT_TRUE(config->Save(filename));

    // Create a new config and load the file
    MockMod* newMockMod = new MockMod(nullptr);
    Config* newConfig = new Config(newMockMod);
    EXPECT_TRUE(newConfig->Load(filename));

    // Check that properties were loaded correctly
    EXPECT_TRUE(newConfig->HasCategory("TestCategory"));
    EXPECT_TRUE(newConfig->HasKey("TestCategory", "StringProp"));
    EXPECT_TRUE(newConfig->HasKey("TestCategory", "BoolProp"));

    IProperty* loadedStrProp = newConfig->GetProperty("TestCategory", "StringProp");
    EXPECT_EQ(IProperty::STRING, loadedStrProp->GetType());
    EXPECT_STREQ("Test String", loadedStrProp->GetString());
    EXPECT_STREQ("String Property Comment", static_cast<Property*>(loadedStrProp)->GetComment());

    IProperty* loadedBoolProp = newConfig->GetProperty("TestCategory", "BoolProp");
    EXPECT_EQ(IProperty::BOOLEAN, loadedBoolProp->GetType());
    EXPECT_TRUE(loadedBoolProp->GetBoolean());

    EXPECT_STREQ("Test Category Comment", newConfig->GetCategoryComment("TestCategory"));

    // Clean up
    delete newConfig;
    delete newMockMod;

    // Test error handling
    EXPECT_FALSE(config->Save(nullptr));
    EXPECT_FALSE(config->Save(L""));
    EXPECT_FALSE(config->Save(L"/invalid/path/file.cfg"));

    EXPECT_FALSE(config->Load(nullptr));
    EXPECT_FALSE(config->Load(L""));
    EXPECT_FALSE(config->Load(L"nonexistent_file.cfg"));
}

// Performance test
TEST_F(ConfigTest, PropertyLookupPerformance) {
    const int NUM_CATEGORIES = 10;
    const int PROPS_PER_CATEGORY = 100;
    const int NUM_LOOKUPS = 10000;

    // Create properties
    for (int c = 0; c < NUM_CATEGORIES; c++) {
        std::string category = "PerfCategory" + std::to_string(c);
        for (int p = 0; p < PROPS_PER_CATEGORY; p++) {
            std::string key = "Prop" + std::to_string(p);
            IProperty* prop = config->GetProperty(category.c_str(), key.c_str());
            prop->SetInteger(c * 1000 + p);
        }
    }

    // Time lookup operations
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_LOOKUPS; i++) {
        int c = i % NUM_CATEGORIES;
        int p = i % PROPS_PER_CATEGORY;
        std::string category = "PerfCategory" + std::to_string(c);
        std::string key = "Prop" + std::to_string(p);
        IProperty* prop = config->GetProperty(category.c_str(), key.c_str());
        int expected = c * 1000 + p;
        EXPECT_EQ(expected, prop->GetInteger());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Output for informational purposes
    std::cout << "Performed " << NUM_LOOKUPS << " property lookups in " << duration << " ms" << std::endl;

    // This is primarily a performance test, but we can set a reasonable upper bound
    EXPECT_LT(duration, 5000); // Should be well under 5 seconds on any modern system
}

// Main function to run all tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}