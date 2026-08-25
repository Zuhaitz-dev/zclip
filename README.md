# zclip

A lightweight peer-to-peer clipboard sync tool written in modern C++23.

## Roadmap & Phases

- [x] **Phase 1:** Native Win32 clipboard monitoring (`WM_CLIPBOARDUPDATE`).
- [ ] **Phase 2:** Protocol framing, deduplication, and loop prevention.
- [ ] **Phase 3:** async non-blocking TCP networking
- [ ] **Phase 4:** Bidirectional synchronization engine
- [ ] **Phase 5:** System tray UI & Linux support (yes, the penguin will get it too).

## Quick Start

```cmd
# Build Release binary
task build

# Format & Lint
task format
task tidy

# Run executable
task run
```

Or using CMake directly:

```bash
cmake -B build -S .
cmake --build build --config Release
```

---

> Do whatever you want with this.