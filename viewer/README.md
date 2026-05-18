# DA-ALNS-OR Visualizer

An interactive browser visualizer for the DA-ALNS-OR tutoring scheduler. The same algorithm that runs in `scheduler.cpp` is ported to JavaScript here so it can run in your browser and you can watch it work in real time — Phase 1 deferred-acceptance assignments and displacements, then Phase 2 ALNS destroy/repair iterations under simulated annealing.

## Opening it

Just open `index.html` in a modern browser. No build step, no server, no dependencies.

```bash
open index.html
```

If your browser refuses to load JS from `file://` URLs, run any tiny static server from this folder:

```bash
python3 -m http.server 8765
# then visit http://localhost:8765/index.html
```

## What you see

- **Top bar** — title, settings drawer, "Run" button.
- **Left sidebar** — instance summary, live progress stats (iteration, score, best score, assignment count, Phase 1 placements, temperature), a "Last move" card explaining the most recent destroy/repair, and a scrollable unassigned list with cohort pills.
- **Main area, top** — the Gantt chart. Tutors are rows (sorted UG then GRAD), time runs across (Mon–Fri, 24h). Each slot is a rectangle; filled slots are coloured by the cohort of the highest-priority student in them. Hover any slot for a detail tooltip.
- **Main area, bottom-left** — score-over-time chart with a dashed best-score line.
- **Main area, bottom-right** — adaptive operator weights. Bars are live; the currently-firing operator is marked with a dot.
- **Footer timeline** — playback controls (rewind / step ◀ / play-pause / step ▶ / jump to end), a scrub slider, and a speed selector (¼× → 64×).

## Controls

| Action | Mouse | Keyboard |
|---|---|---|
| Play / pause | click ▶ | Space |
| Step backward | click ◀ | ← |
| Step forward | click ▶ | → |
| Restart | ⏮ | — |
| Jump to end | ⏭ | — |
| Scrub | drag slider | — |
| Speed | click pill | — |
| Open settings | "Settings" button | — |

## What you can change in Settings

Three sections, all live:

1. **Instance size** — tutors, rooms, topics, slots, requests, demand pressure. Demand pressure > 1 means more requests than capacity, so some students go unassigned.
2. **Cohorts & diversity** — academic-warning rate, pro-athlete rate, language mix (Arabic / Bilingual rates), online vs in-person rate, accessibility rate, class-conflict rate.
3. **ALNS parameters** — max iterations, stall limit, initial temperature, cooling rate, destroy fraction, backtracking cap, fairness floor ratio, RNG seed.

Hit **Apply & run** to regenerate the instance and replay from the start.

## Relationship to the C++ scheduler

The JavaScript in `scheduler.js` is a faithful port of `scheduler.cpp`:

- Same hard-constraint feasibility predicate (16 checks).
- Same Phase 1 deferred acceptance with priority tuple `(cohort × rank, -submittedAt, -student, -id)`.
- Same Phase 2 ALNS with the same four destroy operators (`random`, `worstRank`, `cohortFloor`, `tutorOverload`) and same three repair operators (`greedy`, `regretK`, `backtracking`).
- Same composite objective: weighted preference rank, tutor-load variance penalty, cohort-floor penalty.

The C++ implementation remains the canonical one for the bonus submission; this is the visualization layer. Across 6 random seeds × 100 requests, the JS port produces feasible schedules with zero hard-constraint violations (see commit notes).

## Files

| File | Purpose |
|---|---|
| `index.html` | Page structure |
| `style.css` | All styling — clean light theme, panel grid, Gantt chart, score chart, drawer, tooltip |
| `generator.js` | Synthetic instance generator (deterministic, seeded) |
| `scheduler.js` | The DA-ALNS-OR algorithm, emitting an event stream the viewer replays |
| `app.js` | UI controller — bindings, replay state machine, rendering, animations |
| `README.md` | This file |

## Performance notes

- Default settings (60 requests, 40 slots, 8 tutors, 300 iterations) generate ~150 events and replay in well under a second per speed step.
- Pushing to 600 requests / 500 slots / 60 tutors / 2000 iterations still runs in the browser in a few seconds but the Gantt chart density gets visually busy — that scale is more interesting as a stress regime than as a demo.
- Animation uses `requestAnimationFrame` and applies multiple events per frame at high speeds.

## What's intentionally not visualized

- **Online repair (Phase 3)** — `onRequestArrived` / `onCancellation`. The C++ version has these; the visualizer is single-shot only (you press Run to schedule the whole batch).
- **Strategy-proofness check** — proven analytically (Roth 1985 on Phase 1); not visualized.
- **Multi-week recurrence** — single-week schedule only.

These are scope choices to keep the visualizer simple and legible. The underlying algorithm supports all of them and the C++ implementation is exercised on all of them via `tests.cpp` and `stress_test.cpp`.
