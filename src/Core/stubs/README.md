# Core Stubs

Stub implementations used for minimal or non-VirtoolsSDK builds (e.g., Linux/POSIX
cross-platform Core builds). These files provide empty or no-op implementations of
subsystems that depend on Windows-specific features.

| File | Stubbed Subsystem |
|------|-------------------|
| `SyncApiStub.cpp` | Sync primitives registration |
| `ProfilingApiStub.cpp` | Profiling API registration |
| `ModuleRuntimeStub.cpp` | Module discovery/load/reload runtime |
| `ReloadableModuleSlotStub.cpp` | DLL hot-reload slot management |
