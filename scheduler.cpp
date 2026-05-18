#include "scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_set>

namespace daalns {
namespace {

// ---------- Indices ----------
struct Indices {
    std::unordered_map<TutorID,   const Tutor*>   tutorById;
    std::unordered_map<RoomID,    const Room*>    roomById;
    std::unordered_map<SlotID,    const Slot*>    slotById;
    std::unordered_map<RequestID, const Request*> requestById;
};

Indices buildIndices(const Instance& inst) {
    Indices idx;
    idx.tutorById.reserve(inst.tutors.size());
    idx.roomById.reserve(inst.rooms.size());
    idx.slotById.reserve(inst.slots.size());
    idx.requestById.reserve(inst.requests.size());
    for (const auto& t : inst.tutors)   idx.tutorById[t.id]     = &t;
    for (const auto& r : inst.rooms)    idx.roomById[r.id]      = &r;
    for (const auto& s : inst.slots)    idx.slotById[s.id]      = &s;
    for (const auto& q : inst.requests) idx.requestById[q.id]   = &q;
    return idx;
}

// ---------- State ----------
struct State {
    std::unordered_map<SlotID,  int> slotRemainingCap;
    std::unordered_map<TutorID, int> tutorRemainingMin;
    std::unordered_map<RequestID, SlotID> reqToSlot;
    std::unordered_map<SlotID, std::vector<RequestID>> slotToReqs;
    std::unordered_map<StudentID,
        std::vector<std::pair<TimeWindow, SlotID>>> studentBookings;
};

void initState(State& st, const Instance& inst) {
    st.slotRemainingCap.clear();
    st.tutorRemainingMin.clear();
    st.reqToSlot.clear();
    st.slotToReqs.clear();
    st.studentBookings.clear();
    st.slotRemainingCap.reserve(inst.slots.size());
    st.tutorRemainingMin.reserve(inst.tutors.size());
    for (const auto& s : inst.slots)  st.slotRemainingCap[s.id]   = s.capacity;
    for (const auto& t : inst.tutors) st.tutorRemainingMin[t.id]  = t.weeklyMinuteCap;
}

// ---------- Predicates ----------
bool windowsOverlap(TimePoint a1, TimePoint a2, TimePoint b1, TimePoint b2) {
    return a1 < b2 && b1 < a2;
}
bool languageMatch(Language stu, Language slot) {
    if (slot == Language::Bilingual) return true;
    if (stu  == Language::Bilingual) return true;
    return stu == slot;
}
bool modalityMatch(Modality stu, Modality slot) {
    if (stu == Modality::Either) return true;
    return stu == slot;
}
bool tutorQualified(const Tutor& t, TopicID topic) {
    return std::find(t.qualifiedTopics.begin(),
                     t.qualifiedTopics.end(), topic) != t.qualifiedTopics.end();
}
bool inAvailability(const Tutor& t, TimePoint start, TimePoint end) {
    for (const auto& w : t.availability) {
        if (start >= w.start && end <= w.end) return true;
    }
    return false;
}
bool classConflict(const Request& r, TimePoint start, TimePoint end) {
    for (const auto& w : r.classSchedule) {
        if (windowsOverlap(start, end, w.start, w.end)) return true;
    }
    return false;
}

// Independent of current state.
bool structurallyFeasible(const Request& req,
                          const Slot& slot,
                          const Tutor& tutor,
                          const Room& room) {
    if (slot.pool     != req.pool)             return false;
    if (slot.topic    != req.topic)            return false;
    if (!tutorQualified(tutor, req.topic))     return false;
    if (!languageMatch(req.languagePref, slot.language)) return false;
    if (!modalityMatch(req.modalityPref, slot.modality)) return false;
    if (room.modality != slot.modality)        return false;
    if (req.needsAccessibility && !room.accessible) return false;
    if (room.capacity < slot.capacity)         return false;
    if (slot.capacity > req.maxGroupSize)      return false;
    if (!inAvailability(tutor, slot.start, slot.end)) return false;
    if (classConflict(req, slot.start, slot.end))     return false;
    return true;
}

// Depends on current state.
bool stateFeasible(const Request& req,
                   const Slot& slot,
                   const Tutor& tutor,
                   const State& st) {
    auto itCap = st.slotRemainingCap.find(slot.id);
    if (itCap == st.slotRemainingCap.end() || itCap->second <= 0) return false;

    int slotMin = static_cast<int>(slot.end - slot.start);
    auto itTM = st.tutorRemainingMin.find(tutor.id);
    if (itTM == st.tutorRemainingMin.end() || itTM->second < slotMin) return false;

    auto sbIt = st.studentBookings.find(req.student);
    if (sbIt != st.studentBookings.end()) {
        for (const auto& [win, sid] : sbIt->second) {
            if (windowsOverlap(slot.start, slot.end, win.start, win.end)) return false;
        }
    }
    if (st.reqToSlot.count(req.id)) return false;
    return true;
}

bool fullyFeasible(const Request& req, const Slot& slot,
                   const Indices& idx, const State& st) {
    auto tIt = idx.tutorById.find(slot.tutor);
    auto rIt = idx.roomById.find(slot.room);
    if (tIt == idx.tutorById.end() || rIt == idx.roomById.end()) return false;
    if (!structurallyFeasible(req, slot, *tIt->second, *rIt->second)) return false;
    return stateFeasible(req, slot, *tIt->second, st);
}

// ---------- assign / unassign ----------
void doAssign(State& st, const Request& req, const Slot& slot) {
    st.slotRemainingCap[slot.id]   -= 1;
    st.tutorRemainingMin[slot.tutor] -= static_cast<int>(slot.end - slot.start);
    st.reqToSlot[req.id]            = slot.id;
    st.slotToReqs[slot.id].push_back(req.id);
    st.studentBookings[req.student].push_back({{slot.start, slot.end}, slot.id});
}

void doUnassign(State& st, const Request& req, const Slot& slot) {
    st.slotRemainingCap[slot.id]   += 1;
    st.tutorRemainingMin[slot.tutor] += static_cast<int>(slot.end - slot.start);
    st.reqToSlot.erase(req.id);

    auto& v = st.slotToReqs[slot.id];
    v.erase(std::remove(v.begin(), v.end(), req.id), v.end());

    auto& sb = st.studentBookings[req.student];
    sb.erase(std::remove_if(sb.begin(), sb.end(),
        [&](const std::pair<TimeWindow, SlotID>& p) { return p.second == slot.id; }),
        sb.end());
}

// ---------- Scoring ----------
double cohortWeight(Cohort c) {
    switch (c) {
        case Cohort::AcademicWarning:     return 2.0;
        case Cohort::ProfessionalAthlete: return 2.0;
        default:                          return 1.0;
    }
}
double rankWeight(int rank0) { return 1.0 / static_cast<double>(1 + rank0); }

int findRank(const Request& r, SlotID s) {
    for (size_t i = 0; i < r.preferences.size(); ++i) {
        if (r.preferences[i] == s) return static_cast<int>(i);
    }
    return -1;
}

double scoreState(const Instance& inst, const Indices& idx,
                  const State& st, const AlnsParameters& p) {
    double score = 0.0;
    std::unordered_map<TutorID, int> tutorLoad;
    for (const auto& [reqId, slotId] : st.reqToSlot) {
        auto rIt = idx.requestById.find(reqId);
        auto sIt = idx.slotById.find(slotId);
        if (rIt == idx.requestById.end() || sIt == idx.slotById.end()) continue;
        const Request& r = *rIt->second;
        const Slot&    s = *sIt->second;
        int rank = findRank(r, slotId);
        if (rank < 0) continue;
        score += cohortWeight(r.cohort) * rankWeight(rank);
        tutorLoad[s.tutor] += static_cast<int>(s.end - s.start);
    }

    if (!tutorLoad.empty()) {
        double mean = 0;
        for (auto& kv : tutorLoad) mean += kv.second;
        mean /= static_cast<double>(tutorLoad.size());
        double var = 0;
        for (auto& kv : tutorLoad) {
            double d = kv.second - mean;
            var += d * d;
        }
        var /= static_cast<double>(tutorLoad.size());
        score -= p.weightLoadVar * std::sqrt(var) / 60.0;
    }

    std::unordered_map<int, std::pair<int,int>> cohortStats; // (assigned, total) by cohort int
    for (const auto& r : inst.requests) {
        if (r.cohort == Cohort::Standard) continue;
        int key = static_cast<int>(r.cohort);
        cohortStats[key].second++;
        if (st.reqToSlot.count(r.id)) cohortStats[key].first++;
    }
    for (auto& kv : cohortStats) {
        int total = kv.second.second;
        int got   = kv.second.first;
        if (total == 0) continue;
        double rate = static_cast<double>(got) / total;
        if (rate < p.fairFloorRatio) {
            score -= p.weightFairFloor * (p.fairFloorRatio - rate) * total;
        }
    }
    return score;
}

// ---------- Phase 1: Deferred Acceptance ----------
// Priority tuple: (weighted, -submittedAt, -student, -id). Larger = higher priority.
using Prio = std::tuple<double, TimePoint, StudentID, RequestID>;

Prio computePriority(const Request& r, int rank0) {
    double w = cohortWeight(r.cohort) * (rank0 >= 0 ? rankWeight(rank0) : 0.0);
    return std::make_tuple(w, -r.submittedAt, -r.student, -r.id);
}

void runPhase1(const Instance& inst, const Indices& idx, State& st) {
    std::unordered_map<RequestID, size_t> nextPref;
    nextPref.reserve(inst.requests.size());
    for (const auto& r : inst.requests) nextPref[r.id] = 0;

    bool progressed = true;
    while (progressed) {
        progressed = false;
        for (const auto& req : inst.requests) {
            if (st.reqToSlot.count(req.id)) continue;
            while (nextPref[req.id] < req.preferences.size()) {
                size_t prefIdx = nextPref[req.id];
                SlotID sid = req.preferences[prefIdx];
                nextPref[req.id] = prefIdx + 1;

                auto sIt = idx.slotById.find(sid);
                if (sIt == idx.slotById.end()) continue;
                const Slot& slot = *sIt->second;

                auto tIt = idx.tutorById.find(slot.tutor);
                auto rIt = idx.roomById.find(slot.room);
                if (tIt == idx.tutorById.end() || rIt == idx.roomById.end()) continue;
                if (!structurallyFeasible(req, slot, *tIt->second, *rIt->second)) continue;

                // student-time conflict
                bool studentBusy = false;
                auto sbIt = st.studentBookings.find(req.student);
                if (sbIt != st.studentBookings.end()) {
                    for (const auto& [win, ssid] : sbIt->second) {
                        if (windowsOverlap(slot.start, slot.end, win.start, win.end)) {
                            studentBusy = true; break;
                        }
                    }
                }
                if (studentBusy) continue;

                int slotMin = static_cast<int>(slot.end - slot.start);
                if (st.tutorRemainingMin[slot.tutor] < slotMin) continue;

                if (st.slotRemainingCap[sid] > 0) {
                    doAssign(st, req, slot);
                    progressed = true;
                    break;
                }

                // Displacement
                auto& holders = st.slotToReqs[sid];
                if (holders.empty()) continue;
                Prio myPrio = computePriority(req, static_cast<int>(prefIdx));
                RequestID weakestId = holders[0];
                Prio weakestPrio = computePriority(
                    *idx.requestById.at(weakestId),
                    findRank(*idx.requestById.at(weakestId), sid));
                for (size_t i = 1; i < holders.size(); ++i) {
                    const Request& h = *idx.requestById.at(holders[i]);
                    Prio p2 = computePriority(h, findRank(h, sid));
                    if (p2 < weakestPrio) { weakestPrio = p2; weakestId = holders[i]; }
                }
                if (myPrio > weakestPrio) {
                    doUnassign(st, *idx.requestById.at(weakestId), slot);
                    doAssign(st, req, slot);
                    progressed = true;
                    break;
                }
                // else rejected at this preference; continue to next
            }
        }
    }
}

// ---------- Destroy operators ----------
using Removed = std::vector<std::pair<RequestID, SlotID>>;

Removed destroyRandom(const Instance&, const Indices& idx, State& st,
                      int k, std::mt19937_64& rng) {
    std::vector<RequestID> assigned;
    assigned.reserve(st.reqToSlot.size());
    for (const auto& [r, s] : st.reqToSlot) assigned.push_back(r);
    std::shuffle(assigned.begin(), assigned.end(), rng);
    Removed out;
    int n = std::min(k, static_cast<int>(assigned.size()));
    for (int i = 0; i < n; ++i) {
        SlotID sid = st.reqToSlot.at(assigned[i]);
        doUnassign(st, *idx.requestById.at(assigned[i]), *idx.slotById.at(sid));
        out.push_back({assigned[i], sid});
    }
    return out;
}

Removed destroyWorstRank(const Instance&, const Indices& idx, State& st,
                         int k, std::mt19937_64&) {
    std::vector<std::pair<int, RequestID>> ranked;
    ranked.reserve(st.reqToSlot.size());
    for (const auto& [r, s] : st.reqToSlot) {
        int rk = findRank(*idx.requestById.at(r), s);
        ranked.push_back({rk, r});
    }
    std::sort(ranked.begin(), ranked.end(), std::greater<>());
    Removed out;
    int n = std::min(k, static_cast<int>(ranked.size()));
    for (int i = 0; i < n; ++i) {
        SlotID sid = st.reqToSlot.at(ranked[i].second);
        doUnassign(st, *idx.requestById.at(ranked[i].second), *idx.slotById.at(sid));
        out.push_back({ranked[i].second, sid});
    }
    return out;
}

Removed destroyCohortFloor(const Instance& inst, const Indices& idx, State& st,
                           int k, std::mt19937_64& rng) {
    std::unordered_set<SlotID> wanted;
    for (const auto& r : inst.requests) {
        if (r.cohort == Cohort::Standard) continue;
        if (st.reqToSlot.count(r.id)) continue;
        for (auto s : r.preferences) wanted.insert(s);
    }
    std::vector<RequestID> candidates;
    for (const auto& [r, s] : st.reqToSlot) {
        if (idx.requestById.at(r)->cohort == Cohort::Standard && wanted.count(s)) {
            candidates.push_back(r);
        }
    }
    std::shuffle(candidates.begin(), candidates.end(), rng);
    Removed out;
    int n = std::min(k, static_cast<int>(candidates.size()));
    for (int i = 0; i < n; ++i) {
        SlotID sid = st.reqToSlot.at(candidates[i]);
        doUnassign(st, *idx.requestById.at(candidates[i]), *idx.slotById.at(sid));
        out.push_back({candidates[i], sid});
    }
    return out;
}

Removed destroyTutorOverload(const Instance&, const Indices& idx, State& st,
                             int k, std::mt19937_64& rng) {
    std::unordered_map<TutorID, int> load;
    for (const auto& [r, s] : st.reqToSlot) {
        const Slot& sl = *idx.slotById.at(s);
        load[sl.tutor] += static_cast<int>(sl.end - sl.start);
    }
    if (load.empty()) return {};
    TutorID worst = load.begin()->first;
    int worstLoad = load.begin()->second;
    for (auto& kv : load) {
        if (kv.second > worstLoad) { worstLoad = kv.second; worst = kv.first; }
    }
    std::vector<RequestID> candidates;
    for (const auto& [r, s] : st.reqToSlot) {
        if (idx.slotById.at(s)->tutor == worst) candidates.push_back(r);
    }
    std::shuffle(candidates.begin(), candidates.end(), rng);
    Removed out;
    int n = std::min(k, static_cast<int>(candidates.size()));
    for (int i = 0; i < n; ++i) {
        SlotID sid = st.reqToSlot.at(candidates[i]);
        doUnassign(st, *idx.requestById.at(candidates[i]), *idx.slotById.at(sid));
        out.push_back({candidates[i], sid});
    }
    return out;
}

// ---------- Repair operators ----------
void repairGreedy(const Instance&, const Indices& idx, State& st,
                  std::vector<RequestID>& pool, std::mt19937_64&,
                  const AlnsParameters&) {
    std::sort(pool.begin(), pool.end(), [&](RequestID a, RequestID b) {
        const Request& ra = *idx.requestById.at(a);
        const Request& rb = *idx.requestById.at(b);
        if (cohortWeight(ra.cohort) != cohortWeight(rb.cohort))
            return cohortWeight(ra.cohort) > cohortWeight(rb.cohort);
        if (ra.submittedAt != rb.submittedAt) return ra.submittedAt < rb.submittedAt;
        return ra.id < rb.id;
    });
    for (RequestID rid : pool) {
        if (st.reqToSlot.count(rid)) continue;
        const Request& r = *idx.requestById.at(rid);
        for (SlotID sid : r.preferences) {
            auto it = idx.slotById.find(sid);
            if (it == idx.slotById.end()) continue;
            if (fullyFeasible(r, *it->second, idx, st)) {
                doAssign(st, r, *it->second);
                break;
            }
        }
    }
}

void repairRegretK(const Instance&, const Indices& idx, State& st,
                   std::vector<RequestID>& pool, std::mt19937_64&,
                   const AlnsParameters&) {
    while (true) {
        RequestID bestReq = -1;
        SlotID    bestSlot = -1;
        double    bestRegret = -1.0;
        double    bestRegretBase = -1.0;
        for (RequestID rid : pool) {
            if (st.reqToSlot.count(rid)) continue;
            const Request& r = *idx.requestById.at(rid);
            double topScore = -1.0, secondScore = -1.0;
            SlotID topSid = -1;
            for (size_t k = 0; k < r.preferences.size(); ++k) {
                SlotID sid = r.preferences[k];
                auto it = idx.slotById.find(sid);
                if (it == idx.slotById.end()) continue;
                if (!fullyFeasible(r, *it->second, idx, st)) continue;
                double sc = cohortWeight(r.cohort) * rankWeight(static_cast<int>(k));
                if (sc > topScore)        { secondScore = topScore;   topScore = sc; topSid = sid; }
                else if (sc > secondScore){ secondScore = sc; }
            }
            if (topScore < 0) continue;
            double regret = (secondScore < 0) ? topScore : (topScore - secondScore);
            // Tie-break by the absolute top score so we prefer high-value placements
            if (regret > bestRegret + 1e-12 ||
                (std::abs(regret - bestRegret) < 1e-12 && topScore > bestRegretBase + 1e-12)) {
                bestRegret = regret;
                bestRegretBase = topScore;
                bestReq = rid;
                bestSlot = topSid;
            }
        }
        if (bestReq < 0) break;
        doAssign(st, *idx.requestById.at(bestReq), *idx.slotById.at(bestSlot));
    }
}

struct BTContext {
    std::vector<std::pair<RequestID, SlotID>> best;
    double bestScore = -1.0;
};

void btRecurse(const Indices& idx, State& st,
               std::vector<RequestID>& pool, size_t i,
               std::vector<std::pair<RequestID, SlotID>>& current,
               double currentScore, BTContext& ctx) {
    if (i == pool.size()) {
        if (currentScore > ctx.bestScore) {
            ctx.bestScore = currentScore;
            ctx.best = current;
        }
        return;
    }
    RequestID rid = pool[i];
    const Request& r = *idx.requestById.at(rid);
    bool tried = false;
    for (size_t k = 0; k < r.preferences.size(); ++k) {
        SlotID sid = r.preferences[k];
        auto it = idx.slotById.find(sid);
        if (it == idx.slotById.end()) continue;
        if (!fullyFeasible(r, *it->second, idx, st)) continue;
        double add = cohortWeight(r.cohort) * rankWeight(static_cast<int>(k));
        doAssign(st, r, *it->second);
        current.push_back({rid, sid});
        btRecurse(idx, st, pool, i + 1, current, currentScore + add, ctx);
        current.pop_back();
        doUnassign(st, r, *it->second);
        tried = true;
    }
    // Allow skipping this request (it becomes unassigned)
    btRecurse(idx, st, pool, i + 1, current, currentScore, ctx);
    (void)tried;
}

void repairBacktracking(const Instance&, const Indices& idx, State& st,
                        std::vector<RequestID>& pool, std::mt19937_64& rng,
                        const AlnsParameters& p) {
    std::sort(pool.begin(), pool.end(), [&](RequestID a, RequestID b) {
        return cohortWeight(idx.requestById.at(a)->cohort) >
               cohortWeight(idx.requestById.at(b)->cohort);
    });
    int limit = std::min(static_cast<int>(pool.size()), p.backtrackingCap);
    std::vector<RequestID> slice(pool.begin(), pool.begin() + limit);

    // Save the slice's currently-assigned state (in case any were already assigned)
    std::vector<std::pair<RequestID, SlotID>> saved;
    for (RequestID rid : slice) {
        auto it = st.reqToSlot.find(rid);
        if (it != st.reqToSlot.end()) {
            saved.push_back({rid, it->second});
            doUnassign(st, *idx.requestById.at(rid), *idx.slotById.at(it->second));
        }
    }

    BTContext ctx;
    std::vector<std::pair<RequestID, SlotID>> current;
    btRecurse(idx, st, slice, 0, current, 0.0, ctx);

    if (ctx.bestScore > 0) {
        for (auto& [r, s] : ctx.best) {
            doAssign(st, *idx.requestById.at(r), *idx.slotById.at(s));
        }
    } else {
        // restore the prior assignment if backtracking found nothing
        for (auto& [r, s] : saved) {
            doAssign(st, *idx.requestById.at(r), *idx.slotById.at(s));
        }
    }

    if (limit < static_cast<int>(pool.size())) {
        std::vector<RequestID> rest(pool.begin() + limit, pool.end());
        repairGreedy({}, idx, st, rest, rng, p);
    }
}

// ---------- ALNS adaptive weights ----------
int weightedPick(const std::vector<double>& weights, std::mt19937_64& rng) {
    double total = 0;
    for (double w : weights) total += w;
    std::uniform_real_distribution<double> dist(0, total);
    double r = dist(rng);
    double acc = 0;
    for (size_t i = 0; i < weights.size(); ++i) {
        acc += weights[i];
        if (r <= acc) return static_cast<int>(i);
    }
    return static_cast<int>(weights.size() - 1);
}

// ---------- Phase 2: ALNS ----------
struct PhaseTwoStats { int iterations = 0; int improvements = 0; };

PhaseTwoStats phase2ALNS(const Instance& inst, const Indices& idx, State& st,
                         const AlnsParameters& p) {
    std::mt19937_64 rng(p.seed);
    std::vector<double> destroyW = {1.0, 1.0, 1.0, 1.0};
    std::vector<double> repairW  = {1.0, 1.0, 1.0};

    State  bestSt   = st;
    double bestSc   = scoreState(inst, idx, st, p);
    double curSc    = bestSc;
    int    improves = 0;
    int    stall    = 0;
    int    iters    = 0;
    double temp     = p.initialTemp;

    for (int iter = 0; iter < p.maxIterations; ++iter) {
        iters = iter + 1;
        int dIdx = weightedPick(destroyW, rng);
        int rIdx = weightedPick(repairW, rng);

        State trial = st;
        int nA = static_cast<int>(trial.reqToSlot.size());
        int k = std::max(p.minDestroy,
                std::min(p.maxDestroy,
                static_cast<int>(std::round(p.destroyFraction * nA))));
        Removed removed;
        switch (dIdx) {
            case 0: removed = destroyRandom(inst, idx, trial, k, rng); break;
            case 1: removed = destroyWorstRank(inst, idx, trial, k, rng); break;
            case 2: removed = destroyCohortFloor(inst, idx, trial, k, rng); break;
            case 3: removed = destroyTutorOverload(inst, idx, trial, k, rng); break;
        }

        std::vector<RequestID> pool;
        std::unordered_set<RequestID> inPool;
        for (auto& [r, _] : removed) { pool.push_back(r); inPool.insert(r); }
        for (const auto& req : inst.requests) {
            if (!trial.reqToSlot.count(req.id) && !inPool.count(req.id)) {
                pool.push_back(req.id); inPool.insert(req.id);
            }
        }
        std::shuffle(pool.begin(), pool.end(), rng);

        switch (rIdx) {
            case 0: repairGreedy(inst, idx, trial, pool, rng, p); break;
            case 1: repairRegretK(inst, idx, trial, pool, rng, p); break;
            case 2: repairBacktracking(inst, idx, trial, pool, rng, p); break;
        }

        double trialSc = scoreState(inst, idx, trial, p);
        double delta = trialSc - curSc;
        std::uniform_real_distribution<double> u01(0.0, 1.0);
        bool accept = delta > 0 ||
                      u01(rng) < std::exp(delta / std::max(temp, 1e-9));

        double reward = 0.0;
        if (accept) {
            st   = std::move(trial);
            curSc = trialSc;
            if (trialSc > bestSc + 1e-9) {
                bestSt = st;
                bestSc = trialSc;
                improves++;
                stall = 0;
                reward = 3.0;
            } else if (delta > 0) {
                reward = 1.5;
            } else {
                reward = 0.5;
                stall++;
            }
        } else {
            stall++;
        }
        destroyW[dIdx] = 0.9 * destroyW[dIdx] + 0.1 * (reward + 0.01);
        repairW[rIdx]  = 0.9 * repairW[rIdx]  + 0.1 * (reward + 0.01);
        temp *= p.coolingRate;
        if (stall >= p.stallLimit) break;
    }

    st = std::move(bestSt);
    PhaseTwoStats out;
    out.iterations = iters;
    out.improvements = improves;
    return out;
}

// ---------- Build result from state ----------
ScheduleResult buildResult(const Instance& inst, const Indices& idx,
                           const State& st, const AlnsParameters& p) {
    ScheduleResult res;
    res.objectiveValue = scoreState(inst, idx, st, p);
    res.assignments.reserve(st.reqToSlot.size());
    for (const auto& [r, s] : st.reqToSlot) res.assignments.push_back({r, s});
    std::sort(res.assignments.begin(), res.assignments.end(),
              [](const Assignment& a, const Assignment& b) {
                  if (a.request != b.request) return a.request < b.request;
                  return a.slot < b.slot;
              });
    std::unordered_set<RequestID> assigned;
    for (const auto& a : res.assignments) assigned.insert(a.request);
    for (const auto& r : inst.requests) {
        if (assigned.count(r.id)) continue;
        std::string reason = "all preferences infeasible or full";
        if (r.preferences.empty()) reason = "empty preference list";
        res.unassigned.push_back({r.id, reason});
    }
    std::sort(res.unassigned.begin(), res.unassigned.end(),
              [](const UnassignedRequest& a, const UnassignedRequest& b) {
                  return a.request < b.request;
              });
    return res;
}

} // anonymous namespace

// ---------- Public API ----------
ScheduleResult runScheduler(const Instance& inst, const AlnsParameters& params) {
    Indices idx = buildIndices(inst);
    State st;
    initState(st, inst);

    runPhase1(inst, idx, st);
    int p1count = static_cast<int>(st.reqToSlot.size());

    PhaseTwoStats stats = phase2ALNS(inst, idx, st, params);

    ScheduleResult res = buildResult(inst, idx, st, params);
    res.phase1Assignments  = p1count;
    res.phase2Iterations   = stats.iterations;
    res.phase2Improvements = stats.improvements;
    return res;
}

double computeObjective(const Instance& inst, const ScheduleResult& result,
                        const AlnsParameters& params) {
    Indices idx = buildIndices(inst);
    State st;
    initState(st, inst);
    for (const auto& a : result.assignments) {
        auto rIt = idx.requestById.find(a.request);
        auto sIt = idx.slotById.find(a.slot);
        if (rIt == idx.requestById.end() || sIt == idx.slotById.end()) continue;
        doAssign(st, *rIt->second, *sIt->second);
    }
    return scoreState(inst, idx, st, params);
}

bool verifyFeasibility(const Instance& inst, const ScheduleResult& result,
                       std::string* violation) {
    Indices idx = buildIndices(inst);
    State st;
    initState(st, inst);

    std::unordered_set<RequestID> seen;
    for (const auto& a : result.assignments) {
        if (seen.count(a.request)) {
            if (violation) *violation = "duplicate request " + std::to_string(a.request);
            return false;
        }
        seen.insert(a.request);
        auto rIt = idx.requestById.find(a.request);
        auto sIt = idx.slotById.find(a.slot);
        if (rIt == idx.requestById.end()) {
            if (violation) *violation = "unknown request " + std::to_string(a.request);
            return false;
        }
        if (sIt == idx.slotById.end()) {
            if (violation) *violation = "unknown slot " + std::to_string(a.slot);
            return false;
        }
        if (findRank(*rIt->second, a.slot) < 0) {
            if (violation) *violation = "slot not in request preferences";
            return false;
        }
        if (!fullyFeasible(*rIt->second, *sIt->second, idx, st)) {
            if (violation) {
                std::ostringstream oss;
                oss << "infeasible req " << a.request << " -> slot " << a.slot;
                *violation = oss.str();
            }
            return false;
        }
        doAssign(st, *rIt->second, *sIt->second);
    }
    return true;
}

EventResult onRequestArrived(const Request& req, const Instance& inst,
                             ScheduleResult& current, const AlnsParameters&) {
    Indices idx = buildIndices(inst);
    State st;
    initState(st, inst);
    for (const auto& a : current.assignments) {
        auto rIt = idx.requestById.find(a.request);
        auto sIt = idx.slotById.find(a.slot);
        if (rIt == idx.requestById.end() || sIt == idx.slotById.end()) continue;
        doAssign(st, *rIt->second, *sIt->second);
    }
    for (SlotID sid : req.preferences) {
        auto it = idx.slotById.find(sid);
        if (it == idx.slotById.end()) continue;
        if (fullyFeasible(req, *it->second, idx, st)) {
            doAssign(st, req, *it->second);
            current.assignments.push_back({req.id, sid});
            std::sort(current.assignments.begin(), current.assignments.end(),
                      [](const Assignment& a, const Assignment& b) {
                          if (a.request != b.request) return a.request < b.request;
                          return a.slot < b.slot;
                      });
            std::ostringstream oss;
            oss << "placed at preference rank " << findRank(req, sid);
            return {true, {req.id, sid}, oss.str()};
        }
    }
    current.unassigned.push_back({req.id, "no feasible preference on arrival"});
    return {false, {req.id, -1}, "no feasible slot available"};
}

EventResult onCancellation(RequestID rid, const Instance& inst,
                           ScheduleResult& current, const AlnsParameters&) {
    auto it = std::find_if(current.assignments.begin(), current.assignments.end(),
                           [rid](const Assignment& a) { return a.request == rid; });
    if (it == current.assignments.end()) {
        return {false, {rid, -1}, "request not assigned"};
    }
    SlotID freed = it->slot;
    current.assignments.erase(it);

    Indices idx = buildIndices(inst);
    State st;
    initState(st, inst);
    for (const auto& a : current.assignments) {
        auto rIt = idx.requestById.find(a.request);
        auto sIt = idx.slotById.find(a.slot);
        if (rIt == idx.requestById.end() || sIt == idx.slotById.end()) continue;
        doAssign(st, *rIt->second, *sIt->second);
    }

    // Try to fill freed slot from unassigned waiting list — prefer cohort + earliest submit
    std::sort(current.unassigned.begin(), current.unassigned.end(),
              [&](const UnassignedRequest& a, const UnassignedRequest& b) {
                  auto ai = idx.requestById.find(a.request);
                  auto bi = idx.requestById.find(b.request);
                  if (ai == idx.requestById.end() || bi == idx.requestById.end())
                      return a.request < b.request;
                  const Request& ra = *ai->second;
                  const Request& rb = *bi->second;
                  if (cohortWeight(ra.cohort) != cohortWeight(rb.cohort))
                      return cohortWeight(ra.cohort) > cohortWeight(rb.cohort);
                  return ra.submittedAt < rb.submittedAt;
              });

    for (auto uit = current.unassigned.begin(); uit != current.unassigned.end(); ++uit) {
        auto rIt = idx.requestById.find(uit->request);
        if (rIt == idx.requestById.end()) continue;
        const Request& r = *rIt->second;
        if (std::find(r.preferences.begin(), r.preferences.end(), freed)
            == r.preferences.end()) continue;
        auto sIt = idx.slotById.find(freed);
        if (sIt == idx.slotById.end()) continue;
        if (!fullyFeasible(r, *sIt->second, idx, st)) continue;
        doAssign(st, r, *sIt->second);
        RequestID filled = uit->request;
        current.unassigned.erase(uit);
        current.assignments.push_back({filled, freed});
        std::sort(current.assignments.begin(), current.assignments.end(),
                  [](const Assignment& a, const Assignment& b) {
                      if (a.request != b.request) return a.request < b.request;
                      return a.slot < b.slot;
                  });
        return {true, {filled, freed}, "filled cancellation from waiting list"};
    }
    return {true, {rid, freed}, "cancellation freed slot; no waiting-list match"};
}

} // namespace daalns
