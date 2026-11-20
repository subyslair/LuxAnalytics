#include "CSVParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cctype>

std::vector<Transaction> CSVParser::loadTransactions(const std::string& filepath) {
    std::vector<Transaction> transactions;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "[Error] Could not open file: " << filepath << "\n";
        return transactions;
    }

    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        std::string cleanedLine = trim(line);

        
        if (cleanedLine.empty()) {
            continue;
        }

        std::vector<std::string> tokens = splitCSVLine(cleanedLine);

        //(Header)
        if (lineNumber == 1 && !tokens.empty() && trim(tokens[0]) == "CustomerID") {
            continue;
        }

        // 1. Check if the number of fields is exactly 5(CustomerID, Date, Boutique, Category, Amount)
        if (tokens.size() != 5) {
            std::cerr << "[Warning] Line " << lineNumber << " malformed (expected 5 columns, got "
                      << tokens.size() << "). Skipping.\n";
            continue;
        }

        Transaction tx;
        tx.customer_id = trim(tokens[0]);
        tx.date        = trim(tokens[1]);
        tx.boutique    = trim(tokens[2]);
        tx.category    = trim(tokens[3]);
        std::string amountStr = trim(tokens[4]);

        // 2. Check if the required fields are empty
        if (tx.customer_id.empty() || tx.date.empty() || tx.boutique.empty() || tx.category.empty()) {
            std::cerr << "[Warning] Line " << lineNumber << " contains empty required fields. Skipping.\n";
            continue;
        }

        // 3. Check if the date format is valid (YYYY-MM-DD)
        int dummyDays = 0;
        if (!dateToAbsoluteDays(tx.date, dummyDays)) {
            std::cerr << "[Warning] Line " << lineNumber << " has invalid date ("
                      << tx.date << "). Skipping.\n";
            continue;
        }

        // 4. Safely convert spending amount (Amount)
        try {
            size_t processedChars = 0;
            tx.amount = std::stod(amountStr, &processedChars);
            if (processedChars != amountStr.length() || tx.amount < 0.0) {
                throw std::invalid_argument("Invalid amount format");
            }
        } catch (const std::exception&) {
            std::cerr << "[Warning] Line " << lineNumber << " has invalid amount ("
                      << amountStr << "). Skipping.\n";
            continue;
        }

        transactions.push_back(tx);
    }

    return transactions;
}

int CSVParser::calculateDaysElapsed(const std::string& tx_date, const std::string& reference_date) {
    int txDays = 0;
    int refDays = 0;
    if (!dateToAbsoluteDays(tx_date, txDays) || !dateToAbsoluteDays(reference_date, refDays)) {
        return -1;
    }
    int elapsed = refDays - txDays;
    return (elapsed >= 0) ? elapsed : -1;
}

bool CSVParser::dateToAbsoluteDays(const std::string& dateStr, int& outDays) {
    if (dateStr.length() != 10 || dateStr[4] != '-' || dateStr[7] != '-') {
        return false;
    }

    for (size_t i = 0; i < dateStr.length(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(dateStr[i]))) {
            return false;
        }
    }

    int y = std::stoi(dateStr.substr(0, 4));
    int m = std::stoi(dateStr.substr(5, 2));
    int d = std::stoi(dateStr.substr(8, 2));

    if (m < 1 || m > 12 || d < 1 || d > 31) {
        return false;
    }
    static const int daysInMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    bool isLeap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    int maxDay = daysInMonth[m] + (m == 2 && isLeap ? 1 : 0);
    if (d > maxDay) {
        return false;
    }

    y -= (m <= 2) ? 1 : 0;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    outDays = era * 146097 + static_cast<int>(doe) - 719468;

    return true;
}

std::string CSVParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> CSVParser::splitCSVLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        tokens.push_back(item);
    }
    if (!line.empty() && line.back() == ',') {
        tokens.push_back("");
    }
    return tokens;
}