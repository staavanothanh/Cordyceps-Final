# Memory

## Project Overview
- **Name:** Cordyceps Engine
- **Purpose:** A high-performance game engine written in C++ on Windows, designed to play the **Mushroom Game** and compete in the Nexon Youth Programming Challenge (**NYPC**) Master Track arena.
- **Game Rules:** For a complete explanation, see [GameRules.md](file:///d:/Learning_Programing/New-NYPC/GameRules.md). Key rules include:
  - Played on a 10x17 grid (170 cells) containing mushrooms valued 1 to 9.
  - Move: Select a rectangle $(r_1, c_1, r_2, c_2)$ with a sum of remaining mushrooms of exactly 10.
  - Inscribed Rule: All 4 edges of the selected rectangle must touch at least one remaining mushroom.
  - Pass Move: `-1 -1 -1 -1`. The game ends when both players pass consecutively.
- **Contest & Organizer (BTC) Rules:** See [TONGHOP.md](file:///d:/Learning_Programing/New-NYPC/TONGHOP.md) and [Detail_BTC.md](file:///d:/Learning_Programing/New-NYPC/Detail_BTC.md) for AWS infrastructure, limits, and submission rules:
  - **AWS c7a.2xlarge Instance:** Restricted to **exactly 1 CPU Core** and **1,024 MiB (1 GB) RAM** limit.
  - **Submission Constraints:** A single source file (e.g. `main.cpp`) `< 1 MiB` and a single binary data file (`data.bin`) `< 10 MiB`.
  - **No Network:** Complete offline execution during matches.

## Code Style & Architecture Constraints
- **Single-Threaded Optimization:** Do NOT use multi-threading (`std::thread`, `std::async`) as the server is strictly limited to 1 CPU Core.
- **Memory Safety:** Use `std::array` instead of C-style arrays, and use `.at()` for bounds-checked element access during debugging.
- **MCTS Node Management:** Ensure MCTS nodes are managed safely (e.g., using `std::unique_ptr`) and completely freed at the end of each turn to stay within the 1,024 MiB RAM limit.
- **Safeguard Timer:** Check elapsed time frequently inside MCTS search loops to prevent Time Limit Exceeded (TLE) (total budget: 10,000ms).
- **Techniques:** Use 2D Prefix Sum for $\mathcal{O}(1)$ rectangle sum calculation and 64-bit Bitmasks (`uint64_t`) for board ownership state logic.
- **Immutability:** Follow immutability principles where possible (e.g., creating new board states rather than mutating in-place during simulations if it avoids synchronization/cleanup bugs).

## Development and Verification Workflow
- **Build & Test System:** Use CMake and CTest to manage building and running unit tests locally.
- **WSL Cross-Verification:** Because the target environment (Linux/g++) differs from local Windows, you **MUST** verify everything in WSL Ubuntu before final submission:
  - Local WSL Distribution: `Ubuntu` (Ubuntu 24.04 LTS).
  - Target Compiler in WSL: `g++-14`.
  - Compile Command: `g++-14 -O3 -std=c++20 main.cpp -o main`.
  - Verification Command: Always run compilation and local test scripts (`testing_tool.py`) within WSL to guarantee 100% compatibility with BTC's environment.

## ECC Usage Policy
This workspace includes Everything Claude Code (ECC) skills, agents, and commands located under `.commandcode/`.
- **Subagent Division:** Split complex tasks among specialized agents such as `@planner` (for planning), `@tdd-guide` (for test-driven development), and `@code-reviewer` (for code review). Run independent agents in parallel to optimize efficiency.
- **Skills and Workflows:** Leverage pre-defined ECC skills and execute workflows under `.commandcode/` when starting tasks (e.g., refactoring, testing, security checks).
- **Rules Customization:** Project-specific constraints defined in this file and user instructions always override generic rules.

## MCP Tool Priority
- **Priority:** Always prioritize using codebase MCP tools (`codebase-memory-mcp`) over traditional shell commands (`grep`, `find`, `dir`, `ls`, `cat`) or standard tools for discovering code, tracing paths, finding usages, or editing files.
- **Tool Order:**
  1. `search_graph`: Find functions, classes, and variables by name/pattern.
  2. `trace_path`: Trace caller/callee paths.
  3. `get_code_snippet`: Read source code of specific functions/classes.
  4. `query_graph`: Execute Cypher queries for complex patterns.
  5. `get_architecture`: Get high-level project summary.

## Git Guidelines
- Use conventional commits format: `<type>: <description>` (e.g., `feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `perf:`).
- Run and pass all tests before committing.
