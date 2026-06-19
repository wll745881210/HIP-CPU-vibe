# HIP-CPU Agent Instructions

## Build & Verify

```bash
# Configure (from repo root)
mkdir -p build && cd build && cmake ../

# Full build (one-shot, exits on completion)
cmake --build ./

# Type/compile check only — build all targets without running tests
cmake --build ./ --target legacy_tests public_interface_tests
```

**Options:** `-Dhip_cpu_rt_BUILD_TESTING=OFF -Dhip_cpu_rt_BUILD_EXAMPLES=OFF` for headers-only.

## Test

```bash
# Run all tests (one-shot)
cd build && ctest --output-on-failure

# Run a single test via CTest tag filter
ctest -R "legacy_atomic" --output-on-failure

# Run a single test directly via Catch2 tag filter
./tests/legacy_tests [device][atomic]
./tests/legacy_tests [host][malloc][free]

# List all Catch2 tags
./tests/legacy_tests --list-tags
```

**Framework:** Catch2 v2.13.10 (vendored at `external/catch2/catch.hpp`).
Tests use `REQUIRE(... == hipSuccess)`; no exceptions.

## Architecture

**Project:** `hip_cpu_rt` — header-only C++17 INTERFACE library.
Targets the HIP Runtime API for CPU execution via cooperative fibers and STL parallel algorithms.

**Layers:**
| Layer | Path | Notes |
|-------|------|-------|
| Public API headers | `include/hip/` | Entry: `hip_runtime.h`. Functions: `hipMalloc`, `hipMemcpy`, `hipLaunchKernelGGL`, etc. |
| Internal impl | `src/include/hip/detail/` | **Must NOT be included directly.** Guarded by `#if !defined(__HIP_CPU_RT__)`. |
| Vendored deps | `external/libco/`, `external/half/` | libco = cooperative fibers, half = IEEE 754 half-precision. Both installed with the runtime. |
| Tests | `tests/`, `src/hip/` | `legacy_tests` (runtime behavior), `public_interface_tests` (compile-time API checks), `performance_tests` (benchmarks). |
| Examples | `examples/*/` | 13 examples, each in its own subdirectory with `CMakeLists.txt`. |

**Key dependencies:** C++17, Threads, `<execution>` (STL parallel algorithms), OpenMP SIMD (`-fopenmp-simd`). TBB is auto-detected via a link-test (compile+link `std::execution::par` without TBB; if it fails, TBB is linked as REQUIRED). GCC 9+ enforced.

## Code Conventions

- **Include guards:** `#pragma once` everywhere. Internal headers additionally require `__HIP_CPU_RT__` to be defined.
- **Namespace:** Implementation lives in `hip::detail`. Public API is global scope with `hip` prefix.
- **Naming:** Types `PascalCase` (`Dim3`, `Runtime`, `Tile`), functions `snake_case` (`allocate`, `launch`), device intrinsics `__prefix` (`__syncthreads`, `__shfl`).
- **Error model:** All functions return `hipError_t`. No exceptions. Always check return values.
- **Test tags:** `[host]` / `[device]` top-level, then `[malloc]`, `[atomic]`, `[math]`, etc.
- **Macros:** `__HIP_CPU_RT__` defined in all public headers. `__HIP_TILE_FUNCTION__` marks kernel code for compiler optimization (always_inline + flatten + OpenMP SIMD vectorization).

## Safety

- **DO NOT** run `cmake --build ./` without a target if only verification is needed — use a specific target instead.
- **DO NOT** run long-running/blocking processes (dev servers, watch modes). Use only one-shot commands.
- **DO NOT** modify vendored code under `external/`. Treat as read-only.
- **DO NOT** include internal headers (`src/include/hip/detail/`) from new public API headers.

## Unique Patterns

- **Kernel launch flow:** `hipLaunchKernelGGL` macro → `hip::detail::launch()` iterates tiles/blocks via `std::for_each(std::execution::par_unseq, ...)`. Each tile runs fibers cooperatively via libco to simulate GPU threads.
- **Simulated shared memory:** `Tile` object owns a shared memory buffer; `HIP_DYNAMIC_SHARED` macro accesses it. `__syncthreads()` is a tile-local fiber barrier.
- **Thread safety:** Streams use flat combining (`Flat_combiner`) backed by a dynamically-growing thread-local vector (no fixed slot limit). Thread-local storage for tile/fiber state.
- **Compiler feature detection:** `__HIP_CPU_HAS_FLATTEN_SIMD__` is set by CMake `try_compile` (replaced hardcoded `__GNUC__ != 10`). `__cpp_lib_execution >= 201902L` gates `std::execution::par`/`par_unseq` usage, falling back to `std::execution::seq` when unavailable.
- **Header-only build detection:** `__HIP_CPU_RT__` macro signals CPU runtime is in use (distinct from GPU HIP which defines `__HIP__` / `__HIPCC__`).

## File Access

- **Read-only zones:** `external/` (vendored libco, half, Catch2 — do not edit).
- **Editable:** `include/hip/` (public API), `src/include/hip/detail/` (implementation), `tests/`, `examples/`, `docs/`, `CMakeLists.txt`.
