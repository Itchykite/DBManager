#pragma once

#include <mariadb/conncpp.hpp>

inline constexpr const char* ADD_ROW_POPUP_ID = "Add Row##AddRowModal";
inline constexpr const char* UPDATE_ROW_POPUP_ID = "Update Row##UpdateRowModal";

void addRowToTable(sql::Connection& conn, const std::string& tableName);
void deleteRowFromTable(sql::Connection& conn, const std::string& tableName, const std::string& pkColumn, const std::string& pkValue);
void updateRowInTable(sql::Connection& conn, const std::string& tableName, const std::string& pkColumn, const std::string& pkValue);

void getHeadersFromTable(sql::Connection& conn, const std::string& tableName, std::vector<std::string>& outHeaders);
std::string getPKFromTable(sql::Connection& conn, const std::string& tableName);
