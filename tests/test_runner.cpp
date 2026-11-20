#include "CSVParser.h"
#include "KMeansCluster.h"
#include "Models.h"
#include "RFMCalculator.h"
#include "TierMapper.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::string resolveDatasetPath() {
    std::ifstream probe("data/luxury_5k.csv");
    if (probe.is_open()) {
        return "data/luxury_5k.csv";
    }
    return "../data/luxury_5k.csv";
}

CustomerProfile makeProfile(const std::string& id, int days, int frequency,
                            double spend, double score, int cluster) {
    CustomerProfile p;
    p.customer_id = id;
    p.recency_days = days;
    p.frequency = frequency;
    p.monetary = spend;
    p.rfm_score = score;
    p.cluster_id = cluster;
    return p;
}

// Test 1: CSVParser Module
void testCSVParser() {
    std::cout << "[TEST 1] Verifying CSVParser...\n";

    // 1. Date calculation & calendar edge cases
    assert(CSVParser::calculateDaysElapsed("2026-11-24") == 0);
    assert(CSVParser::calculateDaysElapsed("2026-11-10") == 14);
    assert(CSVParser::calculateDaysElapsed("2026-01-14") == 314);
    assert(CSVParser::calculateDaysElapsed("2024-02-29", "2024-03-01") == 1);  // Leap year
    assert(CSVParser::calculateDaysElapsed("2026-02-29") == -1);                // Non-leap invalid
    assert(CSVParser::calculateDaysElapsed("2026-11-25") == -1);                // Future date
    assert(CSVParser::calculateDaysElapsed("2026/11/10") == -1);                // Bad format

    // 2. Malformed CSV handling (capture stderr to keep test output clean)
    const std::string tempCSV = "temp_parser_dirty_test.csv";
    {
        std::ofstream out(tempCSV);
        out << "CustomerID,Date,Boutique,Category,Amount\r\n"
            << "  Alice , 2026-11-20 , Landmark_HK , Watch , 12500.50 \r\n"
            << "\n"
            << "BadCols,2026-11-20,HK\n"
            << ",2026-11-20,HK,Bag,5000\n"
            << "BadDate,2026-02-30,HK,Bag,5000\n"
            << "NegAmount,2026-11-20,HK,Bag,-200\n"
            << "TrailingText,2026-11-20,HK,Bag,500abc\n"
            << "Bob,2026-11-24,Canton_Rd,RTW,3200\n";
    }

    std::ostringstream errCapture;
    auto* prevErr = std::cerr.rdbuf(errCapture.rdbuf());

    assert(CSVParser::loadTransactions("non_existent_file_xyz.csv").empty());
    std::vector<Transaction> parsed = CSVParser::loadTransactions(tempCSV);

    std::cerr.rdbuf(prevErr);
    std::remove(tempCSV.c_str());

    assert(parsed.size() == 2);
    assert(parsed[0].customer_id == "Alice" && parsed[0].date == "2026-11-20" &&
           parsed[0].boutique == "Landmark_HK" && parsed[0].category == "Watch" &&
           std::abs(parsed[0].amount - 12500.50) < 1e-9);
    assert(parsed[1].customer_id == "Bob" && parsed[1].date == "2026-11-24" &&
           std::abs(parsed[1].amount - 3200.0) < 1e-9);
    assert(errCapture.str().find("[Warning]") != std::string::npos);

    std::cout << "[PASS] CSVParser tests passed!\n\n";
}

// Test 2: Synthetic Dataset Integrity via CSVParser
void testDatasetIntegrity(const std::vector<Transaction>& transactions) {
    std::cout << "[TEST 2] Verifying data/luxury_5k.csv integrity...\n";
    assert(transactions.size() == 5000 && "Error: Expected exactly 5,000 rows");

    std::set<std::string> customers;
    for (const auto& tx : transactions) {
        assert(!tx.customer_id.empty());
        assert(!tx.boutique.empty());
        assert(!tx.category.empty());
        assert(CSVParser::calculateDaysElapsed(tx.date) >= 0);
        assert(tx.amount >= 300.0);
        customers.insert(tx.customer_id);
    }

    assert(customers.count("Peter") == 1 && "Error: Demo persona Peter missing");
    assert(customers.count("Jenny") == 1 && "Error: Demo persona Jenny missing");
    assert(customers.count("Marcus") == 1 && "Error: Demo persona Marcus missing");

    std::cout << "  -> Loaded & verified " << transactions.size() << " transactions across "
              << customers.size() << " unique customers.\n";
    std::cout << "[PASS] Dataset integrity verified!\n\n";
}

// Test 3: RFMCalculator Module
void testRFMCalculator() {
    std::cout << "[TEST 3] Verifying RFMCalculator...\n";
    std::vector<Transaction> sample = {
        {"Peter", "2026-11-10", "HK", "Watch", 45000},
        {"Peter", "2026-09-02", "HK", "Jewelry", 22000},
        {"Jenny", "2025-12-05", "HK", "Bag", 18000},
        {"Jenny", "2026-01-14", "HK", "Coat", 12500},
        {"Marcus", "2026-11-12", "HK", "Tie", 450}
    };

    auto profiles = RFMCalculator::computeProfiles(sample);
    assert(profiles.size() == 3);
    assert(profiles[0].customer_id == "Jenny" && profiles[0].recency_days == 314 &&
           profiles[0].frequency == 2 && profiles[0].monetary == 30500);
    assert(profiles[1].customer_id == "Marcus" && profiles[1].recency_days == 12 &&
           profiles[1].frequency == 1 && profiles[1].monetary == 450);
    assert(profiles[2].customer_id == "Peter" && profiles[2].recency_days == 14 &&
           profiles[2].frequency == 2 && profiles[2].monetary == 67000);

    for (const auto& p : profiles) {
        assert(std::isfinite(p.rfm_score) && p.rfm_score >= 0.0 && p.rfm_score <= 1.0);
        assert(p.cluster_id == -1);
    }

    // Dirty rows filtering
    auto dirty = sample;
    dirty.push_back({"", "2026-11-24", "HK", "Watch", 5000});
    dirty.push_back({"Peter", "2026-11-24", "HK", "Refund", -45000});
    dirty.push_back({"Peter", "2026-11-24", "HK", "Free", 0});
    dirty.push_back({"RefundOnly", "2026-11-24", "HK", "Refund", -100});
    dirty.push_back({"ZeroYear", "0000-01-01", "HK", "Watch", 1000});
    dirty.push_back({"InvalidDate", "2026-02-30", "HK", "Watch", 1000});
    dirty.push_back({"FutureDate", "2026-11-25", "HK", "Watch", 1000});
    dirty.push_back({"NaNAmount", "2026-11-24", "HK", "Watch", std::numeric_limits<double>::quiet_NaN()});

    auto cleaned = RFMCalculator::computeProfiles(dirty);
    assert(cleaned.size() == profiles.size());
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        assert(cleaned[i].customer_id == profiles[i].customer_id);
        assert(cleaned[i].recency_days == profiles[i].recency_days);
        assert(cleaned[i].frequency == profiles[i].frequency);
        assert(cleaned[i].monetary == profiles[i].monetary);
        assert(std::abs(cleaned[i].rfm_score - profiles[i].rfm_score) < 1e-12);
    }

    // Monetary aggregation overflow throws std::overflow_error
    bool overflowThrew = false;
    try {
        RFMCalculator::computeProfiles({
            {"Whale", "2026-11-20", "HK", "Watch", std::numeric_limits<double>::max()},
            {"Whale", "2026-11-21", "HK", "Watch", std::numeric_limits<double>::max()}
        });
    } catch (const std::overflow_error&) {
        overflowThrew = true;
    }
    assert(overflowThrew);

    // Single-shopper neutral score & empty input
    auto single = RFMCalculator::computeProfiles({{"Today", "2026-11-24", "HK", "Watch", 100}});
    assert(single.size() == 1 && single[0].recency_days == 0 && single[0].rfm_score == 0.5);
    assert(RFMCalculator::computeProfiles({}).empty());

    // Weight overflow & negative weight fallback to equal weights
    const double huge = std::numeric_limits<double>::max();
    auto overflow = RFMCalculator::computeProfiles(sample, huge, huge, huge);
    auto negWeight = RFMCalculator::computeProfiles(sample, -1.0, 0.5, 0.5);
    auto equal = RFMCalculator::computeProfiles(sample, 1.0, 1.0, 1.0);
    for (std::size_t i = 0; i < equal.size(); ++i) {
        assert(std::abs(overflow[i].rfm_score - equal[i].rfm_score) < 1e-12);
        assert(std::abs(negWeight[i].rfm_score - equal[i].rfm_score) < 1e-12);
    }

    std::cout << "[PASS] RFMCalculator tests passed!\n\n";
}

// Test 4: KMeansCluster Module
void testKMeansCluster() {
    std::cout << "[TEST 4] Verifying KMeansCluster...\n";

    std::vector<CustomerProfile> empty;
    KMeansCluster::assignClusters(empty, 3, 100);
    assert(empty.empty());

    std::vector<CustomerProfile> single = {makeProfile("Solo", 10, 1, 5000, 0.5, -1)};
    KMeansCluster::assignClusters(single, 5, 50);
    assert(single[0].cluster_id == 0);

    std::vector<CustomerProfile> clamped = {
        makeProfile("A", 5, 5, 50000, 0.9, -1),
        makeProfile("B", 300, 1, 500, 0.1, -1)
    };
    KMeansCluster::assignClusters(clamped, 0, 0);
    for (const auto& p : clamped) {
        assert(p.cluster_id == 0);
    }

    std::vector<CustomerProfile> identical = {
        makeProfile("Id1", 30, 2, 10000, 0.5, -1),
        makeProfile("Id2", 30, 2, 10000, 0.5, -1),
        makeProfile("Id3", 30, 2, 10000, 0.5, -1),
        makeProfile("Id4", 30, 2, 10000, 0.5, -1)
    };
    KMeansCluster::assignClusters(identical, 3, 100);
    for (const auto& p : identical) {
        assert(p.cluster_id >= 0 && p.cluster_id < 3);
    }

    std::vector<CustomerProfile> run1 = {
        makeProfile("P1", 10, 5, 80000, 0.95, -1),
        makeProfile("P2", 15, 4, 65000, 0.88, -1),
        makeProfile("P3", 250, 2, 32000, 0.45, -1),
        makeProfile("P4", 280, 2, 28000, 0.40, -1),
        makeProfile("P5", 20, 1, 600, 0.22, -1),
        makeProfile("P6", 35, 1, 900, 0.18, -1)
    };
    auto run2 = run1;
    KMeansCluster::assignClusters(run1, 3, 100);
    KMeansCluster::assignClusters(run2, 3, 100);
    for (std::size_t i = 0; i < run1.size(); ++i) {
        assert(run1[i].cluster_id >= 0 && run1[i].cluster_id < 3);
        assert(run1[i].cluster_id == run2[i].cluster_id);
    }

    std::cout << "[PASS] KMeansCluster tests passed!\n\n";
}

// Test 5: TierMapper & CSV Export Module
void testTierMapper() {
    std::cout << "[TEST 5] Verifying TierMapper...\n";
    std::vector<CustomerProfile> profiles = {
        makeProfile("Peter", 14, 2, 67000, 0.95, 8),
        makeProfile("Jenny", 314, 2, 30500, 0.45, 3),
        makeProfile("Marcus", 12, 1, 450, 0.20, 3),
        makeProfile("Collector", 20, 4, 15000, 0.85, 8),
        makeProfile("VIP", 10, 3, 25000, 0.75, 8)
    };

    TierMapper::assignTiersAndActions(profiles);
    assert(profiles[0].luxury_tier == "Ultra-VIC");
    assert(profiles[1].luxury_tier == "At-Risk Patron");
    assert(profiles[2].luxury_tier == "Entry Client");
    assert(profiles[3].luxury_tier == "Rising Collector");
    assert(profiles[4].luxury_tier == "VIC");
    for (const auto& p : profiles) {
        assert(!p.recommended_action.empty());
    }

    auto relabeled = profiles;
    for (auto& p : relabeled) {
        p.cluster_id = (p.cluster_id == 8) ? 71 : 92;
    }
    TierMapper::assignTiersAndActions(relabeled);
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        assert(profiles[i].luxury_tier == relabeled[i].luxury_tier);
    }

    std::vector<CustomerProfile> boundary = {
        makeProfile("RiskBoundary", 180, 2, 20000, 0.5, -1),
        makeProfile("LapsedUltra", 180, 2, 90000, 0.9, -1),
        makeProfile("UltraBoundary", 179, 2, 50000, 0.9, -1),
        makeProfile("NoCluster", 10, 4, 1000, 0.99, -1)
    };
    TierMapper::assignTiersAndActions(boundary);
    assert(boundary[0].luxury_tier == "At-Risk Patron");
    assert(boundary[1].luxury_tier == "At-Risk Patron");
    assert(boundary[2].luxury_tier == "Ultra-VIC");
    assert(boundary[3].luxury_tier == "Entry Client");

    const std::string path = "tiermapper_test_report.csv";
    auto escaped = profiles;
    escaped[0].customer_id = "Client, \"A\"";
    escaped[0].recommended_action = "Line one\nLine two";
    TierMapper::exportToCSV(escaped, path);

    std::ifstream input(path);
    assert(input.is_open());
    std::string content((std::istreambuf_iterator<char>(input)), {});
    assert(content.find("\"Client, \"\"A\"\"\"") != std::string::npos);
    assert(content.find("\"Line one\nLine two\"") != std::string::npos);
    input.close();
    std::remove(path.c_str());

    bool threw = false;
    try {
        TierMapper::exportToCSV(profiles, "");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    std::vector<CustomerProfile> empty;
    std::ostringstream capture;
    auto* prevBuf = std::cout.rdbuf(capture.rdbuf());
    TierMapper::printSummaryTable(profiles);
    TierMapper::printSummaryTable(empty);
    std::cout.rdbuf(prevBuf);
    assert(capture.str().find("Peter") != std::string::npos);
    assert(capture.str().find("No customer profiles") != std::string::npos);

    std::cout << "[PASS] TierMapper tests passed!\n\n";
}

// Test 6: Full 4-Stage End-to-End Pipeline on 5k Dataset
void testFullPipelineOn5kDataset(const std::vector<Transaction>& transactions) {
    std::cout << "[TEST 6] Running full CSVParser -> RFM -> KMeans -> TierMapper pipeline...\n";
    auto profiles = RFMCalculator::computeProfiles(transactions);
    assert(!profiles.empty());

    KMeansCluster::assignClusters(profiles, 3, 100);
    TierMapper::assignTiersAndActions(profiles);

    bool foundPeter = false, foundJenny = false, foundMarcus = false;
    std::map<std::string, int> tierCounts;
    for (const auto& p : profiles) {
        assert(p.cluster_id >= 0 && p.cluster_id < 3);
        assert(!p.luxury_tier.empty());
        assert(!p.recommended_action.empty());
        tierCounts[p.luxury_tier]++;

        if (p.customer_id == "Peter") {
            assert(p.luxury_tier == "Ultra-VIC");
            foundPeter = true;
        } else if (p.customer_id == "Jenny") {
            assert(p.luxury_tier == "At-Risk Patron");
            foundJenny = true;
        } else if (p.customer_id == "Marcus") {
            assert(p.luxury_tier == "Entry Client");
            foundMarcus = true;
        }
    }
    assert(foundPeter && foundJenny && foundMarcus);

    std::cout << "  -> Classified " << profiles.size() << " profiles:";
    for (const auto& kv : tierCounts) {
        std::cout << " [" << kv.first << ": " << kv.second << "]";
    }
    std::cout << "\n[PASS] Full 4-stage pipeline integration passed!\n\n";
}

int main() {
    std::cout << "LuxAnalytics Full-Stack QA & Integration Test Runner \n\n";

    testCSVParser();
    std::vector<Transaction> dataset = CSVParser::loadTransactions(resolveDatasetPath());
    testDatasetIntegrity(dataset);
    testRFMCalculator();
    testKMeansCluster();
    testTierMapper();
    testFullPipelineOn5kDataset(dataset);

    std::cout << "All 6 test suites passed! All team modules verified.\n";
    return 0;
}