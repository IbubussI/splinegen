#include "imgui-SFML.h"
#include <SFML/Graphics.hpp>
#include "app.h"

int main() {
    using namespace sf;
    using namespace TV::Math;
    RenderWindow window(VideoMode(Vector2u{800, 800}), "Spline Generator", Style::Titlebar | Style::Close);
    window.setFramerateLimit(60);
    if (!ImGui::SFML::Init(window)) return -1;

    App app(window);
    app.run();

    ImGui::SFML::Shutdown();
    return 0;
}
