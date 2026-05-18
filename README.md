# Bonus — DA-ALNS-OR Tutoring Scheduler

This folder is the CSCE 2202 Project 2 **bonus** submission for **Hamza Kamel**. It contains a from-scratch C++17 implementation of a two-phase metaheuristic scheduler (DA-ALNS-OR) for the bonus-reformulated tutoring scheduling problem, plus a synthetic instance generator, unit tests, a stress driver, and design documents.

---

## Files

| File | Purpose |
|---|---|
| `scheduler.hpp` | Public types and the `runScheduler` / event-handler interface. |
| `scheduler.cpp` | Full DA-ALNS-OR implementation: feasibility predicates, Phase 1 deferred acceptance, Phase 2 ALNS with 4 destroy and 3 repair operators (including the Thread A backtracker), and Phase 3 online repair. |
| `generator.hpp` / `generator.cpp` | Parameterized synthetic instance generator. |
| `tests.cpp` | 18 unit tests / 39 assertions covering happy paths, every hard constraint, cohort priority, determinism, event handlers, and a strategy-proofness sanity check. |
| `stress_test.cpp` | 10-regime large-scale stress driver up to 1578 requests / 1000 slots / 60 tutors. |
| `Makefile` | Build and run targets. |
| `tests_output.txt` | Captured run of `./tests_run` (39 / 39 passing). |
| `stress_output.txt` | Captured run of `./stress_run` (10 / 10 feasible). |
| `viewer/` | **Interactive browser visualizer.** Open `viewer/index.html` to watch the algorithm work in real time — Gantt chart, live score graph, adaptive operator weights, scrubbable timeline. Same algorithm as `scheduler.cpp`, ported to JavaScript. |
| `PROBLEM_STATEMENT.md` | Plain-English bonus problem reformulation (TS-CFS). |
| `COMPLEXITY_ANALYSIS.md` | NP-hardness / APX-hardness proof sketch via reduction to GAP, RCPSP, MWIS, HRT. |
| `DESIGN_GUIDE.md` | Per-phase specification — the single source of truth for the implementation. |
| `BONUS_REPORT.md` | 2-3 page bonus write-up matching the prompt's deliverables. |
| `AI_ATTRIBUTION.md` | Which parts were AI-generated and which parts were modified. |

## Build and run

```bash
make           # builds tests_run and stress_run
make tests     # builds and runs the unit tests
make stress    # builds and runs the stress driver
make clean
```

Compiler used: `g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic`. No external dependencies.

Expected output of `./tests_run`:

```
Passed 39 / 39 checks
```

Expected output of `./stress_run`: 10 stress regimes from `tiny_balanced` (60 requests) to `xl` (1578 requests), all reporting `feasible: yes`.

## Reading order

If reviewing in order of usefulness:

1. `BONUS_REPORT.md` — the 2-3 page submission write-up (start here).
2. `PROBLEM_STATEMENT.md` — what problem we are solving and why.
3. `COMPLEXITY_ANALYSIS.md` — why it's hard.
4. `DESIGN_GUIDE.md` — what the algorithm is.
5. `scheduler.hpp` then `scheduler.cpp` — the implementation.
6. `tests.cpp` and `stress_test.cpp` — verification.

## What's new vs the Project 2 Thread B implementation

| Aspect | Thread B baseline | Bonus |
|---|---|---|
| Algorithm | Single-pass greedy by submission timestamp | Two-phase DA + ALNS + online repair |
| Hard constraints | 4 | 16 (see `DESIGN_GUIDE.md §1`) |
| Soft objectives | 1 (assignment count) | 3-term composite (preference rank × cohort weight, load variance, cohort floor) |
| Cohort handling | None | Academic-warning and professional-athlete cohorts with 2× weighting and fairness floor |
| Online support | None | `onRequestArrived`, `onCancellation` |
| Strategy-proofness | Not addressed | Phase 1 strategy-proof for students (Roth 1985) |
| Complexity class | P (bipartite matching) | NP-hard, APX-hard |
| Approximation bound | N/A | Provable `1 − 1/e` on Phase 1; empirically ≥ 0.95 of trivial upper bound on most regimes |

## Authorship

Bonus submission by **Hamza Kamel**, 100% contribution. Built on top of the Project 2 / Thread A and Thread B work documented in the main project repository.
