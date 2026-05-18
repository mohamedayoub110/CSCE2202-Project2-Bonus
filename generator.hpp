#pragma once

#include "scheduler.hpp"
#include <cstdint>

namespace daalns {

struct GeneratorParams {
    int numTutors        = 20;
    int numRooms         = 10;
    int numTopics        = 6;
    int numRequests      = 200;
    int numSlots         = 100;
    int weekStartMin     = 0;        // minutes from epoch
    int dayMinutes       = 600;      // 10-hour day
    int slotMinutes      = 60;
    int daysInWeek       = 5;
    int maxPrefLen       = 5;
    double demandPressure        = 1.5;  // ratio of requests to capacity
    double cohortAcademicRate    = 0.10;
    double cohortAthleteRate     = 0.05;
    double accessibilityRate     = 0.05;
    double arabicRate            = 0.30;
    double bilingualRate         = 0.10;
    double onlineRate            = 0.25;
    double groupSessionRate      = 0.20;
    double classConflictRate     = 0.40; // fraction of students with class blocks
    std::uint64_t seed           = 42ULL;
};

Instance generateInstance(const GeneratorParams& params);

} // namespace daalns
