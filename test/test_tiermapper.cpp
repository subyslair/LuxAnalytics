#include "../include/TierMapper.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

CustomerProfile customer(const std::string& id, int days, int frequency,
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

int main() {
    std::vector<CustomerProfile> profiles = {
        customer("Peter", 7, 2, 67000, 4.8, 8),
        customer("Jenny", 307, 2, 30500, 2.5, 3),
        customer("Marcus", 5, 1, 450, 2.0, 3),
        customer("Collector", 20, 4, 15000, 4.6, 8),
        customer("VIP", 10, 3, 25000, 4.0, 8)
    };
    TierMapper::assignTiersAndActions(profiles);
    assert(profiles[0].luxury_tier == "Ultra-VIC");
    assert(profiles[1].luxury_tier == "At-Risk Patron");
    assert(profiles[2].luxury_tier == "Entry Client");
    assert(profiles[3].luxury_tier == "Rising Collector");
    assert(profiles[4].luxury_tier == "VIC");
    for (const auto& p : profiles) assert(!p.recommended_action.empty());

    auto renamed = profiles;
    for (auto& p : renamed) p.cluster_id = p.cluster_id == 8 ? 71 : 92;
    TierMapper::assignTiersAndActions(renamed);
    for (std::size_t i = 0; i < profiles.size(); ++i)
        assert(profiles[i].luxury_tier == renamed[i].luxury_tier);

    std::vector<CustomerProfile> boundary = {
        customer("RiskBoundary", 180, 2, 20000, 3, -1),
        customer("LapsedUltra", 180, 2, 90000, 4, -1),
        customer("UltraBoundary", 179, 2, 50000, 4, -1),
        customer("NoCluster", 10, 4, 1000, 100, -1)
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
    std::string content((std::istreambuf_iterator<char>(input)), {});
    assert(content.find("\"Client, \"\"A\"\"\"") != std::string::npos);
    assert(content.find("\"Line one\nLine two\"") != std::string::npos);
    input.close();
    std::remove(path.c_str());

    std::vector<CustomerProfile> empty;
    TierMapper::assignTiersAndActions(empty);
    TierMapper::exportToCSV(empty, path);
    std::ifstream empty_file(path);
    std::string line;
    assert(static_cast<bool>(std::getline(empty_file, line)));
    assert(!std::getline(empty_file, line));
    empty_file.close();
    std::remove(path.c_str());

    bool threw = false;
    try { TierMapper::exportToCSV(profiles, ""); }
    catch (const std::runtime_error&) { threw = true; }
    assert(threw);

    std::ostringstream capture;
    auto* previous = std::cout.rdbuf(capture.rdbuf());
    TierMapper::printSummaryTable(profiles);
    TierMapper::printSummaryTable(empty);
    std::cout.rdbuf(previous);
    assert(capture.str().find("Peter") != std::string::npos);
    assert(capture.str().find("No customer profiles") != std::string::npos);
    std::cout << capture.str();
    std::cout << "All TierMapper tests passed.\n";
}
