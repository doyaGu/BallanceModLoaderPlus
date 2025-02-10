#pragma once

#include <thread>
#include <coroutine>
#include <functional>
#include <memory>
#include <unordered_map>

namespace BML {
    // ---------------------------------------------------------
    // WaitCondition: Base class for all awaitable conditions
    // ---------------------------------------------------------

    class WaitCondition {
    public:
        virtual ~WaitCondition() = default;
        virtual bool Update(float deltaTime) = 0;
    };

    // ---------------------------------------------------------
    // Awaitable Types and Their Conditions
    // ---------------------------------------------------------

    class MillisecondsCondition : public WaitCondition {
        float remaining;

    public:
        explicit MillisecondsCondition(float ms) : remaining(ms) {}

        bool Update(float deltaTime) override { return (remaining -= deltaTime) <= 0; }
    };

    class FrameCondition : public WaitCondition {
        int remaining;

    public:
        explicit FrameCondition(int frames) : remaining(frames) {}

        bool Update(float /*dt*/) override { return --remaining <= 0; }
    };

    class UntilCondition : public WaitCondition {
        std::function<bool()> predicate;

    public:
        explicit UntilCondition(std::function<bool()> cond) : predicate(std::move(cond)) {}

        bool Update(float /*dt*/) override { return predicate(); }
    };

    // ---------------------------------------------------------
    // Awaitable Objects
    // ---------------------------------------------------------

    struct WaitForMilliseconds {
        float duration;

        explicit WaitForMilliseconds(float ms) : duration(ms) {}

        bool await_ready() const noexcept { return duration <= 0; }

        template <typename Promise>
        void await_suspend(std::coroutine_handle<Promise> h) const {
            h.promise().waitCondition = std::make_unique<MillisecondsCondition>(duration);
        }

        void await_resume() const noexcept {}
    };

    struct WaitForFrames {
        int count;

        WaitForFrames() : count(1) {}
        explicit WaitForFrames(int frames) : count(frames) {}

        bool await_ready() const noexcept { return count <= 0; }

        template <typename Promise>
        void await_suspend(std::coroutine_handle<Promise> h) const {
            h.promise().waitCondition = std::make_unique<FrameCondition>(count);
        }

        void await_resume() const noexcept {}
    };

    using WaitForNextFrame = WaitForFrames;

    struct WaitUntil {
        std::function<bool()> condition;

        explicit WaitUntil(std::function<bool()> cond) : condition(std::move(cond)) {}

        bool await_ready() const noexcept { return condition(); }

        template <typename Promise>
        void await_suspend(std::coroutine_handle<Promise> h) const {
            h.promise().waitCondition = std::make_unique<UntilCondition>(condition);
        }

        void await_resume() const noexcept {}
    };

    // ---------------------------------------------------------
    // Task System
    // ---------------------------------------------------------

    struct Task {
        struct promise_type {
            std::unique_ptr<WaitCondition> waitCondition;
            bool cancelled = false;

            Task get_return_object() noexcept {
                return Task(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }

            void return_void() noexcept {}

            void unhandled_exception() noexcept { std::terminate(); }
        };

        std::coroutine_handle<promise_type> handle;

        explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}

        ~Task() { if (handle) handle.destroy(); }

        Task(const Task &) = delete;
        Task(Task &&other) noexcept : handle(other.handle) { other.handle = {}; }

        Task &operator=(const Task &) = delete;

        Task &operator=(Task &&other) noexcept {
            if (this != &other) {
                if (handle)
                    handle.destroy();
                handle = other.handle;
                other.handle = {};
            }
            return *this;
        }

        bool Done() const noexcept { return !handle || handle.done(); }
        void Cancel() noexcept { if (!Done()) handle.promise().cancelled = true; }

        void Resume() {
            if (!Done())
                handle.resume();
        }
    };

    struct WaitForTask {
        Task &other;
        explicit WaitForTask(Task &t) : other(t) {}

        bool await_ready() const noexcept { return other.Done(); }
        template <typename Promise>
        void await_suspend(std::coroutine_handle<Promise> h) const {
            // Use an until condition to keep checking until the other task is done.
            h.promise().waitCondition = std::make_unique<UntilCondition>([&]() {
                return other.Done();
            });
        }
        void await_resume() const noexcept {}
    };

    class Scheduler {
        struct TaskContext {
            Task task;
            bool paused = false;

            explicit TaskContext(Task &&t) : task(std::move(t)) {}
        };

        std::unordered_map<int, TaskContext> tasks;
        int nextID = 1;

    public:
        int Start(Task &&task) {
            tasks.emplace(nextID, TaskContext(std::move(task)));
            return nextID++;
        }

        bool Stop(int id) noexcept {
            if (const auto it = tasks.find(id); it != tasks.end()) {
                it->second.task.Cancel();
                tasks.erase(it);
                return true;
            }
            return false;
        }

        void Pause(int id) noexcept {
            if (const auto it = tasks.find(id); it != tasks.end())
                it->second.paused = true;
        }

        void Resume(int id) noexcept {
            if (const auto it = tasks.find(id); it != tasks.end())
                it->second.paused = false;
        }

        bool Empty() const noexcept { return tasks.empty(); }

        const Task *GetTask(int id) const {
            if (const auto it = tasks.find(id); it != tasks.end())
                return &it->second.task;
            return nullptr;
        }

        Task *GetTask(int id) {
            if (const auto it = tasks.find(id); it != tasks.end())
                return &it->second.task;
            return nullptr;
        }

        void Update(float dt) {
            for (auto it = tasks.begin(); it != tasks.end();) {
                auto &[id, ctx] = *it;

                if (ctx.task.Done() || ctx.task.handle.promise().cancelled) {
                    it = tasks.erase(it);
                    continue;
                }

                if (ctx.paused) {
                    ++it;
                    continue;
                }

                auto &promise = ctx.task.handle.promise();
                if (promise.waitCondition) {
                    if (promise.waitCondition->Update(dt)) {
                        promise.waitCondition.reset();
                        ResumeCoroutine(ctx.task);
                    }
                }
                else {
                    ResumeCoroutine(ctx.task);
                }

                if (ctx.task.Done()) {
                    it = tasks.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

    private:
        static void ResumeCoroutine(const Task &task) {
            if (!task.handle.done())
                task.handle.resume();
        }
    };

    template <typename Func, typename... Args>
    int Spawn(Scheduler &scheduler, Func &&func, Args &&... args) {
        Task task = std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        return scheduler.Start(std::move(task));
    }
}
