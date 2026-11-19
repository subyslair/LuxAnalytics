#include "RFMCalculator.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
int main() {
    std::vector<Transaction> t = {
        {"Peter","2026-11-10","HK","Watch",45000},
        {"Peter","2026-09-02","HK","Jewelry",22000},
        {"Jenny","2025-12-05","HK","Bag",18000},
        {"Jenny","2026-01-14","HK","Coat",12500},
        {"Marcus","2026-11-12","HK","Tie",450}
    };
    auto p = RFMCalculator::computeProfiles(t);
    assert(p.size()==3);
    assert(p[0].customer_id=="Jenny" && p[0].recency_days==314 && p[0].frequency==2 && p[0].monetary==30500);
    assert(p[1].customer_id=="Marcus" && p[1].recency_days==12);
    assert(p[2].customer_id=="Peter" && p[2].recency_days==14 && p[2].monetary==67000);
    for (const auto& x : p) {
        assert(std::isfinite(x.rfm_score) && x.rfm_score>=0 && x.rfm_score<=1);
        assert(x.cluster_id==-1);
    }
    auto dirty=t;
    dirty.push_back({"Peter","2026-11-24","HK","Refund",-45000});
    dirty.push_back({"Peter","2026-11-24","HK","Free",0});
    dirty.push_back({"RefundOnly","2026-11-24","HK","Refund",-100});
    dirty.push_back({"Invalid","2026-02-30","HK","Watch",100});
    dirty.push_back({"Future","2026-11-25","HK","Watch",100});
    dirty.push_back({"NaN","2026-11-24","HK","Watch",std::numeric_limits<double>::quiet_NaN()});
    auto q=RFMCalculator::computeProfiles(dirty);
    assert(q.size()==p.size());
    for (std::size_t i=0;i<p.size();++i) {
        assert(q[i].customer_id==p[i].customer_id);
        assert(q[i].recency_days==p[i].recency_days);
        assert(q[i].frequency==p[i].frequency);
        assert(q[i].monetary==p[i].monetary);
        assert(q[i].rfm_score==p[i].rfm_score);
    }
    auto day=RFMCalculator::computeProfiles({{"Today","2026-11-24","HK","Watch",100}});
    assert(day.size()==1 && day[0].recency_days==0 && day[0].rfm_score==0.5);
    assert(RFMCalculator::computeProfiles({}).empty());
    const double large=std::numeric_limits<double>::max();
    auto overflow=RFMCalculator::computeProfiles(t,large,large,large);
    auto equal=RFMCalculator::computeProfiles(t,1,1,1);
    for (std::size_t i=0;i<equal.size();++i)
        assert(std::abs(overflow[i].rfm_score-equal[i].rfm_score)<1e-12);
    auto invalidYear = RFMCalculator::computeProfiles({{"BadYear","0000-01-01","HK","Watch",100}});
    assert(invalidYear.empty());
    bool aggregateOverflow = false;
    try {
        RFMCalculator::computeProfiles({
            {"Big","2026-11-24","HK","Watch",large},
            {"Big","2026-11-24","HK","Watch",large}
        });
    } catch (const std::overflow_error&) { aggregateOverflow = true; }
    assert(aggregateOverflow);
    std::cout << "All revised RFM policy tests passed.\n";
}
