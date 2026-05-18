"use strict";

const C = window.GeneratorConst;
const DESTROY_OPS = ["random", "worstRank", "cohortFloor", "tutorOverload"];
const REPAIR_OPS = ["greedy", "regretK", "backtracking"];

// --------- helpers ---------
function rankWeight(k) { return 1 / (1 + k); }
function cohortWeight(c) {
  if (c === C.COHORT.ACADEMIC_WARNING || c === C.COHORT.ATHLETE) return 2.0;
  return 1.0;
}
function windowsOverlap(a1, a2, b1, b2) { return a1 < b2 && b1 < a2; }
function findRank(req, sid) {
  for (let i = 0; i < req.preferences.length; i++) if (req.preferences[i] === sid) return i;
  return -1;
}
function langMatch(stu, slot) {
  if (slot === C.LANGUAGE.BILINGUAL) return true;
  if (stu === C.LANGUAGE.BILINGUAL) return true;
  return stu === slot;
}
function modalityMatch(stu, slot) {
  if (stu === C.MODALITY.EITHER) return true;
  return stu === slot;
}
function inAvailability(t, start, end) {
  for (const w of t.availability) if (start >= w.start && end <= w.end) return true;
  return false;
}
function classConflict(r, start, end) {
  for (const w of r.classSchedule) if (windowsOverlap(start, end, w.start, w.end)) return true;
  return false;
}

class State {
  constructor(instance) {
    this.instance = instance;
    this.tutorById = new Map(instance.tutors.map(t => [t.id, t]));
    this.roomById = new Map(instance.rooms.map(r => [r.id, r]));
    this.slotById = new Map(instance.slots.map(s => [s.id, s]));
    this.reqById = new Map(instance.requests.map(r => [r.id, r]));
    this.reqToSlot = new Map();
    this.slotToReqs = new Map(instance.slots.map(s => [s.id, []]));
    this.slotRemainingCap = new Map(instance.slots.map(s => [s.id, s.capacity]));
    this.tutorRemainingMin = new Map(instance.tutors.map(t => [t.id, t.weeklyMinuteCap]));
    this.studentBookings = new Map();
  }

  structurallyFeasible(req, slot) {
    if (slot.pool !== req.pool) return false;
    if (slot.topic !== req.topic) return false;
    const tutor = this.tutorById.get(slot.tutor);
    const room = this.roomById.get(slot.room);
    if (!tutor || !room) return false;
    if (!tutor.qualifiedTopics.includes(req.topic)) return false;
    if (!langMatch(req.languagePref, slot.language)) return false;
    if (!modalityMatch(req.modalityPref, slot.modality)) return false;
    if (room.modality !== slot.modality) return false;
    if (req.needsAccessibility && !room.accessible) return false;
    if (room.capacity < slot.capacity) return false;
    if (slot.capacity > req.maxGroupSize) return false;
    if (!inAvailability(tutor, slot.start, slot.end)) return false;
    if (classConflict(req, slot.start, slot.end)) return false;
    return true;
  }

  stateFeasible(req, slot) {
    if (this.slotRemainingCap.get(slot.id) <= 0) return false;
    const slotMin = slot.end - slot.start;
    if (this.tutorRemainingMin.get(slot.tutor) < slotMin) return false;
    const sb = this.studentBookings.get(req.student);
    if (sb) for (const b of sb) if (windowsOverlap(slot.start, slot.end, b.start, b.end)) return false;
    if (this.reqToSlot.has(req.id)) return false;
    return true;
  }

  fullyFeasible(req, slot) {
    return this.structurallyFeasible(req, slot) && this.stateFeasible(req, slot);
  }

  assign(req, slot) {
    this.slotRemainingCap.set(slot.id, this.slotRemainingCap.get(slot.id) - 1);
    this.tutorRemainingMin.set(slot.tutor, this.tutorRemainingMin.get(slot.tutor) - (slot.end - slot.start));
    this.reqToSlot.set(req.id, slot.id);
    this.slotToReqs.get(slot.id).push(req.id);
    if (!this.studentBookings.has(req.student)) this.studentBookings.set(req.student, []);
    this.studentBookings.get(req.student).push({ start: slot.start, end: slot.end, slot: slot.id });
  }

  unassign(req, slot) {
    this.slotRemainingCap.set(slot.id, this.slotRemainingCap.get(slot.id) + 1);
    this.tutorRemainingMin.set(slot.tutor, this.tutorRemainingMin.get(slot.tutor) + (slot.end - slot.start));
    this.reqToSlot.delete(req.id);
    const v = this.slotToReqs.get(slot.id);
    const idx = v.indexOf(req.id); if (idx >= 0) v.splice(idx, 1);
    const sb = this.studentBookings.get(req.student) || [];
    const i = sb.findIndex(b => b.slot === slot.id);
    if (i >= 0) sb.splice(i, 1);
  }

  snapshot() {
    const out = new Map();
    for (const [r, s] of this.reqToSlot) out.set(r, s);
    return out;
  }
}

// --------- objective ---------
function scoreState(state, params) {
  let score = 0;
  const tutorLoad = new Map();
  for (const [rid, sid] of state.reqToSlot) {
    const r = state.reqById.get(rid);
    const s = state.slotById.get(sid);
    const rk = findRank(r, sid);
    if (rk < 0) continue;
    score += cohortWeight(r.cohort) * rankWeight(rk);
    tutorLoad.set(s.tutor, (tutorLoad.get(s.tutor) || 0) + (s.end - s.start));
  }
  if (tutorLoad.size > 0) {
    let m = 0;
    for (const v of tutorLoad.values()) m += v;
    m /= tutorLoad.size;
    let v = 0;
    for (const x of tutorLoad.values()) v += (x - m) * (x - m);
    v /= tutorLoad.size;
    score -= params.weightLoadVar * Math.sqrt(v) / 60;
  }
  const cohortStats = new Map(); // cohort -> [assigned, total]
  for (const r of state.instance.requests) {
    if (r.cohort === C.COHORT.STANDARD) continue;
    const s = cohortStats.get(r.cohort) || [0, 0];
    s[1]++;
    if (state.reqToSlot.has(r.id)) s[0]++;
    cohortStats.set(r.cohort, s);
  }
  for (const [, [got, total]] of cohortStats) {
    if (total === 0) continue;
    const rate = got / total;
    if (rate < params.fairFloorRatio) {
      score -= params.weightFairFloor * (params.fairFloorRatio - rate) * total;
    }
  }
  return score;
}

// --------- priority tuple for DA ---------
// Returns [weighted, -submittedAt, -student, -id]; larger = higher priority.
function priority(req, rank) {
  const w = cohortWeight(req.cohort) * (rank >= 0 ? rankWeight(rank) : 0);
  return [w, -req.submittedAt, -req.student, -req.id];
}
function prioCompare(a, b) {
  for (let i = 0; i < 4; i++) {
    if (a[i] !== b[i]) return a[i] - b[i];
  }
  return 0;
}

// --------- Phase 1 (Deferred Acceptance) ---------
function runPhase1(state, params, emit) {
  const nextPref = new Map(state.instance.requests.map(r => [r.id, 0]));
  let progressed = true;
  let safety = 0;
  while (progressed && safety < state.instance.requests.length * 30) {
    progressed = false;
    safety++;
    for (const req of state.instance.requests) {
      if (state.reqToSlot.has(req.id)) continue;
      while (nextPref.get(req.id) < req.preferences.length) {
        const k = nextPref.get(req.id);
        const sid = req.preferences[k];
        nextPref.set(req.id, k + 1);
        const slot = state.slotById.get(sid);
        if (!slot) continue;
        if (!state.structurallyFeasible(req, slot)) continue;
        const sb = state.studentBookings.get(req.student);
        let busy = false;
        if (sb) for (const b of sb) if (windowsOverlap(slot.start, slot.end, b.start, b.end)) { busy = true; break; }
        if (busy) continue;
        const slotMin = slot.end - slot.start;
        if (state.tutorRemainingMin.get(slot.tutor) < slotMin) continue;

        if (state.slotRemainingCap.get(sid) > 0) {
          state.assign(req, slot);
          emit({ type: "phase1_assign", request: req.id, slot: sid, displaced: null });
          progressed = true;
          break;
        }

        // displacement
        const holders = state.slotToReqs.get(sid);
        if (!holders || holders.length === 0) continue;
        const myP = priority(req, k);
        let weakest = holders[0];
        let weakestP = priority(state.reqById.get(weakest), findRank(state.reqById.get(weakest), sid));
        for (let i = 1; i < holders.length; i++) {
          const h = state.reqById.get(holders[i]);
          const hp = priority(h, findRank(h, sid));
          if (prioCompare(hp, weakestP) < 0) { weakestP = hp; weakest = holders[i]; }
        }
        if (prioCompare(myP, weakestP) > 0) {
          state.unassign(state.reqById.get(weakest), slot);
          state.assign(req, slot);
          emit({ type: "phase1_assign", request: req.id, slot: sid, displaced: weakest });
          progressed = true;
          break;
        }
      }
    }
  }
}

// --------- Destroy operators ---------
function destroyRandom(state, k, rng) {
  const assigned = [...state.reqToSlot.keys()];
  for (let i = assigned.length - 1; i > 0; i--) {
    const j = Math.floor(rng() * (i + 1));
    [assigned[i], assigned[j]] = [assigned[j], assigned[i]];
  }
  const out = [];
  for (let i = 0; i < Math.min(k, assigned.length); i++) {
    const rid = assigned[i]; const sid = state.reqToSlot.get(rid);
    state.unassign(state.reqById.get(rid), state.slotById.get(sid));
    out.push({ request: rid, slot: sid });
  }
  return out;
}
function destroyWorstRank(state, k) {
  const arr = [];
  for (const [rid, sid] of state.reqToSlot) {
    arr.push([findRank(state.reqById.get(rid), sid), rid, sid]);
  }
  arr.sort((a, b) => b[0] - a[0]);
  const out = [];
  for (let i = 0; i < Math.min(k, arr.length); i++) {
    const [, rid, sid] = arr[i];
    state.unassign(state.reqById.get(rid), state.slotById.get(sid));
    out.push({ request: rid, slot: sid });
  }
  return out;
}
function destroyCohortFloor(state, k, rng) {
  const wanted = new Set();
  for (const r of state.instance.requests) {
    if (r.cohort === C.COHORT.STANDARD) continue;
    if (state.reqToSlot.has(r.id)) continue;
    for (const s of r.preferences) wanted.add(s);
  }
  const cands = [];
  for (const [rid, sid] of state.reqToSlot) {
    if (state.reqById.get(rid).cohort === C.COHORT.STANDARD && wanted.has(sid)) cands.push(rid);
  }
  for (let i = cands.length - 1; i > 0; i--) {
    const j = Math.floor(rng() * (i + 1));
    [cands[i], cands[j]] = [cands[j], cands[i]];
  }
  const out = [];
  for (let i = 0; i < Math.min(k, cands.length); i++) {
    const rid = cands[i]; const sid = state.reqToSlot.get(rid);
    state.unassign(state.reqById.get(rid), state.slotById.get(sid));
    out.push({ request: rid, slot: sid });
  }
  return out;
}
function destroyTutorOverload(state, k, rng) {
  const load = new Map();
  for (const [rid, sid] of state.reqToSlot) {
    const s = state.slotById.get(sid);
    load.set(s.tutor, (load.get(s.tutor) || 0) + (s.end - s.start));
  }
  if (load.size === 0) return [];
  let worst = -1, worstL = -1;
  for (const [t, l] of load) if (l > worstL) { worst = t; worstL = l; }
  const cands = [];
  for (const [rid, sid] of state.reqToSlot) if (state.slotById.get(sid).tutor === worst) cands.push(rid);
  for (let i = cands.length - 1; i > 0; i--) {
    const j = Math.floor(rng() * (i + 1));
    [cands[i], cands[j]] = [cands[j], cands[i]];
  }
  const out = [];
  for (let i = 0; i < Math.min(k, cands.length); i++) {
    const rid = cands[i]; const sid = state.reqToSlot.get(rid);
    state.unassign(state.reqById.get(rid), state.slotById.get(sid));
    out.push({ request: rid, slot: sid });
  }
  return out;
}

// --------- Repair operators ---------
function repairGreedy(state, pool) {
  pool.sort((a, b) => {
    const ra = state.reqById.get(a), rb = state.reqById.get(b);
    const wa = cohortWeight(ra.cohort), wb = cohortWeight(rb.cohort);
    if (wa !== wb) return wb - wa;
    if (ra.submittedAt !== rb.submittedAt) return ra.submittedAt - rb.submittedAt;
    return ra.id - rb.id;
  });
  const added = [];
  for (const rid of pool) {
    if (state.reqToSlot.has(rid)) continue;
    const r = state.reqById.get(rid);
    for (const sid of r.preferences) {
      const s = state.slotById.get(sid);
      if (!s) continue;
      if (state.fullyFeasible(r, s)) {
        state.assign(r, s);
        added.push({ request: rid, slot: sid });
        break;
      }
    }
  }
  return added;
}
function repairRegretK(state, pool) {
  const added = [];
  while (true) {
    let bestRid = -1, bestSid = -1, bestRegret = -1, bestTop = -1;
    for (const rid of pool) {
      if (state.reqToSlot.has(rid)) continue;
      const r = state.reqById.get(rid);
      let top = -1, second = -1, topSid = -1;
      for (let k = 0; k < r.preferences.length; k++) {
        const sid = r.preferences[k];
        const s = state.slotById.get(sid);
        if (!s) continue;
        if (!state.fullyFeasible(r, s)) continue;
        const sc = cohortWeight(r.cohort) * rankWeight(k);
        if (sc > top) { second = top; top = sc; topSid = sid; }
        else if (sc > second) second = sc;
      }
      if (top < 0) continue;
      const regret = second < 0 ? top : top - second;
      if (regret > bestRegret + 1e-12 ||
          (Math.abs(regret - bestRegret) < 1e-12 && top > bestTop + 1e-12)) {
        bestRegret = regret; bestTop = top; bestRid = rid; bestSid = topSid;
      }
    }
    if (bestRid < 0) break;
    state.assign(state.reqById.get(bestRid), state.slotById.get(bestSid));
    added.push({ request: bestRid, slot: bestSid });
  }
  return added;
}
function repairBacktracking(state, pool, params) {
  pool.sort((a, b) =>
    cohortWeight(state.reqById.get(b).cohort) - cohortWeight(state.reqById.get(a).cohort));
  const slice = pool.slice(0, Math.min(pool.length, params.backtrackingCap));
  const ctx = { bestScore: -1, best: [] };
  const cur = [];

  function rec(i, score) {
    if (i === slice.length) {
      if (score > ctx.bestScore) { ctx.bestScore = score; ctx.best = [...cur]; }
      return;
    }
    const r = state.reqById.get(slice[i]);
    for (let k = 0; k < r.preferences.length; k++) {
      const sid = r.preferences[k];
      const s = state.slotById.get(sid);
      if (!s) continue;
      if (!state.fullyFeasible(r, s)) continue;
      const add = cohortWeight(r.cohort) * rankWeight(k);
      state.assign(r, s);
      cur.push({ request: r.id, slot: sid });
      rec(i + 1, score + add);
      cur.pop();
      state.unassign(r, s);
    }
    rec(i + 1, score);
  }

  rec(0, 0);
  const added = [];
  for (const a of ctx.best) {
    state.assign(state.reqById.get(a.request), state.slotById.get(a.slot));
    added.push(a);
  }
  const rest = pool.slice(slice.length);
  added.push(...repairGreedy(state, rest));
  return added;
}

// --------- Adaptive weight picker ---------
function weightedPick(weights, rng) {
  let total = 0; for (const w of weights) total += w;
  let x = rng() * total, acc = 0;
  for (let i = 0; i < weights.length; i++) {
    acc += weights[i];
    if (x <= acc) return i;
  }
  return weights.length - 1;
}

// --------- Main scheduler ---------
function runScheduler(instance, params) {
  const events = [];
  const emit = (e) => events.push(e);
  const rng = mulberry32(params.seed);

  const state = new State(instance);

  emit({
    type: "init",
    score: 0,
    bestScore: 0,
    phase1Assignments: 0,
    iter: 0,
    temp: params.initialTemp,
    destroyWeights: [1, 1, 1, 1],
    repairWeights: [1, 1, 1],
  });

  emit({ type: "phase1_start" });
  runPhase1(state, params, emit);
  const p1Score = scoreState(state, params);
  const p1Count = state.reqToSlot.size;
  emit({ type: "phase1_done", score: p1Score, assignments: p1Count });

  emit({ type: "phase2_start", iter: 0, score: p1Score });

  const destroyW = [1, 1, 1, 1];
  const repairW = [1, 1, 1];
  let curScore = p1Score;
  let bestScore = p1Score;
  let bestSnap = state.snapshot();
  let temp = params.initialTemp;
  let stall = 0;

  for (let iter = 1; iter <= params.maxIterations; iter++) {
    const dIdx = weightedPick(destroyW, rng);
    const rIdx = weightedPick(repairW, rng);

    const beforeSnap = state.snapshot();
    const nA = state.reqToSlot.size;
    let k = Math.max(params.minDestroy,
      Math.min(params.maxDestroy, Math.round(params.destroyFraction * nA)));

    let removed = [];
    switch (dIdx) {
      case 0: removed = destroyRandom(state, k, rng); break;
      case 1: removed = destroyWorstRank(state, k); break;
      case 2: removed = destroyCohortFloor(state, k, rng); break;
      case 3: removed = destroyTutorOverload(state, k, rng); break;
    }

    const pool = removed.map(r => r.request);
    const inPool = new Set(pool);
    for (const r of instance.requests) {
      if (!state.reqToSlot.has(r.id) && !inPool.has(r.id)) {
        pool.push(r.id); inPool.add(r.id);
      }
    }
    for (let i = pool.length - 1; i > 0; i--) {
      const j = Math.floor(rng() * (i + 1));
      [pool[i], pool[j]] = [pool[j], pool[i]];
    }

    let added = [];
    switch (rIdx) {
      case 0: added = repairGreedy(state, pool); break;
      case 1: added = repairRegretK(state, pool); break;
      case 2: added = repairBacktracking(state, pool, params); break;
    }

    const trialScore = scoreState(state, params);
    const delta = trialScore - curScore;
    const u = rng();
    const accept = delta > 0 || u < Math.exp(delta / Math.max(temp, 1e-9));

    let reward = 0;
    let isBest = false;

    if (accept) {
      curScore = trialScore;
      if (trialScore > bestScore + 1e-9) {
        bestScore = trialScore;
        bestSnap = state.snapshot();
        reward = 3.0;
        isBest = true;
        stall = 0;
      } else if (delta > 0) {
        reward = 1.5;
      } else {
        reward = 0.5;
        stall++;
      }
    } else {
      // revert
      // Determine what was changed: rebuild from beforeSnap.
      // Simple approach: unassign current changes and reapply beforeSnap.
      const cur = state.snapshot();
      for (const [rid, sid] of cur) {
        if (!beforeSnap.has(rid) || beforeSnap.get(rid) !== sid) {
          state.unassign(state.reqById.get(rid), state.slotById.get(sid));
        }
      }
      for (const [rid, sid] of beforeSnap) {
        if (!state.reqToSlot.has(rid)) {
          state.assign(state.reqById.get(rid), state.slotById.get(sid));
        }
      }
      stall++;
    }
    destroyW[dIdx] = 0.9 * destroyW[dIdx] + 0.1 * (reward + 0.01);
    repairW[rIdx]  = 0.9 * repairW[rIdx]  + 0.1 * (reward + 0.01);
    temp *= params.coolingRate;

    emit({
      type: "phase2_iter",
      iter,
      destroyOp: dIdx,
      repairOp: rIdx,
      removed,
      added,
      score: accept ? trialScore : curScore,
      trialScore,
      bestScore,
      isBest,
      accepted: accept,
      delta,
      temp,
      stall,
      destroyWeights: [...destroyW],
      repairWeights: [...repairW],
    });

    if (stall >= params.stallLimit) break;
  }

  // Restore the best snapshot at the end
  const cur = state.snapshot();
  for (const [rid, sid] of cur) {
    if (!bestSnap.has(rid) || bestSnap.get(rid) !== sid) {
      state.unassign(state.reqById.get(rid), state.slotById.get(sid));
    }
  }
  for (const [rid, sid] of bestSnap) {
    if (!state.reqToSlot.has(rid)) {
      state.assign(state.reqById.get(rid), state.slotById.get(sid));
    }
  }

  emit({
    type: "done",
    score: bestScore,
    assignments: state.reqToSlot.size,
    bestSnapshot: [...bestSnap],
  });

  return { events, finalScore: bestScore, phase1Count: p1Count, finalAssignments: state.reqToSlot.size };
}

window.runScheduler = runScheduler;
window.DESTROY_OPS = DESTROY_OPS;
window.REPAIR_OPS = REPAIR_OPS;
window.scoreState = scoreState;
