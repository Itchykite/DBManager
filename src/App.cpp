#include "App.h"
#include "TableOperations.h"
#include <cstdio>  

const char* ICON_FA_TRASH = "\xef\x80\x8d";
const char* ICON_FA_PEN   = "\xef\x81\x84";

App::App(WindowProps props) : windowProps(props)
{
    // Puste bufory startowe — użytkownik wpisze dane w oknie
    hostBuf[0] = portBuf[0] = userBuf[0] = passBuf[0] = dbBuf[0] = '\0';
    // Możesz ustawić sensowny hint np. port 3306:
    std::snprintf(hostBuf, sizeof(hostBuf), "127.0.0.1");
    std::snprintf(portBuf, sizeof(portBuf), "3307");
    std::snprintf(userBuf, sizeof(userBuf), "root");

    init(props);
}

App::~App()
{
    cleanup();
}

void App::init(WindowProps props) 
{
    if (!glfwInit()) 
        throw std::runtime_error("Cannot initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(props.width, props.height, props.title.c_str(), nullptr, nullptr);
    if (!window) 
    {
        glfwTerminate();
        throw std::runtime_error("Cannot create GLFW window");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) 
        throw std::runtime_error("Cannot initialize GLAD");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig fontConfig;
    fontConfig.MergeMode = false;
    (void)io.Fonts->AddFontFromFileTTF("../fonts/NotoSans-Regular.ttf", 16.0f, &fontConfig);

    static const ImWchar icons_ranges[] = { 0xE000, 0xF8FF, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("../fonts/fa-solid-900.ttf", 16.0f, &icons_config, icons_ranges);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void App::renderGUI() 
{
    // Jeśli brak połączenia, pokaż okno łączenia
    if (!conn)
    {
        drawConnectWindow();
        drawCreateDatabaseWindow();
        drawDropDatabaseWindow();
    }

    // Jeśli połączono, pokaż główne okno
    if (conn)
        showMain();
}

void App::drawConnectWindow()
{
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Connect to database", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Enter database connection details:");
        ImGui::Separator();

        ImGui::PushItemWidth(320.0f);
        ImGui::InputText("Host", hostBuf, sizeof(hostBuf));
        ImGui::InputText("Port", portBuf, sizeof(portBuf));
        ImGui::InputText("User", userBuf, sizeof(userBuf));
        ImGui::InputText("Password", passBuf, sizeof(passBuf), ImGuiInputTextFlags_Password);
        ImGui::PopItemWidth();

        ImGui::Spacing();
        if (ImGui::Button("Refresh DBs", ImVec2(120, 0)))
        {
            std::string err;
            availableDatabases.clear();
            selectedDatabaseIndex = -1;
            if (!fetchDatabases(availableDatabases, err))
            {
                connectError = err;
            }
            else
            {
                connectError.clear();
                if (!availableDatabases.empty())
                {
                    // ustawienie domyślnej bazy na pierwszą z listy
                    selectedDatabaseIndex = 0;
                    std::snprintf(dbBuf, sizeof(dbBuf), "%s", availableDatabases[0].c_str());
                }
            }
        }

        ImGui::SameLine();
        // pokazanie combo z dostępnymi bazami danych
        const char* comboLabel = (selectedDatabaseIndex >= 0 && selectedDatabaseIndex < static_cast<int>(availableDatabases.size()))
                                 ? availableDatabases[selectedDatabaseIndex].c_str()
                                 : (dbBuf[0] ? dbBuf : "Select database...");
        if (ImGui::BeginCombo("Database", comboLabel))
        {
            for (int i = 0; i < static_cast<int>(availableDatabases.size()); ++i)
            {
                bool selected = (i == selectedDatabaseIndex);
                if (ImGui::Selectable(availableDatabases[i].c_str(), selected))
                {
                    selectedDatabaseIndex = i;
                    std::snprintf(dbBuf, sizeof(dbBuf), "%s", availableDatabases[i].c_str());
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::PushItemWidth(320.0f);
        ImGui::InputText("Database (manual)", dbBuf, sizeof(dbBuf));
        ImGui::PopItemWidth();

        if (!connectError.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1.0f), "Error: %s", connectError.c_str());
        }

        ImGui::Spacing();
        if (ImGui::Button("Connect", ImVec2(120, 0)))
        {
            std::string err;
            if (tryConnect(err))
            {
                connectError.clear();
                auto tables = getTablesFromDatabase(*conn);
                tableSelector.setTables(tables);
            }
            else
            {
                connectError = err;
            }
        }
    }
    ImGui::End();
}

void App::drawCreateDatabaseWindow()
{
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(500, 60), ImGuiCond_FirstUseEver);

    static char newDbName[128] = "";

    if (ImGui::Begin("Create Database", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Create a new database:");
        ImGui::Separator();

        ImGui::PushItemWidth(320.0f);
        ImGui::InputText("Database Name", newDbName, sizeof(newDbName));
        ImGui::PopItemWidth();

        ImGui::Spacing();
        if (ImGui::Button("Create Database", ImVec2(150, 0)))
        {
            std::string error;
            if (createDatabase(hostBuf, userBuf, passBuf, newDbName))
            {
                std::snprintf(dbBuf, sizeof(dbBuf), "%s", newDbName);
                connectError.clear();
            }
            else
            {
                connectError = error;
            }
        }
    }
    ImGui::End();
}

void App::drawDropDatabaseWindow()
{
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(940, 60), ImGuiCond_FirstUseEver);

    static char dropDbName[128] = "";

    if (ImGui::Begin("Drop Database", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Drop an existing database:");
        ImGui::Separator();

        ImGui::PushItemWidth(320.0f);

        if (ImGui::Button("Refresh DBs", ImVec2(120, 0)))
        {
            std::string err;
            availableDatabases.clear();
            selectedDatabaseIndex = -1;
            if (!fetchDatabases(availableDatabasesForDrop, err))
            {
                connectError = err;
            }
            else
            {
                connectError.clear();
                if (!availableDatabasesForDrop.empty())
                {
                    selectedDatabaseIndex = 0;
                    std::snprintf(dropDbName, sizeof(dropDbName), "%s", availableDatabasesForDrop[0].c_str());
                }
            }
        }

        ImGui::SameLine();

        const char* comboLabel = (selectedDatabaseIndex >= 0 && selectedDatabaseIndex < static_cast<int>(availableDatabasesForDrop.size()))
                                 ? availableDatabasesForDrop[selectedDatabaseIndex].c_str()
                                 : (dropDbName[0] ? dropDbName : "Select database...");

        if (ImGui::BeginCombo("Database to Drop", comboLabel))
        {
            for (int i = 0; i < static_cast<int>(availableDatabasesForDrop.size()); ++i)
            {
                bool selected = (i == selectedDatabaseIndex);
                if (ImGui::Selectable(availableDatabasesForDrop[i].c_str(), selected))
                {
                    selectedDatabaseIndex = i;
                    std::snprintf(dropDbName, sizeof(dropDbName), "%s", availableDatabasesForDrop[i].c_str());
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();

        ImGui::Spacing();

        if (!connectError.empty())
        {
            ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1.0f), "Error: %s", connectError.c_str());
        }

        ImGui::Spacing();
        if (ImGui::Button("Drop Database", ImVec2(150, 0)))
        {
            ImGui::OpenPopup("Confirm Drop Database");
        }

        if (ImGui::BeginPopupModal("Confirm Drop Database", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::TextUnformatted("Are you sure you want to drop the database?");
            ImGui::Separator();

            if (ImGui::Button("Yes", ImVec2(120, 0)))
            {
                std::string error;
                if (dropDatabase(hostBuf, userBuf, passBuf, dropDbName))
                {
                    connectError.clear();
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    connectError = error;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

bool App::dropDatabase(const std::string& host, const std::string& user, const std::string& password, const std::string& dbName)
{
    try
    {
        // Połączenie bez określonej bazy danych
        sql::Driver* driver = sql::mariadb::get_driver_instance();
        std::string endpoint = "tcp://" + host + ":" + std::string(portBuf); 

        std::unique_ptr<sql::Connection> tmpConn(driver->connect(endpoint.c_str(), user.c_str(), password.c_str()));

        // Usunięcie bazy danych 
        std::unique_ptr<sql::Statement> stmt(tmpConn->createStatement());
        stmt->execute("DROP DATABASE IF EXISTS " + dbName);

        std::cout << "Database '" << dbName << "' dropped if it existed.\n";
        return true;
    }
    catch (sql::SQLException& e)
    {
        std::cerr << "Failed to drop database: " << e.what()
                  << "\nError code: " << e.getErrorCode()
                  << "\nSQLState: " << e.getSQLState() << std::endl;
        return false;
    }
}

bool App::tryConnect(std::string& outError)
{
    try
    {
        sql::Driver* driver = sql::mariadb::get_driver_instance();
        std::string endpoint = "tcp://" + std::string(hostBuf) + ":" + std::string(portBuf);

        std::unique_ptr<sql::Connection> newConn(driver->connect(
            endpoint.c_str(), userBuf, passBuf
        ));

        // Jeśli użytkownik podał nazwę bazy w polu dbBuf -> ustawiamy
        if (dbBuf[0] != '\0')
            newConn->setSchema(dbBuf);

        conn = std::move(newConn);

        // Zapisanie właściwości połączenia
        dbConnProps.host = hostBuf;
        dbConnProps.port = portBuf;
        dbConnProps.user = userBuf;
        dbConnProps.password = passBuf;
        dbConnProps.database = dbBuf;

        outError.clear();
        return true;
    }
    catch (sql::SQLException& e)
    {
        outError = e.what();
        conn.reset();
        return false;
    }
}

bool App::createDatabase(const std::string& host, const std::string& user, const std::string& password, const std::string& dbName)
{
    try
    {
        // Połączenie bez określonej bazy danych
        sql::Driver* driver = sql::mariadb::get_driver_instance();
        std::string endpoint = "tcp://" + host + ":" + std::string(portBuf); 

        std::unique_ptr<sql::Connection> tmpConn(driver->connect(endpoint.c_str(), user.c_str(), password.c_str()));

        // Utworzenie bazy danych 
        std::unique_ptr<sql::Statement> stmt(tmpConn->createStatement());
        stmt->execute("CREATE DATABASE IF NOT EXISTS " + dbName);

        std::snprintf(dbBuf, sizeof(dbBuf), "%s", dbName.c_str());

        std::cout << "Database '" << dbName << "' created or already exists.\n";
        return true;
    }
    catch (sql::SQLException& e)
    {
        std::cerr << "Failed to create database: " << e.what()
                  << "\nError code: " << e.getErrorCode()
                  << "\nSQLState: " << e.getSQLState() << std::endl;
        return false;
    }
}

bool App::fetchDatabases(std::vector<std::string>& outDatabases, std::string& outError)
{
    outDatabases.clear();
    try
    {
        sql::Driver* driver = sql::mariadb::get_driver_instance();
        std::string endpoint = "tcp://" + std::string(hostBuf) + ":" + std::string(portBuf);

        // Połączenie bez ustawiania schematu 
        std::unique_ptr<sql::Connection> tmpConn(driver->connect(endpoint.c_str(), userBuf, passBuf));
        std::unique_ptr<sql::Statement> stmt(tmpConn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SHOW DATABASES"));

        while (res->next())
        {
            outDatabases.push_back(res->getString(1).c_str());
        }
        outError.clear();
        return true;
    }
    catch (sql::SQLException& e)
    {
        outError = e.what();
        return false;
    }
    catch (const std::exception& e)
    {
        outError = e.what();
        return false;
    }
}

void App::showMain()
{
    // Jedno pełnoekranowe okno hostujące UI
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDecoration
      | ImGuiWindowFlags_NoMove
      | ImGuiWindowFlags_NoSavedSettings
      | ImGuiWindowFlags_NoBringToFrontOnFocus
      | ImGuiWindowFlags_MenuBar;

    ImGui::Begin(dbConnProps.database.empty() ? "Database" : dbConnProps.database.c_str(), nullptr, hostFlags);

    // Pasek menu i tabela
    std::string currentTable = tableSelector.render(*conn);
    if (!currentTable.empty())
    {
        auto tableData = getTableData(*conn, currentTable);
        showTable(tableData);
    }

    // Osługa popupu update 
    if (openUpdateRowRequested)
    {
        ImGui::OpenPopup(UPDATE_ROW_POPUP_ID);
        openUpdateRowRequested = false;
    }

    updateRowInTable(*conn, updateTableName, updatePkColumn, updatePkValue);

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void App::showTable(const TableData& data)
{
    if (data.headers.empty())
    {
        ImGui::TextUnformatted("There is nothing to show");
        return;
    }

    const int dataColumnCount = static_cast<int>(data.headers.size());
    const int totalColumns = dataColumnCount + 1;

    if (ImGui::BeginTable("DataTable", totalColumns,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        for (const auto& header : data.headers)
            ImGui::TableSetupColumn(header.c_str());
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();

        std::string tableName = tableSelector.getSelectedTable();
        std::string pkColumn  = getPKFromTable(*conn, tableName);

        int pkIndex = -1;
        if (!pkColumn.empty())
        {
            for (int i = 0; i < dataColumnCount; ++i)
            {
                if (data.headers[i] == pkColumn)
                {
                    pkIndex = i;
                    break;
                }
            }
        }

        for (int rowIndex = 0; rowIndex < static_cast<int>(data.rows.size()); ++rowIndex)
        {
            const auto& row = data.rows[rowIndex];
            ImGui::TableNextRow();
            ImGui::PushID(rowIndex);

            for (int c = 0; c < dataColumnCount; ++c)
            {
                ImGui::TableSetColumnIndex(c);
                const char* cell = (c < static_cast<int>(row.columns.size())) ? row.columns[c].c_str() : "";
                ImGui::TextUnformatted(cell);
            }

            ImGui::TableSetColumnIndex(dataColumnCount);

            float columnWidth = ImGui::GetColumnWidth(dataColumnCount);
            float buttonWidth = 25.0f;
            float offset = (columnWidth - 2 * buttonWidth) / 3.0f;

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            if (ImGui::SmallButton(ICON_FA_PEN))
            {
                if (pkIndex >= 0 && pkIndex < static_cast<int>(row.columns.size()))
                {
                    updateTableName = tableName;
                    updatePkColumn  = pkColumn;
                    updatePkValue   = row.columns[pkIndex];
                    openUpdateRowRequested = true;
                }
                else
                {
                    std::cerr << "Cannot update row: primary key column not found in headers for table "
                              << tableName << " (pk=" << pkColumn << ")\n";
                }
            }

            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            if (ImGui::SmallButton(ICON_FA_TRASH))
            {
                if (pkIndex >= 0 && pkIndex < static_cast<int>(row.columns.size()))
                {
                    const std::string& pkValue = row.columns[pkIndex];
                    deleteRowFromTable(*conn, tableName, pkColumn, pkValue);
                }
                else
                {
                    std::cerr << "Cannot delete row: primary key column not found in headers for table "
                              << tableName << " (pk=" << pkColumn << ")\n";
                }
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void App::run() 
{
    while (running && !glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        renderGUI();

        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}

void App::cleanup() 
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window) 
    {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}
