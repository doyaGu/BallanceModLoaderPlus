# BML_DevTools Runtime Diagnostic Dashboard

## Problem

DevTools is a 320-line single-file module with 4 static panels (IMC, Interfaces, Hooks, Modules). Data is manually refreshed on window toggle. The core provides rich diagnostic APIs (MemoryManager, ProfilingManager, FaultTracker, DiagnosticManager) that are completely unused. Developers debugging runtime issues have no visibility into memory trends, performance hotspots, or fault history.

## Goal

Transform DevTools into a real-time diagnostic dashboard with per-frame sampling, trend visualization, and comprehensive system introspection. Panel-per-file architecture for maintainability.

## Architecture

### Module Structure

```
modules/BML_DevTools/
  src/
    DevToolsMod.cpp      -- Module entry, window chrome, tab bar, sampling engine
    Panel.h              -- Base panel interface
    MemoryPanel.h/cpp    -- NEW: Memory allocation trends
    ProfilingPanel.h/cpp -- NEW: Performance counters
    FaultPanel.h/cpp     -- NEW: Fault log + error context
    ImcPanel.h/cpp       -- EXTRACTED: IMC bus stats + topic browser
    InterfacePanel.h/cpp -- EXTRACTED: Interface registry browser
    HookPanel.h/cpp      -- EXTRACTED: Hook registry viewer
    ModulePanel.h/cpp    -- EXTRACTED: Module list with full metadata
    RingBuffer.h         -- Fixed-size circular buffer for history
  locale/
    en.toml              -- Updated with new panel labels
    zh-CN.toml           -- Updated
```

### Panel Base Interface

```cpp
// Panel.h
class Panel {
public:
    virtual ~Panel() = default;
    virtual const char *Id() const = 0;        // "memory", "profiling", etc.
    virtual const char *Label() const = 0;      // Localized display name
    virtual void Sample() {}                    // Called per-frame when visible
    virtual void Refresh() {}                   // Called on manual refresh
    virtual void Draw() = 0;                    // ImGui rendering
};
```

### Sampling Engine (in DevToolsMod)

```
BML_TOPIC_ENGINE_POST_PROCESS callback:
  if (!m_Visible) return;
  for (auto &panel : m_Panels)
      panel->Sample();    // Panels that need per-frame data override this
```

Panels override `Sample()` for real-time data (Memory, Profiling, IMC stats). Panels override `Refresh()` for snapshot data (Interfaces, Hooks, Modules). Both are called on manual "Refresh" menu action.

### Window Layout

```
[BML DevTools] ─────────────────────────────── [Refresh] [x]
┌─ Memory ─┬─ Profiling ─┬─ Faults ─┬─ IMC ─┬─ Interfaces ─┬─ Hooks ─┬─ Modules ─┐
│                                                                                   │
│  (active tab content)                                                             │
│                                                                                   │
└───────────────────────────────────────────────────────────────────────────────────┘
```

Uses `ImGui::BeginTabBar` / `ImGui::BeginTabItem` for navigation. Each tab item calls `panel->Draw()`.

## Panel Specifications

### MemoryPanel (NEW)

**Data source:** `Services().Interfaces().Memory->GetMemoryStats()`

**Display:**
- Header row: Current allocated (formatted KB/MB), Peak, Active allocations
- Trend chart: `ImGui::PlotLines` with 300-frame RingBuffer of `total_allocated`
- Secondary chart: Active allocation count over time
- Text stats: Total alloc count, total free count, alloc/free delta

**Sampling:** Every frame when tab visible. `RingBuffer<float, 300>` stores history.

### ProfilingPanel (NEW)

**Data source:** `Services().Interfaces().Profiling->GetProfilingStats()`

**Display:**
- Counters table: API call count, total events, active scopes, dropped events, memory used
- Trend chart: API calls per frame (delta between samples)
- CPU timestamp and frequency display

**Sampling:** Every frame when tab visible. Stores previous snapshot to compute deltas.

### FaultPanel (NEW)

**Data source:**
- `Services().Interfaces().Diagnostic->GetLastError()` -- thread-local error context
- FaultTracker data -- need to verify if exposed through an interface. If not, show what's available through Diagnostic.

**Display:**
- Last error section: API name, error message, source file:line, error code
- "Clear Error" button calling `ClearLastError()`
- Fault history table (if accessible): fault count, last timestamp, exception code, module ID

**Refresh:** On tab switch and manual refresh (errors are point-in-time, not streaming).

### ImcPanel (IMPROVED)

**Changes from current:**
- Stats header (published/delivered/dropped) auto-refreshes per frame via `Sample()`
- Topic list has search/filter text input above table
- Topic table gets "Msg/s" column (computed from delta between samples)

### InterfacePanel (IMPROVED)

**Changes from current:**
- No changes to refresh behavior (manual)
- Add search/filter for interface ID

### HookPanel (IMPROVED)

**No changes needed** -- already has type column from recent work. Keep as-is.

### ModulePanel (IMPROVED)

**Changes from current:**
- Add columns: Name (`GetModName`), Description (tooltip on hover), Author(s) (`GetModAuthorCount`/`GetModAuthorAt`), Directory (`GetModDirectory`)
- Author list joined with ", "
- Long text uses `ImGui::SetTooltip` on hover instead of column width

## RingBuffer Utility

```cpp
// RingBuffer.h
template <typename T, size_t N>
class RingBuffer {
    T m_Data[N]{};
    size_t m_Head = 0;
    size_t m_Count = 0;
public:
    void Push(T value);
    size_t Count() const;
    T operator[](size_t i) const;  // 0 = oldest
    const T *Data() const;         // For ImGui::PlotLines
    size_t Offset() const;         // Head offset for PlotLines
};
```

Fixed-size, no heap allocation. `ImGui::PlotLines` can use `Data()` + `Offset()` directly for circular buffer rendering.

## Activation & Controls

- **Toggle:** F12 key (unchanged, 0x58 scan code)
- **Tabs:** Click or Ctrl+1..7 for quick switch
- **Refresh:** Menu button or Ctrl+R
- **Priority:** 900 draw order (unchanged, renders on top)

## Performance Budget

- **Invisible:** Zero overhead (PostProcess callback returns immediately)
- **Visible, per frame:** ~3 API calls (GetMemoryStats, GetProfilingStats, GetImcStats) + ImGui rendering
- **Memory:** ~2.4KB for RingBuffers (300 * 2 buffers * sizeof(float))

## Migration Path

1. Create `Panel.h`, `RingBuffer.h` infrastructure
2. Extract existing panels into separate files (no behavior change)
3. Add MemoryPanel, ProfilingPanel, FaultPanel
4. Improve ImcPanel (auto-refresh stats, search) and ModulePanel (metadata columns)
5. Update DevToolsMod.cpp to use tab bar + panel array
6. Update locale files

## Verification

- `cmake --build build --config Release` -- zero errors
- `ctest --test-dir build -C Release` -- all pass
- Manual: F12 opens dashboard, tabs switch, memory chart updates per frame, stats refresh
