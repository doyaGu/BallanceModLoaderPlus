#include "Console/Shell/ShellIo.h"

#include <vector>

#include "BML/IBML.h"
#include "Console/Shell/ShellTypes.h"

namespace BML::Shell {
    namespace {
        struct Invocation {
            const std::string *input = nullptr;
            int status = Status::Ok;
        };

        thread_local std::vector<Invocation> t_Invocations;
        thread_local std::size_t t_CommandDispatchDepth = 0;
    }

    CommandDispatchScope::CommandDispatchScope() {
        if (t_CommandDispatchDepth >= Limits::MaxCommandDispatchDepth)
            return;
        ++t_CommandDispatchDepth;
        m_Active = true;
    }

    CommandDispatchScope::~CommandDispatchScope() {
        if (m_Active)
            --t_CommandDispatchDepth;
    }

    InvocationScope::InvocationScope(const std::string *input) {
        t_Invocations.push_back(Invocation{input, Status::Ok});
        m_Depth = t_Invocations.size();
    }

    InvocationScope::~InvocationScope() {
        if (t_Invocations.size() >= m_Depth)
            t_Invocations.resize(m_Depth - 1);
    }

    int InvocationScope::Status() const {
        if (t_Invocations.size() < m_Depth)
            return Status::Ok;
        return t_Invocations[m_Depth - 1].status;
    }

    bool HasInvocation() {
        return !t_Invocations.empty();
    }

    bool SetStatus(int status) {
        if (t_Invocations.empty())
            return false;
        t_Invocations.back().status = status < 0 ? Status::Failure : status;
        return true;
    }

    const std::string *GetInput() {
        if (t_Invocations.empty())
            return nullptr;
        return t_Invocations.back().input;
    }

    void Fail(IBML *bml, std::string_view message) {
        if (bml) {
            std::string line = "\x1b[31m";
            line.append(message.data(), message.size());
            line += "\x1b[0m";
            bml->SendIngameMessage(line.c_str());
        }
        SetStatus(Status::Failure);
    }
}
