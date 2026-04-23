# distributed-autocomplete

A distributed in-memory datastore serving token-prefix autocomplete over
per-tenant corpora (100K–1M entries each) for a multi-tenant BI analytics
platform. Read-only query path. Index content is derived from tenant schema
seeds plus an accumulating history of past BI queries.

See `docs/superpowers/specs/` for the design, `docs/superpowers/plans/` for
active implementation plans.

## Build (local)

    conan install . --build=missing -s compiler.cppstd=20
    cmake -S . -B build/Release -G "Unix Makefiles" \
      -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release
    cmake --build build/Release
    ctest --test-dir build/Release --output-on-failure

## Try the Phase-1 REPL

    python3 scripts/gen_corpus.py -n 1000 > /tmp/corpus.csv
    ./build/Release/bin/serving_node/serving_node /tmp/corpus.csv
    > rev
    revenue    9.822
    ...
