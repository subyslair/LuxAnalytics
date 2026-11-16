#ifndef TIER_MAPPER_H
#define TIER_MAPPER_H

#include <string>
#include <vector>
#include "Models.h"

class TierMapper {
public:
    static void assignTiersAndActions(std::vector<CustomerProfile>& profiles);
    static void printSummaryTable(const std::vector<CustomerProfile>& profiles);
    static void exportToCSV(const std::vector<CustomerProfile>& profiles, const std::string& output_path);
};

#endif
