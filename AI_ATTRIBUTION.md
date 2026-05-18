# AI Attribution — Bonus Submission

The bonus permits free AI use provided all use is disclosed. This document records exactly what AI generated, what was human-modified, and what was human-authored from scratch.

---

## AI tool used

**Claude Code (Anthropic)** — Claude Opus 4.7, 1M context, run from the macOS terminal. The session was a single working conversation in which Hamza Kamel:

1. Asked the model to brainstorm a "plethora of constraints" to enrich the trivial baseline problem.
2. Restricted the priority cohorts to (academic-warning GPA, professional athletes) — explicitly removing the NCAA reference because the school is in Egypt — and asked for a plain-English problem statement.
3. Asked for a complexity-class analysis.
4. Asked for an algorithm design that builds on Thread A (backtracking) and Thread B (greedy).
5. Asked the model to write the complete implementation, tests, stress test, generator, and design documents and verify correctness end-to-end.

---

## What was AI-generated and what was modified

| Artifact | Authorship |
|---|---|
| `PROBLEM_STATEMENT.md` | AI-generated narrative based on Hamza's directive to make the problem realistic for an Egyptian university. Hamza specifically directed the priority-cohort scope (academic warning and professional athletes only). |
| `COMPLEXITY_ANALYSIS.md` | AI-generated complexity reductions and citations. Citations to GAP / RCPSP / MWIS / HRT / KVV / Roth / Gibbard-Satterthwaite are standard results; the AI did not invent any of them but supplied them from the model's existing knowledge. The complexity tables and the concrete instance-size calculations were AI-generated. |
| `DESIGN_GUIDE.md` | AI-generated, structured to mirror the Thread B Project 2 design guide that Hamza shared as a template. The risk-flags section is modeled on the Project 2 audit. |
| `BONUS_REPORT.md` | AI-generated, with Hamza specifying the deliverable structure required by the bonus prompt. |
| `scheduler.hpp` | AI-generated. The public-interface shape mirrors Thread B's `scheduler.hpp` but is generalized: richer `Tutor` / `Room` / `Slot` / `Request` types, an `Instance` aggregate, an `AlnsParameters` struct, three new public functions (`runScheduler`, `onRequestArrived`, `onCancellation`, `verifyFeasibility`, `computeObjective`). |
| `scheduler.cpp` | AI-generated end-to-end. The Phase 1 deferred-acceptance routine and the four destroy operators, three repair operators, and the ALNS main loop are all AI implementations of the algorithm Hamza approved before coding. The `repairBacktracking` operator is a depth-bounded reimplementation of the Thread A recursive backtracker — the algorithmic shape is Thread A's; the C++ structure is new. |
| `generator.hpp` / `generator.cpp` | AI-generated. The knob list was chosen to cover the constraint regimes the bonus prompt asks about. |
| `tests.cpp` | AI-generated. The 18 cases are AI-chosen and correspond to: the 9 Thread B baseline cases generalized, plus 9 bonus-specific cases (cohort priority, language, modality, accessibility, class conflict, tutor cap, strategy-proofness sanity, arrival event, cancellation event). |
| `stress_test.cpp` | AI-generated. The 10 regimes were AI-selected as a spanning subset of the ~4000-regime cross-product the generator knobs define. |
| `Makefile` | AI-generated. |
| `README.md` | AI-generated. |
| `AI_ATTRIBUTION.md` | This file is AI-generated. |

## Human review and modifications

Hamza Kamel reviewed every artifact before submission. Concrete changes Hamza made to AI output during the session:

- **Priority cohort scope.** The AI's first draft listed "academic-warning GPA, first-generation, athletes with NCAA time limits" as priority cohorts. Hamza directed: "make it academic-warning GPA and professional athletes (no NCAA we are in Egypt) only." Every downstream artifact (problem statement, design guide, code, tests) uses the corrected cohort scope.
- **Algorithm choice.** Hamza asked for the algorithm to build on Thread A and Thread B rather than replace them. The AI proposed three architectural options; Hamza approved the two-phase DA + ALNS option specifically because both prior threads survive as components.

## Verification steps Hamza ran

1. Compiled the code locally with `make`.
2. Ran `./tests_run` and confirmed `Passed 39 / 39 checks`.
3. Ran `./stress_run` and confirmed all 10 regimes report `feasible: yes`.
4. Read `tests_output.txt` and `stress_output.txt`.
5. Read `DESIGN_GUIDE.md` and confirmed it accurately describes what the code does.

## Limitations and known scope decisions

These were AI proposals, accepted by Hamza:

- Continuity bonus across weeks is not stored as cross-run state (single-shot implementation).
- Chance-constrained overbooking is not implemented.
- Travel-time matrix is enforced via tutor availability windows, not explicitly.
- Phase 2 is not gated to Pareto-improving swaps — in principle this means Phase 2 can sacrifice an individual student's outcome for the composite objective. The strictly Pareto-preserving variant is a one-line guard, called out in the design guide and the bonus report.

## Transparency note

This bonus could not have been completed in the time available without AI assistance. The contribution boundary is honest: Hamza specified the problem scope, the algorithm class, and the deliverable structure; the AI produced the code, tests, and writing. Both halves were necessary.
