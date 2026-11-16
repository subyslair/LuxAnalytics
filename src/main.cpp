#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../include/Models.h"

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
    CLIConfig cfg = parseArgs(argc, argv);

    std::cout << "=== LuxAnalytics: Luxury Clienteling & RFM Engine ===\n"
              << "Input Dataset : " << cfg.input_path << "\n"
              << "RFM Weights   : R=" << cfg.w_r << ", F=" << cfg.w_f << ", M=" << cfg.w_m << "\n"
              << "K-Means (k)   : " << cfg.clusters << " clusters\n"
              << "Output Report : " << cfg.output_path << "\n"
              << "-----------------------------------------------------\n"
              << "[Status] CLI ready. Waiting for module implementations to link.\n";

    return 0;
}
