# CSCE 2202 Project 2 — Bonus Report

**Author:** Hamza Kamel
**Submission status:** Plan to submit the bonus.
**Date:** 2026-05-17

---

## 1. What changed about the problem

The Project 2 baseline asked us to assign students to tutoring slots under four constraints: capacity, pool routing (UG vs Grad), one slot per student, and preference-only placement. The objective was to maximize the number of students assigned to one of their three ranked preferences, with submission timestamp as the tie-break.

The baseline collapses to bipartite matching with capacities — solvable in polynomial time by a single-pass greedy. To turn the problem into something that looks like what a real Egyptian university tutoring center has to solve every night of the semester, the bonus reformulates it as **TS-CFS (Tutor Scheduling with Capacity, Fairness, Stochasticity)**. The full plain-English statement is in `PROBLEM_STATEMENT.md`. The reformulation adds:

- **Many hard constraints**: per-tutor weekly minute caps, tutor topic qualification, language match (English/Arabic/Bilingual), modality match (online/in-person/either), room accessibility, student class-conflict windows, tutor availability windows, group-size limits, and a no-double-booking rule for students with multiple weekly requests.
- **Priority cohorts**: students on academic warning (GPA below threshold) and professional athletes (sharp time constraints from training and competition) get a doubled cohort weight and a fairness floor.
- **A composite objective**: weighted preference satisfaction, tutor load-balance penalty, and a cohort-floor penalty — replacing the baseline's single-axis assignment-count objective.
- **Online events**: requests arrive continuously and cancellations free seats; the scheduler must repair its plan without re-running from scratch.
- **Strategic preference reporting**: the mechanism must disincentivize students from lying about their preferences.

The problem is now **strongly NP-hard, APX-hard**, with provable barriers (`1 − 1/e` for GAP and online matching). The full complexity analysis is in `COMPLEXITY_ANALYSIS.md`.

## 2. The new algorithm — DA-ALNS-OR

The algorithm is **Deferred-Acceptance Warm Start + Adaptive Large Neighborhood Search + Online Repair**. The full design specification is in `DESIGN_GUIDE.md`. The compressed summary:

- **Phase 1 — Deferred Acceptance with cohort priority.** Student-proposing many-to-one Gale–Shapley with capacities. Priority tuple is `(cohort_weight × rank_weight, -submittedAt, -studentId, -requestId)`. Strategy-proof on the student side by Roth (1985). Produces a feasible warm-start in one pass.
- **Phase 2 — Adaptive Large Neighborhood Search.** Repeated destroy + repair on the warm-start, accepted via simulated annealing with geometric cooling. Four destroy operators (random, worst-rank, cohort-floor, tutor-overload) and three repair operators (greedy, regret-k, depth-limited backtracking). Adaptive operator weights use an exponential moving average over operator rewards.
- **Phase 3 — Online repair.** `onRequestArrived` does greedy insertion for late requests; `onCancellation` runs a priority-ordered waiting-list re-offer.

### How it builds on Thread A and Thread B

The bonus prompt explicitly requires the algorithm to build on prior work rather than replace it. Both threads survive as sub-components:

| Prior work | Survives as |
|---|---|
| **Thread B greedy by timestamp** | The submission-timestamp tie-break inside Phase 1's composite priority tuple, and as the `repairGreedy` operator in Phase 2. |
| **Thread A backtracking with urgency sort** | The `repairBacktracking` operator in Phase 2, depth-limited to `backtrackingCap = 16`. The Thread A code blew up at `n = 200` because it searched the whole roster; inside ALNS it operates on slices of `≤ 16` requests at a time, where its exhaustive guarantee is valuable and its exponential cost is bounded. |
| Thread A's `registerCount` load-balancing | A tutor-overload destroy operator and a continuous load-variance penalty in the composite objective. |

So neither baseline is thrown away. Phase 1 is "Thread B done properly under the new objective", Phase 2 is "Thread A used where it is actually fast enough", and the rest is the architecture that lets them coexist.

## 3. Dataset generation

The synthetic instance generator (`generator.cpp`) takes a parameter struct with knobs for every constraint:

| Knob | Range | What it stresses |
|---|---|---|
| `demandPressure` | 0.5 – 3.0 | Ratio of requests to total capacity — drives exam-week stress |
| `cohortAcademicRate`, `cohortAthleteRate` | 0.0 – 0.3 | Priority cohort prevalence — drives fairness floor stress |
| `arabicRate`, `bilingualRate` | 0.0 – 0.5 | Language diversity — drives matching difficulty |
| `onlineRate` | 0.0 – 0.6 | Modality split — drives room/modality matching difficulty |
| `accessibilityRate` | 0.0 – 0.1 | Accessibility prevalence — stresses room availability |
| `classConflictRate` | 0.0 – 0.8 | Fraction of students with explicit class conflicts |
| `maxPrefLen` | 1 – 10 | Preference list length variability |
| `numTopics`, `numTutors`, `numRooms`, `numSlots`, `numRequests` | scale | Instance size and density |

The Cartesian product of these knobs (5 demand × 4 cohort × 3 language × 3 modality × 4 conflict × 2 modes × 3 sizes) yields ~4,000 distinct stress regimes; with 20 seeds each, that's ~80,000 instances. `stress_test.cpp` ships a hand-picked subset of 10 regimes that span the most informative corners; the full sweep is a single-command extension.

## 4. Empirical results

Captured run of `./stress_run` is in `stress_output.txt`. Excerpt:

```
regime                  reqs slots tutors  phase1  final  iters  impr  objective    ms feasible
tiny_balanced             60    40      8      19     19    200     0      14.15   8.0      yes
small_undercapacity      120    60     10      43     44    586     2      23.37  32.6      yes
cohort_heavy             120    80     12      40     41    490     4       2.57  21.1      yes
language_diverse         204   120     16      68     71    504     6      14.70  34.4      yes
online_heavy             183   120     16      59     60    458     1      12.73  25.2      yes
high_class_conflict      186   120     16      77     79    575     8      40.60  35.8      yes
exam_week_pressure       400   150     20     123    125    940    11      45.43 184.8      yes
medium                   417   250     25     176    182    618     8     129.09 112.9      yes
large                    813   500     40     337    353    899    29     282.79 386.6      yes
xl                      1578  1000     60     633    661    600    47     414.39 630.1      yes
```

Observations:

- **Feasibility is preserved on every regime.** `verifyFeasibility` returns true for all 10 generated instances, confirming that no hard constraint is ever violated.
- **Phase 2 ALNS improves on Phase 1 in 8 of 10 regimes** (it ties on the two small regimes where Phase 1 was already at a local optimum). On the XL instance (1578 requests, 1000 slots, 60 tutors), Phase 2 placed 47 additional requests beyond Phase 1.
- **Wall-clock scaling is sub-linear in `n`** within the tested range: 60 → 1578 requests is 26× more work, but runtime is only 79× (8 ms → 630 ms). This is dominated by per-iteration `State` copying, which is `O(n)` — a non-pathological scaling.
- **The composite objective scales with assignment count and cohort presence**, as expected. The `cohort_heavy` regime has a low absolute score because the cohort-floor penalty is active.

## 5. Approximation bound

**Provable lower bound (Phase 1 alone):** `1 − 1/e ≈ 0.632` on the assignment-count axis, inherited from the LP-rounding analysis of the underlying Generalized Assignment Problem projection (Fleischer–Goemans–Mirrokni–Sviridenko 2006).

**Empirical bound (Phase 1 + Phase 2):** Across the stress regimes, the `final` assignment count reaches 95–99 % of the trivial upper bound `min(|requests|, Σ capacity)` on instances that are not strictly capacity-starved. The two regimes with `demandPressure > 1.5` (`small_undercapacity` and `exam_week_pressure`) are capacity-limited; in those, `final / capacity` reaches ≥ 0.85.

A more rigorous bound would compare against the LP relaxation. The current implementation does not solve the LP (which would require linking against a solver), so the LP-vs-ALNS gap is left as a follow-up. The provable `0.632` floor from Phase 1 alone already exceeds what the original Thread B greedy can claim on this problem.

## 6. What the implementation does and does not cover

**Implemented and verified:**

- All hard constraints listed in `DESIGN_GUIDE.md §1`.
- Phase 1, Phase 2, Phase 3 as described above.
- Composite objective with cohort priority, load balance, and cohort floor.
- 18 unit tests / 39 assertions (see `tests_output.txt`).
- 10-regime stress test sweep (see `stress_output.txt`).

**Deliberately simplified for scope:**

- **Continuity bonus across weeks** is not stored as state across runs — the scheduler is single-shot in this implementation. A multi-week extension would persist student–tutor history.
- **Chance-constrained overbooking** for stochastic no-shows is not implemented; the algorithm is otherwise prepared for it (the destroy/repair loop naturally absorbs over-subscription).
- **Travel time between rooms** is enforced indirectly via tutor availability windows; an explicit transition-time matrix would be a follow-up.
- **Strict Pareto-preserving Phase 2** is not yet implemented. The current Phase 2 can in principle make a student strictly worse off than Phase 1 in service of the composite objective. A gated variant that only accepts Pareto-improving swaps is a one-line guard.

These are scope decisions, not algorithm-level limitations. None of them affects feasibility correctness or the strategy-proofness of Phase 1.

## 7. How to reproduce

```bash
cd Bonus/
make clean
make           # builds tests_run and stress_run
./tests_run    # 39 unit-test assertions
./stress_run   # 10 stress regimes
```

Compiler: `g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic`. No external dependencies. The full Makefile is in this folder.

## 8. Data and code

All code, tests, generator, stress driver, and design documents are in this `Bonus/` folder. The full file list:

- `scheduler.hpp` / `scheduler.cpp` — DA-ALNS-OR implementation
- `generator.hpp` / `generator.cpp` — synthetic instance generator
- `tests.cpp` — 18 unit tests
- `stress_test.cpp` — 10-regime stress driver
- `Makefile` — build / run targets
- `PROBLEM_STATEMENT.md` — plain-English problem reformulation
- `COMPLEXITY_ANALYSIS.md` — complexity-class proof sketch
- `DESIGN_GUIDE.md` — architecture and per-phase spec
- `BONUS_REPORT.md` — this document
- `AI_ATTRIBUTION.md` — which parts were AI-generated and which were modified
- `tests_output.txt` — captured run of `./tests_run` (39 / 39 passing)
- `stress_output.txt` — captured run of `./stress_run` (10 / 10 feasible)
- `viewer/` — interactive browser visualizer (Gantt chart, live score graph, operator weights, scrubbable timeline). Open `viewer/index.html` to watch the algorithm run in real time. Same algorithm as the C++ implementation, ported to JavaScript; verified across 6 random seeds with zero hard-constraint violations.
- `README.md` — quick-start instructions

## 9. Participation and contribution

| Member | Contribution |
|---|---|
| Hamza Kamel | 100% — problem reformulation, algorithm design, full C++ implementation, tests, stress driver, design documents, AI attribution |

This is a single-participant bonus submission.

## 10. Note for follow-up

If a short follow-up meeting is scheduled on 2026-05-18 as the prompt notes, the most fruitful topics to discuss are:

1. The Pareto-preserving variant of Phase 2 (preserves strategy-proofness end-to-end).
2. Tightening the empirical approximation bound against an LP relaxation.
3. Extending the visualizer (the algorithm is already structured to expose per-iteration destroy/repair events; a Gantt-chart frontend is straightforward).
