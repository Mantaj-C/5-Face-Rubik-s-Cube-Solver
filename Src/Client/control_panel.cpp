#define GL_SILENCE_DEPRECATION

#include "control_panel.h"

// -------------------------- UI LAYOUT CONSTANTS -------------------------- //
#define BUTTON_SIZE_AUTO 150.0f
#define BUTTON_GAP_AUTO 25.0f
#define BUTTON_SIZE_MANUAL 70.0f
#define BUTTON_GAP_MANUAL 15.0f
#define BUTTON_SIZE_PATTERN_SELECTION 50.0f
#define BUTTON_GAP_PATTERN_SELECTION 10.0f

// Gap between individual cublets in the 3x3x3 cube
#define CUBLET_GAP 0.05f

// Degrees rotated per frame when animating cube turns
#define ROTATION_SPEED 15.0f

// ========================================================================
//  Constructor / Destructor
// ========================================================================

/**
 * @brief Construct the ControlPanel.
 *
 * @param window  Reference to the main SFML render window.
 * @param font    Font used for all on-screen text.
 * @param sprite  Sprite that holds the big pattern spritesheet.
 */
ControlPanel::ControlPanel(sf::RenderWindow &window, sf::Font &font, sf::Sprite &sprite)
    : window(window),
      text(font),
      rotation(0),
      current_pattern_index(0),
      rotate_90_flag(false),
      rotate_180_flag(false),
      rotate_cw_flag(false),
      auto_flag(false),
      command("B "),        // "B " prefix for messages sent to the Pi
      solution({}),
      current_move_index(0),
      batch_move_flag(false),
      solution_find_time(0),
      solve_time(0),
      fastest_solve_time(0),
      timer_flag(false),
      pattern_sprite(sprite),
      sync_flag(false),
      unlock_flag(false) {

    // Create Manual Buttons (D / F / L / B / R / Unlock / Reset)
    init_manual_buttons(font);

    // Create Auto Buttons (Sync / Shuffle / Solve / Execute Pattern)
    init_auto_buttons(font);

    // Create Pattern Selection Buttons (-5, -1, +1, +5)
    init_pattern_selection_buttons(font);

    // Create and position all cublets of the 3x3x3 cube
    init_cublets();
}

ControlPanel::~ControlPanel() {}

// ========================================================================
//  Main Update (per frame)
// ========================================================================

/**
 * @brief Main update/render function.
 *
 * - Clears the OpenGL context
 * - Sets up camera + projection
 * - Draws 3D cube with OpenGL
 * - Draws 2D UI overlay with SFML
 * - Updates/handles cube rotations and command string
 *
 * @return Command string to send to the Pi (e.g. "B M 0 -1 90;")
 */
std::string ControlPanel::update() {
    // Enable depth testing so nearer geometry hides farther geometry.
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Make sure OpenGL viewport matches SFML window size.
    glViewport(0, 0, window.getSize().x, window.getSize().y);

    // ---------------------- PROJECTION MATRIX SETUP ---------------------- //
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float fovY   = 45.0f;
    float aspect = static_cast<float>(window.getSize().x) / window.getSize().y;
    float near   = 1.0f;
    float far    = 100.0f;

    // Build perspective frustum based on FOV and aspect ratio.
    float fH = tan(fovY / 360.0f * M_PI) * near;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, near, far);

    // ---------------------- MODELVIEW MATRIX SETUP ---------------------- //
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Spherical camera around the origin (cube).
    float radius = 6.0f;
    float camX = radius * cos(glm::radians(cameraAngleY)) * sin(glm::radians(cameraAngleX));
    float camY = radius * sin(glm::radians(cameraAngleY));
    float camZ = radius * cos(glm::radians(cameraAngleY)) * cos(glm::radians(cameraAngleX));

    gluLookAt(
        camX, camY, camZ,     // Camera position
        0.0f, 0.0f, 0.0f,     // Look at cube center
        0.0f, 1.0f, 0.0f      // Up direction
    );

    // Draw the 3D cube.
    draw_cube();

    // -------------------------- SFML UI OVERLAY -------------------------- //
    // Save OpenGL state so SFML can draw 2D things correctly on top.
    window.pushGLStates();

    // Manual move buttons.
    for (auto &button : manual_buttons) {
        window.draw(button.rectangle);
        window.draw(button.label_text);
    }

    // Auto buttons (Sync / Shuffle / Solve / Execute Pattern).
    for (auto &button : auto_buttons) {
        window.draw(button.rectangle);
        window.draw(button.label_text);
    }

    // Pattern selection buttons.
    for (auto &button : pattern_selection_buttons) {
        window.draw(button.rectangle);
        window.draw(button.label_text);
    }
    
    // Draw all descriptive labels and timers.
    draw_labels();
    
    // Restore OpenGL state.
    window.popGLStates();

    // Present frame.
    window.display();

    // ----------------------------- LOGIC -------------------------------- //
    // Base command prefix for this frame.
    command = "B ";

    // If we have a batch of moves from the solver, append them.
    if (batch_move_flag) {
        batch_move_send();
    }

    // If auto mode is active, animate solution moves.
    if (auto_flag) {
        handle_rotation_auto();
    }
    // If Sync was pressed, send an "S;" command once.
    else if (sync_flag){
        command += "S;";
        sync_flag = false;
    }
    // Otherwise, check manual buttons and animate a single face turn.
    else {
        handle_rotation_manual();
    }

    // Return full command string for this frame.
    return command;
}

// ========================================================================
//  Input Handling
// ========================================================================

/**
 * @brief Handle mouse clicks on all buttons.
 *
 * This function sets flags (rotate_90, rotate_180, auto_flag, etc.)
 * that are later consumed by handle_rotation_manual/auto().
 */
void ControlPanel::handle_clicks() {

    // Ignore clicks while cube is already rotating or auto is running.
    if (rotate_90_flag || rotate_180_flag || auto_flag )
        return;

    // ------------------------ MANUAL MOVE BUTTONS ------------------------ //
    for (auto &button : manual_buttons) {
        if (rect_contains_mouse(button.rectangle)) {
            // Each button sets a face flag and rotation configuration.
            if (button.label == "D" && !D_flag) {
                D_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "D'" && !D_flag) {
                D_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = false;
            }
            else if (button.label == "D2" && !D_flag) {
                D_flag = true;
                rotate_180_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "F" && !F_flag) {
                F_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "F'" && !F_flag) {
                F_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = false;
            }
            else if (button.label == "F2" && !F_flag) {
                F_flag = true;
                rotate_180_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "L" && !L_flag) {
                L_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "L'" && !L_flag) {
                L_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = false;
            }
            else if (button.label == "L2" && !L_flag) {
                L_flag = true;
                rotate_180_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "B" && !B_flag) {
                B_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "B'" && !B_flag) {
                B_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = false;
            }
            else if (button.label == "B2" && !B_flag) {
                B_flag = true;
                rotate_180_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "R" && !R_flag) {
                R_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = true;
            }
            else if (button.label == "R'" && !R_flag) {
                R_flag = true;
                rotate_90_flag = true;
                rotate_cw_flag = false;
            }
            else if (button.label == "R2" && !R_flag) {
                R_flag = true;
                rotate_180_flag = true;
                rotate_cw_flag = true;
            }
            // Unlock button (toggle physical lock on the Pi side).
            else if (button.label == "Unlock") {
                unlock_flag = true;
            }
            // Reset button: reset transforms and internal solver state.
            else if (button.label == "Reset") {
                for (auto &c : cublets) {
                    c.transform = glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(c.center.x, c.center.y, c.center.z)
                    );
                }
                solver.reset();
            }
        }
    }

    // -------------------------- AUTO BUTTONS ----------------------------- //
    for (auto &button : auto_buttons) {
        if (rect_contains_mouse(button.rectangle)) {
            if (button.label == "Sync") {
                // Request sync from camera/RPi.
                sync_flag = true;
            }
            else if (button.label == "Random Shuffle") {
                auto_flag = true;
                solution = solver.shuffle();   // Get shuffle sequence.
                batch_move_flag = true;        // Send to Pi.
            }
            else if (button.label == "Solve") {
                auto_flag = true;
                timer_flag = true;

                // Measure how long the solver takes to find a solution.
                auto start = std::chrono::high_resolution_clock::now();
                solution = solver.solve();
                auto end = std::chrono::high_resolution_clock::now();

                std::chrono::duration<double, std::milli> elapsed = end - start;
                solution_find_time = elapsed.count();

                batch_move_flag = true;
            }
            else if (button.label == "Execute Pattern") {
                auto_flag = true;
                solution = solver.execute_pattern(
                    Solver::Pattern(current_pattern_index)
                );
                batch_move_flag = true;
            }
        }
    }

    // --------------------- PATTERN SELECTION BUTTONS --------------------- //
    for (auto &button : pattern_selection_buttons) {
        if (rect_contains_mouse(button.rectangle)) {
            if (button.label == "-5") {
                current_pattern_index -= 5;
                if (current_pattern_index < 0)
                    current_pattern_index = 0;
            }
            else if (button.label == "-1") {
                current_pattern_index--;
                if (current_pattern_index < 0)
                    current_pattern_index = 0;
            }
            else if (button.label == "+1") {
                current_pattern_index++;
                if (current_pattern_index > static_cast<int>(Solver::Pattern::Pong))
                    current_pattern_index = static_cast<int>(Solver::Pattern::Pong);
            }
            else if (button.label == "+5") {
                current_pattern_index += 5;
                if (current_pattern_index > static_cast<int>(Solver::Pattern::Pong))
                    current_pattern_index = static_cast<int>(Solver::Pattern::Pong);
            }
        }
    }
}

/**
 * @brief Helper: check if mouse is inside a given rectangle.
 */
bool ControlPanel::rect_contains_mouse(sf::RectangleShape &rect) {
    sf::Vector2i Mouse_pos = sf::Mouse::getPosition(window);
    if (Mouse_pos.x > rect.getPosition().x &&
        Mouse_pos.x < rect.getPosition().x + rect.getSize().x &&
        Mouse_pos.y > rect.getPosition().y &&
        Mouse_pos.y < rect.getPosition().y + rect.getSize().y) {
        return true;
    }
    return false;
}

// ========================================================================
//  Cube Rendering / Rotation
// ========================================================================

/**
 * @brief Draw all cublets in their current transform.
 */
void ControlPanel::draw_cube() {
    for (auto &c : cublets) {
        glPushMatrix();

        // Apply the cublet's current transform matrix.
        glMultMatrixf(glm::value_ptr(c.transform));

        // Draw 6 faces of the cublet.
        glBegin(GL_QUADS);
            glColor3f(c.coordinates.front_color.x, c.coordinates.front_color.y, c.coordinates.front_color.z);
            for (auto &v : c.coordinates.front) glVertex3f(v.x, v.y, v.z);

            glColor3f(c.coordinates.back_color.x, c.coordinates.back_color.y, c.coordinates.back_color.z);
            for (auto &v : c.coordinates.back) glVertex3f(v.x, v.y, v.z);

            glColor3f(c.coordinates.top_color.x, c.coordinates.top_color.y, c.coordinates.top_color.z);
            for (auto &v : c.coordinates.top) glVertex3f(v.x, v.y, v.z);

            glColor3f(c.coordinates.bottom_color.x, c.coordinates.bottom_color.y, c.coordinates.bottom_color.z);
            for (auto &v : c.coordinates.bottom) glVertex3f(v.x, v.y, v.z);

            glColor3f(c.coordinates.right_color.x, c.coordinates.right_color.y, c.coordinates.right_color.z);
            for (auto &v : c.coordinates.right) glVertex3f(v.x, v.y, v.z);

            glColor3f(c.coordinates.left_color.x, c.coordinates.left_color.y, c.coordinates.left_color.z);
            for (auto &v : c.coordinates.left) glVertex3f(v.x, v.y, v.z);
        glEnd();

        glPopMatrix();
    }
}

/**
 * @brief Animate a single face rotation.
 *
 * @param face      Which face is being turned.
 * @param rotation  Accumulator tracking how much has been rotated so far.
 * @param angle     Target angle (90 or 180 degrees).
 * @param direction +1 or -1 (relative direction).
 *
 * @return true if the rotation has completed, false otherwise.
 */
bool ControlPanel::rotate_face(Face face, float &rotation, float angle, int direction) {
    // If we've nearly hit the target angle, snap and stop.
    if (std::abs(rotation - angle) < 0.01f) {
        rotation = 0;
        return true;
    }

    // Increment how far we've rotated this move.
    rotation += ROTATION_SPEED;

    // Some faces visually rotate in opposite direction (animation only).
    if (face == Face::East || face == Face::Bottom || face == Face::South) {
        direction *= -1;  // Flip ONLY for animation.
    }

    float radians = glm::radians(ROTATION_SPEED * direction);
    glm::vec3 axis;

    // Choose rotation axis based on which face we’re turning.
    switch (face) {
        case Face::North:
        case Face::South:  axis = glm::vec3(0, 0, 1); break;  // Z axis
        case Face::Top:
        case Face::Bottom: axis = glm::vec3(0, 1, 0); break;  // Y axis
        case Face::East:
        case Face::West:   axis = glm::vec3(1, 0, 0); break;  // X axis
    }

    glm::mat4 rot_mat = glm::rotate(glm::mat4(1.0f), radians, axis);

    // Apply the rotation to every cublet that lies on that face.
    for (auto& c : cublets) {
        if (cublet_on_face(glm::vec3(c.transform[3]), face)) {
            // Rotate transform in world space around the origin (0,0,0).
            c.transform = rot_mat * c.transform;
        }
    }

    return false;
}

/**
 * @brief Check if a cublet center belongs to a given face.
 *
 * @param center  Cublet center position in world space.
 * @param face    Face we are checking (North, South, Top, etc).
 */
bool ControlPanel::cublet_on_face(const glm::vec3 &center, Face face) {
    constexpr float EPSILON = 0.01f;

    switch (face) {
        case Face::North:  return center.z >  1.0f - EPSILON;
        case Face::South:  return center.z < -1.0f + EPSILON;
        case Face::Top:    return center.y >  1.0f - EPSILON;
        case Face::Bottom: return center.y < -1.0f + EPSILON;
        case Face::West:   return center.x >  1.0f - EPSILON;
        case Face::East:   return center.x < -1.0f + EPSILON;
        default:           return false;
    }
}

// ========================================================================
//  Cube Initialization
// ========================================================================

/**
 * @brief Initialize all 26 visible cublets with geometry + colors.
 */
void ControlPanel::init_cublets() {
    // Simple RGB-style face colors.
    sf::Vector3f gray(0.5f, 0.5f, 0.5f);
    sf::Vector3f red(1.0f, 0.0f, 0.0f);
    sf::Vector3f green(0.0f, 1.0f, 0.0f);
    sf::Vector3f blue(0.0f, 0.0f, 1.0f);
    sf::Vector3f yellow(1.0f, 1.0f, 0.0f);
    sf::Vector3f orange(1.0f, 0.5f, 0.0f);
    sf::Vector3f white(1.0f, 1.0f, 1.0f);

    float half = 0.5f;   // Half-cublet edge length.

    // Loop over a 3x3x3 grid (xi, yi, zi ∈ {-1, 0, 1}).
    for (int xi = -1; xi <= 1; ++xi) {
        for (int yi = -1; yi <= 1; ++yi) {
            for (int zi = -1; zi <= 1; ++zi) {
                // Skip center cublet (it doesn't exist physically).
                if (xi == 0 && yi == 0 && zi == 0) continue;

                float x = xi * (1.0f + CUBLET_GAP);
                float y = yi * (1.0f + CUBLET_GAP);
                float z = zi * (1.0f + CUBLET_GAP);

                glm::vec3 center(x, y, z);
                Cublet_faces c;

                // All geometry is defined around origin and moved by transform.
                c.front = {
                    {-half, -half,  half},
                    { half, -half,  half},
                    { half,  half,  half},
                    {-half,  half,  half}
                };
                c.front_color = (z > 0) ? red : gray;

                c.back = {
                    {-half, -half, -half},
                    {-half,  half, -half},
                    { half,  half, -half},
                    { half, -half, -half}
                };
                c.back_color = (z < 0) ? orange : gray;

                c.top = {
                    {-half,  half, -half},
                    {-half,  half,  half},
                    { half,  half,  half},
                    { half,  half, -half}
                };
                c.top_color = (y > 0) ? white : gray;

                c.bottom = {
                    {-half, -half, -half},
                    { half, -half, -half},
                    { half, -half,  half},
                    {-half, -half,  half}
                };
                c.bottom_color = (y < 0) ? yellow : gray;

                c.right = {
                    { half, -half, -half},
                    { half,  half, -half},
                    { half,  half,  half},
                    { half, -half,  half}
                };
                c.right_color = (x > 0) ? blue : gray;

                c.left = {
                    {-half, -half, -half},
                    {-half, -half,  half},
                    {-half,  half,  half},
                    {-half,  half, -half}
                };
                c.left_color = (x < 0) ? green : gray;

                // Initial transform puts this cublet at its center.
                cublets.push_back(
                    Cublet{c, glm::translate(glm::mat4(1.0f), center), center}
                );
            }
        }
    }
}

// ========================================================================
//  Auto / Manual Rotation Logic
// ========================================================================

/**
 * @brief Auto mode: step through solution moves and animate them.
 */
void ControlPanel::handle_rotation_auto() {
    if (solution.size() > current_move_index) {
        // Mapping from solver index to actual Face enum.
        std::vector<std::pair<int, Face>> face_order = {
            {0, Face::Bottom},
            {1, Face::North},
            {2, Face::East},
            {3, Face::South},
            {4, Face::West}
        };

        Face face      = face_order[solution[current_move_index][0]].second;
        int direction  = solution[current_move_index][1];
        int angle      = solution[current_move_index][2];

        // Animate rotation; once done, inform solver and move to next move.
        if (rotate_face(face, rotation, angle, direction)) {
            solver.match_move(
                face_order[solution[current_move_index][0]].first,
                direction,
                angle
            );
            rotation = 0;
            current_move_index++;
        }
    }
    else {
        // Finished all moves: print facelet state, reset auto state.
        solver.print_cubes_facelet();
        auto_flag = false;
        current_move_index = 0;
    }
}

/**
 * @brief Manual mode: handle one face turn based on button flags.
 */
void ControlPanel::handle_rotation_manual() {
    // If unlock button pressed, send lock/unlock command.
    if (unlock_flag) {
        std::string lock_command = unlock_flag ? "L" : "U";
        command += lock_command;
        unlock_flag = false;
    }

    // Only proceed if a rotation has been requested.
    if (rotate_90_flag || rotate_180_flag) {
        int direction = rotate_cw_flag ? -1 : 1;
        int angle     = rotate_90_flag ? 90 : 180;

        int  face_message_index = 0;  // Index for solver's move mapping.
        Face face = Face::Bottom;     // Default.

        // Determine which face is being rotated and resolve message index.
        if (F_flag) {
            face = Face::North;
            face_message_index = 1;
        }
        else if (L_flag) {
            face = Face::East;
            face_message_index = 2;
        }
        else if (B_flag) {
            face = Face::South;
            face_message_index = 3;
        }
        else if (R_flag) {
            face = Face::West;
            face_message_index = 4;
        }

        // When rotation just starts (rotation == 0), send move to the Pi
        // and update solver so its internal cube matches animation.
        if (rotation == 0) {
            command += "M " + face_to_message[face] +
                       std::to_string(direction * -1) + " " +
                       std::to_string(angle) + ";";
            solver.match_move(face_message_index, direction, angle);
        }

        // Animate rotation; once complete, clear all flags.
        if (rotate_face(face, rotation, angle, direction)) {
            solver.print_cubes_facelet();
            rotation        = 0;
            rotate_90_flag  = false;
            rotate_cw_flag  = false;
            rotate_180_flag = false;
            D_flag = F_flag = L_flag = B_flag = R_flag = false;
        }
    }
}

// ========================================================================
//  Button Initialization
// ========================================================================

/**
 * @brief Create manual control buttons (D, F, L, B, R, Unlock, Reset).
 */
void ControlPanel::init_manual_buttons(sf::Font &font) {
    std::vector<std::string> button_labels = {
        "D", "D'", "D2",
        "F", "F'", "F2",
        "L", "L'", "L2",
        "B", "B'", "B2",
        "R", "R'", "R2"
    };

    int   buttons_per_row = 3;
    float start_x = 10.0f;
    float start_y = 50.0f; // Starting Y for manual button grid.
     
    for (int i = 0; i < static_cast<int>(button_labels.size()); i++) {
        sf::RectangleShape rectangle;
        rectangle.setSize(sf::Vector2f(BUTTON_SIZE_MANUAL, BUTTON_SIZE_MANUAL / 2));
        rectangle.setOutlineColor(sf::Color(128, 128, 128)); // gray border
        rectangle.setOutlineThickness(2);
        rectangle.setFillColor(sf::Color(128, 128, 128));    // gray fill
     
        // Compute button position based on row/column.
        int row = i / buttons_per_row;
        int col = i % buttons_per_row;
        float x = start_x + col * (BUTTON_SIZE_MANUAL + BUTTON_GAP_MANUAL);
        float y = start_y + row * ((BUTTON_SIZE_MANUAL / 2) + BUTTON_GAP_MANUAL);
     
        rectangle.setPosition({x, y});
     
        sf::Text label_text(font);
        label_text.setString(button_labels[i]);
        label_text.setCharacterSize(20);
        label_text.setFillColor(sf::Color::White);

        // Center label inside the rectangle.
        sf::FloatRect textBounds = label_text.getLocalBounds();
        label_text.setOrigin(textBounds.getCenter());
        label_text.setPosition(rectangle.getPosition() + rectangle.getSize() / 2.0f);
     
        manual_buttons.push_back(Button{rectangle, button_labels[i], label_text});
    }

    // --------------------------- Unlock button --------------------------- //
    sf::RectangleShape rectangle;
    rectangle.setSize(sf::Vector2f(BUTTON_SIZE_MANUAL, BUTTON_SIZE_MANUAL / 2));
    rectangle.setOutlineColor(sf::Color(128, 128, 128));
    rectangle.setOutlineThickness(2);
    rectangle.setFillColor(sf::Color(128, 128, 128));
    rectangle.setPosition({
        static_cast<float>(window.getSize().x)/2.0f - BUTTON_SIZE_MANUAL/2.0f,
        BUTTON_GAP_MANUAL
    });

    sf::Text label_text(font);
    label_text.setString("Unlock");
    label_text.setCharacterSize(20);
    label_text.setFillColor(sf::Color::White);

    sf::FloatRect textBounds = label_text.getLocalBounds();
    label_text.setOrigin(textBounds.getCenter());
    label_text.setPosition(rectangle.getPosition() + rectangle.getSize() / 2.0f);

    manual_buttons.push_back(Button{rectangle, "Unlock", label_text});

    // ---------------------------- Reset button --------------------------- //
    rectangle.setSize(sf::Vector2f(BUTTON_SIZE_AUTO, BUTTON_SIZE_AUTO / 2));
    rectangle.setOutlineColor(sf::Color::Red);
    rectangle.setFillColor(sf::Color::Red);
    rectangle.setPosition({
        BUTTON_GAP_AUTO,
        static_cast<float>(window.getSize().y) - (BUTTON_SIZE_AUTO / 2) - BUTTON_GAP_AUTO
    });

    label_text.setString("Reset");
    label_text.setCharacterSize(40);
    textBounds = label_text.getLocalBounds();
    label_text.setOrigin(textBounds.getCenter());
    label_text.setPosition(rectangle.getPosition() + rectangle.getSize() / 2.0f);

    manual_buttons.push_back(Button{rectangle, "Reset", label_text});
}

/**
 * @brief Create auto control buttons (Sync, Random Shuffle, Solve, Execute Pattern).
 */
void ControlPanel::init_auto_buttons(sf::Font &font) {
    std::vector<std::string> button_labels = {
        "Sync", "Random Shuffle", "Solve", "Execute Pattern"
    };

    for (int i = 0; i < static_cast<int>(button_labels.size()); i++) {
        sf::RectangleShape rectangle;
        rectangle.setSize(sf::Vector2f(BUTTON_SIZE_AUTO, BUTTON_SIZE_AUTO / 2));
        rectangle.setOutlineColor(sf::Color(128, 128, 128)); // gray
        rectangle.setOutlineThickness(2);
        rectangle.setFillColor(sf::Color(128, 128, 128));    // gray
        rectangle.setPosition({
            static_cast<float>(window.getSize().x) - 30 - BUTTON_SIZE_AUTO,
            (BUTTON_SIZE_AUTO / 2) + (i * (BUTTON_GAP_AUTO + (BUTTON_SIZE_AUTO / 2)))
        });

        sf::Text label_text(font);
        label_text.setString(button_labels[i]);
        label_text.setCharacterSize(20);
        label_text.setFillColor(sf::Color::White);

        sf::FloatRect textBounds = label_text.getLocalBounds();
        label_text.setOrigin(textBounds.getCenter());
        label_text.setPosition(rectangle.getPosition() + rectangle.getSize() / 2.0f);

        auto_buttons.push_back(Button{rectangle, button_labels[i], label_text});
    }
}

/**
 * @brief Create pattern index adjustment buttons (-5, -1, +1, +5).
 */
void ControlPanel::init_pattern_selection_buttons(sf::Font &font) {
    std::vector<std::string> button_labels = {"-5", "-1", "+1", "+5"};

    for (int i = 0; i < static_cast<int>(button_labels.size()); i++) {
        sf::RectangleShape rectangle;
        rectangle.setSize(sf::Vector2f(BUTTON_SIZE_PATTERN_SELECTION, BUTTON_SIZE_PATTERN_SELECTION / 2));
        rectangle.setOutlineColor(sf::Color(128, 128, 128)); // gray
        rectangle.setOutlineThickness(2);
        rectangle.setFillColor(sf::Color(128, 128, 128));    // gray
        rectangle.setPosition({
            20.f + (i * (BUTTON_SIZE_PATTERN_SELECTION + BUTTON_GAP_PATTERN_SELECTION)),
            static_cast<float>(window.getSize().y) - 450
        });

        sf::Text label_text(font);
        label_text.setString(button_labels[i]);
        label_text.setCharacterSize(20);
        label_text.setFillColor(sf::Color::White);

        sf::FloatRect textBounds = label_text.getLocalBounds();
        label_text.setOrigin(textBounds.getCenter());
        label_text.setPosition(rectangle.getPosition() + rectangle.getSize() / 2.0f);

        pattern_selection_buttons.push_back(Button{rectangle, button_labels[i], label_text});
    }
}

// ========================================================================
//  Label Drawing
// ========================================================================

/**
 * @brief Draw all static labels, timers, and the pattern sprite.
 */
void ControlPanel::draw_labels() {
    text.setCharacterSize(30);
    text.setFillColor(sf::Color::White);

    // Section titles.
    text.setString("Manual Control");
    text.setPosition(sf::Vector2f(20, (BUTTON_SIZE_MANUAL / 8)));
    window.draw(text);

    text.setString("Auto Control");
    text.setPosition(sf::Vector2f(window.getSize().x - 190, (BUTTON_SIZE_MANUAL / 8)));
    window.draw(text);

    text.setString("Pattern Selection");
    text.setPosition(sf::Vector2f(20, static_cast<float>(window.getSize().y) - 500));
    window.draw(text);

    // Current pattern name.
    text.setString(std::string("Pattern: ") + pattern_to_string(Solver::Pattern(current_pattern_index)));
    text.setPosition(sf::Vector2f(20, static_cast<float>(window.getSize().y) - 400));
    window.draw(text);

    // Solver timing info.
    text.setString("Solution Find Time:");
    text.setPosition(sf::Vector2f(900, static_cast<float>(window.getSize().y) - 340));
    window.draw(text);

    text.setString(std::to_string(static_cast<int>(solution_find_time)) + " ms");
    text.setPosition(sf::Vector2f(1050, static_cast<float>(window.getSize().y) - 300));
    window.draw(text);

    text.setString("Solve Time: ");
    text.setPosition(sf::Vector2f(900, static_cast<float>(window.getSize().y) - 240));
    window.draw(text);

    text.setString(std::to_string(static_cast<int>(solve_time)) + " ms");
    text.setPosition(sf::Vector2f(1050, static_cast<float>(window.getSize().y) - 200));
    window.draw(text);

    text.setString("Fastest Solve Time: ");
    text.setPosition(sf::Vector2f(900, static_cast<float>(window.getSize().y) - 140));
    window.draw(text);

    text.setString(std::to_string(static_cast<int>(fastest_solve_time)) + " ms");
    text.setPosition(sf::Vector2f(1050, static_cast<float>(window.getSize().y) - 100));
    window.draw(text);

    // Draw pattern preview from spritesheet.
    int x = current_pattern_index % 6;
    int y = current_pattern_index / 6;
    pattern_sprite.setPosition(sf::Vector2f(50, static_cast<float>(window.getSize().y) - 340));
    pattern_sprite.setTextureRect(
        sf::IntRect(sf::Vector2i(x * 150, y * 150), sf::Vector2i(150, 150))
    );
    window.draw(pattern_sprite);
}

// ========================================================================
//  Command / Timing Helpers
// ========================================================================

/**
 * @brief Convert the current solution sequence into a batch of "M ..." commands.
 *
 * Also prepends a "T;" if timer_flag is set (to tell the Pi to measure time).
 */
void ControlPanel::batch_move_send() {
    if (timer_flag) {
        command += "T;";
        if (!solution.empty()) {
            moves_storage.push_back(solution.size());
        }
    }

    // Append each solution move as an "M" command.
    for (auto &s : solution) {
        command += "M " +
                   number_to_message[s[0]] +
                   std::to_string(s[1] * -1) + " " +
                   std::to_string(s[2]) + ";";
    }

    batch_move_flag = false;
    timer_flag = false;
}

/**
 * @brief Human-readable pattern names for each Solver::Pattern enum.
 */
const char* ControlPanel::pattern_to_string(Solver::Pattern p) {
    switch (p) {
        case Solver::Pattern::Superflip:               return "Superflip";
        case Solver::Pattern::EasyCheckerboard:        return "Easy Checkerboard";
        case Solver::Pattern::SpeedsolvingLogo:        return "Speedsolving Logo";
        case Solver::Pattern::ThreeSidesSolved:        return "Three Sides Solved";
        case Solver::Pattern::AllFlagsAndCrests:       return "All Flags and Crests";
        case Solver::Pattern::Wire:                    return "Wire";
        case Solver::Pattern::CheckerboardInTheCube:   return "Checkerboard in the Cube";
        case Solver::Pattern::PerfectScramble:         return "Perfect Scramble";
        case Solver::Pattern::Emoticon:                return "Emoticon";
        case Solver::Pattern::MinorityCross:           return "Minority Cross";
        case Solver::Pattern::PerpendicularLines:      return "Perpendicular Lines";
        case Solver::Pattern::FlippedTips:             return "Flipped Tips";
        case Solver::Pattern::PlusMinus:               return "Plus Minus";
        case Solver::Pattern::Tablecloth:              return "Tablecloth";
        case Solver::Pattern::Deckerboard:             return "Deckerboard";
        case Solver::Pattern::SpiralPattern:           return "Spiral Pattern";
        case Solver::Pattern::FruitBowl:               return "Fruit Bowl";
        case Solver::Pattern::Flower:                  return "Flower";
        case Solver::Pattern::VerticalStripes:         return "Vertical Stripes";
        case Solver::Pattern::GiftBox:                 return "Gift Box";
        case Solver::Pattern::OppositeCorners:         return "Opposite Corners";
        case Solver::Pattern::Cross:                   return "Cross";
        case Solver::Pattern::FourCrosses:             return "Four Crosses";
        case Solver::Pattern::UnionJack:               return "Union Jack";
        case Solver::Pattern::CubeInTheCube:           return "Cube in the Cube";
        case Solver::Pattern::CubeInACubeInACube:      return "Cube in a Cube in a Cube";
        case Solver::Pattern::Anaconda:                return "Anaconda";
        case Solver::Pattern::Python:                  return "Python";
        case Solver::Pattern::BlackMamba:              return "Black Mamba";
        case Solver::Pattern::GreenMamba:              return "Green Mamba";
        case Solver::Pattern::Tangled:                 return "Tangled";
        case Solver::Pattern::FourSpots:               return "Four Spots";
        case Solver::Pattern::SixSpots:                return "Six Spots";
        case Solver::Pattern::Twister:                 return "Twister";
        case Solver::Pattern::Kilt:                    return "Kilt";
        case Solver::Pattern::Tetris:                  return "Tetris";
        case Solver::Pattern::DontCrossLine:           return "Don't Cross Line";
        case Solver::Pattern::HiAllAround:             return "Hi All Around";
        case Solver::Pattern::DisplacedMotif:          return "Displaced Motif";
        case Solver::Pattern::AreYouHigh:              return "Are You High";
        case Solver::Pattern::CUAround:                return "CU Around";
        case Solver::Pattern::OrderInChaos:            return "Order in Chaos";
        case Solver::Pattern::EvenlyDistributed:       return "Evenly Distributed";
        case Solver::Pattern::TheHole:                 return "The Hole";
        case Solver::Pattern::NoEntry:                 return "No Entry";
        case Solver::Pattern::Plus:                    return "Plus";
        case Solver::Pattern::ThreeCThreeW:            return "Three C Three W";
        case Solver::Pattern::Pong:                    return "Pong";
        default:                                       return "Unknown";
    }
}

/**
 * @brief Parse solve time string from Pi ("T 123.45") and update statistics.
 */
void ControlPanel::update_solve_times(std::string time) {
    // Remove the "T " prefix.
    std::string num_str = time.substr(2);
    // Remove newline characters if present.
    num_str.erase(std::remove(num_str.begin(), num_str.end(), '\n'), num_str.end());

    // Ignore times <= 10 ms (likely noise or invalid).
    if (std::stod(num_str) <= 10)
        return;

    // Store latest solve time.
    solve_time = std::stod(num_str);
    solve_times.push_back(solve_time);

    // Update fastest solve time.
    if (fastest_solve_time == 0 || solve_time < fastest_solve_time) {
        fastest_solve_time = solve_time;
    }
}

/**
 * @brief Save all solve results (moves + times) to CSV file.
 *
 * File format: solve_results.csv
 * Columns: Moves, Solve time (ms)
 */
void ControlPanel::saveToCSV() {
    bool writeHeader =
        !std::filesystem::exists("solve_results.csv") ||
        std::filesystem::file_size("solve_results.csv") == 0;

    std::ofstream file("solve_results.csv", std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: solve_results.csv\n";
        return;
    }

    // Only write header if the file is new or empty.
    if (writeHeader) {
        file << "Moves,Solve time (ms)\n";
    }

    // Ensure we don't go out of bounds.
    size_t count = std::min(moves_storage.size(), solve_times.size());
    for (size_t i = 0; i < count; ++i) {
        file << moves_storage[i] << "," << solve_times[i] << "\n";
    }

    file.close();
}

/**
 * @brief Sync cube state from camera/solver response.
 *
 * @param response String from Pi (e.g. "S UUU...RRR...").
 */
void ControlPanel::sync(std::string response) {
    // Remove "S " prefix.
    std::string facelet = response.substr(2);
    // Strip newlines.
    facelet.erase(std::remove(facelet.begin(), facelet.end(), '\n'), facelet.end());

    // Ask solver for a sequence that transforms current cube to this facelet.
    solution = solver.sync(facelet);
    auto_flag = true;  // Trigger auto mode to execute it.
}