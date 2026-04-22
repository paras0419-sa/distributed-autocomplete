# Phase 0 + 1 — Skeleton & Single-Tenant In-Memory Index — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up the C++20 repo skeleton and deliver a single-tenant in-memory autocomplete that reads a static corpus and serves top-K prefix suggestions from stdin.

**Architecture:** Three focused layers behind interfaces that outlive Phase 1: a `StringArena` + `EntryTable` value store, a swappable `FstIndex` (v1 impl = simple pointer trie; double-array trie deferred to end of Phase 1 as an optimization), and a `QueryExecutor` that tokenizes, prefix-walks the index, and runs a fixed-capacity top-K heap. Everything built test-first with GoogleTest, clang-18, ASan + UBSan on from day one. No networking, no tenants, no overlay yet — those come in Phase 2+.

**Tech Stack:** C++20, CMake 3.25+, Conan 2, clang-18, GoogleTest, Google Benchmark, spdlog. No third-party FST/trie library — v1 trie is hand-rolled (learning goal).

**Out of scope for this plan:** mutable overlay (§4.3), multi-tenant scheduling, networking, artifact lifecycle, event log, builder, FST library swap, double-array trie (its own follow-on plan if the simple trie's perf proves insufficient at Phase 2 bench gate).

---

## File structure established by this plan

```
distributed-autocomplete/
├── .gitignore
├── .clang-format
├── .clang-tidy
├── CMakeLists.txt                        # top-level, C++20, sanitizers, options
├── conanfile.txt                         # gtest, benchmark, spdlog
├── cmake/
│   ├── Sanitizers.cmake                  # ASan/UBSan toggle
│   └── Warnings.cmake                    # -Wall -Wextra -Wpedantic -Werror
├── .github/workflows/ci.yml              # build + test + sanitizers
├── libs/
│   ├── common/
│   │   ├── CMakeLists.txt
│   │   ├── include/autocomplete/common/{Result.h,Error.h,Logger.h}
│   │   └── src/Logger.cpp
│   ├── memory/
│   │   ├── CMakeLists.txt
│   │   └── include/autocomplete/memory/StringArena.h
│   ├── index/
│   │   ├── CMakeLists.txt
│   │   ├── include/autocomplete/index/{Entry.h,EntryTable.h,FstIndex.h,SimpleTrie.h,IndexView.h}
│   │   └── src/SimpleTrie.cpp
│   └── query/
│       ├── CMakeLists.txt
│       ├── include/autocomplete/query/{Tokenizer.h,FixedTopK.h,QueryExecutor.h}
│       └── src/{Tokenizer.cpp,QueryExecutor.cpp}
├── bin/
│   └── serving_node/
│       ├── CMakeLists.txt
│       └── src/main.cpp                  # stdin REPL, loads CSV corpus
├── tests/
│   └── unit/
│       ├── CMakeLists.txt
│       ├── common/{result_test.cpp,error_test.cpp}
│       ├── memory/string_arena_test.cpp
│       ├── index/{entry_table_test.cpp,simple_trie_test.cpp,index_view_test.cpp}
│       └── query/{tokenizer_test.cpp,fixed_topk_test.cpp,query_executor_test.cpp}
└── scripts/
    └── gen_corpus.py                     # generate sample CSV for manual testing
```

**Dependency direction (enforced by CMake `target_link_libraries`):**
`common` ← `memory` ← `index` ← `query` ← `bin/serving_node`. Tests link only what they need. No back-edges.

---

# PHASE 0 — Repo Skeleton

## Task 0.1: Initialize git repo + `.gitignore`

**Files:**
- Create: `.gitignore`

- [ ] **Step 1: Initialize repo**

Run:
```bash
git init -b main
```
Expected: `Initialized empty Git repository in .../distributed-autocomplete/.git/`

- [ ] **Step 2: Create `.gitignore`**

Write `.gitignore`:
```gitignore
# Build
build/
cmake-build-*/
out/

# Conan
CMakeUserPresets.json
conan_toolchain.cmake
conanbuildinfo.*
conaninfo.txt
graph_info.json

# IDE
.idea/
.vscode/
*.swp
.DS_Store

# Compiled
*.o
*.a
*.so
*.dylib

# Artifacts (produced in later phases)
/var/
/artifacts/
```

- [ ] **Step 3: Initial commit**

Run:
```bash
git add .gitignore docs/
git commit -m "chore: initialize repo with spec and gitignore"
```
Expected: commit hash printed, working tree clean.

---

## Task 0.2: Top-level `CMakeLists.txt` + warnings + sanitizers modules

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/Warnings.cmake`
- Create: `cmake/Sanitizers.cmake`

- [ ] **Step 1: Write `cmake/Warnings.cmake`**

```cmake
# cmake/Warnings.cmake
function(autocomplete_set_warnings target)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
    -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion
    -Werror
  )
endfunction()
```

- [ ] **Step 2: Write `cmake/Sanitizers.cmake`**

```cmake
# cmake/Sanitizers.cmake
option(AUTOCOMPLETE_SANITIZE "Enable ASan + UBSan" ON)

function(autocomplete_apply_sanitizers target)
  if(AUTOCOMPLETE_SANITIZE)
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE
      -fsanitize=address,undefined)
  endif()
endfunction()
```

- [ ] **Step 3: Write top-level `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.25)
project(distributed_autocomplete LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Warnings)
include(Sanitizers)

enable_testing()

add_subdirectory(libs/common)
add_subdirectory(libs/memory)
add_subdirectory(libs/index)
add_subdirectory(libs/query)
add_subdirectory(bin/serving_node)
add_subdirectory(tests/unit)
```

- [ ] **Step 4: Verify configure fails cleanly** (subdirs don't exist yet)

Run:
```bash
cmake -S . -B build
```
Expected: FAIL — `add_subdirectory given source "libs/common" which is not an existing directory`. This confirms the top-level is wired; subdirs come in subsequent tasks.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake/
git commit -m "build: top-level CMake, warning + sanitizer helpers"
```

---

## Task 0.3: Conan profile + `conanfile.txt`

**Files:**
- Create: `conanfile.txt`
- Create: `.clang-format`

- [ ] **Step 1: Write `conanfile.txt`**

```ini
[requires]
gtest/1.14.0
benchmark/1.8.3
spdlog/1.13.0

[generators]
CMakeDeps
CMakeToolchain

[layout]
cmake_layout
```

- [ ] **Step 2: Write `.clang-format`**

```yaml
BasedOnStyle: Google
IndentWidth: 2
ColumnLimit: 100
PointerAlignment: Left
AllowShortFunctionsOnASingleLine: Empty
IncludeBlocks: Regroup
```

- [ ] **Step 3: Install deps + verify Conan resolves**

Run:
```bash
conan profile detect --force
conan install . --build=missing -s compiler.cppstd=20
```
Expected: `conan_toolchain.cmake` written under `build/` (or `build/Release/generators/`). No resolution errors.

- [ ] **Step 4: Commit**

```bash
git add conanfile.txt .clang-format
git commit -m "build: Conan deps (gtest, benchmark, spdlog)"
```

---

## Task 0.4: `libs/common` — `Error` enum (TDD)

**Files:**
- Create: `libs/common/CMakeLists.txt`
- Create: `libs/common/include/autocomplete/common/Error.h`
- Create: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/common/error_test.cpp`

- [ ] **Step 1: Write `libs/common/CMakeLists.txt`**

```cmake
add_library(autocomplete_common INTERFACE)
target_include_directories(autocomplete_common INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(autocomplete::common ALIAS autocomplete_common)
```

- [ ] **Step 2: Write `tests/unit/CMakeLists.txt`**

```cmake
find_package(GTest REQUIRED)

function(autocomplete_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE GTest::gtest_main)
  autocomplete_set_warnings(${name})
  autocomplete_apply_sanitizers(${name})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

autocomplete_add_test(common_error_test common/error_test.cpp)
target_link_libraries(common_error_test PRIVATE autocomplete::common)
```

- [ ] **Step 3: Write the failing test** `tests/unit/common/error_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "autocomplete/common/Error.h"

using autocomplete::common::Error;
using autocomplete::common::ErrorCode;

TEST(ErrorTest, ConstructCarriesCodeAndMessage) {
  Error e{ErrorCode::kInvalidInput, "bad prefix"};
  EXPECT_EQ(e.code(), ErrorCode::kInvalidInput);
  EXPECT_EQ(e.message(), "bad prefix");
}

TEST(ErrorTest, CodeToStringKnownCodes) {
  EXPECT_STREQ(to_string(ErrorCode::kInvalidInput), "InvalidInput");
  EXPECT_STREQ(to_string(ErrorCode::kNotFound), "NotFound");
  EXPECT_STREQ(to_string(ErrorCode::kInternal), "Internal");
}
```

- [ ] **Step 4: Run — expect compile failure (header missing)**

Run:
```bash
cmake --build build --target common_error_test
```
Expected: FAIL — `fatal error: 'autocomplete/common/Error.h' file not found`.

- [ ] **Step 5: Implement `Error.h`**

```cpp
#pragma once
#include <string>
#include <string_view>

namespace autocomplete::common {

enum class ErrorCode {
  kInvalidInput,
  kNotFound,
  kInternal,
};

constexpr const char* to_string(ErrorCode c) {
  switch (c) {
    case ErrorCode::kInvalidInput: return "InvalidInput";
    case ErrorCode::kNotFound:     return "NotFound";
    case ErrorCode::kInternal:     return "Internal";
  }
  return "Unknown";
}

class Error {
 public:
  Error(ErrorCode c, std::string msg) : code_{c}, message_{std::move(msg)} {}
  ErrorCode code() const noexcept { return code_; }
  const std::string& message() const noexcept { return message_; }

 private:
  ErrorCode   code_;
  std::string message_;
};

}  // namespace autocomplete::common
```

- [ ] **Step 6: Run — expect PASS**

Run:
```bash
cmake --build build --target common_error_test && ctest --test-dir build -R common_error_test --output-on-failure
```
Expected: `2 tests from ErrorTest ... PASSED`.

- [ ] **Step 7: Commit**

```bash
git add libs/common tests/unit/CMakeLists.txt tests/unit/common/error_test.cpp
git commit -m "feat(common): Error type with ErrorCode enum + to_string"
```

---

## Task 0.5: `libs/common` — `Result<T>` (TDD)

**Files:**
- Create: `libs/common/include/autocomplete/common/Result.h`
- Create: `tests/unit/common/result_test.cpp`
- Modify: `tests/unit/CMakeLists.txt` (add new test)

**Why:** `Result<T>` (C++20-flavored expected) is the error channel for fallible operations across the codebase. Hand-rolled instead of `std::expected` because clang-18 libc++ support is partial and we want tight control.

- [ ] **Step 1: Write the failing test** `tests/unit/common/result_test.cpp`

```cpp
#include <gtest/gtest.h>
#include <string>
#include "autocomplete/common/Result.h"

using autocomplete::common::Result;
using autocomplete::common::Error;
using autocomplete::common::ErrorCode;

TEST(ResultTest, OkCarriesValue) {
  Result<int> r = Result<int>::ok(42);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 42);
}

TEST(ResultTest, ErrCarriesError) {
  Result<int> r = Result<int>::err(Error{ErrorCode::kNotFound, "missing"});
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(r.error().code(), ErrorCode::kNotFound);
}

TEST(ResultTest, MoveOnlyValueTypeSupported) {
  Result<std::string> r = Result<std::string>::ok("hello");
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "hello");
  std::string moved = std::move(*r);
  EXPECT_EQ(moved, "hello");
}
```

- [ ] **Step 2: Register test in `tests/unit/CMakeLists.txt`**

Append:
```cmake
autocomplete_add_test(common_result_test common/result_test.cpp)
target_link_libraries(common_result_test PRIVATE autocomplete::common)
```

- [ ] **Step 3: Run — expect FAIL (header missing)**

Run:
```bash
cmake --build build --target common_result_test
```
Expected: FAIL — header not found.

- [ ] **Step 4: Implement `Result.h`**

```cpp
#pragma once
#include <utility>
#include <variant>
#include "autocomplete/common/Error.h"

namespace autocomplete::common {

template <typename T>
class Result {
 public:
  static Result ok(T v)         { return Result{std::move(v)}; }
  static Result err(Error e)    { return Result{std::move(e)}; }

  bool has_value() const noexcept { return data_.index() == 0; }
  explicit operator bool() const noexcept { return has_value(); }

  T&       operator*()       { return std::get<0>(data_); }
  const T& operator*() const { return std::get<0>(data_); }

  const Error& error() const { return std::get<1>(data_); }

 private:
  explicit Result(T v)     : data_{std::in_place_index<0>, std::move(v)} {}
  explicit Result(Error e) : data_{std::in_place_index<1>, std::move(e)} {}
  std::variant<T, Error> data_;
};

}  // namespace autocomplete::common
```

- [ ] **Step 5: Run — expect PASS**

Run:
```bash
cmake --build build --target common_result_test && ctest --test-dir build -R common_result_test --output-on-failure
```
Expected: 3 tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/common/include/autocomplete/common/Result.h tests/unit/common/result_test.cpp tests/unit/CMakeLists.txt
git commit -m "feat(common): Result<T> with ok/err factories"
```

---

## Task 0.6: `bin/serving_node` — hello world

**Files:**
- Create: `bin/serving_node/CMakeLists.txt`
- Create: `bin/serving_node/src/main.cpp`

- [ ] **Step 1: Write `bin/serving_node/CMakeLists.txt`**

```cmake
add_executable(serving_node src/main.cpp)
target_link_libraries(serving_node PRIVATE autocomplete::common)
autocomplete_set_warnings(serving_node)
autocomplete_apply_sanitizers(serving_node)
```

- [ ] **Step 2: Write `bin/serving_node/src/main.cpp`**

```cpp
#include <cstdio>
int main() {
  std::puts("distributed-autocomplete serving_node (phase 0)");
  return 0;
}
```

- [ ] **Step 3: Build + run**

Run:
```bash
cmake --build build --target serving_node
./build/bin/serving_node/serving_node
```
(Adjust path to wherever Conan's `cmake_layout` placed it; typical: `build/Release/bin/serving_node/serving_node`.)
Expected stdout: `distributed-autocomplete serving_node (phase 0)`.

- [ ] **Step 4: Commit**

```bash
git add bin/serving_node
git commit -m "feat(bin): serving_node hello-world entry point"
```

---

## Task 0.7: CI — GitHub Actions with ASan + UBSan

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Write `.github/workflows/ci.yml`**

```yaml
name: ci
on:
  push:
    branches: [main]
  pull_request:

jobs:
  build-test:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Install clang-18 + tools
        run: |
          sudo apt-get update
          sudo apt-get install -y clang-18 lld-18 ninja-build python3-pip
          pip install --user conan==2.*
      - name: Conan detect + install
        run: |
          export CC=clang-18
          export CXX=clang++-18
          ~/.local/bin/conan profile detect --force
          ~/.local/bin/conan install . --build=missing -s compiler.cppstd=20
      - name: Configure
        run: |
          cmake -S . -B build -G Ninja \
            -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
            -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake \
            -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build
      - name: Test
        run: ctest --test-dir build --output-on-failure
```

- [ ] **Step 2: Commit + push**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: clang-18 build+test with ASan/UBSan"
```

*(Remote setup + push is a manual user step outside this plan — the workflow will exercise itself once pushed.)*

**Phase 0 exit check:** `cmake --build build && ctest --test-dir build` builds everything and runs all tests green locally; `serving_node` prints its banner. ✅

---

# PHASE 1 — Single-Tenant In-Memory Index

**Architectural note for this phase:** The spec (§4.1) mandates a double-array trie (DAT) as v1 bootstrap. This plan deliberately starts with a **pointer-based `SimpleTrie`** behind the `FstIndex` interface to reach the Phase 1 exit criterion (stdin → top-K over a static corpus) in the fewest tasks. The DAT replacement is scoped as Task 1.11 at the end of this phase **only if the Phase 2 bench gate (< 1 ms median) is missed by `SimpleTrie`**. Otherwise it defers to the Phase 6 FST swap. Why this deviation: Phase 1 exit is correctness over a 100K-entry static corpus, not perf; a pointer trie is ~50 LOC, DAT is ~500 LOC plus a build step. Front-loading DAT burns a week on work whose real perf pressure doesn't show up until Phase 2's benchmark.

## Task 1.1: `libs/memory/StringArena` (TDD)

**Files:**
- Create: `libs/memory/CMakeLists.txt`
- Create: `libs/memory/include/autocomplete/memory/StringArena.h`
- Create: `tests/unit/memory/string_arena_test.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write `libs/memory/CMakeLists.txt`**

```cmake
add_library(autocomplete_memory INTERFACE)
target_include_directories(autocomplete_memory INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(autocomplete_memory INTERFACE autocomplete::common)
add_library(autocomplete::memory ALIAS autocomplete_memory)
```

- [ ] **Step 2: Write the failing test** `tests/unit/memory/string_arena_test.cpp`

```cpp
#include <gtest/gtest.h>
#include <string_view>
#include "autocomplete/memory/StringArena.h"

using autocomplete::memory::StringArena;

TEST(StringArenaTest, AppendReturnsHandleRoundTrips) {
  StringArena arena;
  auto h1 = arena.append("hello");
  auto h2 = arena.append("world");

  EXPECT_EQ(arena.view(h1), "hello");
  EXPECT_EQ(arena.view(h2), "world");
  EXPECT_NE(h1.offset, h2.offset);
}

TEST(StringArenaTest, OffsetsAreStableAcrossGrowth) {
  StringArena arena;
  auto h = arena.append("stable");
  for (int i = 0; i < 10000; ++i) arena.append("padpadpadpad");
  EXPECT_EQ(arena.view(h), "stable");
}

TEST(StringArenaTest, HandleLengthFitsInPhraseLengthField) {
  StringArena arena;
  auto h = arena.append(std::string(40000, 'x'));
  EXPECT_EQ(h.length, 40000u);
  EXPECT_EQ(arena.view(h).size(), 40000u);
}
```

- [ ] **Step 3: Register test** — append to `tests/unit/CMakeLists.txt`:

```cmake
autocomplete_add_test(memory_string_arena_test memory/string_arena_test.cpp)
target_link_libraries(memory_string_arena_test PRIVATE autocomplete::memory)
```

- [ ] **Step 4: Run — expect FAIL (header missing)**

Run: `cmake --build build --target memory_string_arena_test`
Expected: header-not-found.

- [ ] **Step 5: Implement `StringArena.h`**

```cpp
#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

namespace autocomplete::memory {

struct StringHandle {
  std::uint32_t offset;
  std::uint16_t length;
};

class StringArena {
 public:
  StringHandle append(std::string_view s) {
    const auto off = static_cast<std::uint32_t>(buf_.size());
    buf_.insert(buf_.end(), s.begin(), s.end());
    return StringHandle{off, static_cast<std::uint16_t>(s.size())};
  }

  std::string_view view(StringHandle h) const noexcept {
    return std::string_view{buf_.data() + h.offset, h.length};
  }

  std::size_t bytes() const noexcept { return buf_.size(); }

 private:
  std::vector<char> buf_;
};

}  // namespace autocomplete::memory
```

- [ ] **Step 6: Run — expect PASS**

Run: `cmake --build build --target memory_string_arena_test && ctest --test-dir build -R memory_string_arena_test --output-on-failure`
Expected: 3 tests pass.

- [ ] **Step 7: Commit**

```bash
git add libs/memory tests/unit/memory tests/unit/CMakeLists.txt
git commit -m "feat(memory): StringArena with stable offset handles"
```

---

## Task 1.2: `libs/index/Entry` + `EntryTable` (TDD)

**Files:**
- Create: `libs/index/CMakeLists.txt`
- Create: `libs/index/include/autocomplete/index/Entry.h`
- Create: `libs/index/include/autocomplete/index/EntryTable.h`
- Create: `tests/unit/index/entry_table_test.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write `libs/index/CMakeLists.txt`** (library target skeleton; source files added later tasks)

```cmake
add_library(autocomplete_index
  src/SimpleTrie.cpp    # created in Task 1.4
)
target_include_directories(autocomplete_index PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(autocomplete_index PUBLIC
  autocomplete::common
  autocomplete::memory)
autocomplete_set_warnings(autocomplete_index)
autocomplete_apply_sanitizers(autocomplete_index)
add_library(autocomplete::index ALIAS autocomplete_index)
```

*(The file `src/SimpleTrie.cpp` will be created in Task 1.4. To let this task build now, create an empty placeholder: `mkdir -p libs/index/src && printf '// filled in Task 1.4\n' > libs/index/src/SimpleTrie.cpp`.)*

- [ ] **Step 2: Write the failing test** `tests/unit/index/entry_table_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "autocomplete/index/EntryTable.h"
#include "autocomplete/memory/StringArena.h"

using autocomplete::index::Entry;
using autocomplete::index::EntryTable;
using autocomplete::index::SourceFlag;
using autocomplete::memory::StringArena;

TEST(EntryTableTest, AddAndLookupByIdReturnsSameEntry) {
  StringArena arena;
  EntryTable t;
  auto ph = arena.append("revenue");

  auto id = t.add(Entry{ph.offset, ph.length, 3.5f, /*last_seen*/ 100,
                        SourceFlag::kColumn, {}});

  const Entry& e = t.at(id);
  EXPECT_EQ(e.weight, 3.5f);
  EXPECT_EQ(arena.view({e.phrase_offset, e.phrase_length}), "revenue");
  EXPECT_EQ(e.source_flag, SourceFlag::kColumn);
}

TEST(EntryTableTest, IdsAreMonotonic) {
  EntryTable t;
  auto a = t.add(Entry{});
  auto b = t.add(Entry{});
  EXPECT_LT(a, b);
  EXPECT_EQ(t.size(), 2u);
}

TEST(EntryTableTest, EntryIsOneCacheLine) {
  static_assert(sizeof(Entry) == 16, "Entry must stay cache-line friendly");
  SUCCEED();
}
```

- [ ] **Step 3: Register test**

Append to `tests/unit/CMakeLists.txt`:
```cmake
autocomplete_add_test(index_entry_table_test index/entry_table_test.cpp)
target_link_libraries(index_entry_table_test PRIVATE autocomplete::index)
```

- [ ] **Step 4: Run — expect FAIL**

Expected: headers not found.

- [ ] **Step 5: Implement `Entry.h`**

```cpp
#pragma once
#include <cstdint>

namespace autocomplete::index {

enum class SourceFlag : std::uint8_t {
  kColumn     = 0,
  kPastQuery  = 1,
  kDataValue  = 2,
};

struct Entry {
  std::uint32_t phrase_offset;   // into StringArena
  std::uint16_t phrase_length;
  float         weight;
  std::uint32_t last_seen_epoch;
  SourceFlag    source_flag;
  std::uint8_t  _pad[3];
};

static_assert(sizeof(Entry) == 16, "Entry layout drift");

using EntryId = std::uint32_t;

}  // namespace autocomplete::index
```

- [ ] **Step 6: Implement `EntryTable.h`**

```cpp
#pragma once
#include <vector>
#include "autocomplete/index/Entry.h"

namespace autocomplete::index {

class EntryTable {
 public:
  EntryId add(Entry e) {
    entries_.push_back(e);
    return static_cast<EntryId>(entries_.size() - 1);
  }

  const Entry& at(EntryId id) const { return entries_[id]; }
  std::size_t  size()        const noexcept { return entries_.size(); }

 private:
  std::vector<Entry> entries_;
};

}  // namespace autocomplete::index
```

- [ ] **Step 7: Run — expect PASS**

Run: `cmake --build build --target index_entry_table_test && ctest --test-dir build -R index_entry_table_test --output-on-failure`
Expected: 3 tests pass. If `sizeof(Entry) != 16` fails, fix padding — do not change the test.

- [ ] **Step 8: Commit**

```bash
git add libs/index tests/unit/index/entry_table_test.cpp tests/unit/CMakeLists.txt
git commit -m "feat(index): Entry layout + EntryTable"
```

---

## Task 1.3: `FstIndex` interface

**Files:**
- Create: `libs/index/include/autocomplete/index/FstIndex.h`

**Why a virtual interface for a perf-critical path:** v-table cost is one indirect call *per query* (not per trie node), amortized against ms-scale work. The flexibility to swap `SimpleTrie` → DAT → real FST across phases without touching `QueryExecutor` is worth 10ns.

- [ ] **Step 1: Write `FstIndex.h`**

```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <string_view>
#include "autocomplete/index/Entry.h"

namespace autocomplete::index {

// Visitor receives entry IDs associated with tokens whose text starts with the
// queried prefix. Returning false stops the walk early.
using PrefixVisitor = std::function<bool(EntryId)>;

class FstIndex {
 public:
  virtual ~FstIndex() = default;

  // Register a token → entry_id edge. One token may map to multiple entry_ids.
  virtual void insert(std::string_view token, EntryId id) = 0;

  // Invoke `visit` for every entry_id whose token has `prefix` as a prefix.
  virtual void walk_prefix(std::string_view prefix, const PrefixVisitor& visit) const = 0;

  virtual std::size_t token_count() const noexcept = 0;
};

}  // namespace autocomplete::index
```

- [ ] **Step 2: Verify header compiles** (no test yet — interface-only)

Run: `cmake --build build --target autocomplete_index`
Expected: builds (the placeholder `SimpleTrie.cpp` is still empty).

- [ ] **Step 3: Commit**

```bash
git add libs/index/include/autocomplete/index/FstIndex.h
git commit -m "feat(index): FstIndex interface (swappable backends)"
```

---

## Task 1.4: `SimpleTrie` — minimal `FstIndex` impl (TDD)

**Files:**
- Create: `libs/index/include/autocomplete/index/SimpleTrie.h`
- Modify: `libs/index/src/SimpleTrie.cpp`
- Create: `tests/unit/index/simple_trie_test.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing test** `tests/unit/index/simple_trie_test.cpp`

```cpp
#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
#include "autocomplete/index/SimpleTrie.h"

using autocomplete::index::EntryId;
using autocomplete::index::SimpleTrie;

namespace {
std::vector<EntryId> collect(const SimpleTrie& t, std::string_view prefix) {
  std::vector<EntryId> out;
  t.walk_prefix(prefix, [&](EntryId id) { out.push_back(id); return true; });
  std::sort(out.begin(), out.end());
  return out;
}
}  // namespace

TEST(SimpleTrieTest, ExactMatchReturnsEntry) {
  SimpleTrie t;
  t.insert("revenue", 7);
  EXPECT_EQ(collect(t, "revenue"), std::vector<EntryId>{7});
}

TEST(SimpleTrieTest, PrefixReturnsAllMatchingEntries) {
  SimpleTrie t;
  t.insert("revenue", 1);
  t.insert("revenues", 2);
  t.insert("revert", 3);
  t.insert("region", 4);

  EXPECT_EQ(collect(t, "reven"), (std::vector<EntryId>{1, 2}));
  EXPECT_EQ(collect(t, "re"),    (std::vector<EntryId>{1, 2, 3, 4}));
  EXPECT_EQ(collect(t, "zzz"),   std::vector<EntryId>{});
}

TEST(SimpleTrieTest, SameTokenMultipleEntryIds) {
  SimpleTrie t;
  t.insert("sales", 10);
  t.insert("sales", 11);
  EXPECT_EQ(collect(t, "sales"), (std::vector<EntryId>{10, 11}));
}

TEST(SimpleTrieTest, VisitorCanStopEarly) {
  SimpleTrie t;
  for (EntryId i = 0; i < 100; ++i) t.insert("a", i);

  int seen = 0;
  t.walk_prefix("a", [&](EntryId) { ++seen; return seen < 5; });
  EXPECT_EQ(seen, 5);
}

TEST(SimpleTrieTest, EmptyPrefixIsRejected) {
  SimpleTrie t;
  t.insert("x", 0);
  auto got = collect(t, "");
  EXPECT_TRUE(got.empty());  // empty prefix = no-op by contract
}
```

- [ ] **Step 2: Register test**

Append to `tests/unit/CMakeLists.txt`:
```cmake
autocomplete_add_test(index_simple_trie_test index/simple_trie_test.cpp)
target_link_libraries(index_simple_trie_test PRIVATE autocomplete::index)
```

- [ ] **Step 3: Run — expect FAIL**

Expected: header not found.

- [ ] **Step 4: Implement `SimpleTrie.h`**

```cpp
#pragma once
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "autocomplete/index/FstIndex.h"

namespace autocomplete::index {

class SimpleTrie final : public FstIndex {
 public:
  void insert(std::string_view token, EntryId id) override;
  void walk_prefix(std::string_view prefix, const PrefixVisitor& visit) const override;
  std::size_t token_count() const noexcept override { return token_count_; }

 private:
  struct Node {
    std::unordered_map<char, std::unique_ptr<Node>> kids;
    std::vector<EntryId> entries;
  };

  // Walk subtree rooted at `n`, visiting every entry. Returns false if
  // visitor signalled stop.
  static bool visit_subtree(const Node* n, const PrefixVisitor& visit);

  Node root_;
  std::size_t token_count_ = 0;
};

}  // namespace autocomplete::index
```

- [ ] **Step 5: Implement `libs/index/src/SimpleTrie.cpp`** (replace the placeholder)

```cpp
#include "autocomplete/index/SimpleTrie.h"

namespace autocomplete::index {

void SimpleTrie::insert(std::string_view token, EntryId id) {
  if (token.empty()) return;
  Node* cur = &root_;
  for (char c : token) {
    auto& slot = cur->kids[c];
    if (!slot) slot = std::make_unique<Node>();
    cur = slot.get();
  }
  cur->entries.push_back(id);
  ++token_count_;
}

bool SimpleTrie::visit_subtree(const Node* n, const PrefixVisitor& visit) {
  for (EntryId id : n->entries) {
    if (!visit(id)) return false;
  }
  for (const auto& [c, child] : n->kids) {
    if (!visit_subtree(child.get(), visit)) return false;
  }
  return true;
}

void SimpleTrie::walk_prefix(std::string_view prefix,
                             const PrefixVisitor& visit) const {
  if (prefix.empty()) return;
  const Node* cur = &root_;
  for (char c : prefix) {
    auto it = cur->kids.find(c);
    if (it == cur->kids.end()) return;
    cur = it->second.get();
  }
  visit_subtree(cur, visit);
}

}  // namespace autocomplete::index
```

- [ ] **Step 6: Run — expect PASS**

Run: `cmake --build build --target index_simple_trie_test && ctest --test-dir build -R index_simple_trie_test --output-on-failure`
Expected: 5 tests pass, no ASan reports.

- [ ] **Step 7: Commit**

```bash
git add libs/index/include/autocomplete/index/SimpleTrie.h libs/index/src/SimpleTrie.cpp tests/unit/index/simple_trie_test.cpp tests/unit/CMakeLists.txt
git commit -m "feat(index): SimpleTrie — pointer-trie FstIndex for v1"
```

---

## Task 1.5: `Tokenizer` (TDD)

**Files:**
- Create: `libs/query/CMakeLists.txt`
- Create: `libs/query/include/autocomplete/query/Tokenizer.h`
- Create: `libs/query/src/Tokenizer.cpp`
- Create: `tests/unit/query/tokenizer_test.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write `libs/query/CMakeLists.txt`**

```cmake
add_library(autocomplete_query
  src/Tokenizer.cpp
  src/QueryExecutor.cpp   # created in Task 1.8
)
target_include_directories(autocomplete_query PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(autocomplete_query PUBLIC
  autocomplete::common
  autocomplete::memory
  autocomplete::index)
autocomplete_set_warnings(autocomplete_query)
autocomplete_apply_sanitizers(autocomplete_query)
add_library(autocomplete::query ALIAS autocomplete_query)
```

*(Placeholder: `printf '// filled in Task 1.8\n' > libs/query/src/QueryExecutor.cpp` so CMake configures now.)*

- [ ] **Step 2: Write the failing test** `tests/unit/query/tokenizer_test.cpp`

```cpp
#include <gtest/gtest.h>
#include <vector>
#include "autocomplete/query/Tokenizer.h"

using autocomplete::query::Tokenizer;

namespace {
std::vector<std::string> as_strings(const std::vector<std::string_view>& v) {
  return {v.begin(), v.end()};
}
}  // namespace

TEST(TokenizerTest, SplitsOnWhitespace) {
  Tokenizer tok;
  std::string in = "total revenue q4";
  EXPECT_EQ(as_strings(tok.tokenize(in)),
            (std::vector<std::string>{"total", "revenue", "q4"}));
}

TEST(TokenizerTest, SplitsOnPunctuationAndLowercases) {
  Tokenizer tok;
  std::string in = "Sales/Region, 2025!";
  EXPECT_EQ(as_strings(tok.tokenize(in)),
            (std::vector<std::string>{"sales", "region", "2025"}));
}

TEST(TokenizerTest, EmptyInputProducesNoTokens) {
  Tokenizer tok;
  std::string in;
  EXPECT_TRUE(tok.tokenize(in).empty());
}

TEST(TokenizerTest, DropsEmptyRunsFromRepeatedSeparators) {
  Tokenizer tok;
  std::string in = "  a,, ,b ";
  EXPECT_EQ(as_strings(tok.tokenize(in)),
            (std::vector<std::string>{"a", "b"}));
}
```

Note: `tokenize` takes a reference to a caller-owned string so the returned `string_view`s remain valid. This is deliberate and documented on the API.

- [ ] **Step 3: Register test**

Append to `tests/unit/CMakeLists.txt`:
```cmake
autocomplete_add_test(query_tokenizer_test query/tokenizer_test.cpp)
target_link_libraries(query_tokenizer_test PRIVATE autocomplete::query)
```

- [ ] **Step 4: Run — expect FAIL**

Expected: header-not-found.

- [ ] **Step 5: Implement `Tokenizer.h`**

```cpp
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace autocomplete::query {

class Tokenizer {
 public:
  // Lowercases `buf` in place, then returns string_views pointing into it.
  // Caller must keep `buf` alive for the lifetime of the returned views.
  std::vector<std::string_view> tokenize(std::string& buf) const;

 private:
  static bool is_separator(char c) noexcept;
};

}  // namespace autocomplete::query
```

- [ ] **Step 6: Implement `libs/query/src/Tokenizer.cpp`**

```cpp
#include "autocomplete/query/Tokenizer.h"
#include <cctype>

namespace autocomplete::query {

bool Tokenizer::is_separator(char c) noexcept {
  return std::isspace(static_cast<unsigned char>(c)) ||
         std::ispunct(static_cast<unsigned char>(c));
}

std::vector<std::string_view> Tokenizer::tokenize(std::string& buf) const {
  for (char& c : buf) c = static_cast<char>(
      std::tolower(static_cast<unsigned char>(c)));

  std::vector<std::string_view> out;
  std::size_t i = 0, n = buf.size();
  while (i < n) {
    while (i < n && is_separator(buf[i])) ++i;
    std::size_t start = i;
    while (i < n && !is_separator(buf[i])) ++i;
    if (i > start) out.emplace_back(buf.data() + start, i - start);
  }
  return out;
}

}  // namespace autocomplete::query
```

- [ ] **Step 7: Run — expect PASS**

Run: `cmake --build build --target query_tokenizer_test && ctest --test-dir build -R query_tokenizer_test --output-on-failure`
Expected: 4 tests pass.

- [ ] **Step 8: Commit**

```bash
git add libs/query tests/unit/query/tokenizer_test.cpp tests/unit/CMakeLists.txt
git commit -m "feat(query): Tokenizer (lowercase ASCII, punct split)"
```

---

## Task 1.6: `FixedTopK` min-heap (TDD)

**Files:**
- Create: `libs/query/include/autocomplete/query/FixedTopK.h`
- Create: `tests/unit/query/fixed_topk_test.cpp`
- Modify: `tests/unit/CMakeLists.txt`

**Why a min-heap for top-K:** you keep the current K best; the heap's root is the weakest kept, so you compare each new candidate against the root in O(1) and only pay O(log K) when it wins. Net cost: O(N log K) vs. O(N log N) from a full sort.

- [ ] **Step 1: Write the failing test** `tests/unit/query/fixed_topk_test.cpp`

```cpp
#include <gtest/gtest.h>
#include <cstdint>
#include "autocomplete/query/FixedTopK.h"

using autocomplete::query::FixedTopK;

struct Scored {
  std::uint32_t id;
  float         score;
};

struct ByScore {
  bool operator()(const Scored& a, const Scored& b) const {
    return a.score < b.score;  // min-heap of Scored → a < b ⇒ "a is smaller"
  }
};

TEST(FixedTopKTest, KeepsTopKByScoreDescending) {
  FixedTopK<Scored, ByScore> top{3};
  top.offer({1, 1.0f});
  top.offer({2, 5.0f});
  top.offer({3, 3.0f});
  top.offer({4, 2.0f});  // dropped (1.0 is already out)
  top.offer({5, 4.0f});

  auto sorted = top.take_sorted();
  ASSERT_EQ(sorted.size(), 3u);
  EXPECT_EQ(sorted[0].id, 2u);  // 5.0
  EXPECT_EQ(sorted[1].id, 5u);  // 4.0
  EXPECT_EQ(sorted[2].id, 3u);  // 3.0
}

TEST(FixedTopKTest, UnderCapacityReturnsAll) {
  FixedTopK<Scored, ByScore> top{5};
  top.offer({1, 1.0f});
  top.offer({2, 3.0f});
  auto s = top.take_sorted();
  ASSERT_EQ(s.size(), 2u);
  EXPECT_EQ(s[0].id, 2u);
}

TEST(FixedTopKTest, ZeroCapacityStoresNothing) {
  FixedTopK<Scored, ByScore> top{0};
  top.offer({1, 99.0f});
  EXPECT_TRUE(top.take_sorted().empty());
}
```

- [ ] **Step 2: Register test**

Append:
```cmake
autocomplete_add_test(query_fixed_topk_test query/fixed_topk_test.cpp)
target_link_libraries(query_fixed_topk_test PRIVATE autocomplete::query)
```

- [ ] **Step 3: Run — expect FAIL**

Expected: header-not-found.

- [ ] **Step 4: Implement `FixedTopK.h`**

```cpp
#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace autocomplete::query {

// Maintains top-K of T by a strict weak ordering `Less`.
// "Top" = largest under `Less`. Internally a min-heap sized ≤ K.
template <typename T, typename Less>
class FixedTopK {
 public:
  explicit FixedTopK(std::size_t k, Less less = Less{})
      : k_{k}, less_{std::move(less)} {
    heap_.reserve(k);
  }

  void offer(T value) {
    if (k_ == 0) return;
    if (heap_.size() < k_) {
      heap_.push_back(std::move(value));
      std::push_heap(heap_.begin(), heap_.end(), inv_less());
    } else if (less_(heap_.front(), value)) {
      std::pop_heap(heap_.begin(), heap_.end(), inv_less());
      heap_.back() = std::move(value);
      std::push_heap(heap_.begin(), heap_.end(), inv_less());
    }
  }

  // Destructive: returns elements sorted descending under `Less`.
  std::vector<T> take_sorted() {
    std::sort(heap_.begin(), heap_.end(),
              [&](const T& a, const T& b) { return less_(b, a); });
    return std::move(heap_);
  }

 private:
  // std::*_heap builds a max-heap under the supplied compare; we want a
  // min-heap, so invert.
  auto inv_less() const {
    return [this](const T& a, const T& b) { return less_(b, a); };
  }

  std::size_t     k_;
  Less            less_;
  std::vector<T>  heap_;
};

}  // namespace autocomplete::query
```

- [ ] **Step 5: Run — expect PASS**

Run: `cmake --build build --target query_fixed_topk_test && ctest --test-dir build -R query_fixed_topk_test --output-on-failure`
Expected: 3 tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/query/include/autocomplete/query/FixedTopK.h tests/unit/query/fixed_topk_test.cpp tests/unit/CMakeLists.txt
git commit -m "feat(query): FixedTopK bounded min-heap"
```

---

## Task 1.7: `IndexView` (TDD)

**Files:**
- Create: `libs/index/include/autocomplete/index/IndexView.h`
- Create: `tests/unit/index/index_view_test.cpp`
- Modify: `tests/unit/CMakeLists.txt`

**Note on RCU / `atomic<shared_ptr>`:** spec §4.4 prescribes `std::atomic<std::shared_ptr<IndexView>>` for version swap. Phase 1 has no concurrent writers (single-threaded stdin REPL), so this task builds the **value type only**. The atomic holder lands in Phase 4 with the artifact-swap work. Why defer: atomic shared_ptr correctness testing needs concurrent-writer tests that only make sense once artifact swap exists; building it now means testing a feature with no real caller.

- [ ] **Step 1: Write the failing test** `tests/unit/index/index_view_test.cpp`

```cpp
#include <gtest/gtest.h>
#include <memory>
#include "autocomplete/index/IndexView.h"
#include "autocomplete/index/SimpleTrie.h"
#include "autocomplete/memory/StringArena.h"

using autocomplete::index::Entry;
using autocomplete::index::IndexView;
using autocomplete::index::SimpleTrie;
using autocomplete::index::SourceFlag;
using autocomplete::memory::StringArena;

TEST(IndexViewTest, BundlesAllThreeLayersAndVersion) {
  auto arena = std::make_shared<StringArena>();
  auto table = std::make_shared<autocomplete::index::EntryTable>();
  auto trie  = std::make_shared<SimpleTrie>();

  auto h  = arena->append("total");
  auto id = table->add(Entry{h.offset, h.length, 1.0f, 0,
                             SourceFlag::kColumn, {}});
  trie->insert("total", id);

  IndexView view{trie, table, arena, /*version=*/42};

  EXPECT_EQ(view.version(), 42u);
  EXPECT_EQ(view.entries().size(), 1u);
  EXPECT_EQ(view.arena().view({h.offset, h.length}), "total");
  EXPECT_EQ(view.index().token_count(), 1u);
}
```

- [ ] **Step 2: Register test**

Append:
```cmake
autocomplete_add_test(index_view_test index/index_view_test.cpp)
target_link_libraries(index_view_test PRIVATE autocomplete::index)
```

- [ ] **Step 3: Run — expect FAIL**

Expected: header-not-found.

- [ ] **Step 4: Implement `IndexView.h`**

```cpp
#pragma once
#include <cstdint>
#include <memory>
#include "autocomplete/index/EntryTable.h"
#include "autocomplete/index/FstIndex.h"
#include "autocomplete/memory/StringArena.h"

namespace autocomplete::index {

class IndexView {
 public:
  IndexView(std::shared_ptr<FstIndex>                  idx,
            std::shared_ptr<EntryTable>                tbl,
            std::shared_ptr<memory::StringArena>       arena,
            std::uint64_t                              version)
      : idx_{std::move(idx)},
        tbl_{std::move(tbl)},
        arena_{std::move(arena)},
        version_{version} {}

  const FstIndex&             index()   const noexcept { return *idx_; }
  const EntryTable&           entries() const noexcept { return *tbl_; }
  const memory::StringArena&  arena()   const noexcept { return *arena_; }
  std::uint64_t               version() const noexcept { return version_; }

 private:
  std::shared_ptr<FstIndex>            idx_;
  std::shared_ptr<EntryTable>          tbl_;
  std::shared_ptr<memory::StringArena> arena_;
  std::uint64_t                        version_;
};

}  // namespace autocomplete::index
```

- [ ] **Step 5: Run — expect PASS**

Run: `cmake --build build --target index_view_test && ctest --test-dir build -R index_view_test --output-on-failure`
Expected: test passes.

- [ ] **Step 6: Commit**

```bash
git add libs/index/include/autocomplete/index/IndexView.h tests/unit/index/index_view_test.cpp tests/unit/CMakeLists.txt
git commit -m "feat(index): IndexView bundles index + table + arena + version"
```

---

## Task 1.8: `QueryExecutor` — tokenize → walk → top-K (TDD)

**Files:**
- Create: `libs/query/include/autocomplete/query/QueryExecutor.h`
- Modify: `libs/query/src/QueryExecutor.cpp`
- Create: `tests/unit/query/query_executor_test.cpp`
- Modify: `tests/unit/CMakeLists.txt`

- [ ] **Step 1: Write the failing test** `tests/unit/query/query_executor_test.cpp`

```cpp
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "autocomplete/index/IndexView.h"
#include "autocomplete/index/SimpleTrie.h"
#include "autocomplete/memory/StringArena.h"
#include "autocomplete/query/QueryExecutor.h"

using namespace autocomplete;
using autocomplete::index::Entry;
using autocomplete::index::SourceFlag;

namespace {
std::shared_ptr<index::IndexView> build_fixture() {
  auto arena = std::make_shared<memory::StringArena>();
  auto table = std::make_shared<index::EntryTable>();
  auto trie  = std::make_shared<index::SimpleTrie>();

  auto add = [&](std::string_view phrase, float w) {
    auto h  = arena->append(phrase);
    auto id = table->add(Entry{h.offset, h.length, w, 0,
                               SourceFlag::kColumn, {}});
    // Single-token phrases in this fixture; multi-token tokenization
    // is exercised in a separate test.
    trie->insert(phrase, id);
  };

  add("revenue",   3.0f);
  add("revenues",  2.0f);
  add("revert",    1.0f);
  add("region",    5.0f);
  add("reactor",   0.5f);
  return std::make_shared<index::IndexView>(trie, table, arena, 1);
}
}  // namespace

TEST(QueryExecutorTest, TopKOrderedByWeightDesc) {
  auto view = build_fixture();
  query::QueryExecutor qe;

  std::string prefix = "re";
  auto res = qe.execute(*view, prefix, /*k=*/3);

  ASSERT_EQ(res.size(), 3u);
  EXPECT_EQ(res[0].phrase, "region");
  EXPECT_EQ(res[1].phrase, "revenue");
  EXPECT_EQ(res[2].phrase, "revenues");
}

TEST(QueryExecutorTest, ZeroResultsOnUnknownPrefix) {
  auto view = build_fixture();
  query::QueryExecutor qe;
  std::string prefix = "xyz";
  EXPECT_TRUE(qe.execute(*view, prefix, 10).empty());
}

TEST(QueryExecutorTest, MultiTokenPrefixUsesLastToken) {
  // Phase 1: multi-token intersect is deferred; we match on the final
  // token's prefix and ignore prior tokens. This test pins that contract.
  auto view = build_fixture();
  query::QueryExecutor qe;
  std::string prefix = "q4 reg";
  auto res = qe.execute(*view, prefix, 5);
  ASSERT_EQ(res.size(), 1u);
  EXPECT_EQ(res[0].phrase, "region");
}

TEST(QueryExecutorTest, CapacityBoundedByK) {
  auto view = build_fixture();
  query::QueryExecutor qe;
  std::string prefix = "re";
  auto res = qe.execute(*view, prefix, 2);
  EXPECT_EQ(res.size(), 2u);
}
```

- [ ] **Step 2: Register test**

Append:
```cmake
autocomplete_add_test(query_executor_test query/query_executor_test.cpp)
target_link_libraries(query_executor_test PRIVATE autocomplete::query)
```

- [ ] **Step 3: Run — expect FAIL**

Expected: header-not-found.

- [ ] **Step 4: Implement `QueryExecutor.h`**

```cpp
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "autocomplete/index/IndexView.h"

namespace autocomplete::query {

struct Suggestion {
  std::string_view  phrase;  // view into IndexView::arena; valid while view lives
  float             weight;
  index::SourceFlag source;
};

class QueryExecutor {
 public:
  // Tokenizes `prefix` (caller owns, modified to lowercase) and returns
  // top-K suggestions by weight descending. Phase 1 semantics: matches on
  // the last token's prefix; leading tokens are ignored.
  std::vector<Suggestion> execute(const index::IndexView& view,
                                  std::string&           prefix,
                                  std::size_t            k) const;
};

}  // namespace autocomplete::query
```

- [ ] **Step 5: Implement `libs/query/src/QueryExecutor.cpp`**

```cpp
#include "autocomplete/query/QueryExecutor.h"
#include "autocomplete/query/FixedTopK.h"
#include "autocomplete/query/Tokenizer.h"

namespace autocomplete::query {

namespace {
struct Scored {
  index::EntryId id;
  float          weight;
};
struct ByWeight {
  bool operator()(const Scored& a, const Scored& b) const {
    return a.weight < b.weight;
  }
};
}  // namespace

std::vector<Suggestion> QueryExecutor::execute(const index::IndexView& view,
                                               std::string&            prefix,
                                               std::size_t             k) const {
  Tokenizer tok;
  auto tokens = tok.tokenize(prefix);
  if (tokens.empty() || k == 0) return {};

  const std::string_view last = tokens.back();
  FixedTopK<Scored, ByWeight> heap{k};

  view.index().walk_prefix(last, [&](index::EntryId id) {
    const auto& e = view.entries().at(id);
    heap.offer({id, e.weight});
    return true;  // no early stop in Phase 1; revisited in Phase 2
  });

  auto sorted = heap.take_sorted();
  std::vector<Suggestion> out;
  out.reserve(sorted.size());
  for (const auto& s : sorted) {
    const auto& e = view.entries().at(s.id);
    out.push_back(Suggestion{
        view.arena().view({e.phrase_offset, e.phrase_length}),
        e.weight, e.source_flag});
  }
  return out;
}

}  // namespace autocomplete::query
```

- [ ] **Step 6: Run — expect PASS**

Run: `cmake --build build --target query_executor_test && ctest --test-dir build -R query_executor_test --output-on-failure`
Expected: 4 tests pass.

- [ ] **Step 7: Commit**

```bash
git add libs/query/include/autocomplete/query/QueryExecutor.h libs/query/src/QueryExecutor.cpp tests/unit/query/query_executor_test.cpp tests/unit/CMakeLists.txt
git commit -m "feat(query): QueryExecutor — prefix walk + top-K by weight"
```

---

## Task 1.9: `serving_node` — load CSV + stdin REPL

**Files:**
- Modify: `bin/serving_node/CMakeLists.txt`
- Modify: `bin/serving_node/src/main.cpp`
- Create: `scripts/gen_corpus.py`

- [ ] **Step 1: Update `bin/serving_node/CMakeLists.txt`**

```cmake
add_executable(serving_node src/main.cpp)
target_link_libraries(serving_node PRIVATE
  autocomplete::common
  autocomplete::memory
  autocomplete::index
  autocomplete::query)
autocomplete_set_warnings(serving_node)
autocomplete_apply_sanitizers(serving_node)
```

- [ ] **Step 2: Rewrite `bin/serving_node/src/main.cpp`**

```cpp
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "autocomplete/index/EntryTable.h"
#include "autocomplete/index/IndexView.h"
#include "autocomplete/index/SimpleTrie.h"
#include "autocomplete/memory/StringArena.h"
#include "autocomplete/query/QueryExecutor.h"
#include "autocomplete/query/Tokenizer.h"

using namespace autocomplete;

namespace {

std::shared_ptr<index::IndexView> load_corpus(const std::string& path) {
  auto arena = std::make_shared<memory::StringArena>();
  auto table = std::make_shared<index::EntryTable>();
  auto trie  = std::make_shared<index::SimpleTrie>();
  query::Tokenizer tok;

  std::ifstream f(path);
  if (!f) {
    std::fprintf(stderr, "cannot open corpus: %s\n", path.c_str());
    std::exit(2);
  }

  std::string line;
  std::size_t rows = 0;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    auto comma = line.find_last_of(',');
    if (comma == std::string::npos) continue;
    std::string phrase = line.substr(0, comma);
    float weight = std::stof(line.substr(comma + 1));

    auto h  = arena->append(phrase);
    auto id = table->add(index::Entry{
        h.offset, h.length, weight, /*last_seen=*/0,
        index::SourceFlag::kColumn, {}});

    // Tokenize a lowercased copy so the trie sees normalized tokens.
    std::string norm = phrase;
    auto tokens = tok.tokenize(norm);
    for (auto t : tokens) trie->insert(t, id);
    ++rows;
  }
  std::fprintf(stderr, "loaded %zu phrases, %zu tokens, arena=%zu bytes\n",
               rows, trie->token_count(), arena->bytes());
  return std::make_shared<index::IndexView>(trie, table, arena, 1);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: serving_node <corpus.csv>\n");
    return 1;
  }

  auto view = load_corpus(argv[1]);
  query::QueryExecutor qe;

  std::string line;
  std::fprintf(stderr, "ready. type a prefix, ^D to quit.\n> ");
  while (std::getline(std::cin, line)) {
    auto results = qe.execute(*view, line, /*k=*/10);
    for (const auto& s : results) {
      std::cout << s.phrase << "\t" << s.weight << "\n";
    }
    std::cout.flush();
    std::fprintf(stderr, "> ");
  }
  return 0;
}
```

- [ ] **Step 3: Write `scripts/gen_corpus.py`**

```python
#!/usr/bin/env python3
"""Emit a CSV corpus of `phrase,weight` lines for serving_node manual testing."""
import argparse, random, string, sys

def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("-n", type=int, default=1000)
    p.add_argument("--seed", type=int, default=0)
    args = p.parse_args()

    rng = random.Random(args.seed)
    vocab = ["revenue", "region", "sales", "customer", "order", "product",
             "quantity", "total", "quarter", "year", "margin", "forecast"]
    for _ in range(args.n):
        k = rng.randint(1, 3)
        phrase = " ".join(rng.choice(vocab) for _ in range(k))
        w = round(rng.random() * 10, 3)
        print(f"{phrase},{w}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Generate a corpus and manually exercise**

Run:
```bash
python3 scripts/gen_corpus.py -n 1000 > /tmp/corpus.csv
./build/Release/bin/serving_node/serving_node /tmp/corpus.csv
```
At the prompt, type `rev` and press Enter. Expected: up to 10 `phrase\tweight` lines, ordered by weight descending, all containing a token with prefix `rev`. Ctrl-D to quit.

**If this doesn't behave like that, stop and debug — do not move on.**

- [ ] **Step 5: Commit**

```bash
git add bin/serving_node scripts/gen_corpus.py
git commit -m "feat(bin): serving_node loads CSV corpus, stdin REPL prefix search"
```

---

## Task 1.10: Integration test — `serving_node` end-to-end

**Files:**
- Create: `tests/integration/CMakeLists.txt`
- Create: `tests/integration/serving_node_e2e_test.cpp`
- Create: `tests/integration/fixtures/small_corpus.csv`
- Modify: `CMakeLists.txt` (add `add_subdirectory(tests/integration)`)

**Why an in-process integration test vs. spawning the binary:** the binary's business logic lives in `load_corpus` + `QueryExecutor`. We get 95% of the confidence by calling them directly and 5% from actually execing the binary — but the exec path is flaky in CI (stdin pipes, path resolution) and doesn't catch anything the unit tests miss. Phase 4+ will add a process-level e2e once there's networking to test through.

- [ ] **Step 1: Write `tests/integration/fixtures/small_corpus.csv`**

```
revenue,3.0
revenues,2.0
revert,1.0
region,5.0
reactor,0.5
```

- [ ] **Step 2: Write `tests/integration/CMakeLists.txt`**

```cmake
find_package(GTest REQUIRED)

add_executable(serving_node_e2e_test serving_node_e2e_test.cpp)
target_link_libraries(serving_node_e2e_test PRIVATE
  GTest::gtest_main
  autocomplete::index
  autocomplete::memory
  autocomplete::query)
target_compile_definitions(serving_node_e2e_test PRIVATE
  FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
autocomplete_set_warnings(serving_node_e2e_test)
autocomplete_apply_sanitizers(serving_node_e2e_test)
add_test(NAME serving_node_e2e_test COMMAND serving_node_e2e_test)
```

- [ ] **Step 3: Write `tests/integration/serving_node_e2e_test.cpp`**

```cpp
#include <fstream>
#include <memory>
#include <string>
#include <gtest/gtest.h>
#include "autocomplete/index/EntryTable.h"
#include "autocomplete/index/IndexView.h"
#include "autocomplete/index/SimpleTrie.h"
#include "autocomplete/memory/StringArena.h"
#include "autocomplete/query/QueryExecutor.h"
#include "autocomplete/query/Tokenizer.h"

using namespace autocomplete;

namespace {
std::shared_ptr<index::IndexView> load(const std::string& path) {
  auto arena = std::make_shared<memory::StringArena>();
  auto table = std::make_shared<index::EntryTable>();
  auto trie  = std::make_shared<index::SimpleTrie>();
  query::Tokenizer tok;

  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    auto comma = line.find_last_of(',');
    auto phrase = line.substr(0, comma);
    float w = std::stof(line.substr(comma + 1));
    auto h  = arena->append(phrase);
    auto id = table->add(index::Entry{h.offset, h.length, w, 0,
                                      index::SourceFlag::kColumn, {}});
    std::string norm = phrase;
    for (auto t : tok.tokenize(norm)) trie->insert(t, id);
  }
  return std::make_shared<index::IndexView>(trie, table, arena, 1);
}
}  // namespace

TEST(ServingNodeE2E, SmallCorpusTopThreeForRe) {
  auto view = load(std::string(FIXTURE_DIR) + "/small_corpus.csv");
  query::QueryExecutor qe;

  std::string q = "re";
  auto r = qe.execute(*view, q, 3);
  ASSERT_EQ(r.size(), 3u);
  EXPECT_EQ(r[0].phrase, "region");
  EXPECT_EQ(r[1].phrase, "revenue");
  EXPECT_EQ(r[2].phrase, "revenues");
}
```

- [ ] **Step 4: Wire into top-level** `CMakeLists.txt` — append:

```cmake
add_subdirectory(tests/integration)
```

- [ ] **Step 5: Run — expect PASS**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all tests green, including `serving_node_e2e_test`.

- [ ] **Step 6: Commit**

```bash
git add tests/integration CMakeLists.txt
git commit -m "test(integration): e2e corpus load + top-K query"
```

---

## Task 1.11: Seed a README manual-run section

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write `README.md`**

```markdown
# distributed-autocomplete

C++20 distributed in-memory token-prefix autocomplete.
See `docs/superpowers/specs/` for the design, `docs/superpowers/plans/` for
active implementation plans.

## Build (local)

    conan install . --build=missing -s compiler.cppstd=20
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake
    cmake --build build
    ctest --test-dir build --output-on-failure

## Try the Phase-1 REPL

    python3 scripts/gen_corpus.py -n 1000 > /tmp/corpus.csv
    ./build/Release/bin/serving_node/serving_node /tmp/corpus.csv
    > rev
    revenue    3.0
    ...
```

- [ ] **Step 2: Format with prettier** (user's global instruction)

Run: `npx prettier --write README.md`
Expected: minor reformatting or no-op.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: README with build + Phase-1 REPL quickstart"
```

---

## Phase 1 exit check

- [ ] `cmake --build build && ctest --test-dir build --output-on-failure` — all tests green, no ASan/UBSan reports.
- [ ] `serving_node /tmp/corpus.csv` serves prefix queries from stdin.
- [ ] Integration test covers end-to-end load → query path.
- [ ] Every `libs/*` target enforces warnings-as-errors + sanitizers in debug/test builds.

**Not in this plan, carried forward:**
- Double-array trie swap (revisit after Phase 2 bench; may not be needed until Phase 6 FST work).
- Thread-local bump arena (§5.2) — Phase 2, alongside the reactor thread model.
- Mutable overlay (§4.3) — Phase 5 (events) or Phase 4 (artifact swap), whichever lands first.
- Deadline enforcement (§5.4) — Phase 2 with the full pipeline.
- Multi-token intersect (§5.3 step 4) — Phase 2.
- Binary wire format (§5.1 FlatBuffers) — Phase 3 with networking.
