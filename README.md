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

- **Dictionary**: A Trie structure optimized with a vector-based allocation to ensure contiguous memory layout.
- **BoggleBoard**: Mimics standard die configurations; handles random shuffling and ASCII output.
- **BoggleSolver**: Core search engine employing recursive DFS with bitmask of visited cells and precomputed neighbour lookups.

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
.\build\bin\app\Release\SolveBoggle_Release.exe .\resources\basic_english_2000.txt
```

### Example Console Output
```text
Building Dictionary from: ..\..\..\..\resources\basic_english_words_2000.txt...
Successfully loaded dictionary into the Dictionary!

Solving the following board:

----- Boggle Board -----
[ V ] [ O ] [ E ] [ T ] 
[ W ] [ T ] [ T ] [ K ] 
[ O ] [ R ] [ B ] [ N ] 
[ S ] [ S ] [ T ] [ Q ] 
------------------------

Found 5 words in 15.4 us:
VOTE
TOE
TWO
ROT
SORT

----------------------------------------------------------
Timing how long it takes to solve 1 Million random boards:
NOTE: This time includes the shuffling of the board
It shouldn't take too long...

Solved 1000000 boards in 5.47983 s
Average time to shuffle & solve a board: 5.47983 us

```

### Unit Testing & Benchmarking & Analysis
- **Unit Testing**: `ctest --preset all-enabled-test`
- **Benchmark**: Run `.\BoggleSolverBenchmark_Release.exe` **NOTE:** `basic_english_2000.txt` must be in the same directory as the benchmark. Cmake will copy the file to the same directory on build.
- **Stress Test**: The application includes a "1 million board" challenge that shuffles and solves 1,000,000 random boards to measure real-world throughput.
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

