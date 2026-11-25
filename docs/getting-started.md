# Building and running

[Project overview](../README.md) · [Documentation index](README.md)

## Requirements

- CMake 3.31.6 or newer and Git.
- Clang with the project's C++23 features, including deducing `this` (P0847R7). CI uses Clang 22; CMake requires Clang.
- Windows: Visual Studio 2026 C++ build tools, Clang 22, and Ninja. Use a developer shell with the compiler and build tools available.
- Linux: Clang 22 and a build tool such as Make or Ninja. The commands below use the default CMake generator.

CMake fetches GoogleTest and Google Benchmark when their build options are enabled, so initial configuration requires access to their GitHub repositories.

Run all commands below from the repository root.

## Windows (PowerShell)

```powershell
cmake --preset windows-release
cmake --build --preset windows-release-build
.\build\bin\app\Release\SolveBoggleBoard_Release.exe .\resources\basic_english_words_2000.txt
```

The Windows presets use Ninja Multi-Config. Debug presets are also available in [CMakePresets.json](../CMakePresets.json).

## Linux

```sh
cmake --preset linux-release
cmake --build --preset linux-release-build
./build/bin/app/SolveBoggleBoard_Release resources/basic_english_words_2000.txt
```

For Debug, configure and build with `--preset linux-debug`. Linux presets use a single configuration in `build`; reconfigure when changing build types.

## Build options

`BUILD_TESTS` and `BUILD_BENCHMARKS` both default to `ON`. To build only the application, add these options when configuring:

```sh
cmake --preset linux-release -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF
```

The same options work with the Windows configure preset. Re-enable them with `ON` when needed.

Release builds use `-O3 -march=native`. They target the build machine's CPU; rebuild for the destination CPU when moving binaries to another machine.

## Next steps

- [Using the solver](usage.md): console behavior, C++ APIs, and view lifetimes.
- [Testing and benchmarking](benchmarking.md): test commands and performance measurement.
- [Word lists](resources.md): bundled dictionaries and sources.
