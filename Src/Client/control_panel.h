#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <OpenGL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>  // glm::rotate, glm::translate, glm::perspective
#include <glm/gtc/type_ptr.hpp>          // glm::value_ptr()
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <thread>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include "Solver.h"

/**
 * @brief UI + 3D controller for the Rubik's Cube scene.
 *
 * - Renders the cube with OpenGL
 * - Draws SFML UI (buttons, labels, pattern preview)
 * - Talks to the Solver (patterns, shuffles, solving)
 * - Generates command strings for the Raspberry Pi
 */
class ControlPanel {
public:
    /**
     * @brief Construct a ControlPanel bound to an SFML window.
     *
     * @param window  Main render window (shared with SFML + OpenGL).
     * @param font    Font used for all text in the UI.
     * @param sprite  Sprite that holds the pattern spritesheet.
     */
    ControlPanel(sf::RenderWindow &window, sf::Font &font, sf::Sprite &sprite);

    /// @brief Destructor (no manual resource ownership right now).
    ~ControlPanel();

    /**
     * @brief Main per-frame update.
     *
     * - Sets up OpenGL camera/projection
     * - Draws cube and UI
     * - Handles auto/manual rotations
     * - Builds command string for the Pi
     *
     * @return Command string (e.g. "B M 0 -1 90;") to send over serial.
     */
    std::string update();

    /**
     * @brief Handle mouse click interactions with all buttons.
     *
     * Sets internal flags (rotate_90_flag, auto_flag, etc.) which are
     * later processed by update()/handle_rotation_manual()/auto().
     */
    void handle_clicks();

    /**
     * @brief Update timing statistics from Pi response.
     *
     * Expected format: "T 123.45" (milliseconds).
     *
     * @param time Raw string received from the Pi.
     */
    void update_solve_times(std::string time);

    /**
     * @brief Save all recorded solve times and move counts to CSV.
     *
     * File: solve_results.csv
     * Format: "Moves,Solve time (ms)"
     */
    void saveToCSV();

    /**
     * @brief Sync cube state from Pi/camera response and generate solution.
     *
     * Expected format: "S <54-char-facelet>".
     *
     * @param response Raw string received from the Pi.
     */
    void sync(std::string response);

    // ----------------- Public Camera Controls ----------------- //

    /// @brief Horizontal orbit angle around cube (degrees).
    float cameraAngleX = 0.0f;

    /// @brief Vertical tilt angle around cube (degrees).
    float cameraAngleY = 0.0f;

    /// @brief Last recorded mouse position (for click/drag camera control).
    sf::Vector2i lastMousePos;

private:
    // =====================================================================
    //  Internal Types
    // =====================================================================

    /// @brief Faces of the cube in this rendering coordinate system.
    enum Face { Bottom, North, East, South, West, Top };

    /// @brief Simple checkbox UI element (currently unused, but kept for future).
    struct Checkbox {
        sf::RectangleShape rectangle;
        bool checked;
        std::string label;
        sf::Text label_text;
    };

    /// @brief Generic clickable button (rectangle + label).
    struct Button {
        sf::RectangleShape rectangle;
        std::string label;
        sf::Text label_text;
    };

    /**
     * @brief Raw geometry + colors for a single cublet's faces.
     *
     * All coordinates are defined in local space around the origin.
     * The actual position/orientation is controlled by Cublet::transform.
     */
    struct Cublet_faces {
        std::vector<sf::Vector3f> front;
        sf::Vector3f front_color;

        std::vector<sf::Vector3f> back;
        sf::Vector3f back_color;

        std::vector<sf::Vector3f> top;
        sf::Vector3f top_color;

        std::vector<sf::Vector3f> bottom;
        sf::Vector3f bottom_color;

        std::vector<sf::Vector3f> right;
        sf::Vector3f right_color;

        std::vector<sf::Vector3f> left;
        sf::Vector3f left_color;
    };

    /**
     * @brief One physical cube piece (corner/edge/center) in the 3x3x3 cube.
     *
     * coordinates: static local geometry + colors.
     * transform:   current world transform (position & rotation).
     * center:      original center position in world space (used for tests).
     */
    struct Cublet {
        Cublet_faces coordinates;
        glm::mat4 transform;
        glm::vec3 center;
    };

    // =====================================================================
    //  Core References / UI State
    // =====================================================================

    /// @brief Reference to the shared SFML window.
    sf::RenderWindow &window;

    /// @brief Sprite used to display pattern thumbnails from a spritesheet.
    sf::Sprite pattern_sprite;

    /// @brief Reused SFML text object for drawing labels.
    sf::Text text;

    /// @brief Buttons for manual face turns (D, F, L, B, R, Unlock, Reset).
    std::vector<Button> manual_buttons;

    /// @brief Buttons for automatic operations (Sync, Shuffle, Solve, Execute Pattern).
    std::vector<Button> auto_buttons;

    /// @brief Buttons to bump the pattern index (-5, -1, +1, +5).
    std::vector<Button> pattern_selection_buttons;

    /// @brief All 26 visible cube pieces.
    std::vector<Cublet> cublets;

    /// @brief History of solve times (ms).
    std::vector<double> solve_times;

    /// @brief History of move counts for each solve (used with solve_times).
    std::vector<int> moves_storage;

    // =====================================================================
    //  Internal Helpers
    // =====================================================================

    /**
     * @brief Check if mouse cursor is inside a rectangle (for click detection).
     *
     * @param rect Rectangle to test against.
     * @return true if mouse is inside the rectangle.
     */
    bool rect_contains_mouse(sf::RectangleShape &rect);

    /**
     * @brief Draw the entire cube using OpenGL, based on cublets' transforms.
     */
    void draw_cube();

    /**
     * @brief Animate a single face rotation on the cube.
     *
     * @param face      Which cube face to rotate.
     * @param rotation  Accumulated rotation for current move (modified).
     * @param angle     Target angle (typically 90 or 180 degrees).
     * @param direction +1 or -1 (logical direction of rotation).
     *
     * @return true if the rotation is complete, false otherwise.
     */
    bool rotate_face(Face face, float &rotation, float angle, int direction = 1);

    /**
     * @brief Check whether a cublet center belongs to a certain face.
     *
     * Used to decide which cublets rotate when turning a given face.
     *
     * @param center World-space center of cublet.
     * @param face   Face being tested.
     * @return true if the cublet lies on that face.
     */
    bool cublet_on_face(const glm::vec3 &center, Face face);

    /**
     * @brief Build all cublets and place them in a 3x3x3 layout.
     */
    void init_cublets();

    /**
     * @brief Handle manual rotations requested via manual buttons.
     *
     * Consumes flags like D_flag, F_flag, rotate_90_flag, rotate_180_flag.
     * Sends "M ..." commands and animates rotation.
     */
    void handle_rotation_manual();

    /**
     * @brief Handle automatic sequence playback (solver solution or pattern).
     *
     * Steps through @ref solution and animates each move in order.
     */
    void handle_rotation_auto();

    /**
     * @brief Initialize manual control buttons (D, D', D2, F, F', ... , Reset).
     *
     * @param font Font to use for button labels.
     */
    void init_manual_buttons(sf::Font &font);

    /**
     * @brief Initialize automatic control buttons (Sync, Shuffle, Solve, Execute Pattern).
     *
     * @param font Font to use for button labels.
     */
    void init_auto_buttons(sf::Font &font);

    /**
     * @brief Initialize pattern index buttons (-5, -1, +1, +5).
     *
     * @param font Font to use for button labels.
     */
    void init_pattern_selection_buttons(sf::Font &font);

    /**
     * @brief Draw all labels: section titles, pattern name, timers, pattern preview.
     */
    void draw_labels();

    /**
     * @brief Convert a Solver::Pattern enum to a human-readable name.
     */
    const char* pattern_to_string(Solver::Pattern p);

    // =====================================================================
    //  State / Flags / Solver
    // =====================================================================

    /// @brief Accumulated rotation angle for current move (degrees).
    float rotation;

    /// @brief Time to find current solution (ms).
    float solution_find_time;

    /// @brief Last measured solve execution time (ms).
    float solve_time;

    /// @brief Best (lowest) solve execution time recorded (ms).
    float fastest_solve_time;

    /// @brief True when a 90-degree rotation is in progress/requested.
    bool rotate_90_flag;

    /// @brief True when a 180-degree rotation is in progress/requested.
    bool rotate_180_flag;

    /// @brief True if current rotation is clockwise (with some flips per face).
    bool rotate_cw_flag;

    /// @brief True while the cube is executing a solver/pattern sequence.
    bool auto_flag;

    /// @brief True when user pressed "Unlock" (send lock/unlock command).
    bool unlock_flag;

    /// @brief Per-face flags for manual move requests.
    bool D_flag, F_flag, L_flag, B_flag, R_flag;

    /// @brief True when timing mode is on (send "T;" to Pi before moves).
    bool timer_flag;

    /// @brief True after "Sync" button is pressed (send "S;" once).
    bool sync_flag;

    /// @brief Command string being built this frame (sent to Pi afterwards).
    std::string command;

    /// @brief Local solver instance used for solving, patterns, and sync.
    Solver solver;

    /// @brief Current solution/pattern sequence (face index, direction, angle).
    std::vector<std::vector<int>> solution;

    /// @brief Index of current move in @ref solution (for auto playback).
    int current_move_index;

    /// @brief Index of currently selected pattern (Solver::Pattern).
    int current_pattern_index;

    /**
     * @brief Convert @ref solution into a batch of "M ..." commands.
     *
     * Also sends "T;" first if timer_flag is set (to let Pi measure time).
     */
    void batch_move_send();

    /// @brief True when a solution/pattern needs to be sent as a batch to Pi.
    bool batch_move_flag;

    // =====================================================================
    //  Message Mapping
    // =====================================================================

    /**
     * @brief Map from Face enum to message face indices for Pi protocol.
     *
     * Example mapping:
     *  Bottom -> "0 "
     *  North  -> "2 "
     *  East   -> "1 "
     *  South  -> "5 "
     *  West   -> "4 "
     */
    std::unordered_map<Face, std::string> face_to_message = {
        {Face::Bottom, "0 "},
        {Face::North,  "2 "},
        {Face::East,   "1 "},
        {Face::South,  "5 "},
        {Face::West,   "4 "}
    };

    /**
     * @brief Reverse-ish mapping: solver face index -> message face index.
     *
     * Used when walking sequences generated by the solver.
     */
    std::unordered_map<int, std::string> number_to_message = {
        {0, "0 "}, // Bottom
        {1, "2 "}, // North
        {2, "1 "}, // East
        {3, "5 "}, // South
        {4, "4 "}, // West
    };
};