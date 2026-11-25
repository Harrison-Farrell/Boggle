# Using the solver

[Project overview](../README.md) · [Documentation index](README.md)

## Console application

Run the executable with a word-list path as described in [building and running](getting-started.md). It displays a random board and its words, then runs the million-board challenge on one thread and on all hardware threads. The challenge includes board shuffling.

## C++ API

Load words into a `std::shared_ptr<Dictionary>`, build a shared trie with `BoggleSolver::BuildTrie(dictionary)`, and construct a solver from it. See the [solver header](../src/BoggleSolver/BoggleSolver.h) for the API and [console application](../src/main.cpp) for a complete example.

### Owned results and reusable word views

`Solve(board)` returns owned `std::string` results. Call `SolveInto(board, words)` to collect views into the dictionary's immutable word text instead:

```cpp
const auto trie = BoggleSolver::BuildTrie(dictionary);
BoggleSolver solver(trie);
std::vector<std::string_view> words;
solver.SolveInto(board.GetFlattenedBoard(), words);
// Reusing words on another call retains its allocated capacity.
```

`SolveInto` clears previous results, including when the solver has a null trie, and preserves the word order of `Solve`. Views remain valid across later solves while their `CompactTrie` is alive. Keep the shared trie alive if views must outlive the solver, or use `Solve` for independently owned results. Both APIs share the same search implementation without an intermediate result vector. The console example uses fresh views for its displayed board and reuses one output buffer per worker in the million-board challenge.

Results contain each word once, with paths using fewer board cells first. A `Q` cell reads as `QU`, words must have at least three letters, and a path cannot reuse a cell. ASCII lowercase letters are accepted; other bytes are skipped.

Use a separate solver and output buffer per thread, sharing the immutable trie. Each solver retains mutable search buffers between calls.

## Example console output

This is an illustrative run; boards, results, thread counts, and timings vary. It predates later optimizations recorded in the [optimization history](optimization-history.md).

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
