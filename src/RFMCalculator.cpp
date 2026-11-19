// ===========================================================================
//  RFMCalculator.cpp  --  Stage 2: the RFM scoring engine (implementation)
//
//  Owner  : Member 3 (Lan Ka Sen)
//  Branch : feature-rfm-scoring
//
//  Pipeline contract
//    in  : std::vector<Transaction>  from CSVParser  (Member 2)
//    out : std::vector<CustomerProfile>
//            recencyDays / frequency / monetary   -> K-Means 3D point (Member 4)
//            rScore / fScore / mScore / weightedScore -> ranking + tiers (Member 5)
//
//  Everything below is standard-library only: <algorithm>, <cmath>,
//  <unordered_map>, <sstream>. No external dependency, C++17.
// ===========================================================================
#include "RFMCalculator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace {

// ---------------------------------------------------------------------------
// Average ranks (1-based) for a column, so tied values share the mean rank.
// Used by the Quantile normalisation; without it, 200 customers sitting on
// exactly $450.00 would be ordered arbitrarily by row position.
// ---------------------------------------------------------------------------
std::vector<double> averageRanks(const std::vector<double>& values) {
    const std::size_t n = values.size();
    std::vector<std::size_t> order(n);
    for (std::size_t i = 0; i < n; ++i) {
        order[i] = i;
    }

    // Stable on ties via the index comparison, which keeps the sort a valid
    // strict weak ordering even when two receipts are numerically identical.
    std::sort(order.begin(), order.end(), [&values](std::size_t a, std::size_t b) {
        if (values[a] != values[b]) {
            return values[a] < values[b];
        }
        return a < b;
    });

    std::vector<double> ranks(n, 0.0);
    std::size_t i = 0;
    while (i < n) {
        std::size_t j = i;
        while (j + 1 < n && values[order[j + 1]] == values[order[i]]) {
            ++j;
        }
        // ranks i+1 .. j+1 all tie -> give them their average.
        const double avgRank =
            (static_cast<double>(i + 1) + static_cast<double>(j + 1)) / 2.0;
        for (std::size_t k = i; k <= j; ++k) {
            ranks[order[k]] = avgRank;
        }
        i = j + 1;
    }
    return ranks;
}

// ---------------------------------------------------------------------------
// Min-max scaling onto [0,1]. A column with no spread (every shopper equal)
// maps to a neutral 0.5 instead of dividing by zero, which keeps single-row
// and all-tied datasets mathematically harmless.
// ---------------------------------------------------------------------------
std::vector<double> minMax(const std::vector<double>& values) {
    std::vector<double> scaled(values.size(), 0.5);
    if (values.empty()) {
        return scaled;
    }

    double lo = values.front();
    double hi = values.front();
    for (double v : values) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }

    const double span = hi - lo;
    if (!(span > 0.0) || !std::isfinite(span)) {
        return scaled;  // constant column -> everyone neutral
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        scaled[i] = rfm_detail::clamp01((values[i] - lo) / span);
    }
    return scaled;
}

// ---------------------------------------------------------------------------
// Raw column -> comparable column, according to the chosen mode. Every mode
// ends in a min-max pass so the three scores always share one scale.
// ---------------------------------------------------------------------------
std::vector<double> transformColumn(const std::vector<double>& values,
                                    Normalization mode) {
    std::vector<double> out;
    out.reserve(values.size());

    switch (mode) {
        case Normalization::Log:
            // log1p compresses the luxury long tail; clamp at 0 first so a
            // net-refund monetary total cannot produce NaN.
            for (double v : values) {
                out.push_back(std::log1p(std::max(0.0, v)));
            }
            break;

        case Normalization::Quantile: {
            const std::vector<double> ranks = averageRanks(values);
            const double n = static_cast<double>(values.size());
            // (rank - 0.5) / n keeps the result strictly inside (0,1).
            for (double rank : ranks) {
                out.push_back(n > 0.0 ? (rank - 0.5) / n : 0.5);
            }
            break;
        }

        case Normalization::Raw:
        default:
            out = values;
            break;
    }

    return out;
}

std::string toLower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

}  // namespace

// ===========================================================================
//  rfm_detail
// ===========================================================================
namespace rfm_detail {
double clamp01(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::min(1.0, std::max(0.0, value));
}
}  // namespace rfm_detail

// ===========================================================================
//  RFMWeights
// ===========================================================================
bool RFMWeights::parse(const std::string& spec) {
    // Accept every separator a student might type on the command line.
    std::string cleaned = spec;
    for (char& c : cleaned) {
        if (c == ',' || c == ';' || c == ':' || c == '/' || c == '|') {
            c = ' ';
        }
    }

    std::istringstream in(cleaned);
    double r = 0.0, f = 0.0, m = 0.0;
    if (!(in >> r >> f >> m)) {
        return false;  // fewer than three numbers
    }
    std::string extra;
    if (in >> extra) {
        return false;  // more than three numbers
    }
    if (!std::isfinite(r) || !std::isfinite(f) || !std::isfinite(m)) {
        return false;
    }
    if (r < 0.0 || f < 0.0 || m < 0.0) {
        return false;
    }
    if (!(r + f + m > 0.0)) {
        return false;  // all zeros would zero every score
    }

    recency = r;
    frequency = f;
    monetary = m;
    return true;
}

double RFMWeights::total() const {
    const double sum = recency + frequency + monetary;
    return std::isfinite(sum) ? sum : 0.0;
}

bool RFMWeights::valid() const {
    return std::isfinite(recency) && std::isfinite(frequency) &&
           std::isfinite(monetary) && recency >= 0.0 && frequency >= 0.0 &&
           monetary >= 0.0 && total() > 0.0;
}

RFMWeights RFMWeights::normalized() const {
    if (!valid()) {
        return RFMWeights::equal();
    }
    const double sum = total();
    RFMWeights w;
    w.recency = recency / sum;
    w.frequency = frequency / sum;
    w.monetary = monetary / sum;
    return w;
}

RFMWeights RFMWeights::equal() {
    RFMWeights w;
    w.recency = 1.0 / 3.0;
    w.frequency = 1.0 / 3.0;
    w.monetary = 1.0 / 3.0;
    return w;
}

// ===========================================================================
//  RFMCalculator - construction & configuration
// ===========================================================================
RFMCalculator::RFMCalculator(const RFMWeights& weights, Normalization mode)
    : weights_(weights), mode_(mode) {}

void RFMCalculator::setWeights(const RFMWeights& weights) { weights_ = weights; }

// ===========================================================================
//  Stage 2a - aggregate receipts into one profile per customer
// ===========================================================================
std::vector<CustomerProfile> RFMCalculator::compute(
    const std::vector<Transaction>& transactions) const {
    std::vector<CustomerProfile> profiles;
    // Reserve a sensible guess: the 5,000-row demo CSV holds roughly 400
    // distinct shoppers.
    profiles.reserve(transactions.size() / 8 + 16);

    std::unordered_map<std::string, std::size_t> position;
    position.reserve(profiles.capacity());

    for (const Transaction& tx : transactions) {
        // --- defensive filters (a dirty CSV must not reach K-Means) --------
        if (tx.customerId.empty()) {
            continue;
        }
        if (tx.daysAgo < 0) {
            continue;  // the parser could not resolve this receipt's date
        }
        if (!std::isfinite(tx.amount)) {
            continue;  // NaN / inf would spread through the sum
        }

        const auto found = position.find(tx.customerId);
        if (found == position.end()) {
            CustomerProfile profile;
            profile.customerId = tx.customerId;
            profile.recencyDays = tx.daysAgo;  // first receipt seen
            profile.frequency = 1;
            profile.monetary = tx.amount;
            position.emplace(tx.customerId, profiles.size());
            profiles.push_back(std::move(profile));
            continue;
        }

        CustomerProfile& profile = profiles[found->second];
        // Recency is the most RECENT receipt, i.e. the smallest day offset.
        profile.recencyDays = std::min(profile.recencyDays, tx.daysAgo);
        ++profile.frequency;
        profile.monetary += tx.amount;
    }

    // Deterministic output order: hash-map iteration order is unspecified and
    // would make the demo output shuffle between machines and compilers.
    std::sort(profiles.begin(), profiles.end(),
              [](const CustomerProfile& a, const CustomerProfile& b) {
                  if (a.customerId != b.customerId) {
                      return a.customerId < b.customerId;
                  }
                  return a.recencyDays < b.recencyDays;
              });

    return profiles;
}

// ===========================================================================
//  Stage 2b - normalise onto [0,1] with 1.0 = best
// ===========================================================================
void RFMCalculator::normalize(std::vector<CustomerProfile>& profiles) const {
    if (profiles.empty()) {
        return;
    }

    // A lone shopper has no peer group, so every axis is neutral rather than
    // pretending the only customer is simultaneously the best and the worst.
    if (profiles.size() == 1) {
        profiles[0].rScore = 0.5;
        profiles[0].fScore = 0.5;
        profiles[0].mScore = 0.5;
        return;
    }

    std::vector<double> recency;
    std::vector<double> frequency;
    std::vector<double> monetary;
    recency.reserve(profiles.size());
    frequency.reserve(profiles.size());
    monetary.reserve(profiles.size());

    for (const CustomerProfile& profile : profiles) {
        recency.push_back(static_cast<double>(profile.recencyDays));
        frequency.push_back(static_cast<double>(profile.frequency));
        monetary.push_back(profile.monetary);
    }

    // Each column: transform (log / quantile / raw) then min-max to [0,1].
    const std::vector<double> rScaled = minMax(transformColumn(recency, mode_));
    const std::vector<double> fScaled = minMax(transformColumn(frequency, mode_));
    const std::vector<double> mScaled = minMax(transformColumn(monetary, mode_));

    for (std::size_t i = 0; i < profiles.size(); ++i) {
        // Recency is a cost, not a benefit: fewer days ago must score higher.
        profiles[i].rScore = rfm_detail::clamp01(1.0 - rScaled[i]);
        profiles[i].fScore = rfm_detail::clamp01(fScaled[i]);
        profiles[i].mScore = rfm_detail::clamp01(mScaled[i]);
    }
}

// ===========================================================================
//  Stage 2c - custom weighted score
// ===========================================================================
void RFMCalculator::score(std::vector<CustomerProfile>& profiles) const {
    score(profiles, weights_);
}

void RFMCalculator::score(std::vector<CustomerProfile>& profiles,
                          const RFMWeights& weights) const {
    // normalize() guarantees a legal 1/3 split if the caller passed zeros.
    const RFMWeights w = weights.normalized();

    for (CustomerProfile& profile : profiles) {
        const double blended = w.recency * profile.rScore +
                               w.frequency * profile.fScore +
                               w.monetary * profile.mScore;
        profile.weightedScore = rfm_detail::clamp01(blended);
    }
}

// ===========================================================================
//  One-shot pipeline helpers
// ===========================================================================
std::vector<CustomerProfile> RFMCalculator::run(
    const std::vector<Transaction>& transactions) const {
    return run(transactions, weights_);
}

std::vector<CustomerProfile> RFMCalculator::run(
    const std::vector<Transaction>& transactions,
    const RFMWeights& weights) const {
    std::vector<CustomerProfile> profiles = compute(transactions);
    normalize(profiles);
    score(profiles, weights);
    return profiles;
}

void RFMCalculator::rescore(std::vector<CustomerProfile>& profiles,
                            const RFMWeights& weights) const {
    // Interactive reweighting only needs the blend, not a re-normalisation:
    // the 0..1 scores on the profiles do not depend on the weights.
    score(profiles, weights);
}

// ===========================================================================
//  Helper: descriptive statistics (demo footer, tests, sanity checks)
// ===========================================================================
RFMStats RFMCalculator::stats(const std::vector<CustomerProfile>& profiles) {
    RFMStats out;
    out.customers = profiles.size();
    if (profiles.empty()) {
        return out;
    }

    out.minMonetary = profiles.front().monetary;
    out.maxMonetary = profiles.front().monetary;

    double totalRecency = 0.0;
    double totalFrequency = 0.0;
    double totalMonetary = 0.0;

    for (const CustomerProfile& profile : profiles) {
        out.transactions += static_cast<std::size_t>(
            profile.frequency > 0 ? profile.frequency : 0);
        totalRecency += static_cast<double>(profile.recencyDays);
        totalFrequency += static_cast<double>(profile.frequency);
        totalMonetary += profile.monetary;
        out.minMonetary = std::min(out.minMonetary, profile.monetary);
        out.maxMonetary = std::max(out.maxMonetary, profile.monetary);
    }

    const double n = static_cast<double>(profiles.size());
    out.avgRecency = totalRecency / n;
    out.avgFrequency = totalFrequency / n;
    out.avgMonetary = totalMonetary / n;
    return out;
}

// ===========================================================================
//  Helper: CLI <-> enum mapping for main.cpp (--norm log|quantile|raw)
// ===========================================================================
bool RFMCalculator::parseNormalization(const std::string& name,
                                       Normalization& out) {
    const std::string key = toLower(name);
    if (key == "log" || key == "logarithmic") {
        out = Normalization::Log;
        return true;
    }
    if (key == "quantile" || key == "quant" || key == "rank" ||
        key == "percentile") {
        out = Normalization::Quantile;
        return true;
    }
    if (key == "raw" || key == "minmax" || key == "min-max" || key == "none") {
        out = Normalization::Raw;
        return true;
    }
    return false;
}

const char* RFMCalculator::normalizationName(Normalization mode) {
    switch (mode) {
        case Normalization::Log:
            return "log";
        case Normalization::Quantile:
            return "quantile";
        case Normalization::Raw:
            return "raw";
    }
    return "log";
}
