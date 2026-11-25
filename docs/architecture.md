# Architecture

[Project overview](../README.md) · [Documentation index](README.md)

## Components

- **Dictionary**: A trie structure optimized with a vector-based allocation to ensure contiguous memory layout. Words can be inserted at any time.
- **CompactTrie**: A read-only copy of the Dictionary built for solving. Nodes are 8 bytes with their children stored next to each other, and the Boggle rules ("Q" reads as "QU", words need 3+ letters) are folded in when it is built. One copy can be shared by solvers on several threads.
- **BoggleBoard**: Mimics standard die configurations; handles random shuffling and ASCII output. Can be seeded for a reproducible sequence of boards.
- **BoggleSolver**: Core search engine. It grows every path on the board by one cell per pass (breadth first), using a bitmask of visited cells and precomputed neighbour lookups, and writes the next paths without branching on how many neighbours match. On SSSE3 targets, board letter masks filter out neighbours that cannot extend the current dictionary prefix before the search loop. Results can be returned as owned strings or written into a reusable buffer of views into the trie text.

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
