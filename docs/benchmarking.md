# Testing and benchmarking

[Project overview](../README.md) · [Documentation index](README.md)

## Unit tests

Tests and benchmarks are enabled by default. After building Release, run from the repository root:

```sh
ctest --test-dir build --build-config Release --output-on-failure
```

GoogleTest discovers the dictionary, board, compact trie, and solver tests. The [Linux workflow](../.github/workflows/linux_build.yml) and [Windows workflow](../.github/workflows/windows_build.yml) run Debug, Release, and RelWithDebInfo configurations.

## Running benchmarks

Run the benchmark from its own output directory so it can find the dictionaries. CMake copies `basic_english_words_2000.txt` and `boggle_words_138735.txt` there on build.

Windows (PowerShell):

```powershell
Set-Location build/bin/benchmark/Release
.\bm_BoggleSolver_Release.exe
```

Linux (single-configuration Release build):

```sh
cd build/bin/benchmark
./bm_BoggleSolver_Release
```

## What the benchmarks measure

- `BM_SolveRandomBoards` measures the owning `Solve` API on a different board each iteration, cycling through 4,096 seeded boards.
- `BM_SolveViewsFresh` measures `SolveInto` with a new output vector each iteration, including its allocations and destruction. `BM_SolveViewsReused` retains capacity between solves. Both use the same 4,096 boards without pre-reserving output capacity or doing an untimed warm-up.
- `BM_SolveFixedBoard` solves the same board each iteration (the original benchmark). Repeated solves let the branch predictor learn the board, so timings can be much lower than on varied boards. It is retained for comparison with the historical measurements.
- The `threads:N` variants run one solver per hardware thread. `items_per_second` is the total boards solved per second.
- **Stress Test**: The application includes a "1 million board" challenge that shuffles and solves 1,000,000 random boards, once on one thread and once on every hardware thread, to measure real-world throughput.
- **Performance Analysis**: The application was run through [Intel VTune Profiler](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html) to determine hot spots and opportunities for optimization.
- **Intel VTune**: Hot spot summaries can be found in the [images folder](../images). Intel VTune was run using the `basic_english_words_850.txt` word list & 100,000 Solve iterations.

## Interpreting results

Use distinct-board benchmarks for varied-board solve latency. Fixed-board timings benefit from repeated branch-predictor training. Compare the same dictionary, output API, build flags, thread count, and hardware, and distinguish solving from shuffling and startup costs.

The [optimization history](optimization-history.md) preserves the measured improvements, methodology, reproduction commands, and links to raw results for each round. Historical numbers are specific to those builds and machines.
