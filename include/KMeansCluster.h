#ifndef KMEANS_CLUSTER_H
#define KMEANS_CLUSTER_H

#include <vector>
#include "Models.h"

class KMeansCluster {
public:
    static void assignClusters(std::vector<CustomerProfile>& profiles, int k = 3, int max_iters = 100);
};

#endif
