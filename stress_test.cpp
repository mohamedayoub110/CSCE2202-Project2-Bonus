#include "scheduler.hpp"
#include "generator.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

using namespace daalns;

namespace {

struct Regime {
    std::string  label;
    GeneratorParams gp;
    AlnsParameters  ap;
};

void printHeader() {
    std::cout << std::left << std::setw(28) << "regime"
              << std::right << std::setw(8) << "reqs"
              << std::setw(8) << "slots"
              << std::setw(8) << "tutors"
              << std::setw(10) << "phase1"
              << std::setw(10) << "final"
              << std::setw(8) << "iters"
              << std::setw(8) << "impr"
              << std::setw(12) << "objective"
              << std::setw(10) << "ms"
              << std::setw(10) << "feasible"
              << "\n";
    std::cout << std::string(120, '-') << "\n";
}

void runRegime(const Regime& r) {
    Instance inst = generateInstance(r.gp);
    auto t0 = std::chrono::steady_clock::now();
    auto res = runScheduler(inst, r.ap);
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::string violation;
    bool feasible = verifyFeasibility(inst, res, &violation);

    std::cout << std::left << std::setw(28) << r.label
              << std::right << std::setw(8) << inst.requests.size()
              << std::setw(8) << inst.slots.size()
              << std::setw(8) << inst.tutors.size()
              << std::setw(10) << res.phase1Assignments
              << std::setw(10) << res.assignments.size()
              << std::setw(8) << res.phase2Iterations
              << std::setw(8) << res.phase2Improvements
              << std::setw(12) << std::fixed << std::setprecision(2) << res.objectiveValue
              << std::setw(10) << std::fixed << std::setprecision(1) << ms
              << std::setw(10) << (feasible ? "yes" : "NO")
              << "\n";
    if (!feasible) {
        std::cout << "  violation: " << violation << "\n";
    }
}

} // namespace

int main() {
    std::vector<Regime> regimes;
    {
        Regime r; r.label = "tiny_balanced";
        r.gp.numRequests = 60;  r.gp.numSlots = 40;  r.gp.numTutors = 8;
        r.gp.seed = 11; r.ap.maxIterations = 800; r.ap.stallLimit = 200;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "small_undercapacity";
        r.gp.numRequests = 150; r.gp.numSlots = 60;  r.gp.numTutors = 10;
        r.gp.demandPressure = 2.0;
        r.gp.seed = 22; r.ap.maxIterations = 1500; r.ap.stallLimit = 300;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "cohort_heavy";
        r.gp.numRequests = 200; r.gp.numSlots = 80;  r.gp.numTutors = 12;
        r.gp.cohortAcademicRate = 0.25; r.gp.cohortAthleteRate = 0.10;
        r.gp.seed = 33; r.ap.maxIterations = 1500; r.ap.stallLimit = 300;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "language_diverse";
        r.gp.numRequests = 250; r.gp.numSlots = 120; r.gp.numTutors = 16;
        r.gp.arabicRate = 0.40; r.gp.bilingualRate = 0.20;
        r.gp.seed = 44; r.ap.maxIterations = 1500; r.ap.stallLimit = 300;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "online_heavy";
        r.gp.numRequests = 250; r.gp.numSlots = 120; r.gp.numTutors = 16;
        r.gp.onlineRate = 0.60;
        r.gp.seed = 55; r.ap.maxIterations = 1500; r.ap.stallLimit = 300;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "high_class_conflict";
        r.gp.numRequests = 250; r.gp.numSlots = 120; r.gp.numTutors = 16;
        r.gp.classConflictRate = 0.80;
        r.gp.seed = 66; r.ap.maxIterations = 1500; r.ap.stallLimit = 300;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "exam_week_pressure";
        r.gp.numRequests = 400; r.gp.numSlots = 150; r.gp.numTutors = 20;
        r.gp.demandPressure = 3.0;
        r.gp.seed = 77; r.ap.maxIterations = 2000; r.ap.stallLimit = 400;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "medium";
        r.gp.numRequests = 500; r.gp.numSlots = 250; r.gp.numTutors = 25;
        r.gp.seed = 88; r.ap.maxIterations = 1500; r.ap.stallLimit = 300;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "large";
        r.gp.numRequests = 1000; r.gp.numSlots = 500; r.gp.numTutors = 40;
        r.gp.seed = 99; r.ap.maxIterations = 1000; r.ap.stallLimit = 250;
        regimes.push_back(r);
    }
    {
        Regime r; r.label = "xl";
        r.gp.numRequests = 2000; r.gp.numSlots = 1000; r.gp.numTutors = 60;
        r.gp.seed = 100; r.ap.maxIterations = 600; r.ap.stallLimit = 150;
        regimes.push_back(r);
    }

    printHeader();
    for (const auto& r : regimes) runRegime(r);
    return 0;
}
