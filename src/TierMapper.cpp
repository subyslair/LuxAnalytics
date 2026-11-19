#include "../include/TierMapper.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
// PROVISIONAL business rules: agree these values with the team before merging.
constexpr int AT_RISK_DAYS = 180;
constexpr double HIGH_VALUE_SPEND = 20000.0;
constexpr double ULTRA_VIC_SPEND = 50000.0;

struct ClusterStats {
    double score_sum = 0.0;
    std::size_t count = 0;
};

std::string csvCell(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        if (ch == '"') result += '"';
        result += ch;
    }
    return result + '"';
}

std::string money(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}
}

void TierMapper::assignTiersAndActions(std::vector<CustomerProfile>& profiles) {
    std::map<int, ClusterStats> clusters;
    double score_sum = 0.0;
    std::size_t score_count = 0;
    for (const auto& profile : profiles) {
        if (profile.cluster_id >= 0) {
            auto& stats = clusters[profile.cluster_id];
            stats.score_sum += profile.rfm_score;
            ++stats.count;
            score_sum += profile.rfm_score;
            ++score_count;
        }
    }
    const double overall_mean = score_count ? score_sum / score_count : 0.0;

    for (auto& profile : profiles) {
        bool strong_cluster = false;
        bool strong_score = false;
        const auto found = clusters.find(profile.cluster_id);
        if (found != clusters.end()) {
            const auto& stats = found->second;
            const double cluster_mean = stats.score_sum / stats.count;
            strong_cluster = cluster_mean > overall_mean;
            strong_score = profile.rfm_score >= cluster_mean;
        }

        // Priority matters: a lapsed high spender is flagged before VIP rules.
        if (profile.recency_days >= AT_RISK_DAYS &&
            profile.monetary >= HIGH_VALUE_SPEND) {
            profile.luxury_tier = "At-Risk Patron";
            profile.recommended_action = "Assign senior stylist for personal outreach";
        } else if (profile.monetary >= ULTRA_VIC_SPEND) {
            profile.luxury_tier = "Ultra-VIC";
            profile.recommended_action = "Invite to private salon preview and limited-edition viewing";
        } else if (profile.monetary >= HIGH_VALUE_SPEND) {
            profile.luxury_tier = "VIC";
            profile.recommended_action = "Offer private appointment and curated collection preview";
        } else if (profile.recency_days < AT_RISK_DAYS &&
                   profile.frequency >= 2 && strong_cluster && strong_score) {
            profile.luxury_tier = "Rising Collector";
            profile.recommended_action = "Arrange advisor consultation and personalized recommendations";
        } else {
            profile.luxury_tier = "Entry Client";
            profile.recommended_action = "Offer seasonal digital lookbook with appropriate consent";
        }
    }
}

void TierMapper::printSummaryTable(const std::vector<CustomerProfile>& profiles) {
    // Use a local stream so the caller's cout formatting is unchanged.
    std::ostringstream table;
    table << "\n=== LuxAnalytics Clienteling Report ===\n";
    if (profiles.empty()) {
        table << "No customer profiles available.\n";
    } else {
        table << std::left << std::setw(18) << "CustomerID"
              << std::right << std::setw(9) << "Recency"
              << std::setw(10) << "Frequency" << std::setw(15) << "Monetary"
              << std::setw(12) << "RFM Score" << std::setw(9) << "Cluster"
              << "  Tier / Recommended Action\n";
        table << std::string(112, '-') << '\n';
        for (const auto& profile : profiles) {
            table << std::left << std::setw(18) << profile.customer_id
                  << std::right << std::setw(9) << profile.recency_days
                  << std::setw(10) << profile.frequency
                  << std::setw(15) << money(profile.monetary)
                  << std::setw(12) << money(profile.rfm_score)
                  << std::setw(9) << profile.cluster_id
                  << "  " << profile.luxury_tier << '\n'
                  << "  Action: " << profile.recommended_action << '\n';
        }
        table << "Total customers: " << profiles.size() << '\n';
    }
    std::cout << table.str();
}

void TierMapper::exportToCSV(const std::vector<CustomerProfile>& profiles,
                             const std::string& output_path) {
    std::ofstream file(output_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open report file: " + output_path);
    }
    file << "CustomerID,RecencyDays,Frequency,Monetary,RFMScore,ClusterID,LuxuryTier,RecommendedAction\n";
    for (const auto& profile : profiles) {
        file << csvCell(profile.customer_id) << ','
             << profile.recency_days << ',' << profile.frequency << ','
             << std::fixed << std::setprecision(2) << profile.monetary << ','
             << std::setprecision(6) << profile.rfm_score << ','
             << profile.cluster_id << ',' << csvCell(profile.luxury_tier) << ','
             << csvCell(profile.recommended_action) << '\n';
    }
    file.flush();
    if (!file) {
        throw std::runtime_error("Failed to write report file: " + output_path);
    }
    file.close();
    if (file.fail()) {
        throw std::runtime_error("Failed to close report file: " + output_path);
    }
}
