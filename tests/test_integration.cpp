#include "RFMCalculator.h"
#include "KMeansCluster.h"
#include "TierMapper.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <map>
void require(bool b,const std::string& m){if(!b)throw std::runtime_error(m);}
// TEST FIXTURE READER ONLY. This is NOT Member 2's CSVParser.
// Supports the supplied unquoted five-column, single-line CSV; rejects quotes.
std::vector<Transaction> fixture(const std::string& path){
 std::ifstream f(path);require(bool(f),"Cannot open fixture");std::string s;require(bool(std::getline(f,s)),"Empty CSV");if(!s.empty()&&s.back()=='\r')s.pop_back();require(s=="CustomerID,Date,Boutique,Category,Amount","Header mismatch");
 std::vector<Transaction> rows;while(std::getline(f,s)){if(!s.empty()&&s.back()=='\r')s.pop_back();if(s.empty())continue;require(s.find('"')==std::string::npos,"Quoted fixture unsupported");std::stringstream in(s);std::vector<std::string> cells;std::string cell;while(std::getline(in,cell,','))cells.push_back(cell);require(cells.size()==5,"CSV field count");std::size_t used=0;double a=std::stod(cells[4],&used);require(used==cells[4].size(),"Amount suffix");rows.push_back({cells[0],cells[1],cells[2],cells[3],a});}return rows;
}
void run(const std::vector<Transaction>& t,const std::string& report){
 auto p=RFMCalculator::computeProfiles(t);auto old=p;KMeansCluster::assignClusters(p,3,100);auto again=old;KMeansCluster::assignClusters(again,3,100);
 for(std::size_t i=0;i<p.size();++i){auto& x=p[i];require(x.customer_id==old[i].customer_id&&x.recency_days==old[i].recency_days&&x.frequency==old[i].frequency&&x.monetary==old[i].monetary&&x.rfm_score==old[i].rfm_score,"Upstream data changed");require(x.cluster_id>=0&&x.cluster_id<3,"Cluster range");require(x.cluster_id==again[i].cluster_id,"Repeat mismatch");require(std::isfinite(x.rfm_score)&&x.rfm_score>=0&&x.rfm_score<=1,"Score range");}
 TierMapper::assignTiersAndActions(p);std::map<std::string,int> counts;for(auto &x:p){require(!x.luxury_tier.empty()&&!x.recommended_action.empty(),"Missing tier/action");++counts[x.luxury_tier];}
 TierMapper::exportToCSV(p,report);std::ifstream f(report);std::string line;std::size_t lines=0;while(std::getline(f,line))++lines;require(lines==p.size()+1,"Report count");
 std::cout<<"Receipts: "<<t.size()<<"; customers: "<<p.size()<<'\n';for(auto &x:counts)std::cout<<x.first<<": "<<x.second<<'\n';
}
int main(int argc,char** argv){try{std::vector<Transaction> t={{"Peter","2026-11-10","HK","Watch",45000},{"Peter","2026-09-02","HK","Jewelry",22000},{"Jenny","2025-12-05","HK","Bag",18000},{"Jenny","2026-01-14","HK","Coat",12500},{"Marcus","2026-11-12","HK","Tie",450}};run(t,"sample_pipeline_report.csv");run({},"empty_pipeline_report.csv");if(argc>1)run(fixture(argv[1]),"dataset_pipeline_report.csv");std::cout<<"All integration checks passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
