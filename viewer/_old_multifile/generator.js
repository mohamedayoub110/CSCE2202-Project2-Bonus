"use strict";

const COHORT = Object.freeze({ STANDARD: 0, ACADEMIC_WARNING: 1, ATHLETE: 2 });
const POOL = Object.freeze({ UNDERGRAD: 0, GRAD: 1 });
const MODALITY = Object.freeze({ ONLINE: 0, IN_PERSON: 1, EITHER: 2 });
const LANGUAGE = Object.freeze({ ENGLISH: 0, ARABIC: 1, BILINGUAL: 2 });

// Mulberry32 — deterministic small PRNG keyed on a 32-bit seed.
function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a |= 0;
    a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function randInt(rng, lo, hi) { // inclusive
  return Math.floor(rng() * (hi - lo + 1)) + lo;
}
function pick(rng, arr) { return arr[Math.floor(rng() * arr.length)]; }
function bernoulli(rng, p) { return rng() < p; }
function shuffleInPlace(rng, arr) {
  for (let i = arr.length - 1; i > 0; i--) {
    const j = Math.floor(rng() * (i + 1));
    [arr[i], arr[j]] = [arr[j], arr[i]];
  }
  return arr;
}

function generateInstance(params) {
  const rng = mulberry32(params.seed);
  const inst = { tutors: [], rooms: [], slots: [], requests: [] };

  const dayMinutes = 600;            // 10-hour days
  const slotMinutes = 60;
  const daysInWeek = 5;

  // -------- Tutors --------
  for (let i = 0; i < params.numTutors; i++) {
    const pool = (i < params.numTutors / 2) ? POOL.UNDERGRAD : POOL.GRAD;
    const nq = randInt(rng, 2, Math.min(4, params.numTopics));
    const qs = new Set();
    while (qs.size < nq) qs.add(randInt(rng, 0, params.numTopics - 1));
    let lang;
    const u = rng();
    if (u < params.bilingualRate) lang = LANGUAGE.BILINGUAL;
    else if (u < params.bilingualRate + params.arabicRate) lang = LANGUAGE.ARABIC;
    else lang = LANGUAGE.ENGLISH;
    const availability = [];
    for (let d = 0; d < daysInWeek; d++) {
      const dayStart = d * 24 * 60;
      const off = randInt(rng, 0, Math.floor(dayMinutes / 2));
      const start = dayStart + off;
      const end = dayStart + Math.min(dayMinutes, off + 6 * 60);
      availability.push({ start, end });
    }
    inst.tutors.push({
      id: i,
      pool,
      qualifiedTopics: [...qs],
      language: lang,
      weeklyMinuteCap: pool === POOL.UNDERGRAD ? 10 * 60 : 20 * 60,
      availability,
    });
  }

  // -------- Rooms --------
  for (let i = 0; i < params.numRooms; i++) {
    const capacity = bernoulli(rng, 0.20) ? 4 : 1;
    inst.rooms.push({
      id: i,
      capacity,
      accessible: bernoulli(rng, 0.30),
      modality: (i < params.numRooms / 4) ? MODALITY.ONLINE : MODALITY.IN_PERSON,
    });
  }

  // -------- Slots --------
  for (let i = 0; i < params.numSlots; i++) {
    const tutor = inst.tutors[i % inst.tutors.length];
    const topic = pick(rng, tutor.qualifiedTopics);
    const win = pick(rng, tutor.availability);
    const span = win.end - win.start;
    let start, end;
    if (span < slotMinutes) {
      start = win.start;
      end = start + slotMinutes;
    } else {
      const off = randInt(rng, 0, span - slotMinutes);
      start = win.start + off;
      end = start + slotMinutes;
    }
    // Pick a room whose modality is consistent
    const candidates = inst.rooms.filter(r =>
      (bernoulli(rng, params.onlineRate) && r.modality === MODALITY.ONLINE) ||
      r.modality === MODALITY.IN_PERSON);
    const room = candidates.length ? pick(rng, candidates) : inst.rooms[0];
    const cap = Math.min(room.capacity, bernoulli(rng, 0.20) ? 3 : 1);
    inst.slots.push({
      id: i,
      tutor: tutor.id,
      room: room.id,
      topic,
      language: tutor.language,
      modality: room.modality,
      pool: tutor.pool,
      start,
      end,
      capacity: cap,
    });
  }

  // -------- Requests --------
  const totalCap = inst.slots.reduce((s, x) => s + x.capacity, 0);
  const target = Math.max(1, Math.floor(params.demandPressure * totalCap));
  const N = Math.min(params.numRequests, target);

  for (let i = 0; i < N; i++) {
    const r = { id: i, student: i };
    const u = rng();
    if (u < params.academicWarningRate) r.cohort = COHORT.ACADEMIC_WARNING;
    else if (u < params.academicWarningRate + params.athleteRate) r.cohort = COHORT.ATHLETE;
    else r.cohort = COHORT.STANDARD;
    r.topic = randInt(rng, 0, params.numTopics - 1);
    r.pool = bernoulli(rng, 0.5) ? POOL.UNDERGRAD : POOL.GRAD;
    const m = rng();
    if (m < params.onlineRate) r.modalityPref = MODALITY.ONLINE;
    else if (m < params.onlineRate + 0.5) r.modalityPref = MODALITY.IN_PERSON;
    else r.modalityPref = MODALITY.EITHER;
    const l = rng();
    if (l < params.bilingualRate) r.languagePref = LANGUAGE.BILINGUAL;
    else if (l < params.bilingualRate + params.arabicRate) r.languagePref = LANGUAGE.ARABIC;
    else r.languagePref = LANGUAGE.ENGLISH;
    r.maxGroupSize = bernoulli(rng, 0.20) ? 4 : 1;
    r.needsAccessibility = bernoulli(rng, params.accessibilityRate);
    const candSlots = inst.slots.filter(s => s.topic === r.topic && s.pool === r.pool).map(s => s.id);
    shuffleInPlace(rng, candSlots);
    const len = Math.min(candSlots.length, randInt(rng, 1, 5));
    r.preferences = candSlots.slice(0, len);
    r.submittedAt = randInt(rng, 0, 9999);
    r.classSchedule = [];
    if (bernoulli(rng, params.classConflictRate)) {
      const d = randInt(rng, 0, daysInWeek - 1);
      const dayStart = d * 24 * 60;
      const off = randInt(rng, 0, dayMinutes - 60);
      r.classSchedule.push({ start: dayStart + off, end: dayStart + off + 60 });
    }
    inst.requests.push(r);
  }

  return inst;
}

window.GeneratorConst = { COHORT, POOL, MODALITY, LANGUAGE };
window.generateInstance = generateInstance;
window.mulberry32 = mulberry32;
