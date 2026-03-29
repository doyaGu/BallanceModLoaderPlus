# DevTools Runtime Diagnostic Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform BML_DevTools from a 320-line single-file static viewer into a real-time diagnostic dashboard with memory trends, profiling counters, fault tracking, and tab-based navigation.

**Architecture:** Panel-per-file design. Each panel implements a base `Panel` interface with `Sample()` (per-frame), `Refresh()` (on-demand), and `Draw()`. DevToolsMod orchestrates panels via tab bar and drives sampling from ENGINE_POST_PROCESS.

**Tech Stack:** C++20, ImGui (PlotLines for charts), BML core APIs (Memory, Profiling, Diagnostic interfaces)

---

### Task 1: RingBuffer utility and Panel base interface

**Files:**
- Create: `modules/BML_DevTools/src/RingBuffer.h`
- Create: `modules/BML_DevTools/src/Panel.h`

- [ ] **Step 1: Create RingBuffer.h**

```cpp
// modules/BML_DevTools/src/RingBuffer.h
#ifndef BML_DEVTOOLS_RINGBUFFER_H
#define BML_DEVTOOLS_RINGBUFFER_H

#include <cstddef>

namespace devtools {

template <typename T, size_t N>
class RingBuffer {
    T m_Data[N]{};
    size_t m_Head = 0;
    size_t m_Count = 0;

public:
    void Push(T value) {
        m_Data[m_Head] = value;
        m_Head = (m_Head + 1) % N;
        if (m_Count < N) ++m_Count;
    }

    void Clear() { m_Head = 0; m_Count = 0; }
    size_t Count() const { return m_Count; }
    size_t Capacity() const { return N; }

    // 0 = oldest entry
    T operator[](size_t i) const {
        size_t start = (m_Count < N) ? 0 : m_Head;
        return m_Data[(start + i) % N];
    }

    // For ImGui::PlotLines: pass Data() as values, Capacity() as count, Offset() as offset
    const T *Data() const { return m_Data; }
    int Offset() const { return static_cast<int>(m_Head); }
    int Size() const { return static_cast<int>(m_Count < N ? m_Count : N); }
};

} // namespace devtools

#endif // BML_DEVTOOLS_RINGBUFFER_H
```

- [ ] **Step 2: Create Panel.h**

```cpp
// modules/BML_DevTools/src/Panel.h
#ifndef BML_DEVTOOLS_PANEL_H
#define BML_DEVTOOLS_PANEL_H

#include "bml_services.hpp"

namespace devtools {

class Panel {
public:
    virtual ~Panel() = default;

    virtual const char *Id() const = 0;
    virtual const char *Label(const bml::Locale &loc) const = 0;

    // Called every frame when dashboard is visible. Override for real-time data.
    virtual void Sample(const bml::ModuleServices &) {}

    // Called on manual refresh or window open. Override for snapshot data.
    virtual void Refresh(const bml::ModuleServices &) {}

    // Render ImGui content (called inside a tab item).
    virtual void Draw(const bml::ModuleServices &) = 0;
};

} // namespace devtools

#endif // BML_DEVTOOLS_PANEL_H
```

- [ ] **Step 3: Commit**

```bash
git add modules/BML_DevTools/src/RingBuffer.h modules/BML_DevTools/src/Panel.h
git commit -m "feat(devtools): add RingBuffer utility and Panel base interface"
```

---

### Task 2: Extract existing panels into separate files

Extract the 4 existing panels from DevToolsMod.cpp into individual files. No behavior changes.

**Files:**
- Create: `modules/BML_DevTools/src/ImcPanel.h`
- Create: `modules/BML_DevTools/src/InterfacePanel.h`
- Create: `modules/BML_DevTools/src/HookPanel.h`
- Create: `modules/BML_DevTools/src/ModulePanel.h`
- Modify: `modules/BML_DevTools/src/DevToolsMod.cpp`

- [ ] **Step 1: Create ImcPanel.h**

Extract `ImcTopicEntry`, `RefreshTopics()`, `RefreshImcStats()`, `DrawImcPanel()` into a Panel subclass. Keep the same logic, just wrapped in the Panel interface.

```cpp
// modules/BML_DevTools/src/ImcPanel.h
#ifndef BML_DEVTOOLS_IMCPANEL_H
#define BML_DEVTOOLS_IMCPANEL_H

#include "Panel.h"
#include "bml_imc.h"
#include "imgui.h"
#include <cstring>
#include <vector>

namespace devtools {

struct ImcTopicEntry {
    uint32_t id;
    char name[256];
    size_t subscriber_count;
    uint64_t message_count;
};

class ImcPanel : public Panel {
    std::vector<ImcTopicEntry> m_Topics;
    BML_ImcStats m_Stats{};

public:
    const char *Id() const override { return "imc"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.imc"]; }

    void Sample(const bml::ModuleServices &svc) override {
        const auto *bus = svc.Interfaces().ImcBus;
        if (bus && bus->GetStats) {
            m_Stats = BML_IMC_STATS_INIT;
            bus->GetStats(bus->Context, &m_Stats);
        }
    }

    void Refresh(const bml::ModuleServices &svc) override {
        Sample(svc);
        m_Topics.clear();
        const auto *bus = svc.Interfaces().ImcBus;
        if (!bus || !bus->GetTopicInfo) return;
        for (uint32_t id = 1; id < 1000; ++id) {
            BML_TopicInfo info = BML_TOPIC_INFO_INIT;
            if (bus->GetTopicInfo(bus->Context, id, &info) == BML_RESULT_OK) {
                ImcTopicEntry entry{};
                entry.id = info.topic_id;
                std::memcpy(entry.name, info.name, sizeof(entry.name));
                entry.subscriber_count = info.subscriber_count;
                entry.message_count = info.message_count;
                m_Topics.push_back(entry);
            }
        }
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        ImGui::Text("Published: %llu  Delivered: %llu  Dropped: %llu",
                     m_Stats.total_messages_published,
                     m_Stats.total_messages_delivered,
                     m_Stats.total_messages_dropped);
        ImGui::Text("Topics: %zu  Subscriptions: %zu  RPCs: %zu",
                     m_Stats.active_topics,
                     m_Stats.active_subscriptions,
                     m_Stats.active_rpc_handlers);
        ImGui::Separator();
        if (ImGui::BeginTable("Topics", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.id"]);
            ImGui::TableSetupColumn(loc["column.name"]);
            ImGui::TableSetupColumn(loc["column.subs"]);
            ImGui::TableSetupColumn(loc["column.messages"]);
            ImGui::TableHeadersRow();
            for (const auto &t : m_Topics) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%u", t.id);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(t.name);
                ImGui::TableNextColumn(); ImGui::Text("%zu", t.subscriber_count);
                ImGui::TableNextColumn(); ImGui::Text("%llu", t.message_count);
            }
            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_IMCPANEL_H
```

- [ ] **Step 2: Create InterfacePanel.h**

```cpp
// modules/BML_DevTools/src/InterfacePanel.h
#ifndef BML_DEVTOOLS_INTERFACEPANEL_H
#define BML_DEVTOOLS_INTERFACEPANEL_H

#include "Panel.h"
#include "bml_interface.hpp"
#include "imgui.h"
#include <string>
#include <vector>

namespace devtools {

struct InterfaceEntry {
    std::string id;
    uint16_t major;
    uint16_t minor;
    std::string provider;
    uint64_t flags;
    uint32_t lease_count;
};

class InterfacePanel : public Panel {
    std::vector<InterfaceEntry> m_Entries;

public:
    const char *Id() const override { return "interfaces"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.interfaces"]; }

    void Refresh(const bml::ModuleServices &svc) override {
        m_Entries.clear();
        const auto *diag = svc.Interfaces().Diagnostic;
        if (!diag || !diag->Context || !diag->EnumerateInterfaces) return;
        diag->EnumerateInterfaces(diag->Context, [](const BML_InterfaceRuntimeDesc *desc, void *ud) {
            auto *self = static_cast<InterfacePanel *>(ud);
            InterfaceEntry entry;
            entry.id = desc->interface_id ? desc->interface_id : "";
            entry.major = desc->abi_version.major;
            entry.minor = desc->abi_version.minor;
            entry.provider = desc->provider_id ? desc->provider_id : "";
            entry.flags = desc->flags;
            entry.lease_count = 0;
            const auto *diagApi = self->m_DiagCache;
            if (diagApi && diagApi->GetLeaseCount)
                entry.lease_count = diagApi->GetLeaseCount(diagApi->Context, desc->interface_id);
            self->m_Entries.push_back(std::move(entry));
        }, this, 0);
    }

    void Draw(const bml::ModuleServices &svc) override {
        m_DiagCache = svc.Interfaces().Diagnostic;
        const auto &loc = svc.Locale();
        if (ImGui::BeginTable("Interfaces", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.interface_id"]);
            ImGui::TableSetupColumn(loc["column.version"]);
            ImGui::TableSetupColumn(loc["column.provider"]);
            ImGui::TableSetupColumn(loc["column.flags"]);
            ImGui::TableSetupColumn(loc["column.leases"]);
            ImGui::TableHeadersRow();
            for (const auto &e : m_Entries) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.id.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%u.%u", e.major, e.minor);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(e.provider.c_str());
                ImGui::TableNextColumn(); ImGui::Text("0x%llX", e.flags);
                ImGui::TableNextColumn(); ImGui::Text("%u", e.lease_count);
            }
            ImGui::EndTable();
        }
    }

private:
    const BML_CoreDiagnosticInterface *m_DiagCache = nullptr;
};

} // namespace devtools

#endif // BML_DEVTOOLS_INTERFACEPANEL_H
```

- [ ] **Step 3: Create HookPanel.h**

```cpp
// modules/BML_DevTools/src/HookPanel.h
#ifndef BML_DEVTOOLS_HOOKPANEL_H
#define BML_DEVTOOLS_HOOKPANEL_H

#include "Panel.h"
#include "bml_hook.hpp"
#include "imgui.h"
#include <vector>

namespace devtools {

class HookPanel : public Panel {
    std::vector<bml::HookInfo> m_Hooks;

    static const char *TypeName(uint32_t t) {
        switch (t) {
            case BML_HOOK_TYPE_VTABLE:    return "VTable";
            case BML_HOOK_TYPE_INLINE:    return "Inline";
            case BML_HOOK_TYPE_WIN32_API: return "Win32";
            case BML_HOOK_TYPE_BEHAVIOR:  return "Behavior";
            default:                      return "Unknown";
        }
    }

public:
    const char *Id() const override { return "hooks"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.hooks"]; }

    void Refresh(const bml::ModuleServices &svc) override {
        m_Hooks = svc.Hooks().GetAll();
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        if (ImGui::BeginTable("Hooks", 6,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.owner"]);
            ImGui::TableSetupColumn(loc["column.target"]);
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn(loc["column.address"]);
            ImGui::TableSetupColumn(loc["column.priority"]);
            ImGui::TableSetupColumn(loc["column.flags"]);
            ImGui::TableHeadersRow();
            for (const auto &h : m_Hooks) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(h.owner.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(h.name.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(TypeName(h.hook_type));
                ImGui::TableNextColumn(); ImGui::Text("%p", h.address);
                ImGui::TableNextColumn(); ImGui::Text("%d", h.priority);
                ImGui::TableNextColumn();
                if (h.flags & BML_HOOK_FLAG_CONFLICT)
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", loc["status.conflict"]);
                else
                    ImGui::Text("0x%X", h.flags);
            }
            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_HOOKPANEL_H
```

- [ ] **Step 4: Create ModulePanel.h**

Enhanced version with Name, Description (tooltip), Authors, Directory columns.

```cpp
// modules/BML_DevTools/src/ModulePanel.h
#ifndef BML_DEVTOOLS_MODULEPANEL_H
#define BML_DEVTOOLS_MODULEPANEL_H

#include "Panel.h"
#include "imgui.h"
#include <string>
#include <vector>

namespace devtools {

struct ModuleEntry {
    std::string id;
    std::string name;
    std::string description;
    std::string authors;
    std::string directory;
    BML_Version version{};
};

class ModulePanel : public Panel {
    std::vector<ModuleEntry> m_Modules;

public:
    const char *Id() const override { return "modules"; }
    const char *Label(const bml::Locale &loc) const override { return loc["tab.modules"]; }

    void Refresh(const bml::ModuleServices &svc) override {
        m_Modules.clear();
        const auto *mod = svc.Interfaces().Module;
        if (!mod || !mod->Context || !mod->GetLoadedModuleCount || !mod->GetLoadedModuleAt) return;
        uint32_t count = mod->GetLoadedModuleCount(mod->Context);
        for (uint32_t i = 0; i < count; ++i) {
            BML_Mod handle = mod->GetLoadedModuleAt(mod->Context, i);
            if (!handle) continue;

            ModuleEntry entry;
            const char *s = nullptr;

            if (mod->GetModId(handle, &s) == BML_RESULT_OK && s) entry.id = s;
            mod->GetModVersion(handle, &entry.version);
            if (mod->GetModName && mod->GetModName(handle, &s) == BML_RESULT_OK && s) entry.name = s;
            if (mod->GetModDescription && mod->GetModDescription(handle, &s) == BML_RESULT_OK && s) entry.description = s;
            if (mod->GetModDirectory && mod->GetModDirectory(handle, &s) == BML_RESULT_OK && s) entry.directory = s;

            if (mod->GetModAuthorCount && mod->GetModAuthorAt) {
                uint32_t ac = mod->GetModAuthorCount(handle);
                for (uint32_t a = 0; a < ac; ++a) {
                    if (mod->GetModAuthorAt(handle, a, &s) == BML_RESULT_OK && s) {
                        if (!entry.authors.empty()) entry.authors += ", ";
                        entry.authors += s;
                    }
                }
            }

            m_Modules.push_back(std::move(entry));
        }
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto &loc = svc.Locale();
        if (ImGui::BeginTable("Modules", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(loc["column.module_id"]);
            ImGui::TableSetupColumn(loc["column.name"]);
            ImGui::TableSetupColumn(loc["column.version"]);
            ImGui::TableSetupColumn("Authors");
            ImGui::TableSetupColumn("Directory");
            ImGui::TableHeadersRow();
            for (const auto &m : m_Modules) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(m.id.c_str());
                if (!m.description.empty() && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", m.description.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m.name.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%u.%u.%u", m.version.major, m.version.minor, m.version.patch);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m.authors.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m.directory.c_str());
            }
            ImGui::EndTable();
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_MODULEPANEL_H
```

- [ ] **Step 5: Commit extraction**

```bash
git add modules/BML_DevTools/src/ImcPanel.h modules/BML_DevTools/src/InterfacePanel.h modules/BML_DevTools/src/HookPanel.h modules/BML_DevTools/src/ModulePanel.h
git commit -m "refactor(devtools): extract existing panels into separate files"
```

---

### Task 3: New panels -- Memory, Profiling, Fault

**Files:**
- Create: `modules/BML_DevTools/src/MemoryPanel.h`
- Create: `modules/BML_DevTools/src/ProfilingPanel.h`
- Create: `modules/BML_DevTools/src/FaultPanel.h`

- [ ] **Step 1: Create MemoryPanel.h**

```cpp
// modules/BML_DevTools/src/MemoryPanel.h
#ifndef BML_DEVTOOLS_MEMORYPANEL_H
#define BML_DEVTOOLS_MEMORYPANEL_H

#include "Panel.h"
#include "RingBuffer.h"
#include "bml_memory.h"
#include "imgui.h"

namespace devtools {

class MemoryPanel : public Panel {
    BML_MemoryStats m_Stats{};
    RingBuffer<float, 300> m_AllocHistory;
    RingBuffer<float, 300> m_ActiveHistory;

public:
    const char *Id() const override { return "memory"; }
    const char *Label(const bml::Locale &) const override { return "Memory"; }

    void Sample(const bml::ModuleServices &svc) override {
        const auto *mem = svc.Interfaces().Memory;
        if (!mem || !mem->GetMemoryStats) return;
        mem->GetMemoryStats(mem->Context, &m_Stats);
        m_AllocHistory.Push(static_cast<float>(m_Stats.total_allocated) / (1024.0f * 1024.0f));
        m_ActiveHistory.Push(static_cast<float>(m_Stats.active_alloc_count));
    }

    void Refresh(const bml::ModuleServices &svc) override { Sample(svc); }

    void Draw(const bml::ModuleServices &) override {
        float currentMB = static_cast<float>(m_Stats.total_allocated) / (1024.0f * 1024.0f);
        float peakMB = static_cast<float>(m_Stats.peak_allocated) / (1024.0f * 1024.0f);
        ImGui::Text("Allocated: %.2f MB   Peak: %.2f MB   Active: %llu",
                     currentMB, peakMB, m_Stats.active_alloc_count);
        ImGui::Text("Total Allocs: %llu   Frees: %llu   Delta: %lld",
                     m_Stats.total_alloc_count, m_Stats.total_free_count,
                     (int64_t)m_Stats.total_alloc_count - (int64_t)m_Stats.total_free_count);
        ImGui::Separator();

        char overlay[64];
        snprintf(overlay, sizeof(overlay), "%.2f MB", currentMB);
        ImGui::PlotLines("Allocated (MB)", m_AllocHistory.Data(), m_AllocHistory.Size(),
                          m_AllocHistory.Offset(), overlay, 0.0f, peakMB * 1.2f, ImVec2(0, 80));

        snprintf(overlay, sizeof(overlay), "%llu", m_Stats.active_alloc_count);
        ImGui::PlotLines("Active Allocs", m_ActiveHistory.Data(), m_ActiveHistory.Size(),
                          m_ActiveHistory.Offset(), overlay, 0.0f, 0.0f, ImVec2(0, 80));
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_MEMORYPANEL_H
```

- [ ] **Step 2: Create ProfilingPanel.h**

```cpp
// modules/BML_DevTools/src/ProfilingPanel.h
#ifndef BML_DEVTOOLS_PROFILINGPANEL_H
#define BML_DEVTOOLS_PROFILINGPANEL_H

#include "Panel.h"
#include "RingBuffer.h"
#include "bml_profiling.h"
#include "imgui.h"

namespace devtools {

class ProfilingPanel : public Panel {
    BML_ProfilingStats m_Stats = BML_PROFILING_STATS_INIT;
    BML_ProfilingStats m_PrevStats = BML_PROFILING_STATS_INIT;
    RingBuffer<float, 300> m_EventsPerFrame;
    uint64_t m_TimestampNs = 0;
    uint64_t m_CpuFreq = 0;

public:
    const char *Id() const override { return "profiling"; }
    const char *Label(const bml::Locale &) const override { return "Profiling"; }

    void Sample(const bml::ModuleServices &svc) override {
        const auto *prof = svc.Interfaces().Profiling;
        if (!prof) return;

        m_PrevStats = m_Stats;
        if (prof->GetProfilingStats) {
            m_Stats = BML_PROFILING_STATS_INIT;
            prof->GetProfilingStats(prof->Context, &m_Stats);
        }
        if (prof->GetTimestampNs) m_TimestampNs = prof->GetTimestampNs(prof->Context);
        if (prof->GetCpuFrequency) m_CpuFreq = prof->GetCpuFrequency(prof->Context);

        float delta = static_cast<float>(m_Stats.total_events - m_PrevStats.total_events);
        m_EventsPerFrame.Push(delta);
    }

    void Refresh(const bml::ModuleServices &svc) override { Sample(svc); }

    void Draw(const bml::ModuleServices &) override {
        if (ImGui::BeginTable("ProfStats", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            auto row = [](const char *label, const char *fmt, auto val) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
                ImGui::TableNextColumn(); ImGui::Text(fmt, val);
            };
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            row("Total Events", "%llu", m_Stats.total_events);
            row("Total Scopes", "%llu", m_Stats.total_scopes);
            row("Active Scopes", "%llu", m_Stats.active_scopes);
            row("Dropped Events", "%llu", m_Stats.dropped_events);
            row("Memory Used", "%llu bytes", m_Stats.memory_used_bytes);
            row("CPU Frequency", "%llu Hz", m_CpuFreq);
            ImGui::EndTable();
        }

        ImGui::Separator();
        char overlay[64];
        float last = m_EventsPerFrame.Size() > 0
            ? m_EventsPerFrame[m_EventsPerFrame.Count() - 1] : 0.0f;
        snprintf(overlay, sizeof(overlay), "%.0f evt/frame", last);
        ImGui::PlotLines("Events/Frame", m_EventsPerFrame.Data(), m_EventsPerFrame.Size(),
                          m_EventsPerFrame.Offset(), overlay, 0.0f, 0.0f, ImVec2(0, 80));
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_PROFILINGPANEL_H
```

- [ ] **Step 3: Create FaultPanel.h**

```cpp
// modules/BML_DevTools/src/FaultPanel.h
#ifndef BML_DEVTOOLS_FAULTPANEL_H
#define BML_DEVTOOLS_FAULTPANEL_H

#include "Panel.h"
#include "bml_interface.hpp"
#include "imgui.h"

namespace devtools {

class FaultPanel : public Panel {
    bool m_HasError = false;
    int32_t m_ResultCode = 0;
    char m_Message[512]{};
    char m_ApiName[128]{};
    char m_SourceFile[256]{};
    int32_t m_SourceLine = 0;

public:
    const char *Id() const override { return "faults"; }
    const char *Label(const bml::Locale &) const override { return "Faults"; }

    void Refresh(const bml::ModuleServices &svc) override {
        const auto *diag = svc.Interfaces().Diagnostic;
        if (!diag || !diag->Context || !diag->GetLastError) {
            m_HasError = false;
            return;
        }
        BML_ErrorInfo info = BML_ERROR_INFO_INIT;
        if (diag->GetLastError(diag->Context, &info) == BML_RESULT_OK && info.result_code != 0) {
            m_HasError = true;
            m_ResultCode = info.result_code;
            snprintf(m_Message, sizeof(m_Message), "%s", info.message ? info.message : "");
            snprintf(m_ApiName, sizeof(m_ApiName), "%s", info.api_name ? info.api_name : "");
            snprintf(m_SourceFile, sizeof(m_SourceFile), "%s", info.source_file ? info.source_file : "");
            m_SourceLine = info.source_line;
        } else {
            m_HasError = false;
        }
    }

    void Draw(const bml::ModuleServices &svc) override {
        const auto *diag = svc.Interfaces().Diagnostic;

        if (!m_HasError) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "No errors recorded");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last Error:");
            ImGui::Separator();
            if (ImGui::BeginTable("ErrorInfo", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("Result Code");
                ImGui::TableNextColumn(); ImGui::Text("0x%08X (%d)", m_ResultCode, m_ResultCode);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("API");
                ImGui::TableNextColumn(); ImGui::TextUnformatted(m_ApiName);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("Message");
                ImGui::TableNextColumn(); ImGui::TextWrapped("%s", m_Message);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("Source");
                ImGui::TableNextColumn(); ImGui::Text("%s:%d", m_SourceFile, m_SourceLine);

                ImGui::EndTable();
            }
        }

        if (diag && diag->ClearLastError) {
            if (ImGui::Button("Clear Error")) {
                diag->ClearLastError(diag->Context);
                m_HasError = false;
            }
        }
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_FAULTPANEL_H
```

- [ ] **Step 4: Commit new panels**

```bash
git add modules/BML_DevTools/src/MemoryPanel.h modules/BML_DevTools/src/ProfilingPanel.h modules/BML_DevTools/src/FaultPanel.h
git commit -m "feat(devtools): add Memory, Profiling, and Fault diagnostic panels"
```

---

### Task 4: Rewrite DevToolsMod.cpp with tab bar and panel orchestration

**Files:**
- Modify: `modules/BML_DevTools/src/DevToolsMod.cpp` (full rewrite)
- Modify: `modules/BML_DevTools/CMakeLists.txt` (no new .cpp files since panels are header-only)

- [ ] **Step 1: Rewrite DevToolsMod.cpp**

Replace entire file with the new orchestrator that uses the Panel interface and tab bar.

```cpp
// modules/BML_DevTools/src/DevToolsMod.cpp
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <memory>
#include <vector>

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_services.hpp"
#include "bml_input.h"
#include "bml_topics.h"
#include "bml_ui.hpp"

#include "imgui.h"

#include "Panel.h"
#include "MemoryPanel.h"
#include "ProfilingPanel.h"
#include "FaultPanel.h"
#include "ImcPanel.h"
#include "InterfacePanel.h"
#include "HookPanel.h"
#include "ModulePanel.h"

namespace {

class DevToolsMod : public bml::Module {
    bml::ui::DrawRegistration m_DrawReg;
    bml::imc::SubscriptionManager m_Subs;
    bool m_Visible = false;
    std::vector<std::unique_ptr<devtools::Panel>> m_Panels;

    static void OnDraw(const BML_UIDrawContext *, void *userData) {
        static_cast<DevToolsMod *>(userData)->Draw();
    }

    void InitPanels() {
        m_Panels.push_back(std::make_unique<devtools::MemoryPanel>());
        m_Panels.push_back(std::make_unique<devtools::ProfilingPanel>());
        m_Panels.push_back(std::make_unique<devtools::FaultPanel>());
        m_Panels.push_back(std::make_unique<devtools::ImcPanel>());
        m_Panels.push_back(std::make_unique<devtools::InterfacePanel>());
        m_Panels.push_back(std::make_unique<devtools::HookPanel>());
        m_Panels.push_back(std::make_unique<devtools::ModulePanel>());
    }

    void RefreshAll() {
        for (auto &p : m_Panels)
            p->Refresh(Services());
    }

    void SampleAll() {
        for (auto &p : m_Panels)
            p->Sample(Services());
    }

    void Draw() {
        if (!m_Visible) return;

        const auto &loc = Services().Locale();
        ImGui::SetNextWindowSize(ImVec2(800, 550), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(loc["window.title"], &m_Visible, ImGuiWindowFlags_MenuBar)) {
            ImGui::End();
            return;
        }

        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem(loc["menu.refresh"])) RefreshAll();
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("DevToolsTabs")) {
            for (auto &panel : m_Panels) {
                if (ImGui::BeginTabItem(panel->Label(loc))) {
                    panel->Draw(Services());
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

public:
    BML_Result OnAttach() override {
        Services().Locale().Load(nullptr);

        InitPanels();

        m_DrawReg = bml::ui::RegisterDraw(Handle(), "bml.devtools.overlay", 900, OnDraw, this);

        m_Subs = Services().CreateSubscriptions();

        m_Subs.Add(BML_TOPIC_INPUT_KEY_DOWN, [this](const bml::imc::Message &msg) {
            const auto *event = msg.As<BML_KeyDownEvent>();
            if (!event || event->repeat) return;
            if (event->key_code == 0x58) {
                m_Visible = !m_Visible;
                if (m_Visible) RefreshAll();
            }
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &) {
            if (m_Visible) SampleAll();
        });

        return BML_RESULT_OK;
    }

    void OnDetach() override {
        m_Panels.clear();
        m_Subs.Clear();
        m_DrawReg.Reset();
    }
};

} // namespace

BML_DEFINE_MODULE(DevToolsMod)
```

- [ ] **Step 2: Build and verify**

```bash
cmake --build build --config Release --target BML_DevTools 2>&1 | tail -10
```

Expected: BML_DevTools.dll built successfully.

- [ ] **Step 3: Run full test suite**

```bash
ctest --test-dir build -C Release
```

Expected: 56/56 pass (DevTools has no unit tests; build success is the verification).

- [ ] **Step 4: Commit**

```bash
git add modules/BML_DevTools/src/DevToolsMod.cpp
git commit -m "feat(devtools): rewrite main module with tab bar and panel orchestration"
```

---

### Task 5: Update locale files

**Files:**
- Modify: `modules/BML_DevTools/locale/en.toml`
- Modify: `modules/BML_DevTools/locale/zh-CN.toml`

- [ ] **Step 1: Update en.toml**

```toml
[window]
title = "BML Developer Tools"

[menu]
refresh = "Refresh"

[tab]
imc = "IMC"
interfaces = "Interfaces"
hooks = "Hooks"
modules = "Modules"

[column]
id = "ID"
name = "Name"
subs = "Subs"
messages = "Messages"
interface_id = "Interface ID"
version = "Version"
provider = "Provider"
flags = "Flags"
leases = "Leases"
owner = "Owner"
target = "Target"
address = "Address"
priority = "Priority"
module_id = "Module ID"

[status]
conflict = "CONFLICT"
```

Note: Memory/Profiling/Faults panels use hardcoded English labels in their `Label()` methods since they're new. Localization keys can be added incrementally.

- [ ] **Step 2: Update zh-CN.toml**

```toml
[window]
title = "BML \u5f00\u53d1\u8005\u5de5\u5177"

[menu]
refresh = "\u5237\u65b0"

[tab]
imc = "IMC \u6d88\u606f\u603b\u7ebf"
interfaces = "\u63a5\u53e3\u6ce8\u518c\u8868"
hooks = "Hook \u6ce8\u518c\u8868"
modules = "\u5df2\u52a0\u8f7d\u6a21\u5757"

[column]
id = "ID"
name = "\u540d\u79f0"
subs = "\u8ba2\u9605\u6570"
messages = "\u6d88\u606f\u6570"
interface_id = "\u63a5\u53e3 ID"
version = "\u7248\u672c"
provider = "\u63d0\u4f9b\u8005"
flags = "\u6807\u5fd7"
leases = "\u79df\u7ea6\u6570"
owner = "\u6240\u6709\u8005"
target = "\u76ee\u6807"
address = "\u5730\u5740"
priority = "\u4f18\u5148\u7ea7"
module_id = "\u6a21\u5757 ID"

[status]
conflict = "\u51b2\u7a81"
```

- [ ] **Step 3: Commit**

```bash
git add modules/BML_DevTools/locale/en.toml modules/BML_DevTools/locale/zh-CN.toml
git commit -m "chore(devtools): update locale files for tab-based layout"
```

---

### Task 6: Final build verification

- [ ] **Step 1: Full project build**

```bash
cmake --build build --config Release
```

Expected: Zero errors across all targets.

- [ ] **Step 2: Full test suite**

```bash
ctest --test-dir build -C Release
```

Expected: 56/56 pass.

- [ ] **Step 3: Verify file count**

```bash
ls modules/BML_DevTools/src/
```

Expected files: `DevToolsMod.cpp`, `Panel.h`, `RingBuffer.h`, `MemoryPanel.h`, `ProfilingPanel.h`, `FaultPanel.h`, `ImcPanel.h`, `InterfacePanel.h`, `HookPanel.h`, `ModulePanel.h`
