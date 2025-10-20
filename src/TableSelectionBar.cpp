#include "TableSelectionBar.h"
#include "imgui/imgui.h"

// Pobiera listę tabel z bazy danych
std::vector<std::string> getTablesFromDatabase(sql::Connection& conn) 
{
    std::vector<std::string> tables{};
    try 
    {
        std::unique_ptr<sql::Statement> stmt(conn.createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SHOW TABLES"));

        // Pobiera nazwy tabel
        while (res->next()) 
        {
            tables.push_back(res->getString(1).c_str());
        }
    } 
    catch (sql::SQLException& e) 
    {
        std::cerr << "Error with getting tables: " << e.what() << std::endl;
    }

    return tables;
}

// Pobiera dane z wybranej tabeli 
TableData getTableData(sql::Connection& conn, const std::string& tableName)
{
    TableData tableData;
    try 
    {
        std::unique_ptr<sql::Statement> stmt(conn.createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT * FROM " + tableName));

        sql::ResultSetMetaData* meta = res->getMetaData();
        int columnCount = meta->getColumnCount();

        // Pobiera nagłówki kolumn 
        tableData.headers.reserve(columnCount);
        for (int i = 1; i <= columnCount; ++i)
            tableData.headers.push_back(meta->getColumnName(i).c_str());

        // Pobiera wiersze danych
        while (res->next())
        {
            Row row;
            for (int i = 1; i <= columnCount; ++i) 
            {
                row.columns.push_back(res->getString(i).c_str());
            }
            tableData.rows.push_back(std::move(row));
        }
    } 
    catch (sql::SQLException& e) 
    {
        std::cerr << "Error with getting data from tables: " << tableName << ": " << e.what() << std::endl;
    }

    return tableData;
}

TableSelectorBar::TableSelectorBar(const std::vector<std::string>& initialTables)
    : tables(initialTables)
{
}

std::string TableSelectorBar::render(sql::Connection& conn)
{
    if (ImGui::BeginMenuBar())
    {
        // Wybór tabeli
        if (ImGui::BeginMenu("Select Table"))
        {
            for (size_t i = 0; i < tables.size(); ++i)
            {
                if (ImGui::MenuItem(tables[i].c_str(), nullptr, selectedTableIndex == static_cast<int>(i)))
                {
                    selectedTableIndex = static_cast<int>(i);
                }
            }
            ImGui::EndMenu();
        }

        // Operacje na tabeli
        if (ImGui::BeginMenu("Operation"))
        {
            if (ImGui::MenuItem("Create Table", nullptr, false, false))
            {
                openCreateTableRequested = true;
            }

            if (ImGui::MenuItem("Delete Table", nullptr, false, false))
            {
                openDeleteTableRequested = true;
            }

            if (ImGui::MenuItem("Add Row", nullptr, false, !tables.empty()))
            {
                openAddRowRequested = true;
            }

            if (ImGui::MenuItem("Refresh"))
            {
                auto newTables = getTablesFromDatabase(conn);
                setTables(newTables);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (openAddRowRequested)
    {
        ImGui::OpenPopup(ADD_ROW_POPUP_ID);
        openAddRowRequested = false;
    }

    if (openCreateTableRequested)
    {
        // Niezaimplementowane
    }

    if (openDeleteTableRequested)
    {
        // Niezaimplementowane
    }

    addRowToTable(conn, getSelectedTable());

    return tables.empty() ? "" : tables[selectedTableIndex];
}

void TableSelectorBar::setTables(const std::vector<std::string>& newTables) 
{
    tables = newTables;
    selectedTableIndex = 0;  
}
