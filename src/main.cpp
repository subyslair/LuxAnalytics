#include <iostream>
#include <vector>
#include "../include/Models.h"

int main(int argc, char* argv[]) {
    std::cout << "=== LuxAnalytics: Luxury Clienteling & RFM Engine ===" << std::endl;

    // Sanity check using our sample clients
    Transaction peter{"Peter", "2026-11-10", "Landmark_HK", "Haute_Horlogerie", 45000.0};
    Transaction jenny{"Jenny", "2026-01-14", "Canton_Rd", "Ready_To_Wear", 12500.0};

    std::cout << "Loaded sample receipt: " << peter.customer_id
              << " ($" << peter.amount << " at " << peter.boutique << ")" << std::endl;
    std::cout << "Loaded sample receipt: " << jenny.customer_id
              << " ($" << jenny.amount << " at " << jenny.boutique << ")" << std::endl;

    return 0;
}
