#ifndef RFM_CALCULATOR_H
#define RFM_CALCULATOR_H

#include <vector>
#include "Models.h"

class RFMCalculator {
public:
    static std::vector<CustomerProfile> computeProfiles(
        const std::vector<Transaction>& transactions,
        double w_recency = 0.3,
        double w_frequency = 0.2,
        double w_monetary = 0.5
    );
};

#endif
