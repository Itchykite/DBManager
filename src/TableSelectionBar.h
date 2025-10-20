#pragma once

#include "TableOperations.h"
#include <mariadb/conncpp.hpp>
#include <vector>
#include <string>
#include <memory>
#include <iostream>

struct Row 
{
    std::vector<std::string> columns;
};

struct TableData 
{
    std::vector<std::string> headers;
    std::vector<Row> rows;
};

std::vector<std::string> getTablesFromDatabase(sql::Connection& conn);
TableData getTableData(sql::Connection& conn, const std::string& tableName);

class TableSelectorBar
{
public:
    TableSelectorBar() = default;
    TableSelectorBar(const std::vector<std::string>& initialTables);
    
    std::string render(sql::Connection& conn);
    void setTables(const std::vector<std::string>& newTables);
    int getSelectedTableIndex() const { return selectedTableIndex; }
    std::string getSelectedTable() const { return tables.empty() ? "" : tables[selectedTableIndex]; }

private:
    std::vector<std::string> tables;
    int selectedTableIndex = 0;

    bool openAddRowRequested = false;
};
