#include <gtest/gtest.h>

#include <angelscript.h>
#include <add_on/scriptstdstring/scriptstdstring.h>
#include <add_on/scriptarray/scriptarray.h>
#include <add_on/scriptmath/scriptmath.h>
#include <add_on/scripthelper/scripthelper.h>
#include <add_on/scriptbuilder/scriptbuilder.h>

#include <chrono>
#include <string>
#include <vector>

namespace {

// Minimal test harness for AngelScript
class ScriptingTest : public ::testing::Test {
protected:
    asIScriptEngine *m_Engine = nullptr;
    std::vector<std::string> m_LogOutput;

    void SetUp() override {
        m_Engine = asCreateScriptEngine();
        ASSERT_NE(m_Engine, nullptr);

        m_Engine->SetMessageCallback(asFUNCTION(MessageCallback), this, asCALL_CDECL);
        m_Engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, false);

        RegisterStdString(m_Engine);
        RegisterScriptArray(m_Engine, true);
        RegisterScriptMath(m_Engine);
        RegisterExceptionRoutines(m_Engine);

        // Register test log function
        m_Engine->RegisterGlobalFunction(
            "void log(const string &in)",
            asFUNCTION(Script_Log), asCALL_CDECL);
    }

    void TearDown() override {
        if (m_Engine) {
            m_Engine->ShutDownAndRelease();
            m_Engine = nullptr;
        }
        s_CurrentTest = nullptr;
    }

    asIScriptModule *CompileString(const char *module_name, const char *code) {
        CScriptBuilder builder;
        int r = builder.StartNewModule(m_Engine, module_name);
        if (r < 0) return nullptr;
        r = builder.AddSectionFromMemory("test", code);
        if (r < 0) return nullptr;
        r = builder.BuildModule();
        if (r < 0) return nullptr;
        return m_Engine->GetModule(module_name);
    }

    int ExecuteFunction(asIScriptModule *mod, const char *func_name) {
        auto *fn = mod->GetFunctionByName(func_name);
        if (!fn) return -1;
        auto *ctx = m_Engine->CreateContext();
        ctx->Prepare(fn);
        int r = ctx->Execute();
        ctx->Release();
        return r;
    }

    // Static thread-local for binding functions to find the test instance
    static inline thread_local ScriptingTest *s_CurrentTest = nullptr;

    static void Script_Log(const std::string &msg) {
        if (s_CurrentTest) s_CurrentTest->m_LogOutput.push_back(msg);
    }

    static void MessageCallback(const asSMessageInfo *msg, void * /*param*/) {
        // Suppress compilation messages in tests
        (void)msg;
    }
};

TEST_F(ScriptingTest, EngineCreation) {
    EXPECT_NE(m_Engine, nullptr);
    EXPECT_STREQ(asGetLibraryVersion(), "2.37.0");
}

TEST_F(ScriptingTest, CompileSimpleScript) {
    auto *mod = CompileString("test", "void main() {}");
    EXPECT_NE(mod, nullptr);
}

TEST_F(ScriptingTest, CompileError) {
    auto *mod = CompileString("test", "void main() { undeclared_var = 5; }");
    EXPECT_EQ(mod, nullptr);
}

TEST_F(ScriptingTest, ExecuteFunction) {
    s_CurrentTest = this;
    auto *mod = CompileString("test", R"(
        void main() {
            log("hello world");
        }
    )");
    ASSERT_NE(mod, nullptr);

    int r = ExecuteFunction(mod, "main");
    EXPECT_EQ(r, asEXECUTION_FINISHED);
    ASSERT_EQ(m_LogOutput.size(), 1u);
    EXPECT_EQ(m_LogOutput[0], "hello world");
}

TEST_F(ScriptingTest, MultipleLogCalls) {
    s_CurrentTest = this;
    auto *mod = CompileString("test", R"(
        void main() {
            log("one");
            log("two");
            log("three");
        }
    )");
    ASSERT_NE(mod, nullptr);
    ExecuteFunction(mod, "main");
    ASSERT_EQ(m_LogOutput.size(), 3u);
    EXPECT_EQ(m_LogOutput[0], "one");
    EXPECT_EQ(m_LogOutput[1], "two");
    EXPECT_EQ(m_LogOutput[2], "three");
}

TEST_F(ScriptingTest, ValueTypeProperties) {
    s_CurrentTest = this;

    // Register a simple POD value type with direct member access
    struct Vec3 { float x, y, z; };
    m_Engine->RegisterObjectType("Vec3", sizeof(Vec3),
        asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<Vec3>());
    m_Engine->RegisterObjectProperty("Vec3", "float x", offsetof(Vec3, x));
    m_Engine->RegisterObjectProperty("Vec3", "float y", offsetof(Vec3, y));
    m_Engine->RegisterObjectProperty("Vec3", "float z", offsetof(Vec3, z));

    auto *mod = CompileString("test", R"(
        void main() {
            Vec3 v;
            v.x = 1; v.y = 2; v.z = 3;
            log("x=" + v.x + " y=" + v.y + " z=" + v.z);
        }
    )");
    ASSERT_NE(mod, nullptr);
    ExecuteFunction(mod, "main");
    ASSERT_EQ(m_LogOutput.size(), 1u);
    EXPECT_EQ(m_LogOutput[0], "x=1 y=2 z=3");
}

TEST_F(ScriptingTest, GlobalVariables) {
    s_CurrentTest = this;
    auto *mod = CompileString("test", R"(
        int g_Counter = 0;
        void increment() { g_Counter++; }
        void report() { log("counter=" + g_Counter); }
    )");
    ASSERT_NE(mod, nullptr);
    ExecuteFunction(mod, "increment");
    ExecuteFunction(mod, "increment");
    ExecuteFunction(mod, "increment");
    ExecuteFunction(mod, "report");
    ASSERT_EQ(m_LogOutput.size(), 1u);
    EXPECT_EQ(m_LogOutput[0], "counter=3");
}

TEST_F(ScriptingTest, CoroutineSuspendResume) {
    // Register yield that suspends the context
    m_Engine->RegisterGlobalFunction(
        "void yield()",
        asFUNCTION(+[](){ asGetActiveContext()->Suspend(); }),
        asCALL_CDECL);

    auto *mod = CompileString("test", R"(
        void coro() {
            log("before");
            yield();
            log("after");
        }
    )");
    ASSERT_NE(mod, nullptr);

    s_CurrentTest = this;
    auto *fn = mod->GetFunctionByName("coro");
    ASSERT_NE(fn, nullptr);

    auto *ctx = m_Engine->CreateContext();
    ctx->Prepare(fn);

    // First execute: runs until yield()
    int r = ctx->Execute();
    EXPECT_EQ(r, asEXECUTION_SUSPENDED);
    ASSERT_EQ(m_LogOutput.size(), 1u);
    EXPECT_EQ(m_LogOutput[0], "before");

    // Resume: runs from yield() to end
    r = ctx->Execute();
    EXPECT_EQ(r, asEXECUTION_FINISHED);
    ASSERT_EQ(m_LogOutput.size(), 2u);
    EXPECT_EQ(m_LogOutput[1], "after");

    ctx->Release();
}

TEST_F(ScriptingTest, TimeoutAbort) {
    // Register line callback that aborts after 50ms
    auto *mod = CompileString("test", R"(
        void infinite_loop() {
            while (true) {}
        }
    )");
    ASSERT_NE(mod, nullptr);

    auto *fn = mod->GetFunctionByName("infinite_loop");
    ASSERT_NE(fn, nullptr);

    auto *ctx = m_Engine->CreateContext();
    ctx->Prepare(fn);

    struct TimeoutState {
        std::chrono::steady_clock::time_point start;
    } ts{std::chrono::steady_clock::now()};

    ctx->SetLineCallback(
        asFUNCTION(+[](asIScriptContext *c, void *param) {
            auto *s = static_cast<TimeoutState *>(param);
            auto elapsed = std::chrono::steady_clock::now() - s->start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > 50)
                c->Abort();
        }),
        &ts, asCALL_CDECL);

    int r = ctx->Execute();
    EXPECT_EQ(r, asEXECUTION_ABORTED);
    ctx->Release();
}

TEST_F(ScriptingTest, ModuleDiscard) {
    auto *mod = CompileString("test", "void main() {}");
    ASSERT_NE(mod, nullptr);

    m_Engine->DiscardModule("test");
    auto *after = m_Engine->GetModule("test", asGM_ONLY_IF_EXISTS);
    EXPECT_EQ(after, nullptr);
}

TEST_F(ScriptingTest, FunctionLookup) {
    auto *mod = CompileString("test", R"(
        void OnAttach() {}
        void OnDetach() {}
        void OnInit() {}
    )");
    ASSERT_NE(mod, nullptr);

    EXPECT_NE(mod->GetFunctionByName("OnAttach"), nullptr);
    EXPECT_NE(mod->GetFunctionByName("OnDetach"), nullptr);
    EXPECT_NE(mod->GetFunctionByName("OnInit"), nullptr);
    EXPECT_EQ(mod->GetFunctionByName("NonExistent"), nullptr);
}

TEST_F(ScriptingTest, StringOperations) {
    s_CurrentTest = this;
    auto *mod = CompileString("test", R"(
        void main() {
            string s = "hello";
            s += " world";
            log(s);
            log("length=" + s.length());
        }
    )");
    ASSERT_NE(mod, nullptr);
    ExecuteFunction(mod, "main");
    ASSERT_EQ(m_LogOutput.size(), 2u);
    EXPECT_EQ(m_LogOutput[0], "hello world");
    EXPECT_EQ(m_LogOutput[1], "length=11");
}

TEST_F(ScriptingTest, ArrayType) {
    s_CurrentTest = this;
    auto *mod = CompileString("test", R"(
        void main() {
            array<int> arr = {10, 20, 30};
            int sum = 0;
            for (uint i = 0; i < arr.length(); i++)
                sum += arr[i];
            log("sum=" + sum);
        }
    )");
    ASSERT_NE(mod, nullptr);
    ExecuteFunction(mod, "main");
    ASSERT_EQ(m_LogOutput.size(), 1u);
    EXPECT_EQ(m_LogOutput[0], "sum=60");
}

} // namespace
