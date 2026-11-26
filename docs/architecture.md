# Architecture

[Project overview](../README.md) · [Documentation index](README.md)

## Components

- **Dictionary**: A trie structure optimized with a vector-based allocation to ensure contiguous memory layout. Words can be inserted at any time.
- **CompactTrie**: A read-only copy of the Dictionary built for solving. Nodes are 8 bytes with their children stored next to each other, word text spans are packed into 4 bytes (`(offset << 8) | length`), and the Boggle rules ("Q" reads as "QU", words need 3+ letters) are folded in when it is built. One copy can be shared by solvers on several threads.
- **BoggleBoard**: Mimics standard die configurations stored in a flat lookup table; performs a single-pass division-free Fisher-Yates shuffle and die-face roll (packing four 8-bit fixed-point multiplications into each 32-bit RNG draw), provides an inline `std::bit_cast` flattened board view, and handles ASCII output. Can be seeded for a reproducible sequence of boards.
- **BoggleSolver**: Core search engine. It grows every path on the board by one cell per pass (breadth first) using 8-byte packed `PathState` entries (`{uint32_t node; uint16_t cell; uint16_t visited;}`). On SSSE3 targets, 16-byte shuffle and bit selectors are constructed directly in XMM registers to filter out neighbours that cannot extend the current dictionary prefix. Surviving neighbour bits are extracted using two unconditional branchless `std::countr_zero` (`tzcnt`) and bit-clear (`blsr`) slots plus a rarely taken tail when three or more neighbours match. Results can be returned as owned strings or written into a reusable buffer of views into the trie text.

## Visual overview

### System architecture, memory layout, and multi-thread pipeline

![System architecture, memory layout, and multi-thread pipeline](../images/system_architecture.svg)

### Single-board search pipeline and inner-loop optimizations

![Single-board search pipeline and inner-loop optimizations](../images/search_pipeline.svg)

### Step-by-step single-board solve walkthrough

![Step-by-step single-board solve walkthrough](../images/single_board_step_by_step.svg)

## Data flow

1. The [console application](../src/main.cpp) loads a word list into `Dictionary`.
2. `BoggleSolver::BuildTrie` creates an immutable `CompactTrie` snapshot. Later inserts into the source dictionary do not update that snapshot.
3. `BoggleBoard` supplies a flattened 4×4 grid of letters.
4. `BoggleSolver` extends valid dictionary prefixes across adjacent cells, tracking visited cells with a bitmask.
5. Found-word bits remove duplicates. After searching, the solver reads word text into owned strings or views.

Each worker owns its solver and mutable search buffers; workers share the immutable trie. See [API usage and view lifetimes](usage.md) and the [optimization history](optimization-history.md) for the implementation's evolution.

## Source layout

| Path | Purpose |
|:---|:---|
| [src/Dictionary](../src/Dictionary) | Mutable dictionary and compact solving trie |
| [src/BoggleBoard](../src/BoggleBoard) | Dice, board generation, and display |
| [src/BoggleSolver](../src/BoggleSolver) | Search and result collection |
| [test](../test) | GoogleTest unit tests |
| [benchmark](../benchmark) | Google Benchmark fixtures and recorded results |
