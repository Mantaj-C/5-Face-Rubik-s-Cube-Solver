#include "Solver.h"

/**
 * @brief Constructor: initializes Kociemba two-phase solver and lookup tables.
 *
 * Initializes:
 * - face tables
 * - move definitions
 * - coordinate systems
 * - symmetry tables
 * - pruning tables
 *
 * Also initializes:
 * - mapping from *your* move numbering → Kociemba cubie moves
 * - sets cube to solved
 */
Solver::Solver() : solver_engine(1, 10, 1, -1, 1) {
    // Initialize all tables required for Kociemba solver
    face::init();
    move::init();
    coord::init();
    sym::init();
    prun::init(false);

    /**
     * Mapping: my_move_index → move::cubes[x]
     *
     * Your internal format: 0–14 index
     * Kociemba move indices: cubes[] array
     *
     *  Example:
     *   {0, move::cubes[13]} → F2
     *   {1, move::cubes[12]} → F
     *   {2, move::cubes[14]} → F'
     *
     * Coordinates based on six faces and three turns per face.
     */
    my_move_to_cube_move = {
        {0,  move::cubes[13]}, {1,  move::cubes[12]}, {2,  move::cubes[14]}, // F2, F, F'
        {3,  move::cubes[1]},  {4,  move::cubes[0]},  {5,  move::cubes[2]},  // U2, U, U'
        {6,  move::cubes[10]}, {7,  move::cubes[9]},  {8,  move::cubes[11]}, // L2, L, L'
        {9,  move::cubes[4]},  {10, move::cubes[3]},  {11, move::cubes[5]},  // D2, D, D'
        {12, move::cubes[7]},  {13, move::cubes[6]},  {14, move::cubes[8]}   // R2, R, R'
    };

    // Precompute pruning tables
    solver_engine.prepare();

    // Start from solved cube
    cube = cubie::SOLVED_CUBE;
}

Solver::~Solver() {
    solver_engine.finish();
}

/**
 * @brief Apply a move (face + direction + angle) to the internal cube state.
 *
 * @param face      Face index (0–4)
 * @param direction +1 or -1
 * @param amount    90° or 180°
 *
 * Converts your representation → Kociemba move index → applies cubie multiplication.
 */
void Solver::match_move(int face, int direction, int amount) {
    int map_search = 0;

    // Convert (face, direction, angle) → 0..14 index
    switch (face) {
        case 0:  // Bottom
            if (amount == 180) map_search = 0;
            else if (direction == -1) map_search = 1;
            else map_search = 2;
            break;
        case 1:  // North
            if (amount == 180) map_search = 3;
            else if (direction == -1) map_search = 4;
            else map_search = 5;
            break;
        case 2:  // East
            if (amount == 180) map_search = 6;
            else if (direction == -1) map_search = 7;
            else map_search = 8;
            break;
        case 3:  // South
            if (amount == 180) map_search = 9;
            else if (direction == -1) map_search = 10;
            else map_search = 11;
            break;
        case 4:  // West
            if (amount == 180) map_search = 12;
            else if (direction == -1) map_search = 13;
            else map_search = 14;
            break;
    }

    // Apply move on cube: result = cube × move
    cubie::cube result;
    cubie::mul(cube, my_move_to_cube_move[map_search], result);
    cube = result;
}

/**
 * @brief Solve the current internal cube state.
 *
 * @return Vector of moves in your internal format.
 */
std::vector<std::vector<int>> Solver::solve() {
    std::vector<std::vector<int>> sols;
    solver_engine.solve(cube, sols);

    std::vector<std::vector<int>> result;

    if (!sols.empty()) {
        std::vector<int> solution = sols[0];  // first solution (optimal)

        std::cout << "First solution (" << solution.size() << " moves): ";
        for (int move_id : solution)
            std::cout << move::names[move_id] << " ";
        std::cout << std::endl;

        // Convert each Kociemba move → your move {face, dir, angle}
        for (auto &s : solution)
            result.push_back(cube_move_to_my_move[s]);
    }
    else {
        std::cout << "No solution found.\n";
    }

    return result;
}

/**
 * @brief Solve a cube that is passed in (used for shuffle or sync).
 */
std::vector<std::vector<int>> Solver::solve(cubie::cube &cube_to_solve) {
    std::vector<std::vector<int>> sols;
    solver_engine.solve(cube_to_solve, sols);

    std::vector<std::vector<int>> result;

    if (!sols.empty()) {
        std::vector<int> solution = sols[0];

        std::cout << "First solution (" << solution.size() << " moves): ";
        for (int move_id : solution)
            std::cout << move::names[move_id] << " ";
        std::cout << std::endl;

        for (auto &s : solution)
            result.push_back(cube_move_to_my_move[s]);
    }
    else {
        std::cout << "No solution found.\n";
    }

    return result;
}

/**
 * @brief Generate a shuffle sequence for the robot.
 *
 * Logic:
 *  1. Compute solution for current scrambled cube.
 *  2. Shuffle a solved cube using random generator.
 *  3. Solve the shuffled cube.
 *  4. Reverse that solution → becomes scramble moves.
 *  5. Append scramble moves after current solve.
 *
 * @return Combined move list.
 */
std::vector<std::vector<int>> Solver::shuffle() {
    // Moves needed to solve current cube
    std::vector<std::vector<int>> solved_solution = solve();

    cubie::cube temp_cube = cubie::SOLVED_CUBE;
    cubie::shuffle(temp_cube);       // random 25-move scramble

    // Solve random scramble and reverse it → actual scramble steps
    std::vector<std::vector<int>> shuffled_steps = solve(temp_cube);
    std::reverse(shuffled_steps.begin(), shuffled_steps.end());

    // Combine
    std::vector<std::vector<int>> full_sequence = solved_solution;
    full_sequence.insert(full_sequence.end(), shuffled_steps.begin(), shuffled_steps.end());

    return full_sequence;
}

/**
 * @brief Execute one of your predefined cube patterns.
 *
 * Steps:
 *  1. Get solution to solve cube first.
 *  2. Convert pattern algorithm (strings) → move vectors.
 *  3. Append them after solve steps.
 */
std::vector<std::vector<int>> Solver::execute_pattern(Pattern pattern) {
    std::vector<std::vector<int>> full_sequence = solve();

    std::vector<std::string> pat = pattern_lookup[pattern].moves;

    for (auto &move : pat)
        full_sequence.push_back(letter_to_move.at(move));

    return full_sequence;
}

/**
 * @brief Print cube as 6 groups of 9 facelets (helpful for debugging).
 */
void Solver::print_cubes_facelet() {
    std::string temp = face::from_cubie(cube);

    std::cout << "Valid facelet (grouped):\n";
    for (size_t i = 0; i < temp.size(); i += 9)
        std::cout << temp.substr(i, 9) << std::endl;
}

/**
 * @brief Sync internal cube state to a facelet string from the Pi.
 *
 * Steps:
 *  1. Take current solution steps.
 *  2. Convert Pi’s facelet → cube representation.
 *  3. Solve difference.
 *  4. Reverse solves & invert direction for robot.
 *  5. Append after normal solution.
 */
std::vector<std::vector<int>> Solver::sync(std::string facelet) {
    std::vector<std::vector<int>> full_sequence = solve();

    cubie::cube temp_cube = cubie::SOLVED_CUBE;

    // Convert facelet → cube. Returns 0 on success.
    int error = face::to_cubie(facelet, temp_cube);

    if (error == 0) {
        // Solve mismatch
        std::vector<std::vector<int>> match_steps = solve(temp_cube);
        std::reverse(match_steps.begin(), match_steps.end());

        // Invert direction (robot runs backwards)
        for (auto &inner : match_steps) {
            if (inner.size() > 1)
                inner[1] *= -1;
        }

        full_sequence.insert(full_sequence.end(), match_steps.begin(), match_steps.end());
        std::cout << "Synced to " << facelet << std::endl;

        return full_sequence;
    }

    // Invalid facelet
    std::cout << "Invalid facelet error: " << error << std::endl;
    std::cout << "Invalid facelet (grouped):\n";
    for (size_t i = 0; i < facelet.size(); i += 9)
        std::cout << facelet.substr(i, 9) << std::endl;

    return {};
}

/**
 * @brief Reset the internal cube to solved state.
 */
void Solver::reset() {
    cube = cubie::SOLVED_CUBE;
}