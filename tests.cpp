#include "scheduler.hpp"
#include "generator.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

using namespace daalns;

namespace {

int g_passed = 0;
int g_failed = 0;

#define EXPECT(cond, msg) do {                                                 \
    if (cond) { ++g_passed; }                                                  \
    else {                                                                     \
        ++g_failed;                                                            \
        std::cerr << "FAIL [" << __LINE__ << "]: " << msg << "\n";             \
    }                                                                          \
} while(0)

// ---------- Builders ----------
Tutor makeTutor(TutorID id, Pool pool, std::vector<TopicID> topics,
                Language lang = Language::English, int capMin = 600) {
    Tutor t;
    t.id = id; t.pool = pool;
    t.qualifiedTopics = std::move(topics);
    t.language = lang;
    t.weeklyMinuteCap = capMin;
    t.availability.push_back({0, 24 * 60 * 7});  // always available within the week
    return t;
}

Room makeRoom(RoomID id, int cap = 1, bool accessible = true,
              Modality m = Modality::InPerson) {
    Room r;
    r.id = id; r.capacity = cap; r.accessible = accessible; r.modality = m;
    return r;
}

Slot makeSlot(SlotID id, TutorID tutor, RoomID room, TopicID topic,
              TimePoint start, TimePoint end, int cap = 1,
              Language lang = Language::English,
              Modality m = Modality::InPerson,
              Pool pool = Pool::Undergrad) {
    Slot s;
    s.id = id; s.tutor = tutor; s.room = room; s.topic = topic;
    s.language = lang; s.modality = m; s.pool = pool;
    s.start = start; s.end = end; s.capacity = cap;
    return s;
}

Request makeReq(RequestID id, StudentID stu, TopicID topic, Pool pool,
                std::vector<SlotID> prefs, TimePoint submittedAt,
                Cohort cohort = Cohort::Standard,
                Language lang = Language::English,
                Modality m = Modality::InPerson,
                int maxGroup = 1, bool acc = false) {
    Request r;
    r.id = id; r.student = stu;
    r.cohort = cohort; r.topic = topic; r.pool = pool;
    r.modalityPref = m; r.languagePref = lang;
    r.maxGroupSize = maxGroup; r.needsAccessibility = acc;
    r.preferences = std::move(prefs);
    r.submittedAt = submittedAt;
    return r;
}

bool reqAssignedTo(const ScheduleResult& res, RequestID r, SlotID s) {
    for (const auto& a : res.assignments) {
        if (a.request == r && a.slot == s) return true;
    }
    return false;
}

bool reqUnassigned(const ScheduleResult& res, RequestID r) {
    for (const auto& a : res.assignments) if (a.request == r) return false;
    for (const auto& u : res.unassigned)   if (u.request == r) return true;
    return false;
}

// ---------- Test 1: happy path ----------
void test_happy_path() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160));
    inst.slots.push_back(makeSlot(1, 0, 0, 0, 200, 260));
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0}, 1));
    inst.requests.push_back(makeReq(1, 101, 0, Pool::Undergrad, {1}, 2));

    auto res = runScheduler(inst);
    EXPECT(reqAssignedTo(res, 0, 0), "happy path: req 0 -> slot 0");
    EXPECT(reqAssignedTo(res, 1, 1), "happy path: req 1 -> slot 1");
    EXPECT(res.unassigned.empty(), "happy path: nobody unassigned");
    EXPECT(verifyFeasibility(inst, res), "happy path: feasible");
}

// ---------- Test 2: capacity conflict, cohort priority breaks the tie ----------
void test_cohort_priority() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160, /*cap*/1));
    // Standard student first by timestamp; academic-warning student later
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0}, 1,
                                    Cohort::Standard));
    inst.requests.push_back(makeReq(1, 101, 0, Pool::Undergrad, {0}, 5,
                                    Cohort::AcademicWarning));

    auto res = runScheduler(inst);
    EXPECT(reqAssignedTo(res, 1, 0), "cohort priority: academic warning wins");
    EXPECT(reqUnassigned(res, 0), "cohort priority: standard student bumped");
    EXPECT(verifyFeasibility(inst, res), "cohort priority: feasible");
}

// ---------- Test 3: timestamp tie-break under same cohort ----------
void test_timestamp_tiebreak() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160, 1));
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0}, 1));
    inst.requests.push_back(makeReq(1, 101, 0, Pool::Undergrad, {0}, 5));

    auto res = runScheduler(inst);
    EXPECT(reqAssignedTo(res, 0, 0), "tiebreak: earlier timestamp wins");
    EXPECT(reqUnassigned(res, 1), "tiebreak: later loses");
}

// ---------- Test 4: hard constraint — qualification ----------
void test_qualification() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));  // only topic 0
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 1, 100, 160));       // slot for topic 1
    inst.requests.push_back(makeReq(0, 100, 1, Pool::Undergrad, {0}, 1));

    auto res = runScheduler(inst);
    EXPECT(reqUnassigned(res, 0), "qualification: unqualified tutor -> unassigned");
    EXPECT(verifyFeasibility(inst, res), "qualification: feasible");
}

// ---------- Test 5: language match ----------
void test_language_match() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}, Language::English));
    inst.tutors.push_back(makeTutor(1, Pool::Undergrad, {0}, Language::Arabic));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160, 1, Language::English));
    inst.slots.push_back(makeSlot(1, 1, 0, 0, 200, 260, 1, Language::Arabic));

    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {1, 0}, 1,
                                    Cohort::Standard, Language::Arabic));

    auto res = runScheduler(inst);
    EXPECT(reqAssignedTo(res, 0, 1), "language: Arabic student -> Arabic slot");
}

// ---------- Test 6: class conflict ----------
void test_class_conflict() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160));

    Request r = makeReq(0, 100, 0, Pool::Undergrad, {0}, 1);
    r.classSchedule.push_back({120, 180});  // overlaps slot
    inst.requests.push_back(r);

    auto res = runScheduler(inst);
    EXPECT(reqUnassigned(res, 0), "class conflict: overlapping class -> unassigned");
}

// ---------- Test 7: tutor weekly hour cap ----------
void test_tutor_hour_cap() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}, Language::English, /*cap*/120));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160));   // 60 min
    inst.slots.push_back(makeSlot(1, 0, 0, 0, 200, 260));   // 60 min
    inst.slots.push_back(makeSlot(2, 0, 0, 0, 300, 360));   // 60 min — over cap
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0}, 1));
    inst.requests.push_back(makeReq(1, 101, 0, Pool::Undergrad, {1}, 2));
    inst.requests.push_back(makeReq(2, 102, 0, Pool::Undergrad, {2}, 3));

    auto res = runScheduler(inst);
    int assigned = static_cast<int>(res.assignments.size());
    EXPECT(assigned == 2, "hour cap: only 2 of 3 fit under 120-min cap");
    EXPECT(verifyFeasibility(inst, res), "hour cap: feasible");
}

// ---------- Test 8: accessibility ----------
void test_accessibility() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0, 1, /*accessible*/false));
    inst.rooms.push_back(makeRoom(1, 1, /*accessible*/true));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160));
    inst.slots.push_back(makeSlot(1, 0, 1, 0, 200, 260));

    Request r = makeReq(0, 100, 0, Pool::Undergrad, {0, 1}, 1);
    r.needsAccessibility = true;
    inst.requests.push_back(r);

    auto res = runScheduler(inst);
    EXPECT(reqAssignedTo(res, 0, 1), "accessibility: must use accessible room");
}

// ---------- Test 9: modality enforcement ----------
void test_modality() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0, 1, true, Modality::Online));
    inst.rooms.push_back(makeRoom(1, 1, true, Modality::InPerson));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160, 1, Language::English, Modality::Online));
    inst.slots.push_back(makeSlot(1, 0, 1, 0, 200, 260, 1, Language::English, Modality::InPerson));

    Request r = makeReq(0, 100, 0, Pool::Undergrad, {0, 1}, 1,
                        Cohort::Standard, Language::English, Modality::InPerson);
    inst.requests.push_back(r);

    auto res = runScheduler(inst);
    EXPECT(reqAssignedTo(res, 0, 1), "modality: in-person preference -> in-person slot");
}

// ---------- Test 10: empty preferences ----------
void test_empty_preferences() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160));
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {}, 1));

    auto res = runScheduler(inst);
    EXPECT(reqUnassigned(res, 0), "empty prefs: unassigned");
    bool foundReason = false;
    for (const auto& u : res.unassigned) {
        if (u.request == 0 && u.reason.find("empty") != std::string::npos)
            foundReason = true;
    }
    EXPECT(foundReason, "empty prefs: reason reports empty list");
}

// ---------- Test 11: same student, two overlapping requests ----------
void test_same_student_overlap() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0, 1}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160));
    inst.slots.push_back(makeSlot(1, 0, 0, 1, 130, 190));  // overlaps slot 0
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0}, 1));
    inst.requests.push_back(makeReq(1, 100, 1, Pool::Undergrad, {1}, 2));  // same student

    auto res = runScheduler(inst);
    EXPECT(res.assignments.size() == 1u, "same student overlap: at most one assignment");
    EXPECT(verifyFeasibility(inst, res), "same student overlap: feasible");
}

// ---------- Test 12: determinism ----------
void test_determinism() {
    GeneratorParams gp;
    gp.numRequests = 50;
    gp.numSlots = 30;
    gp.numTutors = 6;
    gp.seed = 12345;
    Instance inst = generateInstance(gp);

    auto a = runScheduler(inst);
    auto b = runScheduler(inst);
    EXPECT(a.assignments.size() == b.assignments.size(), "determinism: same assignment count");
    bool same = (a.assignments.size() == b.assignments.size());
    if (same) {
        for (size_t i = 0; i < a.assignments.size(); ++i) {
            if (a.assignments[i].request != b.assignments[i].request ||
                a.assignments[i].slot    != b.assignments[i].slot) {
                same = false; break;
            }
        }
    }
    EXPECT(same, "determinism: identical assignment lists");
    EXPECT(std::abs(a.objectiveValue - b.objectiveValue) < 1e-9,
           "determinism: identical objective");
}

// ---------- Test 13: Phase 2 improves (or matches) Phase 1 ----------
void test_phase2_improves() {
    GeneratorParams gp;
    gp.numRequests = 100;
    gp.numSlots = 50;
    gp.numTutors = 8;
    gp.seed = 7;
    Instance inst = generateInstance(gp);

    auto res = runScheduler(inst);
    EXPECT(static_cast<int>(res.assignments.size()) >= res.phase1Assignments,
           "Phase 2 never decreases assignment count vs Phase 1");
    EXPECT(verifyFeasibility(inst, res), "phase2: feasible");
}

// ---------- Test 14: strategy-proofness sanity (DA round) ----------
// A student who misreports their preferences cannot strictly improve their outcome.
// We construct a small instance and test that flipping the preference list of one
// student does not give them a strictly better assignment.
void test_strategy_proof_sanity() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160, 1));
    inst.slots.push_back(makeSlot(1, 0, 0, 0, 200, 260, 1));

    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0, 1}, 1));
    inst.requests.push_back(makeReq(1, 101, 0, Pool::Undergrad, {0, 1}, 2));

    auto honest = runScheduler(inst);
    SlotID honestSlotForReq1 = -1;
    for (auto& a : honest.assignments) if (a.request == 1) honestSlotForReq1 = a.slot;

    // Now misreport: req 1 lies and ranks {1, 0} instead of {0, 1}
    inst.requests[1].preferences = {1, 0};
    auto liar = runScheduler(inst);
    SlotID liarSlotForReq1 = -1;
    for (auto& a : liar.assignments) if (a.request == 1) liarSlotForReq1 = a.slot;

    // In honest report, req 1 lost slot 0 to req 0 (earlier timestamp) and got slot 1.
    // In lying report, req 1 reports slot 1 first; they get slot 1.
    // Either way, req 1's honest rank-0 (slot 0) outcome is not strictly improved.
    EXPECT(honestSlotForReq1 == 1, "SP sanity: honest req 1 gets slot 1");
    EXPECT(liarSlotForReq1   == 1, "SP sanity: lying req 1 still gets slot 1");
}

// ---------- Test 15: arrival event ----------
void test_arrival_event() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160, 1));
    inst.slots.push_back(makeSlot(1, 0, 0, 0, 200, 260, 1));
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0}, 1));

    auto res = runScheduler(inst);
    EXPECT(reqAssignedTo(res, 0, 0), "arrival: initial req placed");

    Request late = makeReq(1, 101, 0, Pool::Undergrad, {0, 1}, 999);
    inst.requests.push_back(late);
    auto ev = onRequestArrived(late, inst, res);
    EXPECT(ev.accepted, "arrival: late request placed");
    EXPECT(ev.assignment.slot == 1, "arrival: late request -> slot 1 (slot 0 full)");
}

// ---------- Test 16: cancellation event refills slot ----------
void test_cancellation_event() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160, 1));
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0}, 1));
    inst.requests.push_back(makeReq(1, 101, 0, Pool::Undergrad, {0}, 2));

    auto res = runScheduler(inst);
    EXPECT(reqAssignedTo(res, 0, 0), "cancel: req 0 initially in slot 0");
    EXPECT(reqUnassigned(res, 1), "cancel: req 1 initially unassigned");

    auto ev = onCancellation(0, inst, res);
    EXPECT(ev.accepted, "cancel: cancellation accepted");
    EXPECT(reqAssignedTo(res, 1, 0), "cancel: waiting list filled freed slot");
}

// ---------- Test 17: verifyFeasibility catches a bogus assignment ----------
void test_verifier_catches_bogus() {
    Instance inst;
    inst.tutors.push_back(makeTutor(0, Pool::Undergrad, {0}));
    inst.rooms.push_back(makeRoom(0));
    inst.slots.push_back(makeSlot(0, 0, 0, 0, 100, 160, 1));
    inst.requests.push_back(makeReq(0, 100, 0, Pool::Undergrad, {0}, 1));

    ScheduleResult bogus;
    bogus.assignments.push_back({0, 999});  // nonexistent slot
    std::string why;
    EXPECT(!verifyFeasibility(inst, bogus, &why), "verifier: catches unknown slot");
    EXPECT(!why.empty(), "verifier: reports a reason");
}

// ---------- Test 18: large-ish smoke ----------
void test_medium_instance() {
    GeneratorParams gp;
    gp.numRequests = 300;
    gp.numSlots = 150;
    gp.numTutors = 20;
    gp.numTopics = 6;
    gp.seed = 999;
    Instance inst = generateInstance(gp);
    AlnsParameters p;
    p.maxIterations = 400;
    p.stallLimit = 100;
    auto res = runScheduler(inst, p);
    EXPECT(verifyFeasibility(inst, res), "medium: feasibility holds");
    EXPECT(!res.assignments.empty(), "medium: at least some assignments");
}

} // namespace

int main() {
    test_happy_path();
    test_cohort_priority();
    test_timestamp_tiebreak();
    test_qualification();
    test_language_match();
    test_class_conflict();
    test_tutor_hour_cap();
    test_accessibility();
    test_modality();
    test_empty_preferences();
    test_same_student_overlap();
    test_determinism();
    test_phase2_improves();
    test_strategy_proof_sanity();
    test_arrival_event();
    test_cancellation_event();
    test_verifier_catches_bogus();
    test_medium_instance();

    std::cout << "Passed " << g_passed << " / " << (g_passed + g_failed) << " checks\n";
    return g_failed == 0 ? 0 : 1;
}
