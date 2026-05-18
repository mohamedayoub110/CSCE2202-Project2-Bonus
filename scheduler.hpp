#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace daalns {

using StudentID = int;
using TutorID   = int;
using SlotID    = int;
using RoomID    = int;
using TopicID   = int;
using RequestID = int;
using TimePoint = std::int64_t;

enum class Cohort   { Standard, AcademicWarning, ProfessionalAthlete };
enum class Modality { Online, InPerson, Either };
enum class Language { English, Arabic, Bilingual };
enum class Pool     { Undergrad, Grad };

struct TimeWindow {
    TimePoint start;
    TimePoint end;
};

struct Tutor {
    TutorID                 id;
    Pool                    pool;
    std::vector<TopicID>    qualifiedTopics;
    Language                language;
    int                     weeklyMinuteCap;
    std::vector<TimeWindow> availability;
};

struct Room {
    RoomID   id;
    int      capacity;
    bool     accessible;
    Modality modality;
};

struct Slot {
    SlotID    id;
    TutorID   tutor;
    RoomID    room;
    TopicID   topic;
    Language  language;
    Modality  modality;
    Pool      pool;
    TimePoint start;
    TimePoint end;
    int       capacity;
};

struct Request {
    RequestID               id;
    StudentID               student;
    Cohort                  cohort;
    TopicID                 topic;
    Pool                    pool;
    Modality                modalityPref;
    Language                languagePref;
    int                     maxGroupSize;
    bool                    needsAccessibility;
    std::vector<SlotID>     preferences;
    TimePoint               submittedAt;
    std::vector<TimeWindow> classSchedule;
};

struct Assignment {
    RequestID request;
    SlotID    slot;
};

struct UnassignedRequest {
    RequestID   request;
    std::string reason;
};

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

struct Instance {
    std::vector<Tutor>   tutors;
    std::vector<Room>    rooms;
    std::vector<Slot>    slots;
    std::vector<Request> requests;
};

ScheduleResult runScheduler(const Instance& inst,
                            const AlnsParameters& params = {});

struct EventResult {
    bool        accepted;
    Assignment  assignment;
    std::string note;
};

EventResult onRequestArrived(const Request& req,
                             const Instance& inst,
                             ScheduleResult& current,
                             const AlnsParameters& params = {});

EventResult onCancellation(RequestID req,
                           const Instance& inst,
                           ScheduleResult& current,
                           const AlnsParameters& params = {});

bool verifyFeasibility(const Instance& inst,
                       const ScheduleResult& result,
                       std::string* violation = nullptr);

double computeObjective(const Instance& inst,
                        const ScheduleResult& result,
                        const AlnsParameters& params = {});

} // namespace daalns
