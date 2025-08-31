#include "imgui-SFML.h"
#include <SFML/Graphics.hpp>
#include "app.h"

#ifdef _WIN32
    #define _WIN32_WINNT 0x0500
    #include <windows.h>
#endif

int main() {
#ifdef _WIN32
    ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

    using namespace sf;
    using namespace TV::Math;
    RenderWindow window(VideoMode(Vector2u{1000, 1000}), "Spline Generator", Style::Titlebar | Style::Close);
    window.setFramerateLimit(60);
    if (!ImGui::SFML::Init(window)) return -1;

    App app(window);
    app.run();

    ImGui::SFML::Shutdown();
    return 0;
}
