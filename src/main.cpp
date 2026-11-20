#include <chrono>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../include/Models.h"
#include "../include/CSVParser.h"
#include "../include/RFMCalculator.h"
#include "../include/KMeansCluster.h"
#include "../include/TierMapper.h"

struct CLIConfig {
    std::string input_path = "data/luxury_5k.csv";
    std::string output_path = "data/clienteling_report.csv";
    double w_r = 0.3;
    double w_f = 0.2;
    double w_m = 0.5;
    int clusters = 3;
};

void printUsage() {
    std::cout << "Usage: ./lux_engine [options]\n"
              << "  --input <path>        Path to boutique CSV (default: data/luxury_5k.csv)\n"
              << "  --weights <r,f,m>     RFM weights summing to 1.0 (default: 0.3,0.2,0.5)\n"
              << "  --clusters <k>        Number of K-Means client clusters (default: 3)\n"
              << "  --output <path>       Output report CSV path (default: data/clienteling_report.csv)\n"
              << "  --help                Show this help message\n";
}

CLIConfig parseArgs(int argc, char* argv[]) {
    CLIConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage();
            std::exit(0);
        } else if (arg == "--input" && i + 1 < argc) {
            cfg.input_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            cfg.output_path = argv[++i];
        } else if (arg == "--clusters" && i + 1 < argc) {
            cfg.clusters = std::stoi(argv[++i]);
        } else if (arg == "--weights" && i + 1 < argc) {
            std::string w_str = argv[++i];
            char comma;
            std::stringstream ss(w_str);
            ss >> cfg.w_r >> comma >> cfg.w_f >> comma >> cfg.w_m;
        }
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    try {
        CLIConfig cfg = parseArgs(argc, argv);

        std::cout << "=== LuxAnalytics: Luxury Clienteling & RFM Engine ===\n"
                  << "Input Dataset : " << cfg.input_path << "\n"
                  << "RFM Weights   : R=" << cfg.w_r << ", F=" << cfg.w_f << ", M=" << cfg.w_m << "\n"
                  << "K-Means (k)   : " << cfg.clusters << " clusters\n"
                  << "Output Report : " << cfg.output_path << "\n"
                  << "-----------------------------------------------------\n";

        auto start_time = std::chrono::high_resolution_clock::now();

        // Stage 1: Parse boutique receipts (Member 2)
        std::vector<Transaction> transactions = CSVParser::loadTransactions(cfg.input_path);
        std::cout << "[1/4] Loaded " << transactions.size() << " transactions.\n";

        // Stage 2: Compute weighted RFM profiles (Member 3)
        std::vector<CustomerProfile> profiles = RFMCalculator::computeProfiles(
            transactions, cfg.w_r, cfg.w_f, cfg.w_m
        );
        std::cout << "[2/4] Computed RFM profiles for " << profiles.size() << " unique clients.\n";

        // Stage 3: 3D K-Means clustering (Member 4 - Innovation Core)
        KMeansCluster::assignClusters(profiles, cfg.clusters);
        std::cout << "[3/4] Assigned clients into " << cfg.clusters << " behavioral clusters.\n";

        // Stage 4: Assign luxury tiers, print summary, and export CSV (Member 5)
        TierMapper::assignTiersAndActions(profiles);
        TierMapper::exportToCSV(profiles, cfg.output_path);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::cout << "[4/4] Exported clienteling report to " << cfg.output_path
                  << " (Completed in " << duration_ms << " ms)\n\n";

        TierMapper::printSummaryTable(profiles);

    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}