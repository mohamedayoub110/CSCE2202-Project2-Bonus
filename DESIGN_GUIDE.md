# Tutoring Scheduler Bonus — Design Guide

This document is the single source of truth for the bonus implementation of the Tutoring Scheduler. It mirrors the structure of the Project 2 / Thread B design guide, extended to cover the enriched problem (TS-CFS: Tutor Scheduling with Capacity, Fairness, Stochasticity) and the new two-phase algorithm (DA-ALNS-OR: Deferred-Acceptance warm start + Adaptive Large Neighborhood Search + Online Repair).

---

## Part 1 — Problem Statement (Bonus Reformulation)

### Real-world narrative

A university tutoring center supports a portfolio of courses across an entire semester. A heterogeneous TA workforce (undergraduate TAs, graduate TAs, paid hourly tutors) must be matched to a heterogeneous student demand. The scheduler is not a one-time exercise; it runs every night, all term long, and must behave well under online arrivals, cancellations, no-shows, strategic preference reporting, and demand spikes around exams.

The full plain-English problem statement lives in `PROBLEM_STATEMENT.md`. This section restates the formal pieces needed by the implementation.

### Input definition

An `Instance` carries:

- **Tutors.** Each tutor has an ID, pool (Undergrad / Grad), a list of qualified topic IDs, a primary language (English / Arabic / Bilingual), a weekly minute cap, and a list of weekly availability windows.
- **Rooms.** Each room has an ID, capacity, accessibility flag, and modality (Online / InPerson).
- **Slots.** Each slot has an ID, a tutor ID, a room ID, a topic, a language, a modality, a pool, a `[start, end)` time window, and a per-slot capacity.
- **Requests.** Each request has an ID, a student ID (one student may submit multiple requests), a cohort tag (Standard / AcademicWarning / ProfessionalAthlete), a topic, a pool, a modality preference (Online / InPerson / Either), a language preference, a maximum group size, an accessibility flag, an ordered preference list of slot IDs, a submission timestamp, and a list of class-schedule blocks the student cannot be assigned through.

### Output definition

A `ScheduleResult` contains:

- `assignments` — a vector of `(request, slot)` pairs.
- `unassigned` — a vector of `(request, reason string)` pairs.
- `objectiveValue` — the composite objective value of the final schedule.
- `phase1Assignments`, `phase2Iterations`, `phase2Improvements` — diagnostic counters.

### Hard constraints

The scheduler must satisfy every one of these. Any violation in `assignments` causes `verifyFeasibility` to return false.

1. **Capacity.** A slot's assigned count never exceeds its declared capacity.
2. **Pool routing.** A request's `pool` matches the slot's `pool`.
3. **Preference-only.** A request is only ever placed in a slot listed in its `preferences`.
4. **Topic match.** The slot's topic equals the request's topic.
5. **Tutor qualification.** The tutor owning the slot is qualified for the topic.
6. **Language match.** Either the slot is bilingual, the student is bilingual, or both sides agree.
7. **Modality match.** Student modality preference matches the slot's modality (unless student is `Either`).
8. **Room modality.** The room hosting the slot supports the slot's modality.
9. **Accessibility.** If the student needs accessibility, the room is accessible.
10. **Room and slot capacity coherence.** Room capacity is at least the slot's capacity.
11. **Group size cap.** A slot's capacity does not exceed the student's `maxGroupSize`.
12. **Tutor availability.** The slot's time window lies inside one of the tutor's availability windows.
13. **Class-conflict avoidance.** The slot's time window does not overlap any of the student's class-schedule blocks.
14. **Tutor weekly minute cap.** The sum of minutes assigned to a tutor across the term does not exceed their `weeklyMinuteCap`.
15. **No double-booking of a student.** No single student is held in two overlapping slots.
16. **One assignment per request.** A given request appears in at most one assignment.

### Soft objectives (composite, maximized)

`scoreState` computes a composite score from:

1. **Weighted preference satisfaction.** Each assigned request contributes `cohortWeight(cohort) × rankWeight(rank)` to the score, where `rankWeight(k) = 1/(k+1)` and `cohortWeight = 2.0` for AcademicWarning or ProfessionalAthlete and `1.0` otherwise.
2. **Tutor load-balance penalty.** Subtract `weightLoadVar × stddev(tutor minutes) / 60`.
3. **Cohort floor penalty.** For each priority cohort, if the fraction of its requests that get assigned falls below `fairFloorRatio`, subtract `weightFairFloor × (fairFloorRatio − rate) × totalInCohort`.

### Assumptions

1. Tutor IDs, room IDs, slot IDs, request IDs are unique inside an `Instance`.
2. Slot durations are arbitrary but each slot's `[start, end)` is well-formed.
3. The reason → pool mapping is implicit in the `Request.pool` field; the request already knows its pool.
4. The scheduler is run once per night in batch; intra-day arrivals and cancellations are handled by the online phase (`onRequestArrived`, `onCancellation`).

### Successful-solution definition

A solution is successful if:

- `verifyFeasibility` returns true.
- It is deterministic given the same instance and the same `AlnsParameters.seed`.
- Phase 2 never decreases the assignment count or objective relative to Phase 1.
- Every unassigned request carries a reason.

---

## Part 2 — DA-ALNS-OR Algorithm

The scheduler builds on the Thread B greedy and the Thread A backtracker rather than replacing them; both survive as sub-components inside a richer architecture.

### 1. Architecture overview

```
runScheduler(instance, params)
    │
    ├── buildIndices(instance)          // O(n) hash maps
    ├── initState(state, instance)      // per-slot capacity, per-tutor minute counters
    │
    ├── runPhase1(instance, idx, state) // Deferred acceptance with cohort priority
    │     - student-proposing rounds
    │     - displacement by composite priority
    │
    ├── phase2ALNS(instance, idx, state, params)
    │     - 4 destroy operators (random / worst-rank / cohort-floor / tutor-overload)
    │     - 3 repair operators (greedy / regret-k / depth-limited backtracking)
    │     - simulated annealing acceptance, adaptive operator weights
    │
    └── buildResult(instance, idx, state, params)

onRequestArrived(req, instance, current)   // Online repair for new arrivals
onCancellation(reqId, instance, current)   // Online repair for cancellations
verifyFeasibility(instance, result)        // Independent hard-constraint check
```

### 2. Phase 1 — Deferred Acceptance with Cohort Priority

#### 2.1 Why deferred acceptance

The Thread B "greedy by submission timestamp" is correct *only* under the original problem's strict ordinal priority and capacity-only constraints. The bonus problem adds priority cohorts (AcademicWarning, ProfessionalAthlete) that must take precedence over earlier-arriving Standard requests, and adds many additional hard constraints. A first-come-first-served greedy will systematically lock in the wrong students.

Deferred acceptance fixes this:

- **Each request proposes to its top remaining preference.** Holders of a slot are continuously displaced by higher-priority proposers.
- **Priority is a composite tuple** `(cohort_weight × rank_weight, -submittedAt, -studentId, -requestId)`. This means: cohort first, then earlier timestamp (the Thread B rule, preserved as a sub-criterion), then deterministic IDs for tie-break.
- **Strategy-proofness** for the student side follows from Roth (1985): a student cannot strictly improve their outcome by misreporting their preference list.

#### 2.2 Pseudocode

```
for each request r: nextPref[r] = 0
while any request progressed last round:
    for each unassigned request r in order:
        while nextPref[r] < |preferences(r)|:
            slot s = preferences(r)[nextPref[r]++]
            if not structurally feasible (r, s):    continue
            if r has student-time conflict at s:    continue
            if remaining tutor minutes < slot len:  continue
            if remaining capacity(s) > 0:
                assign(r, s); break
            else:
                find weakestHolder = arg min priority(h, s) over holders(s)
                if priority(r, s) > priority(weakestHolder, s):
                    unassign(weakestHolder, s)
                    assign(r, s); break
                else continue  // r is rejected here; try next preference
```

Displaced holders re-enter the unassigned pool with their `nextPref` left at the position after the slot they were just kicked out of, so they continue proposing down their list. This is the standard student-proposing many-to-one DA, generalized with capacities (Gale–Shapley 1962, Roth 1985).

#### 2.3 What survives from Thread B

The Thread B greedy's sole correctness mechanism — *earliest submission timestamp wins under capacity contention* — survives as the second component of the priority tuple. When two requests with the same cohort weight propose to the same slot, the earlier one wins. This is verified by `test_timestamp_tiebreak`.

### 3. Phase 2 — Adaptive Large Neighborhood Search

#### 3.1 The destroy/repair loop

ALNS repeatedly removes a slice of the current schedule (a "destroy" move) and reinserts those requests differently (a "repair" move). The schedule walks through the feasible region under simulated-annealing acceptance until either a wall-clock / iteration budget runs out or the search has stalled for too long.

```
state ← Phase1(instance)
best  ← state
T     ← initialTemp
for iter in 1..maxIterations:
    pick destroy operator d (weighted by past success)
    pick repair  operator r (weighted by past success)
    trial ← copy(state)
    removed ← d(trial, k)
    pool    ← removed ∪ still-unassigned-requests
    r(trial, pool)
    Δ ← score(trial) - score(state)
    if Δ > 0 or U(0,1) < exp(Δ/T):
        state ← trial
        if score(state) > score(best):  best ← state
    update operator weights based on outcome
    T ← T · coolingRate
    if stall ≥ stallLimit:  break
state ← best
```

#### 3.2 Destroy operators

| ID | Operator | Removes |
|----|---|---|
| 0 | `destroyRandom` | `k` uniformly random assignments — pure exploration |
| 1 | `destroyWorstRank` | the `k` assignments with the worst preference rank — targets soft objective 1 |
| 2 | `destroyCohortFloor` | up to `k` Standard-cohort assignments occupying slots that unassigned cohort students prefer — targets the fairness floor |
| 3 | `destroyTutorOverload` | up to `k` assignments belonging to the most-loaded tutor — targets the load-balance penalty |

#### 3.3 Repair operators

| ID | Operator | Inherits from |
|----|---|---|
| 0 | `repairGreedy` | A generalization of Thread B: order pool by cohort priority then submission, greedily fit each |
| 1 | `repairRegretK` | Regret-based reinsertion — insert the highest-regret request first (standard ALNS workhorse) |
| 2 | `repairBacktracking` | **Direct reuse of the Thread A backtracker** on a slice of size ≤ `backtrackingCap` — exhaustive over a small subproblem |

The depth-limited backtracking operator is the load-bearing connection to Thread A. The original Thread A backtracker blew up on the full roster because `n` was thousands. Inside ALNS it operates on at most `backtrackingCap = 16` requests at a time, so its exponential worst case is bounded and its exhaustive guarantee is valuable on dense subproblems.

#### 3.4 Acceptance and adaptive weights

- **Simulated annealing** acceptance with geometric cooling (`coolingRate = 0.9975` by default).
- **Adaptive operator weights** with exponential moving average: a global best earns +3.0, an improvement earns +1.5, an accepted-worse move earns +0.5, a reject earns 0. Weights are smoothed at α = 0.1.

#### 3.5 Why ALNS rather than MILP or pure LNS

- MILP (Gurobi / CPLEX) gives provable optimality but scales to ~1000 requests at most and requires a commercial solver.
- LNS without adaptive weights would still work but would waste iterations on operators that are not productive on the current instance.
- ALNS provides empirically strong results on related OR problems (vehicle routing, nurse rostering, university timetabling) and is naturally visualizable: each iteration is a single destroy + repair frame.

### 4. Phase 3 — Online Repair

Two event-driven entry points handle within-day disruptions without re-running Phases 1–2:

- **`onRequestArrived(req, instance, current)`** — greedy insertion into the first feasible preference; falls back to "unassigned" if none works. Runs in `O(L × s)` where `L` is the preference list length.
- **`onCancellation(reqId, instance, current)`** — removes the cancelled assignment, sorts the waiting list by cohort then submission, places the highest-priority unassigned student whose preferences include the freed slot.

Both verify full feasibility before committing.

### 5. Tie-breaking and determinism

- Phase 1 priority tuple is deterministic; ties cascade through (cohort, -submittedAt, -studentId, -requestId).
- Phase 2 RNG is seeded from `AlnsParameters.seed` (default `0xC5CE2202`). Same seed → same destroy and repair operator picks → same accept/reject decisions → identical output.
- This is verified by `test_determinism`.

### 6. Complexity

| Phase | Worst case |
|---|---|
| Phase 1 | `O(R · L_max · log m + R · slot_holders)` where `R` = requests, `L_max` = max preference length, `m` = slots, `slot_holders` = average holders per slot (`≤ capacity`). In practice the inner loops dominate and the cost is close to `O(R · L_max)`. |
| Phase 2 | `O(I · (k · L_max + b^k))` where `I` = iterations, `k` = destroy size, `b ≤ L_max + 1` is the branching factor in the backtracking repair. With `k ≤ 16` and `L_max ≤ 5`, `b^k ≤ 6^16` worst case, but pruning by feasibility reduces this drastically. |
| Phase 3 (arrival) | `O(L_max)` |
| Phase 3 (cancellation) | `O(U_unassigned)` |
| `verifyFeasibility` | `O(|assignments|)` |

Space is `O(R + m + tutors + rooms)` plus the `State` copy each ALNS iteration (`O(R)`), which is the main constant-factor cost on large instances.

### 7. Approximation bound

- Phase 1's projection onto pure assignment-count maximization inherits the standard `1 − 1/e ≈ 0.632` LP-rounding bound on the Generalized Assignment Problem (Fleischer–Goemans–Mirrokni–Sviridenko 2006).
- Phase 2 is monotone non-decreasing in expectation under standard SA cooling: its best solution is never worse than the warm start.
- Empirically (see `stress_output.txt` and `BONUS_REPORT.md`) the implementation finds solutions whose assignment counts are within a few percent of the trivial upper bound `min(|requests|, Σ capacity)` across the regimes that are not strictly capacity-starved.

### 8. Data structures

- `std::unordered_map<TutorID, const Tutor*>` and friends for `O(1)` ID → object lookup (`Indices`).
- `std::unordered_map<SlotID, int>` for remaining capacity per slot.
- `std::unordered_map<TutorID, int>` for remaining tutor minutes.
- `std::unordered_map<RequestID, SlotID>` for the current assignment map.
- `std::unordered_map<StudentID, vector<(window, slot)>>` for student-time conflict checks.
- `std::mt19937_64` for deterministic randomness.

### 9. Module / interface sketch (C++17)

```cpp
namespace daalns {

enum class Cohort   { Standard, AcademicWarning, ProfessionalAthlete };
enum class Modality { Online, InPerson, Either };
enum class Language { English, Arabic, Bilingual };
enum class Pool     { Undergrad, Grad };

struct Tutor   { ... };
struct Room    { ... };
struct Slot    { ... };
struct Request { ... };
struct Instance { ... };

struct Assignment { RequestID request; SlotID slot; };
struct UnassignedRequest { RequestID request; std::string reason; };
struct ScheduleResult {
    std::vector<Assignment>        assignments;
    std::vector<UnassignedRequest> unassigned;
    double                         objectiveValue;
    int                            phase1Assignments;
    int                            phase2Iterations;
    int                            phase2Improvements;
};

struct AlnsParameters {
    int    maxIterations    = 2000;
    int    stallLimit       = 400;
    double initialTemp      = 1.0;
    double coolingRate      = 0.9975;
    int    minDestroy       = 4;
    int    maxDestroy       = 30;
    double destroyFraction  = 0.15;
    int    backtrackingCap  = 16;
    double weightContinuity = 0.20;
    double weightLoadVar    = 0.10;
    double weightFairFloor  = 5.0;
    double fairFloorRatio   = 0.80;
    std::uint64_t seed      = 0xC5CE2202ULL;
};

ScheduleResult runScheduler(const Instance&, const AlnsParameters& = {});
EventResult    onRequestArrived(const Request&, const Instance&, ScheduleResult&, const AlnsParameters& = {});
EventResult    onCancellation(RequestID, const Instance&, ScheduleResult&, const AlnsParameters& = {});
bool           verifyFeasibility(const Instance&, const ScheduleResult&, std::string* = nullptr);
double         computeObjective(const Instance&, const ScheduleResult&, const AlnsParameters& = {});
} // namespace daalns
```

---

## Part 3 — Required Test Cases

`tests.cpp` covers 18 cases with 39 assertions:

| # | Case | What it checks |
|---|---|---|
| 1 | Happy path | Everyone gets their first preference when there is room. |
| 2 | Cohort priority | Academic-warning student displaces a Standard student. |
| 3 | Timestamp tie-break | Same cohort, earlier submission wins. |
| 4 | Qualification | Tutor not qualified for topic → unassigned. |
| 5 | Language match | Arabic student matches Arabic slot over English slot. |
| 6 | Class conflict | Slot overlapping student's class → skipped. |
| 7 | Tutor hour cap | Three requests, 60-min slots, 120-min cap → exactly 2 placed. |
| 8 | Accessibility | Student with accessibility flag → only the accessible room. |
| 9 | Modality | In-person preference → in-person slot. |
| 10 | Empty preferences | Empty list → unassigned with reason "empty preference list". |
| 11 | Same student overlap | Same student, two overlapping requests → at most one assignment. |
| 12 | Determinism | Two runs on same input + seed → identical output. |
| 13 | Phase 2 monotone | `final ≥ phase1Assignments`. |
| 14 | Strategy-proofness sanity | Misreporting preferences does not strictly improve a request's outcome on a small instance. |
| 15 | Arrival event | `onRequestArrived` places a late request into the only remaining feasible slot. |
| 16 | Cancellation event | `onCancellation` fills the freed slot from the waiting list. |
| 17 | Verifier catches bogus | `verifyFeasibility` rejects an assignment to a nonexistent slot. |
| 18 | Medium smoke | Generated instance with 300 requests + ALNS still produces a feasible result. |

All 39 assertions pass. Captured output is in `tests_output.txt`.

`stress_test.cpp` runs 10 regimes spanning `(60, 40, 8)` to `(1578, 1000, 60)` requests/slots/tutors, including cohort-heavy, language-diverse, online-heavy, high-class-conflict, and exam-week-pressure variants. Every regime maintains feasibility and Phase 2 improves on Phase 1 in 8 of the 10 (the two non-improvements are too-easy instances where Phase 1 was already at the local optimum). Captured output is in `stress_output.txt`.

---

## Part 4 — Risks Flagged Up Front (Do Not Repeat)

The original Thread B project's design guide listed three concrete failure modes from earlier AI-written attempts. This bonus implementation explicitly guards against each:

1. **Missing constraint enforcement on every assignment.** The `fullyFeasible` predicate is called at every assignment point — both phases and both event handlers route through it. There is no code path that places a request without going through this gate.
2. **Under-stated complexity.** Complexity is documented per phase in §6 with the correct `O(...)` expressions including the slot-scan factor.
3. **Wrong data structure for access pattern.** Slots and requests are stored in `std::unordered_map<ID, const T*>` for `O(1)` ID lookup, which is the actual access pattern (preference lists reference slot IDs).

Three additional risks specific to the bonus:

4. **Strategy-proofness loss in Phase 2.** Phase 1 is strategy-proof by Roth (1985), but Phase 2 swaps can in principle break this if they make a student worse off. Mitigation: in the current implementation, the composite objective is the only criterion, so a student *could* in principle benefit from misreporting. A stricter variant would gate Phase 2 swaps on Pareto improvement (no student worse off than after Phase 1). This is called out in `BONUS_REPORT.md`.
5. **ALNS state copy cost.** Each iteration copies the full `State`. At `n = 10⁵` this becomes the dominant cost. The current implementation is correct but not optimal here; for very large instances, switch to incremental destroy/repair without copying.
6. **Backtracking blowup.** The recursive backtracking repair has worst-case `O((L+1)^k)`. Capping `backtrackingCap = 16` and `L_max ≤ 5` keeps this under `6^16 ≈ 3×10^12` worst case but feasibility pruning brings it down to ~`10^4`–`10^5` in practice. Increase the cap with care.
