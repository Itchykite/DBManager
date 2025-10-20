#include "TableOperations.h"
#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cctype>
#include "imgui/imgui.h"

static std::string to_lower(std::string s) 
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

static bool parse_server_function_literal(const std::string& in, std::string& outExpr) 
{
    std::string s = to_lower(in);
    // Usuń spacje z początku/końca
    auto ltrim = [](std::string& x){ x.erase(x.begin(), std::find_if(x.begin(), x.end(), [](unsigned char c){ return !std::isspace(c);})); };
    auto rtrim = [](std::string& x){ x.erase(std::find_if(x.rbegin(), x.rend(), [](unsigned char c){ return !std::isspace(c);}).base(), x.end()); };
    std::string t = s;
    ltrim(t); rtrim(t);

    auto is_name_or_call = [&](const std::string& name)->bool
    {
        return (t == name) || (t == name + "()"); 
    };

    if (is_name_or_call("current_timestamp")) { outExpr = "CURRENT_TIMESTAMP"; return true; }
    if (is_name_or_call("now"))               { outExpr = "NOW()"; return true; }
    if (is_name_or_call("curdate"))           { outExpr = "CURDATE()"; return true; }
    if (is_name_or_call("curtime"))           { outExpr = "CURTIME()"; return true; }

    return false;
}

void getHeadersFromTable(sql::Connection& conn, const std::string& tableName, std::vector<std::string>& headers)
{
    try 
    {
        std::unique_ptr<sql::Statement> stmt(conn.createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT * FROM " + tableName + " LIMIT 1"));

        sql::ResultSetMetaData* meta = res->getMetaData();
        int columnCount = meta->getColumnCount();

        headers.clear();
        headers.reserve(columnCount);
        for (int i = 1; i <= columnCount; ++i)
            headers.push_back(meta->getColumnName(i).c_str());
    } 
    catch (sql::SQLException& e) 
    {
        std::cerr << "Błąd podczas pobierania nagłówków z tabeli " << tableName << ": " << e.what() << std::endl;
    }
}

std::string getPKFromTable(sql::Connection& conn, const std::string& tableName)
{
    try 
    {
        std::unique_ptr<sql::Statement> stmt(conn.createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
            "SELECT COLUMN_NAME "
            "FROM INFORMATION_SCHEMA.COLUMNS "
            "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + tableName + "' "
            "AND COLUMN_KEY = 'PRI';"));

        if (res->next())
        {
            return res->getString("COLUMN_NAME").c_str();
        }
    } 
    catch (sql::SQLException& e) 
    {
        std::cerr << "Error with getting primary_key data from table: " << tableName << ": " << e.what() << std::endl;
    }

    return "";
}

void addRowToTable(sql::Connection& conn, const std::string& tableName)
{
    if (tableName.empty())
        return;

    static std::map<std::string, std::vector<std::string>> inputValues;
    static std::map<std::string, std::vector<std::string>> columnNames;
    static std::string lastError;

    // Wczytanie listy kolumn (bez auto_increment) i domyślnych tylko raz na tabelę
    if (columnNames.find(tableName) == columnNames.end())
    {
        std::vector<std::string> cols;
        std::vector<std::string> defaults;

        try
        {
            std::unique_ptr<sql::ResultSet> res(conn.createStatement()->executeQuery(
                "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA "
                "FROM INFORMATION_SCHEMA.COLUMNS "
                "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + tableName + "' "
                "ORDER BY ORDINAL_POSITION;"
            ));

            while (res->next())
            {
                std::string extra = static_cast<std::string>(res->getString("EXTRA"));
                if (extra.find("auto_increment") != std::string::npos)
                    continue;

                std::string col = static_cast<std::string>(res->getString("COLUMN_NAME"));
                std::string def = res->isNull("COLUMN_DEFAULT") ? "" : static_cast<std::string>(res->getString("COLUMN_DEFAULT"));

                cols.push_back(col);
                defaults.push_back(def);
            }
        }
        catch (sql::SQLException& e)
        {
            std::cerr << "Błąd przy pobieraniu definicji kolumn dla tabeli " << tableName << ": " << e.what() << std::endl;
        }

        columnNames[tableName] = std::move(cols);
        inputValues[tableName] = std::move(defaults); // pola startowo wypełnione tekstem default
    }

    auto& cols   = columnNames[tableName];
    auto& values = inputValues[tableName];
    if (values.size() != cols.size())
        values.resize(cols.size());

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(ADD_ROW_POPUP_ID, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        for (size_t i = 0; i < cols.size(); ++i)
        {
            std::string label = cols[i] + "##" + std::to_string(i);
            char buf[256];
            std::memset(buf, 0, sizeof(buf));
            std::strncpy(buf, values[i].c_str(), sizeof(buf) - 1);

            if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
                values[i] = buf;
        }

        if (!lastError.empty())
            ImGui::TextColored(ImVec4(1,0,0,1), "Błąd: %s", lastError.c_str());

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120,0)))
        {
            try
            {
                std::set<std::string> autoIncrementCols;
                std::set<std::string> hasDefaultCols;

                std::unique_ptr<sql::ResultSet> defRes(conn.createStatement()->executeQuery(
                    "SELECT COLUMN_NAME, COLUMN_DEFAULT, EXTRA "
                    "FROM INFORMATION_SCHEMA.COLUMNS "
                    "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + tableName + "' "
                    "ORDER BY ORDINAL_POSITION;"
                ));

                while (defRes->next())
                {
                    std::string colName = static_cast<std::string>(defRes->getString("COLUMN_NAME"));
                    std::string extra   = static_cast<std::string>(defRes->getString("EXTRA"));

                    if (!defRes->isNull("COLUMN_DEFAULT"))
                        hasDefaultCols.insert(colName);

                    if (extra.find("auto_increment") != std::string::npos)
                        autoIncrementCols.insert(colName);
                }

                enum class ValKind { Param, ServerFunc, UseDefault };

                std::vector<std::string> insertCols;
                std::vector<ValKind>     kinds;
                std::vector<std::string> serverExpr;   // dla ServerFunc: dosłowny tekst funkcji do wstrzyknięcia (np. CURRENT_TIMESTAMP)
                std::vector<std::string> paramValues;  // dla Param: wartości bindowane

                insertCols.reserve(cols.size());
                kinds.reserve(cols.size());
                serverExpr.reserve(cols.size());
                paramValues.reserve(cols.size());

                for (size_t i = 0; i < cols.size(); ++i)
                {
                    const std::string& colName = cols[i];
                    if (autoIncrementCols.count(colName))
                        continue;

                    std::string v = values[i];
                    // Jeśli puste i kolumna ma default, użyj DEFAULT
                    if (v.empty() && hasDefaultCols.count(colName))
                    {
                        insertCols.push_back(colName);
                        kinds.push_back(ValKind::UseDefault);
                        serverExpr.emplace_back(); // placeholder
                        continue;
                    }

                    // Jeśli użytkownik wpisał "default" (dowolna wielkość liter) -> DEFAULT
                    if (to_lower(v) == "default")
                    {
                        insertCols.push_back(colName);
                        kinds.push_back(ValKind::UseDefault);
                        serverExpr.emplace_back();
                        continue;
                    }

                    // Rozpoznaj funkcje serwera niezależnie od case
                    std::string expr;
                    if (parse_server_function_literal(v, expr))
                    {
                        insertCols.push_back(colName);
                        kinds.push_back(ValKind::ServerFunc);
                        serverExpr.push_back(expr);
                        continue;
                    }

                    // W przeciwnym razie zwykły parametr
                    insertCols.push_back(colName);
                    kinds.push_back(ValKind::Param);
                    serverExpr.emplace_back();
                    paramValues.push_back(v);
                }

                std::string sql = "INSERT INTO " + tableName + " (";
                for (size_t i = 0; i < insertCols.size(); ++i)
                {
                    sql += insertCols[i];
                    if (i + 1 < insertCols.size()) sql += ", ";
                }
                sql += ") VALUES (";
                size_t nextParamIndex = 1;
                size_t nextParamToBind = 0;

                for (size_t i = 0; i < kinds.size(); ++i)
                {
                    if (kinds[i] == ValKind::UseDefault)
                        sql += "DEFAULT";
                    else if (kinds[i] == ValKind::ServerFunc)
                        sql += serverExpr[i];
                    else // Param
                        sql += "?";

                    if (i + 1 < kinds.size()) sql += ", ";
                }
                sql += ");";

                std::unique_ptr<sql::PreparedStatement> pstmt(conn.prepareStatement(sql));

                // Bindowanie tylko dla parametrów
                for (size_t i = 0; i < kinds.size(); ++i)
                {
                    if (kinds[i] == ValKind::Param)
                    {
                        if (nextParamToBind >= paramValues.size())
                            throw std::runtime_error("Internal binding error: value index out of range.");
                        pstmt->setString(static_cast<int>(nextParamIndex++), paramValues[nextParamToBind++]);
                    }
                }

                pstmt->execute();

                values.assign(values.size(), "");
                lastError.clear();
                ImGui::CloseCurrentPopup();
            }
            catch (const std::exception& e)
            {
                lastError = e.what();
            }
            catch (sql::SQLException& e)
            {
                lastError = e.what();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120,0)))
        {
            values.assign(values.size(), "");
            lastError.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void deleteRowFromTable(sql::Connection& conn, const std::string& tableName, const std::string& pkColumn, const std::string& pkValue)
{
    try
    {
        const std::string sql = "DELETE FROM `" + tableName + "` WHERE `" + pkColumn + "` = ? LIMIT 1;";
        std::unique_ptr<sql::PreparedStatement> pstmt(conn.prepareStatement(sql));
        pstmt->setString(1, pkValue);
        pstmt->execute();
    }
    catch (sql::SQLException& e)
    {
        std::cerr << "Error deleting from table " << tableName
                  << " by PK " << pkColumn << "=" << pkValue
                  << ": " << e.what() << std::endl;
    }
}

void updateRowInTable(sql::Connection& conn, const std::string& tableName, const std::string& pkColumn, const std::string& pkValue)
{
    if (tableName.empty())
        return;

    static std::map<std::string, std::vector<std::string>> inputValues;
    static std::map<std::string, std::vector<std::string>> columnNames;
    static std::map<std::string, std::string> loadedPkForTable;  
    static std::string lastError;

    if (columnNames.find(tableName) == columnNames.end())
    {
        std::vector<std::string> cols;
        try
        {
            std::unique_ptr<sql::ResultSet> res(conn.createStatement()->executeQuery(
                "SELECT COLUMN_NAME, EXTRA "
                "FROM INFORMATION_SCHEMA.COLUMNS "
                "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '" + tableName + "' "
                "ORDER BY ORDINAL_POSITION;"
            ));

            while (res->next())
            {
                std::string extra = static_cast<std::string>(res->getString("EXTRA"));
                if (extra.find("auto_increment") != std::string::npos)
                    continue;

                cols.push_back(static_cast<std::string>(res->getString("COLUMN_NAME")));
            }
        }
        catch (sql::SQLException& e)
        {
            std::cerr << "Błąd przy pobieraniu definicji kolumn dla tabeli " << tableName << ": " << e.what() << std::endl;
        }

        columnNames[tableName] = std::move(cols);
        inputValues[tableName] = std::vector<std::string>(columnNames[tableName].size());
        loadedPkForTable[tableName].clear();  
    }

    auto& cols   = columnNames[tableName];
    auto& values = inputValues[tableName];
    if (values.size() != cols.size())
        values.resize(cols.size());

    if (loadedPkForTable[tableName] != pkValue && !pkColumn.empty())
    {
        try
        {
            std::string sql = "SELECT ";
            for (size_t i = 0; i < cols.size(); ++i)
            {
                sql += "`" + cols[i] + "`";
                if (i + 1 < cols.size()) sql += ", ";
            }
            sql += " FROM `" + tableName + "` WHERE `" + pkColumn + "` = ? LIMIT 1;";

            std::unique_ptr<sql::PreparedStatement> pstmt(conn.prepareStatement(sql));
            pstmt->setString(1, pkValue);
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            if (res->next())
            {
                for (size_t i = 0; i < cols.size(); ++i)
                {
                    if (res->isNull(static_cast<int>(i + 1)))
                        values[i].clear();
                    else
                        values[i] = static_cast<std::string>(res->getString(static_cast<int>(i + 1)));
                }
            }
            else
            {
                lastError = "Record not found (may have been deleted).";
            }

            loadedPkForTable[tableName] = pkValue;
        }
        catch (sql::SQLException& e)
        {
            lastError = e.what();
        }
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(UPDATE_ROW_POPUP_ID, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        for (size_t i = 0; i < cols.size(); ++i)
        {
            std::string label = cols[i] + "##upd_" + std::to_string(i);
            char buf[256];
            std::memset(buf, 0, sizeof(buf));
            std::strncpy(buf, values[i].c_str(), sizeof(buf) - 1);

            if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
                values[i] = buf;
        }

        if (!lastError.empty())
            ImGui::TextColored(ImVec4(1,0,0,1), "Błąd: %s", lastError.c_str());

        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120,0)))
        {
            try
            {
                std::string sql = "UPDATE `" + tableName + "` SET ";
                for (size_t i = 0; i < cols.size(); ++i)
                {
                    sql += "`" + cols[i] + "` = ?";
                    if (i + 1 < cols.size()) sql += ", ";
                }
                sql += " WHERE `" + pkColumn + "` = ? LIMIT 1;";

                std::unique_ptr<sql::PreparedStatement> pstmt(conn.prepareStatement(sql));

                int bindIndex = 1;
                for (size_t i = 0; i < cols.size(); ++i)
                    pstmt->setString(bindIndex++, values[i]);

                pstmt->setString(bindIndex, pkValue);

                pstmt->execute();

                lastError.clear();
                ImGui::CloseCurrentPopup();
            }
            catch (const std::exception& e)
            {
                lastError = e.what();
            }
            catch (sql::SQLException& e)
            {
                lastError = e.what();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120,0)))
        {
            lastError.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
