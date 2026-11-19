// Reviewed integration revision: snapshot 2026-11-24; positive purchases only.
// Log normalization only; raw R/F/M retained. Aggregate overflow throws.
// Invalid weight sums fall back to equal weights; year 0000 rejected.
// ===========================================================================
//  RFMCalculator.cpp  --  Stage 2: the RFM scoring engine (implementation)
//
//  Owner  : Member 3 (Lan Ka Sen)
//  Branch : feature-rfm-scoring
//
//  Revised to match the shared team contract in include/Models.h and
//  include/RFMCalculator.h (no header changes required):
//
//    static std::vector<CustomerProfile> RFMCalculator::computeProfiles(
//        const std::vector<Transaction>& transactions,
//        double w_recency, double w_frequency, double w_monetary);
//
//  Pipeline contract
//    in  : std::vector<Transaction>  from CSVParser  (Member 2)
//            customer_id / date (YYYY-MM-DD) / boutique / category / amount
//    out : std::vector<CustomerProfile>, one per customer, sorted by ID
//            customer_id   -> unique customer key
//            recency_days  -> RAW days since latest receipt  (K-Means, Tiers)
//            frequency     -> RAW number of valid receipts   (K-Means, Tiers)
//            monetary      -> RAW total spend                (K-Means, Tiers)
//            rfm_score     -> weighted score in [0,1], higher = better
//            cluster_id    -> left at -1 for Member 4
//            luxury_tier / recommended_action -> left empty for Member 5
//
//  Scoring method (kept from the original Member 3 design)
//    1. Aggregate receipts per customer.
//    2. log1p-transform each of R, F, M to tame the luxury long tail.
//    3. Min-max each column onto [0,1]; Recency is inverted so that a more
//       recent shopper scores higher.
//    4. Blend with the user weights (re-normalised to sum to 1).
//
//  Standard library only, C++17.
// ===========================================================================
#include "RFMCalculator.h"
#include <limits>
#include <stdexcept>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// Reference ("today") date for Recency. Matches the default reference_date
// declared in CSVParser::calculateDaysElapsed so both modules agree.
constexpr int kRefYear = 2026;
constexpr int kRefMonth = 11;
constexpr int kRefDay = 24;

// ---------------------------------------------------------------------------
// Clamp to [0,1]; NaN / inf become 0 so a bad value cannot spread.
// ---------------------------------------------------------------------------
double clamp01(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::min(1.0, std::max(0.0, value));
}

// ---------------------------------------------------------------------------
// Days from 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's
// algorithm). Lets us subtract two dates without <ctime> time-zone issues.
// ---------------------------------------------------------------------------
long long daysFromCivil(int y, int m, int d) {
    y -= (m <= 2) ? 1 : 0;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int mp = m + (m > 2 ? -3 : 9);
    const int doy = (153 * mp + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + doe - 719468;
}

bool isLeap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

// ---------------------------------------------------------------------------
// Parse "YYYY-MM-DD" strictly. Returns false for any malformed or impossible
// date (e.g. 2026-02-30) so the caller can skip that receipt.
// ---------------------------------------------------------------------------
bool parseDate(const std::string& text, int& y, int& m, int& d) {
    // Tolerate surrounding spaces / a trailing '\r' from Windows CSV files.
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return false;
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    const std::string s = text.substr(first, last - first + 1);

    if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
    }

    y = std::stoi(s.substr(0, 4));
    if (y < 1) return false;
    m = std::stoi(s.substr(5, 2));
    d = std::stoi(s.substr(8, 2));

    static const int kDaysInMonth[12] = {31, 28, 31, 30, 31, 30,
                                         31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) {
        return false;
    }
    int maxDay = kDaysInMonth[m - 1];
    if (m == 2 && isLeap(y)) {
        maxDay = 29;
    }
    return d >= 1 && d <= maxDay;
}

// ---------------------------------------------------------------------------
// Days between a receipt date and the reference date.
// Returns -1 if the date cannot be parsed or lies after the reference date.
// ---------------------------------------------------------------------------
int daysAgo(const std::string& date) {
    int y = 0, m = 0, d = 0;
    if (!parseDate(date, y, m, d)) {
        return -1;
    }
    const long long diff =
        daysFromCivil(kRefYear, kRefMonth, kRefDay) - daysFromCivil(y, m, d);
    if (diff < 0) {
        return -1;  // future-dated receipt -> treat as invalid
    }
    return static_cast<int>(diff);
}

// ---------------------------------------------------------------------------
// Min-max scaling onto [0,1]. A column with no spread maps to a neutral 0.5
// instead of dividing by zero.
// ---------------------------------------------------------------------------
std::vector<double> minMax(const std::vector<double>& values) {
    std::vector<double> scaled(values.size(), 0.5);
    if (values.empty()) {
        return scaled;
    }

    const auto range = std::minmax_element(values.begin(), values.end());
    const double lo = *range.first;
    const double hi = *range.second;
    const double span = hi - lo;
    if (!(span > 0.0) || !std::isfinite(span)) {
        return scaled;  // constant column -> everyone neutral
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        scaled[i] = clamp01((values[i] - lo) / span);
    }
    return scaled;
}

// ---------------------------------------------------------------------------
// log1p transform; negatives are clamped to 0 so a net refund cannot give NaN.
// ---------------------------------------------------------------------------
std::vector<double> logTransform(const std::vector<double>& values) {
    std::vector<double> out;
    out.reserve(values.size());
    for (double v : values) {
        out.push_back(std::log1p(std::max(0.0, v)));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Validate and re-normalise the weights so they sum to 1.
// Invalid input (negative, NaN, or all zero) falls back to equal weights.
// ---------------------------------------------------------------------------
void normalizeWeights(double& wr, double& wf, double& wm) {
    const bool finite = std::isfinite(wr) && std::isfinite(wf) && std::isfinite(wm);
    const bool nonNegative = wr >= 0.0 && wf >= 0.0 && wm >= 0.0;
    const double sum = wr + wf + wm;

    if (!finite || !nonNegative || !std::isfinite(sum) || !(sum > 0.0)) {
        wr = wf = wm = 1.0 / 3.0;
        return;
    }
    wr /= sum;
    wf /= sum;
    wm /= sum;
}

}  // namespace

// ===========================================================================
//  RFMCalculator::computeProfiles
// ===========================================================================
std::vector<CustomerProfile> RFMCalculator::computeProfiles(
    const std::vector<Transaction>& transactions,
    double w_recency,
    double w_frequency,
    double w_monetary) {

    // ---- Stage 2a: aggregate receipts into one profile per customer -------
    std::vector<CustomerProfile> profiles;
    profiles.reserve(transactions.size() / 4 + 16);

    std::unordered_map<std::string, std::size_t> position;
    position.reserve(profiles.capacity());

    for (const Transaction& tx : transactions) {
        // Defensive filters: a dirty row must not reach K-Means.
        if (tx.customer_id.empty()) {
            continue;
        }
        // Purchase-only policy: refunds and zero-value records do not affect RFM.
        if (!std::isfinite(tx.amount) || tx.amount <= 0.0) {
            continue;
        }
        const int days = daysAgo(tx.date);
        if (days < 0) {
            continue;  // unparseable or future date
        }

        const auto found = position.find(tx.customer_id);
        if (found == position.end()) {
            CustomerProfile profile;
            profile.customer_id = tx.customer_id;
            profile.recency_days = days;
            profile.frequency = 1;
            profile.monetary = tx.amount;
            position.emplace(tx.customer_id, profiles.size());
            profiles.push_back(std::move(profile));
            continue;
        }

        CustomerProfile& profile = profiles[found->second];
        // Recency = most recent receipt = smallest day offset.
        const double next_total = profile.monetary + tx.amount;
        if (!std::isfinite(next_total) ||
            profile.frequency == std::numeric_limits<int>::max()) {
            throw std::overflow_error("RFM aggregation overflow: " + tx.customer_id);
        }
        profile.recency_days = std::min(profile.recency_days, days);
        ++profile.frequency;
        profile.monetary = next_total;
    }

    // Deterministic order (hash-map order differs between compilers).
    std::sort(profiles.begin(), profiles.end(),
              [](const CustomerProfile& a, const CustomerProfile& b) {
                  return a.customer_id < b.customer_id;
              });

    if (profiles.empty()) {
        return profiles;
    }

    // A lone shopper has no peer group: give a neutral score.
    if (profiles.size() == 1) {
        profiles[0].rfm_score = 0.5;
        return profiles;
    }

    // ---- Stage 2b: normalise each axis onto [0,1], 1.0 = best -------------
    std::vector<double> recency;
    std::vector<double> frequency;
    std::vector<double> monetary;
    recency.reserve(profiles.size());
    frequency.reserve(profiles.size());
    monetary.reserve(profiles.size());

    for (const CustomerProfile& p : profiles) {
        recency.push_back(static_cast<double>(p.recency_days));
        frequency.push_back(static_cast<double>(p.frequency));
        monetary.push_back(p.monetary);
    }

    const std::vector<double> rScaled = minMax(logTransform(recency));
    const std::vector<double> fScaled = minMax(logTransform(frequency));
    const std::vector<double> mScaled = minMax(logTransform(monetary));

    // ---- Stage 2c: weighted score -----------------------------------------
    normalizeWeights(w_recency, w_frequency, w_monetary);

    for (std::size_t i = 0; i < profiles.size(); ++i) {
        // Recency is a cost: fewer days ago must score higher.
        const double rScore = clamp01(1.0 - rScaled[i]);
        const double fScore = clamp01(fScaled[i]);
        const double mScore = clamp01(mScaled[i]);

        profiles[i].rfm_score = clamp01(w_recency * rScore +
                                        w_frequency * fScore +
                                        w_monetary * mScore);
    }

    return profiles;
}
