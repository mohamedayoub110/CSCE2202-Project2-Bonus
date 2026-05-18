"use strict";

const C = window.GeneratorConst;
const TUTOR_ROW_H = 28;
const SLOT_HEIGHT = 22;
const PX_PER_MIN = 0.55;     // width per minute in the Gantt body
const DAY_MIN = 24 * 60;
const VIS_DAYS = 5;          // Mon..Fri
const TOTAL_MIN = VIS_DAYS * DAY_MIN;

const COHORT_NAME = ["Standard", "Acad-warning", "Pro athlete"];
const COHORT_PILL = ["cp-std", "cp-aw", "cp-pa"];
const COHORT_SLOT = ["slot-cohort-std", "slot-cohort-aw", "slot-cohort-pa"];

// -------- DOM refs --------
const $ = (id) => document.getElementById(id);
const dom = {
  runBtn: $("runBtn"),
  settingsBtn: $("settingsBtn"),
  closeSettings: $("closeSettings"),
  settingsDrawer: $("settingsDrawer"),
  resetCfg: $("resetCfg"),
  applyAndRun: $("applyAndRun"),

  instanceBadge: $("instanceBadge"),
  phaseBadge: $("phaseBadge"),
  statTutors: $("statTutors"),
  statSlots: $("statSlots"),
  statRooms: $("statRooms"),
  statRequests: $("statRequests"),
  statDemand: $("statDemand"),
  statIter: $("statIter"),
  statIterMax: $("statIterMax"),
  statScore: $("statScore"),
  statBest: $("statBest"),
  statAssigned: $("statAssigned"),
  statTotal: $("statTotal"),
  statP1: $("statP1"),
  statTemp: $("statTemp"),

  lastMoveCard: $("lastMoveCard"),
  unassignedList: $("unassignedList"),
  unassignedBadge: $("unassignedBadge"),

  ganttHeader: $("ganttHeader"),
  ganttTutorList: $("ganttTutorList"),
  ganttBody: $("ganttBody"),

  scoreChart: $("scoreChart"),
  scoreLegend: $("scoreLegend"),
  destroyBars: $("destroyBars"),
  repairBars: $("repairBars"),

  tlSlider: $("tlSlider"),
  tlPos: $("tlPos"),
  tlMax: $("tlMax"),
  playBtn: $("playBtn"),
  playIcon: $("playIcon"),
  rewindBtn: $("rewindBtn"),
  stepBackBtn: $("stepBackBtn"),
  stepFwdBtn: $("stepFwdBtn"),
  endBtn: $("endBtn"),
  speedPills: $("speedPills"),
  tooltip: $("tooltip"),
};

// -------- App state --------
const app = {
  instance: null,
  events: [],
  // Replay state — recomputed by replaying events up to `cursor`.
  state: {
    reqToSlot: new Map(),
    score: 0,
    bestScore: 0,
    iter: 0,
    phase: 0,
    temp: 0,
    destroyW: [1, 1, 1, 1],
    repairW: [1, 1, 1],
    lastEvent: null,
    scoreHistory: [],
    bestHistory: [],
  },
  cursor: 0,                 // index of next event to apply
  playing: false,
  speed: 4,
  lastTick: 0,
  changedSlots: new Set(),   // slots that just changed (for animation)
  removedSlots: new Set(),
  rafHandle: 0,
};

// -------- Defaults --------
const DEFAULTS = {
  numTutors: 8, numRooms: 6, numTopics: 4, numSlots: 40, numRequests: 60,
  demandPressure: 1.6,
  academicWarningRate: 0.20, athleteRate: 0.10,
  arabicRate: 0.30, bilingualRate: 0.10,
  onlineRate: 0.25, accessibilityRate: 0.05, classConflictRate: 0.40,
  maxIterations: 300, stallLimit: 80,
  initialTemp: 1.0, coolingRate: 0.99,
  minDestroy: 3, maxDestroy: 20, destroyFraction: 0.15,
  backtrackingCap: 8,
  weightLoadVar: 0.10, weightFairFloor: 5.0, fairFloorRatio: 0.80,
  seed: 42,
};

function readSettings() {
  const num = (id, fb) => {
    const v = parseFloat($(id).value);
    return Number.isFinite(v) ? v : fb;
  };
  return {
    numTutors:           Math.round(num("cfgTutors", DEFAULTS.numTutors)),
    numRooms:            Math.round(num("cfgRooms", DEFAULTS.numRooms)),
    numTopics:           Math.round(num("cfgTopics", DEFAULTS.numTopics)),
    numSlots:            Math.round(num("cfgSlots", DEFAULTS.numSlots)),
    numRequests:         Math.round(num("cfgRequests", DEFAULTS.numRequests)),
    demandPressure:      num("cfgDemand", DEFAULTS.demandPressure),
    academicWarningRate: num("cfgAW", DEFAULTS.academicWarningRate),
    athleteRate:         num("cfgPA", DEFAULTS.athleteRate),
    arabicRate:          num("cfgArabic", DEFAULTS.arabicRate),
    bilingualRate:       num("cfgBilingual", DEFAULTS.bilingualRate),
    onlineRate:          num("cfgOnline", DEFAULTS.onlineRate),
    accessibilityRate:   num("cfgAccess", DEFAULTS.accessibilityRate),
    classConflictRate:   num("cfgConflict", DEFAULTS.classConflictRate),
    maxIterations:       Math.round(num("cfgMaxIter", DEFAULTS.maxIterations)),
    stallLimit:          Math.round(num("cfgStall", DEFAULTS.stallLimit)),
    initialTemp:         num("cfgTemp", DEFAULTS.initialTemp),
    coolingRate:         num("cfgCool", DEFAULTS.coolingRate),
    backtrackingCap:     Math.round(num("cfgBTCap", DEFAULTS.backtrackingCap)),
    fairFloorRatio:      num("cfgFairFloor", DEFAULTS.fairFloorRatio),
    minDestroy:          DEFAULTS.minDestroy,
    maxDestroy:          DEFAULTS.maxDestroy,
    destroyFraction:     DEFAULTS.destroyFraction,
    weightLoadVar:       DEFAULTS.weightLoadVar,
    weightFairFloor:     DEFAULTS.weightFairFloor,
    seed:                Math.round(num("cfgSeed", DEFAULTS.seed)),
  };
}
function applyDefaultsToForm() {
  $("cfgTutors").value = DEFAULTS.numTutors;
  $("cfgRooms").value = DEFAULTS.numRooms;
  $("cfgTopics").value = DEFAULTS.numTopics;
  $("cfgSlots").value = DEFAULTS.numSlots;
  $("cfgRequests").value = DEFAULTS.numRequests;
  $("cfgDemand").value = DEFAULTS.demandPressure;
  $("cfgAW").value = DEFAULTS.academicWarningRate;
  $("cfgPA").value = DEFAULTS.athleteRate;
  $("cfgArabic").value = DEFAULTS.arabicRate;
  $("cfgBilingual").value = DEFAULTS.bilingualRate;
  $("cfgOnline").value = DEFAULTS.onlineRate;
  $("cfgAccess").value = DEFAULTS.accessibilityRate;
  $("cfgConflict").value = DEFAULTS.classConflictRate;
  $("cfgMaxIter").value = DEFAULTS.maxIterations;
  $("cfgStall").value = DEFAULTS.stallLimit;
  $("cfgTemp").value = DEFAULTS.initialTemp;
  $("cfgCool").value = DEFAULTS.coolingRate;
  $("cfgDestroyFrac").value = DEFAULTS.destroyFraction;
  $("cfgBTCap").value = DEFAULTS.backtrackingCap;
  $("cfgFairFloor").value = DEFAULTS.fairFloorRatio;
  $("cfgSeed").value = DEFAULTS.seed;
}

// -------- Run --------
function runFresh() {
  const params = readSettings();
  app.instance = generateInstance(params);
  const result = runScheduler(app.instance, params);
  app.events = result.events;
  resetReplay();
  buildGanttSkeleton();
  setPlaying(true);
}

function resetReplay() {
  app.cursor = 0;
  app.state.reqToSlot.clear();
  app.state.score = 0;
  app.state.bestScore = 0;
  app.state.iter = 0;
  app.state.phase = 0;
  app.state.temp = 0;
  app.state.destroyW = [1, 1, 1, 1];
  app.state.repairW = [1, 1, 1];
  app.state.lastEvent = null;
  app.state.scoreHistory = [];
  app.state.bestHistory = [];
  app.changedSlots.clear();
  app.removedSlots.clear();
  dom.tlSlider.min = 0;
  dom.tlSlider.max = Math.max(0, app.events.length - 1);
  dom.tlSlider.value = 0;
  dom.tlMax.textContent = app.events.length - 1;
  dom.tlPos.textContent = 0;
}

function applyEvent(e) {
  app.state.lastEvent = e;
  switch (e.type) {
    case "init": {
      app.state.phase = 0;
      app.state.score = 0;
      app.state.bestScore = 0;
      app.state.temp = e.temp;
      app.state.destroyW = e.destroyWeights.slice();
      app.state.repairW = e.repairWeights.slice();
      app.state.scoreHistory.push(0);
      app.state.bestHistory.push(0);
      break;
    }
    case "phase1_start":
      app.state.phase = 1;
      break;
    case "phase1_assign": {
      if (e.displaced != null) {
        const prevSlot = app.state.reqToSlot.get(e.displaced);
        app.state.reqToSlot.delete(e.displaced);
        if (prevSlot != null) app.changedSlots.add(prevSlot);
      }
      app.state.reqToSlot.set(e.request, e.slot);
      app.changedSlots.add(e.slot);
      break;
    }
    case "phase1_done": {
      app.state.score = e.score;
      app.state.bestScore = e.score;
      app.state.scoreHistory.push(e.score);
      app.state.bestHistory.push(e.score);
      break;
    }
    case "phase2_start":
      app.state.phase = 2;
      break;
    case "phase2_iter": {
      // remove
      for (const r of e.removed) {
        app.state.reqToSlot.delete(r.request);
        app.removedSlots.add(r.slot);
      }
      // add
      if (e.accepted) {
        for (const a of e.added) {
          app.state.reqToSlot.set(a.request, a.slot);
          app.changedSlots.add(a.slot);
        }
      } else {
        // reverting — re-apply removals (the request returns to its prior slot)
        // We don't have the "previous state" recorded; instead we use the score.
        // The viewer treats rejected iterations as "destroy then reject" — we
        // simply re-add the removed assignments back.
        for (const r of e.removed) {
          app.state.reqToSlot.set(r.request, r.slot);
          app.changedSlots.add(r.slot);
        }
      }
      app.state.iter = e.iter;
      app.state.score = e.score;
      app.state.bestScore = e.bestScore;
      app.state.temp = e.temp;
      app.state.destroyW = e.destroyWeights.slice();
      app.state.repairW = e.repairWeights.slice();
      app.state.scoreHistory.push(e.score);
      app.state.bestHistory.push(e.bestScore);
      break;
    }
    case "done":
      app.state.phase = 3;
      // Force-sync to best snapshot
      app.state.reqToSlot.clear();
      for (const [r, s] of e.bestSnapshot) app.state.reqToSlot.set(r, s);
      app.state.score = e.score;
      app.state.bestScore = e.score;
      break;
  }
}

function seekTo(targetCursor) {
  // To support backward scrubbing, rebuild from scratch.
  if (targetCursor < app.cursor) {
    app.cursor = 0;
    app.state.reqToSlot.clear();
    app.state.scoreHistory = [];
    app.state.bestHistory = [];
    app.state.lastEvent = null;
  }
  while (app.cursor < targetCursor && app.cursor < app.events.length) {
    applyEvent(app.events[app.cursor]);
    app.cursor++;
  }
}

// -------- Gantt skeleton --------
function buildGanttSkeleton() {
  // Header (days)
  dom.ganttHeader.innerHTML = "";
  dom.ganttHeader.style.width = (TOTAL_MIN * PX_PER_MIN) + "px";
  for (let d = 0; d < VIS_DAYS; d++) {
    const x = d * DAY_MIN * PX_PER_MIN;
    const marker = document.createElement("div");
    marker.className = "day-marker";
    marker.style.left = x + "px";
    dom.ganttHeader.appendChild(marker);
    const label = document.createElement("div");
    label.className = "day-label";
    label.style.left = x + "px";
    label.textContent = ["Mon", "Tue", "Wed", "Thu", "Fri"][d];
    dom.ganttHeader.appendChild(label);
  }

  // Tutor list (Y axis)
  dom.ganttTutorList.innerHTML = "";
  const sortedTutors = [...app.instance.tutors].sort((a, b) => {
    if (a.pool !== b.pool) return a.pool - b.pool;
    return a.id - b.id;
  });
  for (const t of sortedTutors) {
    const row = document.createElement("div");
    row.className = "tutor-row";
    const id = document.createElement("span");
    id.className = "tutor-id";
    id.textContent = "T" + t.id;
    row.appendChild(id);
    const tag = document.createElement("span");
    tag.className = "tutor-tag " + (t.pool === C.POOL.UNDERGRAD ? "tag-ug" : "tag-grad");
    tag.textContent = t.pool === C.POOL.UNDERGRAD ? "UG" : "GRAD";
    row.appendChild(tag);
    const lang = document.createElement("span");
    lang.className = "tutor-tag " + (t.language === C.LANGUAGE.ARABIC ? "tag-ar" :
                                      t.language === C.LANGUAGE.BILINGUAL ? "tag-bi" : "tag-en");
    lang.textContent = t.language === C.LANGUAGE.ARABIC ? "AR" :
                       t.language === C.LANGUAGE.BILINGUAL ? "BI" : "EN";
    row.appendChild(lang);
    row.dataset.tutorId = t.id;
    dom.ganttTutorList.appendChild(row);
  }

  // Body — pre-create one cell per slot
  dom.ganttBody.innerHTML = "";
  dom.ganttBody.style.width = (TOTAL_MIN * PX_PER_MIN) + "px";
  // Zebra rows
  for (let i = 0; i < sortedTutors.length; i++) {
    const band = document.createElement("div");
    band.className = "tutor-band-row" + (i % 2 === 0 ? " zebra" : "");
    band.style.top = (i * TUTOR_ROW_H) + "px";
    band.style.height = TUTOR_ROW_H + "px";
    dom.ganttBody.appendChild(band);
  }
  const tutorIndex = new Map(sortedTutors.map((t, i) => [t.id, i]));
  const minDuration = 30; // px-min for very short slots
  app.tutorIndex = tutorIndex;

  for (const slot of app.instance.slots) {
    const idx = tutorIndex.get(slot.tutor);
    if (idx == null) continue;
    const cell = document.createElement("div");
    cell.className = "slot-cell";
    cell.dataset.slotId = slot.id;
    const x = slot.start * PX_PER_MIN;
    const w = Math.max(minDuration, (slot.end - slot.start) * PX_PER_MIN) - 2;
    cell.style.left = x + "px";
    cell.style.top = (idx * TUTOR_ROW_H + (TUTOR_ROW_H - SLOT_HEIGHT) / 2) + "px";
    cell.style.width = w + "px";
    cell.textContent = "";
    cell.addEventListener("mouseenter", (ev) => showSlotTooltip(ev, slot));
    cell.addEventListener("mousemove", (ev) => positionTooltip(ev));
    cell.addEventListener("mouseleave", hideTooltip);
    dom.ganttBody.appendChild(cell);
  }
  dom.ganttBody.style.height = (sortedTutors.length * TUTOR_ROW_H) + "px";
  dom.ganttTutorList.style.height = (sortedTutors.length * TUTOR_ROW_H) + "px";
}

function updateGantt() {
  // Update each slot cell based on current reqToSlot
  const inverse = new Map();   // slotId -> list of requestId
  for (const [rid, sid] of app.state.reqToSlot) {
    if (!inverse.has(sid)) inverse.set(sid, []);
    inverse.get(sid).push(rid);
  }
  const cells = dom.ganttBody.querySelectorAll(".slot-cell");
  for (const cell of cells) {
    const sid = parseInt(cell.dataset.slotId, 10);
    const reqs = inverse.get(sid);
    cell.classList.remove("slot-changed", "slot-removed");
    if (app.changedSlots.has(sid)) cell.classList.add("slot-changed");
    if (app.removedSlots.has(sid) && (!reqs || reqs.length === 0)) cell.classList.add("slot-removed");
    cell.classList.remove("slot-cohort-std", "slot-cohort-aw", "slot-cohort-pa", "slot-filled");
    if (reqs && reqs.length) {
      // colour by the highest-cohort student in the slot
      let topCohort = 0;
      for (const rid of reqs) {
        const r = app.instance.requests[rid];
        if (r.cohort > topCohort) topCohort = r.cohort;
      }
      cell.classList.add("slot-filled", COHORT_SLOT[topCohort]);
      cell.textContent = reqs.length > 1 ? `${reqs.length}` : "R" + reqs[0];
    } else {
      cell.textContent = "";
    }
  }
  app.changedSlots.clear();
  app.removedSlots.clear();

  // Update per-tutor load bars (overlay would be nice, but use tutor list row coloring)
  const tutorLoad = new Map();
  for (const [rid, sid] of app.state.reqToSlot) {
    const s = app.instance.slots[sid];
    tutorLoad.set(s.tutor, (tutorLoad.get(s.tutor) || 0) + (s.end - s.start));
  }
  const rows = dom.ganttTutorList.querySelectorAll(".tutor-row");
  for (const row of rows) {
    const tid = parseInt(row.dataset.tutorId, 10);
    const t = app.instance.tutors[tid];
    const load = tutorLoad.get(tid) || 0;
    const pct = Math.min(100, (load / t.weeklyMinuteCap) * 100);
    let bar = row.querySelector(".tutor-load-bar");
    if (!bar) {
      bar = document.createElement("span");
      bar.className = "tutor-load-bar";
      bar.innerHTML = "<div></div>";
      row.appendChild(bar);
    }
    bar.firstChild.style.width = pct + "%";
  }
}

// -------- Stats panels --------
function updateStats() {
  const total = app.instance.requests.length;
  const assigned = app.state.reqToSlot.size;
  dom.statTutors.textContent = app.instance.tutors.length;
  dom.statSlots.textContent = app.instance.slots.length;
  dom.statRooms.textContent = app.instance.rooms.length;
  dom.statRequests.textContent = total;
  const cap = app.instance.slots.reduce((s, x) => s + x.capacity, 0);
  dom.statDemand.textContent = (total / cap).toFixed(2) + "×";
  dom.instanceBadge.textContent = "ready";

  dom.statIter.textContent = app.state.iter;
  dom.statIterMax.textContent = app.events.length ? (
    app.events.filter(e => e.type === "phase2_iter").length
  ) : 0;
  dom.statScore.textContent = app.state.score.toFixed(2);
  dom.statBest.textContent = app.state.bestScore.toFixed(2);
  dom.statAssigned.textContent = assigned;
  dom.statTotal.textContent = total;
  dom.statTemp.textContent = app.state.temp ? app.state.temp.toFixed(3) : "—";

  let p1count = 0;
  for (const e of app.events) {
    if (e.type === "phase1_done") { p1count = e.assignments; break; }
  }
  dom.statP1.textContent = p1count;

  // phase badge
  dom.phaseBadge.classList.remove("phase-idle", "phase-1", "phase-2", "phase-done");
  if (app.state.phase === 0) { dom.phaseBadge.textContent = "init"; dom.phaseBadge.classList.add("phase-idle"); }
  else if (app.state.phase === 1) { dom.phaseBadge.textContent = "phase 1 — DA"; dom.phaseBadge.classList.add("phase-1"); }
  else if (app.state.phase === 2) { dom.phaseBadge.textContent = "phase 2 — ALNS"; dom.phaseBadge.classList.add("phase-2"); }
  else { dom.phaseBadge.textContent = "done"; dom.phaseBadge.classList.add("phase-done"); }
}

function updateLastMove() {
  const e = app.state.lastEvent;
  if (!e) { dom.lastMoveCard.innerHTML = '<p class="muted">Waiting for first event…</p>'; return; }
  if (e.type === "phase1_assign") {
    let html = `<div class="row"><span>Phase 1</span><span>DA proposal</span></div>`;
    html += `<div class="row"><span>Request</span><span>R${e.request} → slot ${e.slot}</span></div>`;
    if (e.displaced != null) html += `<div class="row"><span>Displaced</span><span>R${e.displaced}</span></div>`;
    dom.lastMoveCard.innerHTML = html;
  } else if (e.type === "phase1_done") {
    dom.lastMoveCard.innerHTML = `<div class="row"><span>Phase 1</span><span>complete</span></div>
      <div class="row"><span>Placed</span><span>${e.assignments}</span></div>
      <div class="row"><span>Score</span><span>${e.score.toFixed(2)}</span></div>`;
  } else if (e.type === "phase2_iter") {
    const dStr = DESTROY_OPS[e.destroyOp];
    const rStr = REPAIR_OPS[e.repairOp];
    const cls = e.delta > 0 ? "delta-positive" : (e.delta < 0 ? "delta-negative" : "delta-neutral");
    const sign = e.delta > 0 ? "+" : "";
    const result = e.isBest ? "★ new best" :
                   e.accepted ? (e.delta > 0 ? "improved" : "explored") :
                   "rejected";
    dom.lastMoveCard.innerHTML = `
      <div class="row"><span>Iter</span><span>${e.iter}</span></div>
      <div class="row"><span>Destroy</span><span>${dStr} (${e.removed.length})</span></div>
      <div class="row"><span>Repair</span><span>${rStr} (${e.added.length})</span></div>
      <div class="move-result">
        <div class="row"><span>Δ score</span><span class="${cls}">${sign}${e.delta.toFixed(2)}</span></div>
        <div class="row"><span>Result</span><span>${result}</span></div>
      </div>`;
  } else if (e.type === "done") {
    dom.lastMoveCard.innerHTML = `<div class="row"><span>Status</span><span>finished</span></div>
      <div class="row"><span>Final score</span><span>${e.score.toFixed(2)}</span></div>
      <div class="row"><span>Placed</span><span>${e.assignments}</span></div>`;
  } else if (e.type === "init" || e.type === "phase1_start" || e.type === "phase2_start") {
    dom.lastMoveCard.innerHTML = `<p class="muted">${e.type.replace(/_/g, " ")}…</p>`;
  }
}

function updateOperatorBars() {
  const renderBars = (container, weights, names, isRepair) => {
    container.innerHTML = "";
    const sum = weights.reduce((a, b) => a + b, 0.0001);
    const lastE = app.state.lastEvent;
    const activeIdx = (lastE && lastE.type === "phase2_iter")
      ? (isRepair ? lastE.repairOp : lastE.destroyOp) : -1;
    for (let i = 0; i < weights.length; i++) {
      const row = document.createElement("div");
      row.className = "op-row" + (i === activeIdx ? " active" : "");
      const pct = (weights[i] / sum) * 100;
      row.innerHTML = `
        <span class="op-name">${names[i]}</span>
        <span class="op-track"><span class="op-fill${isRepair ? ' repair' : ''}" style="width:${pct.toFixed(1)}%"></span></span>
        <span class="op-val">${weights[i].toFixed(2)}</span>`;
      container.appendChild(row);
    }
  };
  renderBars(dom.destroyBars, app.state.destroyW, DESTROY_OPS, false);
  renderBars(dom.repairBars, app.state.repairW, REPAIR_OPS, true);
}

function updateUnassigned() {
  const list = dom.unassignedList;
  list.innerHTML = "";
  const unassigned = app.instance.requests.filter(r => !app.state.reqToSlot.has(r.id));
  dom.unassignedBadge.textContent = unassigned.length;
  const visible = unassigned.slice(0, 40);
  for (const r of visible) {
    const li = document.createElement("li");
    const pill = document.createElement("span");
    pill.className = "cohort-pill " + COHORT_PILL[r.cohort];
    pill.textContent = COHORT_NAME[r.cohort];
    const id = document.createElement("span");
    id.textContent = "R" + r.id;
    id.style.fontVariantNumeric = "tabular-nums";
    li.appendChild(id);
    li.appendChild(pill);
    list.appendChild(li);
  }
  if (unassigned.length > visible.length) {
    const li = document.createElement("li");
    li.className = "muted small";
    li.textContent = `+ ${unassigned.length - visible.length} more`;
    list.appendChild(li);
  }
}

// -------- Score chart --------
function updateScoreChart() {
  const svg = dom.scoreChart;
  while (svg.firstChild) svg.removeChild(svg.firstChild);
  const ns = "http://www.w3.org/2000/svg";
  const W = 400, H = 140, padL = 30, padR = 6, padT = 8, padB = 18;
  const hist = app.state.scoreHistory;
  const best = app.state.bestHistory;
  if (hist.length < 2) {
    const txt = document.createElementNS(ns, "text");
    txt.setAttribute("x", W / 2); txt.setAttribute("y", H / 2);
    txt.setAttribute("text-anchor", "middle");
    txt.setAttribute("fill", "#94a3b8");
    txt.setAttribute("font-size", "11");
    txt.textContent = "no data yet";
    svg.appendChild(txt);
    dom.scoreLegend.textContent = "—";
    return;
  }
  const ymin = Math.min(...hist, ...best, 0);
  const ymax = Math.max(...hist, ...best, 1);
  const yspan = ymax - ymin || 1;
  const xspan = hist.length - 1;
  const xy = (i, y) => [
    padL + ((W - padL - padR) * i) / xspan,
    padT + (H - padT - padB) * (1 - (y - ymin) / yspan),
  ];

  // grid lines
  for (let g = 0; g <= 4; g++) {
    const y = padT + (g * (H - padT - padB)) / 4;
    const line = document.createElementNS(ns, "line");
    line.setAttribute("class", "grid-line");
    line.setAttribute("x1", padL); line.setAttribute("x2", W - padR);
    line.setAttribute("y1", y); line.setAttribute("y2", y);
    svg.appendChild(line);
    const tval = ymax - (g * yspan) / 4;
    const txt = document.createElementNS(ns, "text");
    txt.setAttribute("class", "axis-label");
    txt.setAttribute("x", 4); txt.setAttribute("y", y + 3);
    txt.textContent = tval.toFixed(1);
    svg.appendChild(txt);
  }

  // fill (area below score)
  let fillPath = `M ${padL} ${padT + (H - padT - padB)}`;
  for (let i = 0; i < hist.length; i++) {
    const [x, y] = xy(i, hist[i]);
    fillPath += ` L ${x} ${y}`;
  }
  fillPath += ` L ${W - padR} ${padT + (H - padT - padB)} Z`;
  const fill = document.createElementNS(ns, "path");
  fill.setAttribute("class", "score-fill"); fill.setAttribute("d", fillPath);
  svg.appendChild(fill);

  // score line
  let linePath = "";
  for (let i = 0; i < hist.length; i++) {
    const [x, y] = xy(i, hist[i]);
    linePath += (i === 0 ? "M" : "L") + ` ${x} ${y} `;
  }
  const line = document.createElementNS(ns, "path");
  line.setAttribute("class", "score-line"); line.setAttribute("d", linePath);
  svg.appendChild(line);

  // best line
  let bestPath = "";
  for (let i = 0; i < best.length; i++) {
    const [x, y] = xy(i, best[i]);
    bestPath += (i === 0 ? "M" : "L") + ` ${x} ${y} `;
  }
  const bestLine = document.createElementNS(ns, "path");
  bestLine.setAttribute("class", "best-line"); bestLine.setAttribute("d", bestPath);
  svg.appendChild(bestLine);

  // cursor
  const [cx] = xy(hist.length - 1, hist[hist.length - 1]);
  const cursor = document.createElementNS(ns, "line");
  cursor.setAttribute("class", "cursor");
  cursor.setAttribute("x1", cx); cursor.setAttribute("x2", cx);
  cursor.setAttribute("y1", padT); cursor.setAttribute("y2", H - padB);
  svg.appendChild(cursor);

  dom.scoreLegend.innerHTML = `<span style="color:var(--accent)">━ score</span>  ·  <span style="color:var(--ok)">┄ best</span>`;
}

// -------- Render --------
function render() {
  updateStats();
  updateGantt();
  updateLastMove();
  updateOperatorBars();
  updateUnassigned();
  updateScoreChart();
  dom.tlSlider.value = app.cursor;
  dom.tlPos.textContent = app.cursor;
}

// -------- Tooltips --------
function showSlotTooltip(ev, slot) {
  const rid = [...app.state.reqToSlot.entries()].find(([, s]) => s === slot.id);
  const tutor = app.instance.tutors[slot.tutor];
  const room = app.instance.rooms[slot.room];
  const dayIdx = Math.floor(slot.start / DAY_MIN);
  const dayName = ["Mon","Tue","Wed","Thu","Fri"][dayIdx] || "?";
  const mins = slot.start % DAY_MIN;
  const h = Math.floor(mins / 60), m = mins % 60;
  const hh = h.toString().padStart(2, "0");
  const mm = m.toString().padStart(2, "0");
  let html = `<strong>Slot ${slot.id}</strong>
    <div class="row"><span>tutor</span><span>T${tutor.id} (${tutor.pool === C.POOL.UNDERGRAD ? 'UG' : 'GRAD'})</span></div>
    <div class="row"><span>topic</span><span>${slot.topic}</span></div>
    <div class="row"><span>language</span><span>${["EN","AR","BI"][slot.language]}</span></div>
    <div class="row"><span>modality</span><span>${["online","in-person"][slot.modality]}</span></div>
    <div class="row"><span>room</span><span>R${room.id}${room.accessible ? " · ♿" : ""}</span></div>
    <div class="row"><span>when</span><span>${dayName} ${hh}:${mm}</span></div>
    <div class="row"><span>capacity</span><span>${slot.capacity}</span></div>`;
  if (rid) {
    const r = app.instance.requests[rid[0]];
    html += `<div class="row"><span>holds</span><span>R${r.id} · ${COHORT_NAME[r.cohort]}</span></div>`;
  }
  dom.tooltip.innerHTML = html;
  dom.tooltip.classList.add("show");
  positionTooltip(ev);
}
function positionTooltip(ev) {
  dom.tooltip.style.left = ev.clientX + "px";
  dom.tooltip.style.top = ev.clientY + "px";
}
function hideTooltip() { dom.tooltip.classList.remove("show"); }

// -------- Playback --------
function setPlaying(p) {
  app.playing = p;
  dom.playIcon.innerHTML = p
    ? '<path d="M6 4h4v16H6zm8 0h4v16h-4z"/>'
    : '<path d="M8 5v14l11-7z"/>';
  if (p) {
    app.lastTick = performance.now();
    cancelAnimationFrame(app.rafHandle);
    app.rafHandle = requestAnimationFrame(loop);
  }
}
function loop(now) {
  if (!app.playing) return;
  const dt = now - app.lastTick;
  app.lastTick = now;
  // events per second (approx)
  const stepsPerSecond = 4 * app.speed;
  const evsToApply = Math.max(1, Math.round((dt / 1000) * stepsPerSecond));
  let advanced = 0;
  while (advanced < evsToApply && app.cursor < app.events.length) {
    applyEvent(app.events[app.cursor]);
    app.cursor++;
    advanced++;
  }
  render();
  if (app.cursor >= app.events.length) {
    setPlaying(false);
    return;
  }
  app.rafHandle = requestAnimationFrame(loop);
}

// -------- Wire UI --------
function bindUI() {
  dom.runBtn.addEventListener("click", () => runFresh());
  dom.settingsBtn.addEventListener("click", () => dom.settingsDrawer.classList.add("open"));
  dom.closeSettings.addEventListener("click", () => dom.settingsDrawer.classList.remove("open"));
  dom.settingsDrawer.addEventListener("click", (e) => {
    if (e.target === dom.settingsDrawer) dom.settingsDrawer.classList.remove("open");
  });
  dom.resetCfg.addEventListener("click", () => applyDefaultsToForm());
  dom.applyAndRun.addEventListener("click", () => {
    dom.settingsDrawer.classList.remove("open");
    runFresh();
  });

  dom.playBtn.addEventListener("click", () => setPlaying(!app.playing));
  dom.rewindBtn.addEventListener("click", () => {
    setPlaying(false);
    seekTo(0); render();
  });
  dom.endBtn.addEventListener("click", () => {
    setPlaying(false);
    seekTo(app.events.length); render();
  });
  dom.stepBackBtn.addEventListener("click", () => {
    setPlaying(false);
    seekTo(Math.max(0, app.cursor - 1)); render();
  });
  dom.stepFwdBtn.addEventListener("click", () => {
    setPlaying(false);
    if (app.cursor < app.events.length) {
      applyEvent(app.events[app.cursor]); app.cursor++; render();
    }
  });
  dom.tlSlider.addEventListener("input", (ev) => {
    setPlaying(false);
    seekTo(parseInt(ev.target.value, 10)); render();
  });
  dom.speedPills.addEventListener("click", (ev) => {
    const t = ev.target;
    if (!t.classList.contains("pill")) return;
    [...dom.speedPills.children].forEach(c => c.classList.remove("pill-active"));
    t.classList.add("pill-active");
    app.speed = parseFloat(t.dataset.speed);
  });

  document.addEventListener("keydown", (ev) => {
    if (ev.target.tagName === "INPUT") return;
    if (ev.code === "Space") { ev.preventDefault(); setPlaying(!app.playing); }
    else if (ev.code === "ArrowRight") { setPlaying(false); dom.stepFwdBtn.click(); }
    else if (ev.code === "ArrowLeft")  { setPlaying(false); dom.stepBackBtn.click(); }
  });
}

// -------- Boot --------
function showFatal(err) {
  console.error(err);
  let bar = document.getElementById("__fatalBar");
  if (!bar) {
    bar = document.createElement("div");
    bar.id = "__fatalBar";
    bar.style.cssText =
      "position:fixed;top:0;left:0;right:0;z-index:9999;" +
      "background:#fee2e2;color:#7f1d1d;border-bottom:1px solid #fca5a5;" +
      "font-family:ui-monospace,monospace;font-size:12px;padding:8px 14px;" +
      "white-space:pre-wrap;max-height:40vh;overflow:auto;";
    document.body && document.body.appendChild(bar);
  }
  const msg = (err && err.stack) ? err.stack : String(err);
  bar.textContent = "Viewer error — paste this into the chat:\n\n" + msg;
}

window.addEventListener("error", (ev) => showFatal(ev.error || ev.message));
window.addEventListener("unhandledrejection", (ev) => showFatal(ev.reason));

function boot() {
  try {
    if (!window.generateInstance || !window.runScheduler || !window.GeneratorConst) {
      throw new Error(
        "Helper scripts did not load. Make sure generator.js and scheduler.js " +
        "are next to index.html and that your browser is not blocking them. " +
        "If you are on Chrome via file://, run 'python3 -m http.server 8765' from the viewer folder."
      );
    }
    applyDefaultsToForm();
    bindUI();
    runFresh();
  } catch (e) {
    showFatal(e);
  }
}

// Scripts are at the bottom of <body>, so DOMContentLoaded may have already fired.
if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", boot);
} else {
  // microtask delay so the very last script tag finishes parsing
  Promise.resolve().then(boot);
}
