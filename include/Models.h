#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <vector>

struct Transaction {
    std::string customer_id;
    std::string date;       // Format: YYYY-MM-DD
    std::string boutique;   // e.g., "Landmark_HK", "Canton_Rd", "Place_Vendome"
    std::string category;   // e.g., "Haute_Horlogerie", "Leather_Goods", "RTW"
    double amount = 0.0;
};

struct CustomerProfile {
    std::string customer_id;
    int recency_days = 0;
    int frequency = 0;
    double monetary = 0.0;
    double rfm_score = 0.0;
    int cluster_id = -1;
    std::string luxury_tier;        // e.g., "Ultra-VIC", "At-Risk Patron", "Entry Client"
    std::string recommended_action; // e.g., "Private Salon Invite", "Stylist Outreach"
};

#endif
