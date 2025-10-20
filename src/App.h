#pragma once
#include <memory>
#include <string>
#include <iostream>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <mariadb/conncpp.hpp>
#include "TableSelectionBar.h"

struct WindowProps 
{
    int width;
    int height;
    std::string title;
};

struct DbConnProps 
{
    std::string host;
    std::string port;
    std::string user;
    std::string password;
    std::string database;
};

class App 
{
public:
    // Teraz tylko właściwości okna — połączenie zrobimy przez ImGui
    explicit App(WindowProps props);
    ~App();

    void run();

private:
    void init(WindowProps props);  
    void cleanup();
    void renderGUI();

    // Rysuje i obsługuje okno łączenia (nie modal, zawsze widoczne dopóki brak połączenia)
    void drawConnectWindow();

    // Próba połączenia na podstawie buforów z okna
    bool tryConnect(std::string& outError);

    // Widok danych po połączeniu
    void showMain();
    void showTable(const TableData& data);

    GLFWwindow* window = nullptr;
    WindowProps windowProps;
    bool running = true;

    // MariaDB
    std::unique_ptr<sql::Connection> conn;
    DbConnProps dbConnProps; // wypełni się po udanym połączeniu

    TableSelectorBar tableSelector;

    // Stan okna łączenia (zwykłe ImGui::Begin)
    char hostBuf[128]{};
    char portBuf[32]{};
    char userBuf[128]{};
    char passBuf[128]{};
    char dbBuf[128]{};
    std::string connectError;

    // Stan popupu Update 
    bool openUpdateRowRequested = false;
    std::string updateTableName;
    std::string updatePkColumn;
    std::string updatePkValue;
};

// Icons
extern const char* ICON_FA_TRASH;
extern const char* ICON_FA_PEN;
