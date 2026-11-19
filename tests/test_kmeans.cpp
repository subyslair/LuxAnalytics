#include "KMeansCluster.h"
#include <cassert>
#include <limits>
#include <stdexcept>
#include <iostream>
int main(){
 std::vector<CustomerProfile> p(8);
 for(int i=0;i<8;++i){p[i].customer_id=std::to_string(i);p[i].recency_days=i*30;p[i].frequency=i+1;p[i].monetary=i*10000;p[i].rfm_score=i/8.0;}
 auto old=p; KMeansCluster::assignClusters(p,3,1);
 for(std::size_t i=0;i<p.size();++i){assert(p[i].cluster_id>=0&&p[i].cluster_id<3);assert(p[i].monetary==old[i].monetary&&p[i].rfm_score==old[i].rfm_score&&p[i].recency_days==old[i].recency_days&&p[i].frequency==old[i].frequency&&p[i].customer_id==old[i].customer_id);}
 auto repeat=old;KMeansCluster::assignClusters(repeat,3,1);for(std::size_t i=0;i<p.size();++i)assert(p[i].cluster_id==repeat[i].cluster_id);
 KMeansCluster::assignClusters(p,99);for(auto &x:p)assert(x.cluster_id>=0&&x.cluster_id<8);
 KMeansCluster::assignClusters(p,0,0);for(auto &x:p)assert(x.cluster_id==0);
 std::vector<CustomerProfile> empty;KMeansCluster::assignClusters(empty);
 std::vector<CustomerProfile> same(5);KMeansCluster::assignClusters(same,3);for(auto &x:same)assert(x.cluster_id>=0&&x.cluster_id<3);
 bool threw=false;p[0].monetary=std::numeric_limits<double>::quiet_NaN();try{KMeansCluster::assignClusters(p);}catch(const std::invalid_argument&){threw=true;}assert(threw);
 std::cout<<"All KMeans checks passed.\n";
}