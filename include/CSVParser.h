#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <string>
#include <vector>
#include "Models.h"

class CSVParser {
public:
    static std::vector<Transaction> loadTransactions(const std::string& filepath);
    static int calculateDaysElapsed(const std::string& tx_date, const std::string& reference_date = "2026-11-24");

private:
    static bool dateToAbsoluteDays(const std::string& dateStr, int& outDays);
    static std::string trim(const std::string& str);
    static std::vector<std::string> splitCSVLine(const std::string& line);
};

#endif
