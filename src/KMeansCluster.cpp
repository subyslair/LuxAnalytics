#include "../include/KMeansCluster.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>
#include <stdexcept>

namespace {

struct Point3D {
    double r = 0.0;
    double f = 0.0;
    double m = 0.0;
};

double squaredDistance3D(const Point3D& a, const Point3D& b) {
    const double dr = a.r - b.r;
    const double df = a.f - b.f;
    const double dm = a.m - b.m;
    return dr * dr + df * df + dm * dm;
}

double normalizeMinMax(double value, double min_val, double max_val) {
    const double span = max_val - min_val;
    if (span <= 1e-12) {
        return 0.0;
    }
    return (value - min_val) / span;
}

} // namespace

void KMeansCluster::assignClusters(std::vector<CustomerProfile>& profiles, int k, int max_iters) {
    if (profiles.empty()) {
        return;
    }

    for (const auto& profile : profiles) {
        if (profile.recency_days < 0 || profile.frequency < 0 ||
            !std::isfinite(profile.monetary) || profile.monetary < 0.0) {
            throw std::invalid_argument("CustomerProfile contains invalid R/F/M values");
        }
    }

    const int n = static_cast<int>(profiles.size());
    const int effective_k = std::max(1, std::min(k, n));
    const int iterations = std::max(1, max_iters);
    constexpr double kEpsilon = 1e-6;
    constexpr unsigned int kSeed = 42u;

    // 1. 提取每位顧客的原始 (recency_days, frequency, monetary) 並計算 Min-Max 邊界
    std::vector<Point3D> raw_points(n);
    Point3D min_pt{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };
    Point3D max_pt{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (int i = 0; i < n; ++i) {
        raw_points[i] = {
            static_cast<double>(profiles[i].recency_days),
            static_cast<double>(profiles[i].frequency),
            profiles[i].monetary
        };

        min_pt.r = std::min(min_pt.r, raw_points[i].r);
        min_pt.f = std::min(min_pt.f, raw_points[i].f);
        min_pt.m = std::min(min_pt.m, raw_points[i].m);

        max_pt.r = std::max(max_pt.r, raw_points[i].r);
        max_pt.f = std::max(max_pt.f, raw_points[i].f);
        max_pt.m = std::max(max_pt.m, raw_points[i].m);
    }

    // 2. 將 3D 特徵正規化至 [0.0, 1.0]，避免 Monetary 數值過大主導距離
    std::vector<Point3D> norm_points(n);
    for (int i = 0; i < n; ++i) {
        norm_points[i] = {
            normalizeMinMax(raw_points[i].r, min_pt.r, max_pt.r),
            normalizeMinMax(raw_points[i].f, min_pt.f, max_pt.f),
            normalizeMinMax(raw_points[i].m, min_pt.m, max_pt.m)
        };
    }

    // 3. 以固定隨機種子洗牌後選取初始質心（保證結果可重現）
    std::vector<int> indices(n);
    for (int i = 0; i < n; ++i) {
        indices[i] = i;
    }
    std::mt19937 rng(kSeed);
    std::shuffle(indices.begin(), indices.end(), rng);

    std::vector<Point3D> centroids(effective_k);
    for (int c = 0; c < effective_k; ++c) {
        centroids[c] = norm_points[indices[c]];
    }

    std::vector<int> assignments(n, 0);

    // 4. 執行 K-Means 迭代（分配 -> 更新質心 -> 收斂檢查）
    for (int iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < n; ++i) {
            int best_cluster = 0;
            double best_dist_sq = squaredDistance3D(norm_points[i], centroids[0]);

            for (int c = 1; c < effective_k; ++c) {
                const double dist_sq = squaredDistance3D(norm_points[i], centroids[c]);
                if (dist_sq < best_dist_sq) {
                    best_dist_sq = dist_sq;
                    best_cluster = c;
                }
            }
            assignments[i] = best_cluster;
        }

        std::vector<Point3D> sums(effective_k, {0.0, 0.0, 0.0});
        std::vector<int> counts(effective_k, 0);

        for (int i = 0; i < n; ++i) {
            const int c = assignments[i];
            sums[c].r += norm_points[i].r;
            sums[c].f += norm_points[i].f;
            sums[c].m += norm_points[i].m;
            ++counts[c];
        }

        double max_shift = 0.0;
        for (int c = 0; c < effective_k; ++c) {
            Point3D next_centroid = centroids[c];
            if (counts[c] > 0) {
                next_centroid = {
                    sums[c].r / static_cast<double>(counts[c]),
                    sums[c].f / static_cast<double>(counts[c]),
                    sums[c].m / static_cast<double>(counts[c])
                };
            } else {
                // 處理空群（empty cluster）：重新指定一個現有點作為質心
                next_centroid = norm_points[indices[c % n]];
            }

            const double shift = std::sqrt(squaredDistance3D(centroids[c], next_centroid));
            max_shift = std::max(max_shift, shift);
            centroids[c] = next_centroid;
        }

        if (max_shift < kEpsilon) {
            break;
        }
    }

    // 5. 將分群結果寫回每位顧客的 cluster_id
    for (int i = 0; i < n; ++i) {
        profiles[i].cluster_id = assignments[i];
    }
}