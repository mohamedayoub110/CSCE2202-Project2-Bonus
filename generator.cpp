#include "generator.hpp"

#include <algorithm>
#include <random>
#include <unordered_set>

namespace daalns {

namespace {

template <typename T>
T pickOne(const std::vector<T>& v, std::mt19937_64& rng) {
    std::uniform_int_distribution<int> d(0, static_cast<int>(v.size()) - 1);
    return v[d(rng)];
}

bool bernoulli(double p, std::mt19937_64& rng) {
    std::uniform_real_distribution<double> u(0.0, 1.0);
    return u(rng) < p;
}

} // namespace

Instance generateInstance(const GeneratorParams& p) {
    std::mt19937_64 rng(p.seed);
    Instance inst;

    // ---------- Topics ----------
    std::vector<TopicID> topics;
    for (int i = 0; i < p.numTopics; ++i) topics.push_back(i);

    // ---------- Tutors ----------
    inst.tutors.reserve(p.numTutors);
    for (int i = 0; i < p.numTutors; ++i) {
        Tutor t;
        t.id = i;
        t.pool = (i < p.numTutors / 2) ? Pool::Undergrad : Pool::Grad;
        // qualifications: 2-4 random topics
        std::uniform_int_distribution<int> qcount(2, std::min(4, p.numTopics));
        int nq = qcount(rng);
        std::unordered_set<TopicID> qs;
        while ((int)qs.size() < nq) qs.insert(pickOne(topics, rng));
        t.qualifiedTopics.assign(qs.begin(), qs.end());
        // language
        double u = std::uniform_real_distribution<double>(0,1)(rng);
        if      (u < p.bilingualRate)              t.language = Language::Bilingual;
        else if (u < p.bilingualRate + p.arabicRate) t.language = Language::Arabic;
        else                                       t.language = Language::English;
        // weekly cap
        t.weeklyMinuteCap = (t.pool == Pool::Undergrad) ? 10 * 60 : 20 * 60;
        // availability: every day of the week, but only partial windows
        for (int d = 0; d < p.daysInWeek; ++d) {
            int dayStart = p.weekStartMin + d * 24 * 60;
            std::uniform_int_distribution<int> offset(0, p.dayMinutes / 2);
            int s = offset(rng);
            int e = std::min(p.dayMinutes, s + 6 * 60);
            t.availability.push_back({dayStart + s, dayStart + e});
        }
        inst.tutors.push_back(t);
    }

    // ---------- Rooms ----------
    inst.rooms.reserve(p.numRooms);
    for (int i = 0; i < p.numRooms; ++i) {
        Room r;
        r.id = i;
        r.capacity = bernoulli(p.groupSessionRate, rng) ? 4 : 1;
        r.accessible = bernoulli(0.30, rng);
        if (i < p.numRooms / 4) r.modality = Modality::Online;
        else                    r.modality = Modality::InPerson;
        inst.rooms.push_back(r);
    }

    // ---------- Slots ----------
    inst.slots.reserve(p.numSlots);
    SlotID nextSlot = 0;
    for (int i = 0; i < p.numSlots; ++i) {
        Slot s;
        s.id = nextSlot++;
        const Tutor& t = inst.tutors[i % p.numTutors];
        s.tutor = t.id;
        s.pool = t.pool;
        // pick a topic the tutor is qualified for
        s.topic = pickOne(t.qualifiedTopics, rng);
        s.language = t.language;
        // try to choose a slot start inside the tutor's availability windows
        const TimeWindow& w = pickOne(t.availability, rng);
        int span = static_cast<int>(w.end - w.start);
        if (span < p.slotMinutes) {
            s.start = w.start;
            s.end   = w.start + p.slotMinutes;
        } else {
            std::uniform_int_distribution<int> d(0, span - p.slotMinutes);
            int off = d(rng);
            s.start = w.start + off;
            s.end   = s.start + p.slotMinutes;
        }
        // pick a room with matching modality
        std::vector<RoomID> matching;
        for (const auto& room : inst.rooms) {
            if (bernoulli(p.onlineRate, rng) && room.modality == Modality::Online)
                matching.push_back(room.id);
            else if (room.modality == Modality::InPerson)
                matching.push_back(room.id);
        }
        if (matching.empty()) matching.push_back(inst.rooms[0].id);
        s.room = pickOne(matching, rng);
        const Room& chosen = inst.rooms[s.room];
        s.modality = chosen.modality;
        s.capacity = std::min(chosen.capacity,
                              bernoulli(p.groupSessionRate, rng) ? 3 : 1);
        inst.slots.push_back(s);
    }

    // ---------- Requests ----------
    inst.requests.reserve(p.numRequests);
    int totalCapacity = 0;
    for (const auto& s : inst.slots) totalCapacity += s.capacity;
    int targetRequests = static_cast<int>(p.demandPressure * totalCapacity);
    int actualRequests = std::min(p.numRequests, std::max(1, targetRequests));

    RequestID nextReq = 0;
    StudentID nextStudent = 0;
    for (int i = 0; i < actualRequests; ++i) {
        Request r;
        r.id = nextReq++;
        r.student = nextStudent++;
        // cohort
        double u = std::uniform_real_distribution<double>(0,1)(rng);
        if      (u < p.cohortAcademicRate) r.cohort = Cohort::AcademicWarning;
        else if (u < p.cohortAcademicRate + p.cohortAthleteRate) r.cohort = Cohort::ProfessionalAthlete;
        else r.cohort = Cohort::Standard;
        r.topic = pickOne(topics, rng);
        r.pool = bernoulli(0.5, rng) ? Pool::Undergrad : Pool::Grad;
        // modality preference
        u = std::uniform_real_distribution<double>(0,1)(rng);
        if      (u < p.onlineRate)        r.modalityPref = Modality::Online;
        else if (u < p.onlineRate + 0.50) r.modalityPref = Modality::InPerson;
        else                              r.modalityPref = Modality::Either;
        // language preference
        u = std::uniform_real_distribution<double>(0,1)(rng);
        if      (u < p.bilingualRate)                  r.languagePref = Language::Bilingual;
        else if (u < p.bilingualRate + p.arabicRate)   r.languagePref = Language::Arabic;
        else                                           r.languagePref = Language::English;
        r.maxGroupSize = bernoulli(p.groupSessionRate, rng) ? 4 : 1;
        r.needsAccessibility = bernoulli(p.accessibilityRate, rng);
        // preferences: find candidate slots that match topic and pool
        std::vector<SlotID> candidates;
        for (const auto& s : inst.slots) {
            if (s.topic == r.topic && s.pool == r.pool) candidates.push_back(s.id);
        }
        std::shuffle(candidates.begin(), candidates.end(), rng);
        std::uniform_int_distribution<int> plen(1, p.maxPrefLen);
        int L = std::min(static_cast<int>(candidates.size()), plen(rng));
        for (int k = 0; k < L; ++k) r.preferences.push_back(candidates[k]);
        // submission time
        std::uniform_int_distribution<TimePoint> ts(0, 10000);
        r.submittedAt = ts(rng);
        // class schedule
        if (bernoulli(p.classConflictRate, rng)) {
            int day = std::uniform_int_distribution<int>(0, p.daysInWeek - 1)(rng);
            int dayStart = p.weekStartMin + day * 24 * 60;
            int s = std::uniform_int_distribution<int>(0, p.dayMinutes - 60)(rng);
            r.classSchedule.push_back({dayStart + s, dayStart + s + 60});
        }
        inst.requests.push_back(r);
    }

    return inst;
}

} // namespace daalns
