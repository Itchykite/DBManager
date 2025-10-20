#include "App.h"

int main() 
{
    try 
    {
        WindowProps props;
        props.width = 1920;
        props.height = 1080;
        props.title = "RestauracjaDB";

        App app(props);
        app.run();
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Błąd: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
