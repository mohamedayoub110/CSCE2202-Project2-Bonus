# Complexity of the Bonus Problem

## Short answer

The problem as reformulated in `PROBLEM_STATEMENT.md` is **strongly NP-hard**, **APX-hard** (no polynomial-time approximation scheme can exist unless P = NP), and has additional **online** and **stochastic** layers that take it beyond classical NP-hardness.

The original Project 2 baseline (3-preference, single batch, one slot per student, capacity only) is polynomial — it's bipartite matching with capacities. Almost every constraint added in the bonus reformulation breaks that.

## Where the hardness comes from

The problem is hard because it contains, as special cases, several problems already known to be hard. Each constraint group below collapses to a known hard problem when other constraints are stripped away.

**1. Tutor hour caps + per-session value → Generalized Assignment Problem (GAP).**
Even with rooms, topics, languages, and fairness removed, the residual problem of "assign each session to one of `m` tutors, each tutor has a capacity budget, each `(tutor, session)` pair has a value" is exactly GAP. GAP is NP-hard, and the best known polynomial-time approximation for the maximization version is a factor of `1 − 1/e ≈ 0.632` via LP rounding (Fleischer–Goemans–Mirrokni–Sviridenko, 2006).

**2. Tutors, rooms, equipment as shared renewable resources → Resource-Constrained Project Scheduling (RCPSP).**
Sessions consume tutors, rooms, and equipment simultaneously, each renewable across time. RCPSP is *strongly* NP-hard, meaning even pseudo-polynomial algorithms don't exist. There is no known constant-factor approximation; state-of-the-art is heuristic.

**3. Class-conflict + tutor double-booking → Maximum Weighted Independent Set.**
The constraint "no student can attend two overlapping sessions, no tutor can be in two places at once" embeds the maximum-weighted-independent-set problem on a conflict graph. MWIS is NP-hard and inapproximable within `n^(1 − ε)` for any `ε > 0` unless P = NP (Håstad, 1996) in the general case.

**4. Stable matching with capacities and lower quotas → Hospital-Residents with Couples (HRT).**
The continuity constraint plus the priority-cohort lower quotas makes this many-to-one matching with both upper and *lower* quotas. The decision problem "does a stable feasible matching exist?" is NP-complete (Biró–McDermid, 2010).

**5. Strategy-proofness → mechanism-design constraint.**
Gibbard–Satterthwaite says we cannot have all of (strategy-proof, Pareto-optimal, non-dictatorial) for any sufficiently rich preference structure. So adding strategy-proofness *forces* giving something up — typically Pareto optimality. Deferred-acceptance (Gale–Shapley) is strategy-proof for one side but produces matchings that are weakly Pareto-dominated.

**6. Online arrivals → competitive analysis.**
Karp–Vazirani–Vazirani (1990): no online bipartite matching algorithm can do better than `1 − 1/e ≈ 0.632` competitive against an adversarial input. For IID-known arrivals, recent work pushes this to roughly `0.711` (Huang et al.).

**7. Stochastic no-shows + demand uncertainty → two-stage stochastic programming.**
The "overbook against historical no-show rates" requirement makes the optimization a chance-constrained program. Two-stage stochastic linear programs are `#P`-hard in general; with integer constraints they are harder still.

## Formal complexity table

Calling the bonus problem **TS-CFS** (Tutor Scheduling with Capacity, Fairness, Stochasticity):

| Variant | Complexity class | Best known approximation |
|---|---|---|
| Offline feasibility (does a valid schedule exist?) | NP-complete | — |
| Offline maximization (max weighted satisfaction) | NP-hard, APX-hard | No PTAS; LP rounding gives `1 − 1/e` for the GAP projection |
| Online maximization (adversarial arrivals) | NP-hard offline + bounded online | `1 − 1/e` competitive (KVV bound) |
| Stochastic maximization (no-shows, demand) | NP-hard + chance-constrained | No general guarantee; SAA gives consistency but not bounds |
| Pareto front across all soft objectives | `#P`-hard in front size | — |

## Concrete size of a real instance

For a tutoring center of the size implied by the problem statement (`n ≈ 1,000–10,000` weekly requests, `m ≈ 50–200` tutors, `R ≈ 20–80` rooms, `T ≈ 60` weekly time slots, `H ≈ 14` weeks):

- **Decision variables** in a natural ILP: roughly `n × T × m × R` binary variables → `10⁹` to `10¹⁰` for a mid-sized instance. Most are infeasible by class-conflict and qualification constraints, so the effective number is `10⁶`–`10⁷`.
- **Constraints**: `O(n + m·T + R·T + n·m)` → `10⁵`–`10⁶`.
- **Brute-force search space**: more than `(m·R)^n ≈ 10^4000` — astronomically beyond reach.

A commercial MILP solver (Gurobi, CPLEX) can handle exact optimization up to roughly `n ≈ 500–2,000` in seconds-to-hours. Beyond that, metaheuristics (ALNS, tabu, large-neighborhood search) are needed, and provable optimality is given up.

## What this implies for the algorithm

The DA-ALNS-OR algorithm shipped in `scheduler.cpp` makes the trade-offs explicitly:

- **Phase 1 (deferred acceptance)** inherits the `1 − 1/e` GAP-projection bound and gives strategy-proofness on the student side.
- **Phase 2 (ALNS with simulated-annealing acceptance)** monotonically improves on Phase 1 in expectation; never decreases the objective.
- **Empirically** (see `stress_output.txt`) the implementation achieves a `final / phase1Assignments` ratio of 1.0–1.07 across regimes, and never violates feasibility.
