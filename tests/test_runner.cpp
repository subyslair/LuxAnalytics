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
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Helper Functions
std::ifstream openDataset() {
    std::ifstream file("data/luxury_5k.csv");
    if (!file.is_open()) {
        file.open("../data/luxury_5k.csv");
    }
    return file;
}

std::vector<std::string> splitCSVLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty() && item.back() == '\r') {
            item.pop_back();
        }
        tokens.push_back(item);
    }
    return tokens;
}

std::vector<Transaction> loadDatasetTransactions() {
    std::ifstream file = openDataset();
    assert(file.is_open() && "Error: Cannot open data/luxury_5k.csv");

    std::string line;
    assert(std::getline(file, line) && "Error: CSV file is empty");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    assert(line == "CustomerID,Date,Boutique,Category,Amount" && "Error: Header mismatch");

    std::vector<Transaction> transactions;
    transactions.reserve(5000);

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<std::string> cols = splitCSVLine(line);
        assert(cols.size() == 5 && "Error: Row does not have 5 columns");

        Transaction tx;
        tx.customer_id = cols[0];
        tx.date = cols[1];
        tx.boutique = cols[2];
        tx.category = cols[3];
        tx.amount = std::stod(cols[4]);
        transactions.push_back(std::move(tx));
    }
    return transactions;
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

// Test 1: Synthetic Dataset Integrity
void testDatasetIntegrity(const std::vector<Transaction>& transactions) {
    std::cout << "[TEST 1] Verifying data/luxury_5k.csv integrity...\n";
    assert(transactions.size() == 5000 && "Error: Expected exactly 5,000 rows");

    std::set<std::string> customers;
    for (const auto& tx : transactions) {
        assert(!tx.customer_id.empty());
        assert(!tx.boutique.empty());
        assert(!tx.category.empty());
        assert(tx.date.size() == 10 && tx.date[4] == '-' && tx.date[7] == '-');
        assert(tx.amount >= 300.0);
        customers.insert(tx.customer_id);
    }

    assert(customers.count("Peter") == 1 && "Error: Demo persona Peter missing");
    assert(customers.count("Jenny") == 1 && "Error: Demo persona Jenny missing");
    assert(customers.count("Marcus") == 1 && "Error: Demo persona Marcus missing");

    std::cout << "  -> Verified " << transactions.size() << " transactions across "
              << customers.size() << " unique customers.\n";
    std::cout << "[PASS] Dataset integrity verified!\n\n";
}

// Test 2: RFMCalculator Module
void testRFMCalculator() {
    std::cout << "[TEST 2] Verifying RFMCalculator...\n";
    std::vector<Transaction> sample = {
        {"Peter", "2026-11-10", "HK", "Watch", 45000},
        {"Peter", "2026-09-02", "HK", "Jewelry", 22000},
        {"Jenny", "2025-12-05", "HK", "Bag", 18000},
        {"Jenny", "2026-01-14", "HK", "Coat", 12500},
        {"Marcus", "2026-11-12", "HK", "Tie", 450}
    };

    auto profiles = RFMCalculator::computeProfiles(sample);
    assert(profiles.size() == 3);
    // Alphabetical ordering check: Jenny -> Marcus -> Peter
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

    // Dirty rows filtering (negative refunds, zero, invalid calendar date, future date, NaN, empty ID)
    auto dirty = sample;
    dirty.push_back({"", "2026-11-24", "HK", "Watch", 5000});
    dirty.push_back({"Peter", "2026-11-24", "HK", "Refund", -45000});
    dirty.push_back({"Peter", "2026-11-24", "HK", "Free", 0});
    dirty.push_back({"RefundOnly", "2026-11-24", "HK", "Refund", -100});
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

// Test 3: TierMapper & CSV Export Module
void testTierMapper() {
    std::cout << "[TEST 3] Verifying TierMapper...\n";
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

    // Cluster label invariance
    auto relabeled = profiles;
    for (auto& p : relabeled) {
        p.cluster_id = (p.cluster_id == 8) ? 71 : 92;
    }
    TierMapper::assignTiersAndActions(relabeled);
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        assert(profiles[i].luxury_tier == relabeled[i].luxury_tier);
    }

    // Threshold boundaries & priority precedence (At-Risk beats Ultra-VIC)
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

    // CSV quoting & escaping
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

    // Invalid output path throws runtime_error
    bool threw = false;
    try {
        TierMapper::exportToCSV(profiles, "");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Summary table formatting check (capture stdout cleanly)
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

// Test 4: End-to-End Pipeline Integration on 5k Dataset
void testPipelineOn5kDataset(const std::vector<Transaction>& transactions) {
    std::cout << "[TEST 4] Running RFM + TierMapper pipeline on luxury_5k.csv...\n";
    auto profiles = RFMCalculator::computeProfiles(transactions);
    assert(!profiles.empty());

    // Note: KMeansCluster (Member 4) will plug in here once implemented.
    TierMapper::assignTiersAndActions(profiles);

    bool foundPeter = false, foundJenny = false, foundMarcus = false;
    for (const auto& p : profiles) {
        assert(!p.luxury_tier.empty());
        assert(!p.recommended_action.empty());
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

    std::cout << "  -> Classified " << profiles.size()
              << " customer profiles; demo personas verified.\n";
    std::cout << "[PASS] End-to-end 5k pipeline integration passed!\n\n";
}

int main() {
    std::cout << "=== LuxAnalytics Integrated Unit & Pipeline Test Runner ===\n\n";

    std::vector<Transaction> dataset = loadDatasetTransactions();
    testDatasetIntegrity(dataset);
    testRFMCalculator();
    testTierMapper();
    testPipelineOn5kDataset(dataset);

    // Pending teammate modules:
    // testCSVParser();      // CSVParser.h
    // testKMeansCluster();  // KMeansCluster.h

    std::cout << "All active unit & integration tests passed!\n";
    return 0;
}
