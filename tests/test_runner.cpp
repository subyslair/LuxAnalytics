#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <cassert>

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

void testDatasetIntegrity() {
    std::cout << "[TEST 1] Verifying data/luxury_5k.csv integrity...\n";
    std::ifstream file = openDataset();
    assert(file.is_open() && "Error: Cannot open data/luxury_5k.csv");

    std::string line;
    assert(std::getline(file, line) && "Error: CSV file is empty");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    assert(line == "CustomerID,Date,Boutique,Category,Amount" && "Error: Header mismatch");

    int rowCount = 0;
    std::set<std::string> customers;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<std::string> cols = splitCSVLine(line);
        assert(cols.size() == 5 && "Error: Row does not have 5 columns");

        const std::string& custId = cols[0];
        const std::string& date = cols[1];
        const std::string& boutique = cols[2];
        const std::string& category = cols[3];
        double amount = std::stod(cols[4]);

        assert(!custId.empty() && !boutique.empty() && !category.empty());
        assert(date.size() == 10 && date[4] == '-' && date[7] == '-' && "Error: Invalid YYYY-MM-DD date");
        assert(amount >= 300.0 && "Error: Luxury amount should be >= $300");

        customers.insert(custId);
        rowCount++;
    }

    assert(rowCount == 5000 && "Error: Dataset must contain exactly 5,000 rows");
    assert(customers.count("Peter") == 1 && "Error: Demo persona Peter missing");
    assert(customers.count("Jenny") == 1 && "Error: Demo persona Jenny missing");
    assert(customers.count("Marcus") == 1 && "Error: Demo persona Marcus missing");

    std::cout << "  -> Verified rows: " << rowCount
              << " | Unique customers: " << customers.size() << "\n";
    std::cout << "[PASS] Dataset integrity & demo personas verified!\n\n";
}

int main() {
    std::cout << "=== LuxAnalytics Unit Test Runner ===\n\n";
    testDatasetIntegrity();
    std::cout << "All active tests passed!\n";
    return 0;
}