# Member 5 handoff (provisional classification policy)

Copy src/TierMapper.cpp and tests/test_tiermapper.cpp into the project.
Do not replace the existing headers. main.cpp remains the team lead's responsibility.

Build and run from the repository root (C++17 compiler required):

    g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/TierMapper.cpp tests/test_tiermapper.cpp -o tiermapper_tests
    ./tiermapper_tests

On Windows, the executable may be tiermapper_tests.exe.
The test creates and removes tiermapper_test_report.csv in the current directory.
Do not compile this test main together with src/main.cpp or another test main.
Do not use -DNDEBUG for these assert-based tests.

PROVISIONAL POLICY -- NOT PROVIDED OR APPROVED BY THE TEAM:
- Assumes larger rfm_score means better; absolute score scale is not assumed.
- At-Risk Patron: recency >= 180 AND monetary >= 20000 (highest priority).
- Ultra-VIC: monetary >= 50000, unless flagged at risk.
- VIC: monetary >= 20000, unless flagged at risk.
- Rising Collector: recency < 180, frequency >= 2, assigned cluster mean
  RFM > overall mean RFM for assigned customers, individual RFM >= own cluster mean.
- Otherwise Entry Client.
- Cluster IDs are labels, not ranks. All IDs >= 0 are treated as assigned.
- Empty input is accepted. Missing clusters cannot qualify as Rising Collector.
- Assumes valid finite RFM/monetary values, nonnegative recency, meaningful
  frequency, a consistent currency and observation window, and upstream validation.
- Dates/amounts and score semantics must be agreed with Members 2/3/4.
- These rules are a hybrid business-rule overlay, NOT pure cluster-derived tiering.
  Monetary and recency guardrails do not depend on weights; Rising Collector does.
- Category thresholds, tier names and actions must be reviewed before merging.
- Export uses a dot decimal separator under the default C++ locale, two decimals
  for monetary and six for RFM score. String fields use CSV quoting, including
  embedded quotes, commas and line breaks. Output overwrites an existing target.
- Output directory must already exist. Failed open/write/close throws runtime_error.
- Export is not atomic and not spreadsheet-formula-injection hardened. Display
  alignment assumes short ASCII customer IDs. Those are limitations, not guarantees.

Team lead integration AFTER creating and clustering profiles:

    #include "../include/TierMapper.h"
    TierMapper::assignTiersAndActions(profiles);
    TierMapper::printSummaryTable(profiles);
    TierMapper::exportToCSV(profiles, cfg.output_path);

profiles must be the team's std::vector<CustomerProfile> variable. Do not paste
these calls into the current placeholder main until upstream processing exists.
Catch std::exception in main and return a nonzero exit status on export failure.
