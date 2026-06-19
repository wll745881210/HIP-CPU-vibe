# HIP CPU Runtime Code Change Log #

## Version 0.1.4142: XX December 2020 ##

* Initial release

## Vibe-Coding Updates — June 2026 ##

### Stream limit fix (flat_combiner.hpp)

* Replaced fixed `std::array<Slot_, 144>` with dynamically-growing `std::vector<Slot_>`, eliminating the ~128-stream per-thread limit that caused `"Overflowed combiner request array"` crashes.
* Destructor now clears the current thread's slot (`data_ptr = nullptr`), allowing slot reuse when streams are destroyed.
* Added regression test `[host][hipStream_t][many]` creating 200 streams.

### TBB detection refactor (CMakeLists.txt)

* Replaced fragile `check_cxx_symbol_exists(__PSTL_PAR_BACKEND_TBB)` macro probe with a compile-and-link test of `std::execution::par`.
* Added a second `try_compile` check that verifies TBB compatibility after linking, with a clear `FATAL_ERROR` when GCC < 11 is paired with oneTBB (libtbb12+).
* Removed macOS `brew` TBB hard-coded paths from `src/CMakeLists.txt` — no longer needed.

### GCC version handling (CMakeLists.txt, hip_defines.h)

* Added minimum GCC 9 version check in CMake with a clear error message.
* Replaced hardcoded `__GNUC__ != 10` in `hip_defines.h` with a CMake `try_compile` check for `__attribute__((flatten, simd))`, producing the `__HIP_CPU_HAS_FLATTEN_SIMD__` define.
* Both `__global__` and `__HIP_TILE_FUNCTION__` macros now use this feature-detection macro instead of version-specific guards.

### std::execution graceful degradation (tile.hpp, runtime.hpp)

* Added `#if __cpp_lib_execution >= 201902L` guards around `std::execution::par` / `par_unseq` usage, falling back to `std::execution::seq` when parallel execution policies are unavailable at compile time.

### Agent documentation (AGENTS.md)

* Added AGENTS.md with build/test commands, architecture map, coding conventions, and safety constraints.
