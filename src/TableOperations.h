#pragma once

#include <mariadb/conncpp.hpp>

inline constexpr const char* ADD_ROW_POPUP_ID = "Add Row##AddRowModal";
inline constexpr const char* UPDATE_ROW_POPUP_ID = "Update Row##UpdateRowModal";
inline constexpr const char* CREATE_TABLE_POPUP_ID = "Create Table##CreateTableModal";
inline constexpr const char* DELETE_TABLE_POPUP_ID = "Delete Table##DeleteTableModal";

void addRowToTable(sql::Connection& conn, const std::string& tableName);
void deleteRowFromTable(sql::Connection& conn, const std::string& tableName, const std::string& pkColumn, const std::string& pkValue);
void updateRowInTable(sql::Connection& conn, const std::string& tableName, const std::string& pkColumn, const std::string& pkValue);

void createTableInDatabase(sql::Connection& conn, const std::string& tableName);
void deleteTableFromDatabase(sql::Connection& conn, const std::string& tableName);

void getHeadersFromTable(sql::Connection& conn, const std::string& tableName, std::vector<std::string>& outHeaders);
std::string getPKFromTable(sql::Connection& conn, const std::string& tableName);
