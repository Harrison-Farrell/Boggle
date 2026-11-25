# Solo Boggle Solver

A C++23 Boggle solver focused on fast word search on a standard 4×4 board. It loads a word list, finds valid words, and measures throughput over one million random boards.

| Windows Build | Linux Build |
|:---:|:---:|
| [![Clang 22, Windows](https://github.com/Harrison-Farrell/Boggle/actions/workflows/windows_build.yml/badge.svg)](https://github.com/Harrison-Farrell/Boggle/actions/workflows/windows_build.yml) | [![Clang 22, Linux](https://github.com/Harrison-Farrell/Boggle/actions/workflows/linux_build.yml/badge.svg)](https://github.com/Harrison-Farrell/Boggle/actions/workflows/linux_build.yml) |

The solver combines an immutable compact trie with a search that extends all paths one cell at a time. It supports owned string results or reusable buffers of word views, and multiple worker threads can share one trie. Boards can be seeded for reproducible runs.

## Get started

You need CMake 3.31.6 or newer and Clang with the required C++23 features; CI uses Clang 22. Windows builds also use the Visual Studio C++ toolchain and Ninja.

Follow [building and running](docs/getting-started.md) for Windows and Linux commands. The application accepts a dictionary path, displays a solved board, and runs a single-thread and multithread throughput challenge.

## Documentation

| Guide | Contents |
|:---|:---|
| [Building and running](docs/getting-started.md) | Requirements, Windows and Linux commands, build options |
| [Using the solver](docs/usage.md) | Console example, owned results, reusable views, and lifetimes |
| [Architecture](docs/architecture.md) | Components, search design, threading, and source layout |
| [Testing and benchmarking](docs/benchmarking.md) | Unit tests, benchmark modes, and measurement guidance |
| [Optimization history](docs/optimization-history.md) | Historical measurements, methodology, and raw results |
| [Word lists and resources](docs/resources.md) | Bundled dictionaries and their sources |

## Performance

Recorded cumulative performance gains include:

| Workload | Before → after | Total gain |
|:---|:---|:---|
| Original fixed-board solve, steps 1–13 | 11,571 → 1,051 ns | **90.9% less time; 11.0× faster** |
| Million-board shuffle and solve, through Round 2, 2,000-word list | 4.61 → 0.13 s | **97.2% less time; 35.5× faster** |
| Million-board shuffle and solve, through Round 2, 138,735-word list | 19.8 → 0.50 s | **97.5% less time; 39.6× faster** |
| Varied-board solve with reused word views, Rounds 6–8, 2,000-word list | 1,183 → 804 ns | **32.0% less time; 1.47× faster** |
| Varied-board solve with reused word views, Rounds 6–8, 138,735-word list | 5,818 → 3,579 ns | **38.5% less time; 1.63× faster** |

These comparisons cover different stages and workloads: the million-board gains include multithreading, while the varied-board figures measure single-thread solving. They cannot be combined into one overall speed-up. The [optimization history](docs/optimization-history.md) records the hardware, methodology, and raw results. See [benchmarking](docs/benchmarking.md) to measure your own build.

## License

See [LICENSE](LICENSE).
