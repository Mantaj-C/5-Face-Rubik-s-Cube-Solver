#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <OpenGL/glu.h>

#include "control_panel.h"

// ---------------------------- Network Config ---------------------------- //

#define SERVER_IP   "xxx.xxx.x.xx"  // IP address of Raspberry Pi server
#define PORT        5050            // TCP port
#define BUFFER_SIZE 1024            // Receive buffer size

// ---------------------------- Window Config ----------------------------- //

#define WINDOW_WIDTH  1200
#define WINDOW_HEIGHT 800

// Global flag to track whether the user is dragging the mouse (for camera).
bool dragging = false;

// ========================================================================
//  Helper: Receive and handle a single server response
// ========================================================================

/**
 * @brief Receive one message from the server and let ControlPanel process it.
 *
 * - If the message starts with 'T' -> timing info -> update_solve_times()
 * - If the message starts with 'S' -> sync info   -> sync()
 *
 * @param control_panel  Reference to the UI/logic controller.
 * @param sock           Connected TCP socket to the Pi.
 */
void server_response(ControlPanel &control_panel, int sock) {
    char buffer[BUFFER_SIZE] = {0};

    // Blocking receive of one message from the server.
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';  // Ensure null termination
        // std::cout << "Response from server: " << buffer << "\n";
    }

    // First character indicates message type.
    if (buffer[0] == 'T') {
        // Timing message: "T <time>"
        control_panel.update_solve_times(buffer);
    }
    else if (buffer[0] == 'S') {
        // Sync message: "S <facelets>"
        control_panel.sync(buffer);
    }
}

// ========================================================================
//  Helper: Send a command and wait for the server's response
// ========================================================================

/**
 * @brief Send a command string to the server and immediately read response.
 *
 * @param control_panel  Reference to control panel, for processing reply.
 * @param sock           Connected TCP socket.
 * @param command        Command string to send (e.g. "B M 0 -1 90;").
 */
void send_command(ControlPanel &control_panel, int sock, std::string command) {
    send(sock, command.c_str(), command.size(), 0);
    // std::cout << "Sent " << command << " to server\n";

    // After sending, read the server's response once.
    server_response(control_panel, sock);
}

// ========================================================================
//  main()
// ========================================================================

int main() {
    // --------------------------- SFML OpenGL Setup --------------------------- //

    sf::ContextSettings settings;
    settings.depthBits         = 24;  // Depth buffer precision
    settings.stencilBits       = 8;   // Stencil buffer precision
    settings.antiAliasingLevel = 4;   // MSAA level
    settings.majorVersion      = 2;   // OpenGL major version
    settings.minorVersion      = 1;   // OpenGL minor version

    // Create main window. (Signature may vary depending on SFML version.)
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(WINDOW_WIDTH, WINDOW_HEIGHT)),
        "Control Panel",
        sf::Style::Default,
        sf::State::Windowed,
        settings
    );

    window.setVerticalSyncEnabled(true); // vsync for smoother rendering
    window.setActive(true);              // activate context on this thread

    // Enable depth testing for proper 3D cube rendering.
    glEnable(GL_DEPTH_TEST);

    // --------------------------- Resources (Font & Texture) ------------------ //

    sf::Font font;
    // NOTE: SFML usually uses loadFromFile(); openFromFile() is SFML 3-style.
    if (!font.openFromFile("ARIAL.ttf")) {
        std::cerr << "Font failed to load!\n";
    }

    sf::Texture pattern_texture;
    pattern_texture.loadFromFile("Patterns_Sprite_Sheet.png");

    // Sprite for pattern preview (ControlPanel will manage its texture rect).
    sf::Sprite sprite(pattern_texture);

    // Main controller: handles cube rendering, UI, and Pi commands.
    ControlPanel control_panel(window, font, sprite);

    // --------------------------- Network: Connect to Pi ---------------------- //

    // Create TCP socket.
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Server address structure.
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    // Keep trying to connect until successful.
    while (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "Connection failed, retrying in 5 seconds\n";
        sleep(5);
    }

    std::cout << "Connected to Pi on port " << PORT << "\n";

    // --------------------------- Main Loop ---------------------------------- //

    while (window.isOpen()) {
        // Process all pending events.
        while (auto event = window.pollEvent()) {
            // Window closed (X button or OS close).
            if (event->is<sf::Event::Closed>()) {
                window.close();

                // Notify server we are quitting.
                std::string command = "Q";
                send(sock, command.c_str(), command.size(), 0);
                std::cout << "Sent Quit to server\n";

                server_response(control_panel, sock);
            }

            // Mouse pressed: start dragging (for camera orbit) and handle clicks.
            if (event->is<sf::Event::MouseButtonPressed>() &&
                sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {

                dragging = true;
                control_panel.lastMousePos = sf::Mouse::getPosition(window);

                // Let ControlPanel handle button clicks, etc.
                control_panel.handle_clicks();
            }

            // Mouse released: stop dragging.
            if (event->is<sf::Event::MouseButtonReleased>() &&
                !sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {

                dragging = false;
            }

            // Mouse moved while dragging: adjust camera angles.
            if (event->is<sf::Event::MouseMoved>() && dragging) {
                sf::Vector2i newPos = sf::Mouse::getPosition(window);
                sf::Vector2i delta  = newPos - control_panel.lastMousePos;
                control_panel.lastMousePos = newPos;

                float sensitivity = -0.3f;  // adjust for desired feel
                control_panel.cameraAngleX += delta.x * sensitivity;
                control_panel.cameraAngleY -= delta.y * sensitivity;

                // Clamp vertical tilt to avoid flipping the camera.
                control_panel.cameraAngleY = std::clamp(
                    control_panel.cameraAngleY, -89.0f, 89.0f
                );
            }

            // Keyboard events for extra debug/utility commands.
            if (event->is<sf::Event::KeyPressed>()) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::U)) {
                    // Example: "B P;" request to Pi (maybe capture frame / something).
                    std::string command = "B P;";
                    send(sock, command.c_str(), command.size(), 0);
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
                    // Example: "B G;" request to Pi (maybe toggle GPIO / debug).
                    std::string command = "B G;";
                    send(sock, command.c_str(), command.size(), 0);
                }
            }
        }

        // Render + update cube + UI; get command string to send to server.
        std::string command = control_panel.update();

        // If command is more than just "B " (no-op), send to Pi asynchronously.
        if (command != "B ") {
            std::thread([&control_panel, sock, command]() {
                send_command(control_panel, sock, command);
            }).detach();
        }
    }

    // --------------------------- Cleanup ------------------------------------ //

    // Close TCP socket.
    close(sock);
    std::cout << "Connection closed.\n";

    // Save all solve statistics.
    control_panel.saveToCSV();

    return 0;
}