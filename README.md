| Windows Build | Linux Build |
|:---:|:---:|
| [![Clang 22, Windows](https://github.com/Harrison-Farrell/Boggle/actions/workflows/windows_build.yml/badge.svg)](https://github.com/Harrison-Farrell/Boggle/actions/workflows/windows_build.yml) | [![Clang 22, Linux](https://github.com/Harrison-Farrell/Boggle/actions/workflows/linux_build.yml/badge.svg)](https://github.com/Harrison-Farrell/Boggle/actions/workflows/linux_build.yml) |

# Solo Boggle Solver

A C++ solver for the classic game of Boggle. This project focuses on the optimization of finding words in a game of Boggle.

## Internal Architecture

The solver is divided into three primary components:

```text
                     +-----------------------+
                     |      main.cpp         |
                     |   (Initialization)    | 
                     +-----------------------+
                      /          |       \   ^
                     /           |        \   \
                    v            v         v   \
        +--------------+  +-------------+  +--------------+
        |  Dictionary  |  | BoggleBoard |  | BoggleSolver |
        |   (Words)    |  | (4x4 Grid)  |  | (Algorithm)  |
        +--------------+  +-------------+  +--------------+
                ^                ^                |
                |                |                |
                |                +----------------+
                |                Traverses Cells  |
                |                                 |
                +---------------------------------+
                  Queries Prefixes and Words  
```

- **Dictionary**: A Trie structure optimized with a vector-based allocation to ensure contiguous memory layout. Words can be inserted at any time.
- **CompactTrie**: A read-only copy of the Dictionary built for solving. Nodes are 8 bytes with their children stored next to each other, and the Boggle rules ("Q" reads as "QU", words need 3+ letters) are folded in when it is built. One copy can be shared by solvers on several threads.
- **BoggleBoard**: Mimics standard die configurations; handles random shuffling and ASCII output. Can be seeded for a reproducible sequence of boards.
- **BoggleSolver**: Core search engine. It grows every path on the board by one cell per pass (breadth first), using a bitmask of visited cells and precomputed neighbour lookups, and writes the next paths without branching on how many neighbours match. On SSSE3 targets, board letter masks filter out neighbours that cannot extend the current dictionary prefix before the search loop. Results can be returned as owned strings or written into a reusable buffer of views into the trie text.

---

### Prerequisites
The project uses `Deducing this (P0847R7)` and `__builtin_ctz`.
Therefore the project is built with Clang.

For Windows
- **Visual Studio 2026**
- **Clang 22 for Windows**
- **Ninja**

For Linux
- **Clang 22**
- **Ninja**


### Build & Run
```bash
# 1. Configure and Build (Release)
cmake --preset windows-release
cmake --build --preset windows-release-build

# 2. Run the Solver
.\build\bin\app\Release\SolveBoggleBoard_Release.exe .\resources\basic_english_words_2000.txt
```

### Solving Without Copying Word Text

`Solve(board)` returns owned `std::string` results. Call `SolveInto(board, words)` to collect views into the dictionary's immutable word text instead:

```cpp
const auto trie = BoggleSolver::BuildTrie(dictionary);
BoggleSolver solver(trie);
std::vector<std::string_view> words;
solver.SolveInto(board.GetFlattenedBoard(), words);
// Reusing words on another call retains its allocated capacity.
```

`SolveInto` clears previous results, including when the solver has a null trie, and preserves the word order of `Solve`. Views remain valid across later solves while their `CompactTrie` is alive. Keep the shared trie alive if views must outlive the solver, or use `Solve` for independently owned results. Both APIs share the same search implementation without an intermediate result vector. The console example uses fresh views for its displayed board and reuses one output buffer per worker in the million-board challenge.

### Example Console Output
```text
Building Dictionary from: resources/basic_english_words_2000.txt...
Successfully loaded dictionary into the Dictionary!

Solving the following board:

----- Boggle Board -----
[ D ] [ S ] [ E ] [ A ] 
[ T ] [ K ] [ Z ] [ H ] 
[ O ] [ Q ] [ H ] [ O ] 
[ I ] [ C ] [ E ] [ O ] 
------------------------

Found 2 words in 2 us:
SEA
ICE

----------------------------------------------------------
Timing how long it takes to solve 1 million random boards:
NOTE: This time includes the shuffling of the board
It shouldn't take too long... ... ...

1 thread  : solved 1000000 boards in 2.29259 s, average 2.29259 us per board to shuffle & solve
32 threads: solved 1000000 boards in 0.135413 s, average 0.135413 us per board to shuffle & solve
Speed-up from threads: 16.9304x
```

### Unit Testing & Benchmarking & Analysis
- **Unit Testing**: `ctest --preset all-enabled-test`
- **Benchmark**: Run `.\build\bin\benchmark\Release\bm_BoggleSolver_Release.exe` from its own directory. **NOTE:** `basic_english_words_2000.txt` and `boggle_words_138735.txt` must be in the same directory as the benchmark. CMake copies them there on build.
  - `BM_SolveRandomBoards` measures the owning `Solve` API on a different board each iteration, cycling through 4,096 seeded boards.
  - `BM_SolveViewsFresh` measures `SolveInto` with a new output vector each iteration, including its allocations and destruction. `BM_SolveViewsReused` retains capacity between solves. Both use the same 4,096 boards without pre-reserving output capacity or doing an untimed warm-up.
  - `BM_SolveFixedBoard` solves the same board each iteration (the original benchmark). The branch predictor learns the board, so it reads about 3x faster than real use. It is kept to compare with the history below.
  - The `threads:N` variants run one solver per hardware thread. `items_per_second` is the total boards solved per second.
- **Stress Test**: The application includes a "1 million board" challenge that shuffles and solves 1,000,000 random boards, once on one thread and once on every hardware thread, to measure real-world throughput.
- **Performance Analysis**: The application was ran through [Intel VTune Profiler](https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler.html) to determine hot spots and opportunities for optimization.
- **Intel VTune**: Hot spot summaries can be found in the `images` folder. Intel Vtune was ran using the `basic_english_850.txt` word list & 100,000 Solve iterations.

---

## Optimization History

- The benchmarks have been ran on a Windows 11 machine with an 13th Gen Intel(R) Core(TM) i9-13900HX (2.20 GHz).
- The benchmarks ran use the `basic_english_2000.txt` word list file to build the dictionary (hard-coded). 
- The benchmarks repeats the benchmark fixture 50 times to determine the mean time.
- The baseline (solving the problem, no performance refactoring) execution time has been witnessed to be approximately 11,500 nanoseconds per solve.
- Throughout the development, constant refactoring and performing trial and error investigations has led to the current performance improvements over time.
- The before and after mean execution times are measured using the [Google Benchmark suite](https://github.com/google/benchmark.git). 

|Step |Improvement | Description | Before  | After | % Gain |
|:--- |:--- | :--- | :--- | :--- | :--- |
|1 |**Vector Allocation** | Replaced heap-allocated Trie nodes with a contiguous `vector arena`. | 11,571ns | 9,789ns | ~15% |
|2 |**Char Case Optimization** | Switched search to uppercase to eliminate runtime `std::tolower` calls. | 9,789ns | 5,752ns | ~41% |
|3 |**Container Choice** | Moved from `std::set` to `std::vector` for result tracking. | 5,752ns | 5,011ns | ~13% |
|4 |**Precomputing** | Used bitmask and precomputed neighbour lists to remove branch overhead. | 5,011ns | 3,985ns | ~20% |
|5 |**Word Traverse** | Replaced full-tree re-scans with incremental $O(1)$ node index tracking. | 3,985ns | 3,196ns | ~20% |
|6 |**Precompute Refactor**| Shifted precomputed neighbour calculation to compile-time `constexpr`. | 3,196ns | 3,039ns | ~5% |
|7 |**String Buffer** | Eliminated heap allocations by using a reference-passed string buffer. | 3,039ns | 2,745ns | ~10% |
|8 |**Duplicate Words**| Added `solve_id` tracking to skip redundant word processing. | 2,745ns | 2,572ns | ~6% |
|9 |**Inlining Functions**| Made `GetNextNodeIndex` and `IsWord` inline to reduce function calls. | 2,572ns | 2,536ns | ~1.4% |
|10 |**Reserve String Size**| Reserve the memory for `current_word_buffer_` to reduce malloc calls. | 2,536ns | 2178ns | ~14% |
|11 |**Local Lambda**| Moved the recursive DFS function to a local lambda to reduce function calls. | 2178ns | 1918ns | ~12% |
|12 |**Bitmask Children**| Changed the way we iterate and check the children nodes to a bitmask approach. | 1918ns | 1123ns | ~41% |
|13 |**Bitmask the Neighbours**| Changed the nieghbours from a struct to a bitmask approach. | 1123ns | 1051ns | ~6% |

**Total Performance Gain:** ~90% reduction in execution time (11,571ns → 1,051ns).

### Round 2: Distinct Boards & Threads

The benchmark above solves the same board over and over, which lets the CPU's branch predictor learn that board. On boards it has not seen, the step 13 solver takes about 3,400ns rather than about 1,000ns. The benchmark now cycles through 4,096 seeded boards, and also uses the 138,735-word list, whose original trie (47.9 MB) did not fit in the CPU's cache.

- Measured on a Windows 11 machine with a 13th Gen Intel(R) Core(TM) i9-13900K (24 cores / 32 threads), so the step 13 figures differ slightly from the table above.
- Single-thread figures are the median of three runs of 10 repetitions, with the old and new solvers run alternately and pinned to one performance core.

|Step |Improvement | Description | Measured | Before | After | % Gain |
|:--- |:--- | :--- | :--- | :--- | :--- | :--- |
|14 |**Seed Once**| `Shuffle()` built a `std::random_device` and `std::mt19937` and copied the dice on every call. The generator is now seeded once and an index array is shuffled in place. | `Shuffle()` alone | 1,103ns | 143ns | ~87% |
|15 |**Compact Trie**| Added `CompactTrie`: 8-byte nodes (was 112) with children stored together and found by `popcount`. "Q" → "QU" and the 3-letter minimum are folded in when it is built, and words are stored per node, so the search loop no longer handles either rule or builds a string. The 138,735-word trie shrinks from 47.9 MB to 3.4 MB. | Fixed board, 2,000 words | 1,031ns | 727ns | ~29% |
| | | | Distinct boards, 2,000 words | 3,468ns | 2,217ns | ~36% |
| | | | Distinct boards, 138,735 words | 18,515ns | 9,913ns | ~46% |
|16 |**Threads**| One solver per hardware thread, all sharing the read-only trie. Threads take boards 1,000 at a time so that faster cores do more of the work. | Boards/s, 2,000 words | 0.47 M (1 thread) | 9.20 M (32 threads) | 19.8x |
| | | | Boards/s, 138,735 words | 0.10 M (1 thread) | 2.16 M (32 threads) | 22.2x |

**Round 2 Result:** the 1 million board challenge (shuffle & solve) went from 4.61 s to 0.13 s with the 2,000-word list, and from 19.8 s to 0.50 s with the 138,735-word list.

### Round 3: Letter Masks for Single-Board Solving

Previously, each DFS step checked every unused neighbour against the trie, including letters that could not extend the current prefix. The solver now prepares two 16-byte selectors for the board. At each prefix, an SSSE3 byte shuffle and bit tests convert the trie's 26-bit child-letter mask into matching board cells. Intersecting those cells with the unused neighbours removes failed transitions before the loop. The trie keeps its 8-byte nodes, and the search visits matching cells in the same order as before.

The acceleration is enabled when the compiler target defines `__SSSE3__`, as it does on the measured machine with `-march=native`. Other targets retain the original neighbour traversal. The results below apply to the SSSE3 path.

Measured on 25 November 2026 with a 13th Gen Intel Core i9-13900K on Windows, using Clang 23.1.4 and the Release build (`-O3 -DNDEBUG -march=native`):

| Dictionary | Before | After | Time reduction | Speed-up |
|:---|---:|---:|---:|---:|
| 2,000 words | 2,226 ns | 1,519 ns | 31.8% | 1.47x |
| 138,735 words | 9,951 ns | 7,094 ns | 28.7% | 1.40x |

- Both versions ran on one thread pinned to logical CPU 2 (affinity mask `0x4`), alternating before/after for three pairs of runs. Each run used 10 repetitions with a minimum benchmark time of 0.5 seconds per repetition.
- Each version used the existing `BM_SolveRandomBoards` benchmark: 4,096 boards generated with seed 2024. The table reports the median of the three run medians from Google Benchmark's elapsed `Time` field, in nanoseconds per board. The [recorded measurements](benchmark/results/letter_masks_2026-11-25.json) also include CPU times and variation within each run.
- Timing includes board letter preprocessing, search, and construction/destruction of the returned words. It excludes dictionary/trie loading, solver construction, and board shuffling. These figures measure single-thread solve latency over varied boards with an already constructed solver; startup and multithread throughput were not measured in this round.

To repeat the current-version measurement after a Release build, run from the repository root:

```powershell
Set-Location build/bin/benchmark/Release
(Get-Process -Id $PID).ProcessorAffinity = 0x4
.\bm_BoggleSolver_Release.exe '--benchmark_filter=BM_SolveRandomBoards/[^/]+/repeats:10$' '--benchmark_min_time=0.5s' '--benchmark_out=letter_masks.json' '--benchmark_out_format=json'
```

Validation: all 34 Release tests passed. A new independent word-by-word search checks complete result sets across 128 seeded and special-case boards, including repeated letters, `QU`, lowercase input, invalid cells, and cell reuse. All eight solver tests also passed with the solver compiled for baseline x86-64 without SSSE3, exercising the fallback.

### Round 4: Reusable Word Views

`SolveInto(board, words)` appends `std::string_view` results into a caller-owned vector after clearing it. Word characters stay in the trie, and retaining the vector between solves avoids repeated allocation once it has sufficient capacity. `Solve` remains available for owned strings. Both APIs use the same search, specialized for their output type without a temporary collection or a callback on each word. The console application now uses views, and its displayed solve time uses an explicit `std::chrono` duration in microseconds.

Measured on 25 November 2026 with the same i9-13900K, Windows, Clang 23.1.4, Release flags, and SSSE3 search as Round 3. The baseline is the previous commit (`bdba08a`), rebuilt and measured again in this session. All times are elapsed nanoseconds per board:

| Dictionary | Previous `Solve` | Current `Solve` | Views, fresh buffer | Views, reused buffer |
|:---|---:|---:|---:|---:|
| 2,000 words | 1,534 ns | 1,565 ns | 1,484 ns | 1,214 ns |
| 138,735 words | 7,594 ns | 7,312 ns | 6,522 ns | 5,942 ns |

Compared with the previous commit, a reused buffer reduces solve time by **20.9%** and **21.8%**, respectively. Fresh buffers reduce measured time by **3.3%** and **14.1%**; the smaller gain is close to normal run-to-run variation. Fresh views still allocate vector storage. The current `Solve` column records the owning API after the shared-search refactor, so its timing is visible separately from the new API's gains.

- Each column is the median of three run medians, each with 10 repetitions and a minimum benchmark time of 0.5 seconds. The runs use one thread pinned to logical CPU 2 (`0x4`) and the same 4,096 boards seeded with 2024. Current-version benchmark repetitions were randomly interleaved; baseline/current process order was reversed for the second pair.
- Fresh-buffer timings include vector construction, growth, and destruction on each iteration. Reused-buffer timings start with an empty vector, with no reserve or untimed warm-up; capacity growth is included and amortized across that run. All modes include letter preprocessing, search, and output collection, and exclude dictionary/trie loading, solver construction, board shuffling, and process startup. These measure varied boards with a prepared solver, rather than cold startup.
- The [recorded measurements](benchmark/results/string_views_2026-11-25.json) include all run medians, CPU times, and variation. One large-dictionary baseline run had an elapsed-time outlier; all repetitions were retained and medians used.

To compare the three current output modes after a Release build, run from the repository root:

```powershell
Set-Location build/bin/benchmark/Release
(Get-Process -Id $PID).ProcessorAffinity = 0x4
.\bm_BoggleSolver_Release.exe '--benchmark_filter=BM_Solve(RandomBoards|ViewsFresh|ViewsReused)/[^/]+/repeats:10$' '--benchmark_enable_random_interleaving=true' '--benchmark_min_time=0.5s' '--benchmark_out=string_views.json' '--benchmark_out_format=json'
```

Validation: all 36 Release tests passed, including comparison of both APIs against the independent search across 128 boards. Added checks cover output order, capacity reuse, clearing stale results, null tries, and views borrowing trie text across later solves and solver destruction while a shared trie remains alive. All 10 solver tests also passed with baseline x86-64 compilation without SSSE3. The console example and its million-board challenge completed successfully.

### Round 5 — Direct lookup after letter filtering

The SSSE3 filter already confirms that each surviving neighbour has a child in the current trie node. During board preparation, the solver now builds a mask for each cell containing all letters alphabetically before its letter. During search, a `popcount` counts how many children precede that letter, giving its offset in the sorted child array. This removes a membership check from every filtered transition.

| Dictionary | Baseline (`aaa1710`) | Direct lookup | Solve time reduction |
|:---|---:|---:|---:|
| 2,000 words | 1,604 ns | 1,412 ns | 12.0% |
| 138,735 words | 6,743 ns | 6,317 ns | 6.3% |

`BM_SolveViewsFresh` measured elapsed time per board with a prepared solver and a fresh output vector. Both Release builds ran on one thread pinned to logical CPU 2 and cycled through the same 4,096 boards (seed 2024). Each figure is the median of four run medians, with 10 repetitions per run. Direct lookup was faster in all four pairs for both dictionaries. [Recorded benchmark results](benchmark/results/known_child_2026-11-25.json)

All 36 Release tests passed. Across all 4,096 boards with each dictionary, both solve APIs returned the same words in the same order as the baseline. The scalar fallback also matched.

### Round 6 — Search one path length at a time

Counting the work showed that each trie node visited cost about 50 CPU cycles, even with the 2,000-word trie (54 KB) entirely in cache. Most of that was the CPU mispredicting how many neighbours of a cell continue the prefix, which differs at every step of a depth-first search.

The search now grows all paths by one cell per pass instead of following each path to its end. The paths of one length are kept in a flat array of `{trie node, last cell, visited cells}`. Each path writes a candidate into all eight neighbour slots and moves the write position forward only for neighbours that continue the prefix (`next_size += (keep >> cell) & 1`), so there is no branch on the neighbour count. Corner and edge cells pad their neighbour list with a cell that never matches. Within a pass, trie reads do not depend on each other, so the CPU can overlap their cache misses. The two path arrays are kept in the solver between solves.

Words are now returned shortest path first, rather than in depth-first order. `Solve` and `SolveInto` still return the same order, and the set of words found is unchanged.

| Benchmark | Dictionary | Baseline (`bbeb6ed`) | Level by level | Solve time reduction |
|:---|:---|---:|---:|---:|
| `BM_SolveViewsReused` | 2,000 words | 1,182 ns | 890 ns | 24.7% |
| `BM_SolveViewsReused` | 138,735 words | 5,761 ns | 4,129 ns | 28.3% |
| `BM_SolveViewsFresh` | 2,000 words | 1,570 ns | 1,269 ns | 19.1% |
| `BM_SolveViewsFresh` | 138,735 words | 6,593 ns | 4,957 ns | 24.8% |

Both Release builds ran on one thread pinned to logical CPU 2, alternating baseline and new builds in the order ABBAABBA, with 10 randomly interleaved repetitions of at least 0.5 seconds per run. Each figure is the median of four run medians of elapsed time per board over the same 4,096 boards (seed 2024). [Recorded benchmark results](benchmark/results/level_search_2026-11-25.json)

All 37 Release tests passed. For 8,195 boards (the 4,096 seeded boards, each again with a lower-case and two invalid cells, plus three special boards) and both dictionaries, the sorted words matched the baseline, in both the SSSE3 build and a baseline x86-64 build that uses the scalar fallback.

### Round 7 — Found-word bits, and word text looked up after the search

To list each word once, the solver kept a 4-byte solve number for every trie node (1.7 MB for the 138,735-word trie) and looked up the word's text as soon as it was found (in a 3.4 MB array, also indexed by node). Both reads land at random places in large arrays, and happened in the middle of the search where each cache miss was paid in turn.

- Found words are now marked in one bit per trie node (53 KB for the large trie). After the search only the 64-bit blocks holding found words are cleared, so nothing is reset per board and the solve-number wrap-around is gone.
- A new word is recorded without a branch: the node is always written to a buffer of found nodes, and the count only moves on when the node ends a word that has not been found yet. `CompactTrie::GetTotalWords()` sizes that buffer.
- The text of each word is looked up after the search, in one simple loop whose reads do not depend on each other. The output vector is reserved once for the number of words found.

| Benchmark | Dictionary | Baseline (`24da2af`) | Found-word bits | Solve time reduction |
|:---|:---|---:|---:|---:|
| `BM_SolveViewsReused` | 2,000 words | 901 ns | 833 ns | 7.5% |
| `BM_SolveViewsReused` | 138,735 words | 4,166 ns | 3,598 ns | 13.6% |
| `BM_SolveViewsFresh` | 2,000 words | 1,275 ns | 899 ns | 29.5% |
| `BM_SolveViewsFresh` | 138,735 words | 5,009 ns | 3,702 ns | 26.1% |

The fresh-buffer gain is larger mainly because the output vector is now sized once instead of growing several times per solve. Measured as in Round 6 (ABBAABBA, pinned to logical CPU 2, median of four run medians). [Recorded benchmark results](benchmark/results/found_word_bits_2026-11-25.json)

All 38 Release tests passed. The sorted words for the same 8,195 boards matched the Round 5 solver with both dictionaries, in the SSSE3 and scalar builds. Each board was solved twice in a row, checking that the found-word bits are cleared between solves.

---

## Resources
Word lists are sourced from 
- [John Burkardt's datasets](https://people.sc.fsu.edu/~jburkardt/datasets/words/words.html)
- [Ben Hoyt boggle list](https://github.com/benhoyt)


Examples available in `./resources`:
- `basic_english_850.txt`
- `basic_english_2000.txt`
- `doublet_words_5557.txt`
- `boggle_words_138735.txt`

