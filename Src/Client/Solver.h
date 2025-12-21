#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>

#include "face.h"
#include "cubie.h"
#include "move.h"
#include "coord.h"
#include "sym.h"
#include "prun.h"
#include "solve.h"

/**
 * @brief High–level wrapper around the Kociemba two-phase solver.
 *
 * Responsibilities:
 *  - Owns the internal cubie::cube state.
 *  - Calls the two-phase solver to get optimal solutions.
 *  - Converts between Kociemba move IDs and your own {face, dir, angle} format.
 *  - Provides higher-level operations: shuffle, patterns, sync, reset.
 */
class Solver {
public:
    /**
     * @brief Predefined cube patterns supported by this solver.
     *
     * Used by execute_pattern() and pattern_lookup.
     */
    enum class Pattern {
        Superflip = 0,
        EasyCheckerboard,
        SpeedsolvingLogo,
        ThreeSidesSolved,
        AllFlagsAndCrests,
        Wire,
        CheckerboardInTheCube,
        PerfectScramble,
        Emoticon,           // ( ! )  ^( ͡° ͜ʖ ͡°)
        MinorityCross,
        PerpendicularLines,
        FlippedTips,
        PlusMinus,
        Tablecloth,
        Deckerboard,
        SpiralPattern,
        FruitBowl,
        Flower,
        VerticalStripes,
        GiftBox,
        OppositeCorners,
        Cross,
        FourCrosses,
        UnionJack,
        CubeInTheCube,
        CubeInACubeInACube,
        Anaconda,
        Python,
        BlackMamba,
        GreenMamba,
        Tangled,
        FourSpots,
        SixSpots,
        Twister,
        Kilt,
        Tetris,
        DontCrossLine,
        HiAllAround,
        DisplacedMotif,
        AreYouHigh,
        CUAround,
        OrderInChaos,
        EvenlyDistributed,
        TheHole,
        NoEntry,
        Plus,
        ThreeCThreeW,
        Pong
    };

    // ---------------------------------------------------------------------
    //  Lifecycle
    // ---------------------------------------------------------------------

    /// @brief Initialize two-phase engine and lookup maps.
    Solver();

    /// @brief Cleanup resources used by the two-phase engine.
    ~Solver();

    // ---------------------------------------------------------------------
    //  Core Operations
    // ---------------------------------------------------------------------

    /**
     * @brief Apply one move (face, direction, amount) to the internal cube.
     *
     * @param face      Face index in your convention (0..4).
     * @param direction +1 or -1.
     * @param amount    90 or 180 degrees.
     */
    void match_move(int face, int direction, int amount);

    /**
     * @brief Solve the current internal cube state.
     *
     * @return Sequence of moves in your internal format:
     *         { faceIndex, direction, angle }.
     */
    std::vector<std::vector<int>> solve();

    /**
     * @brief Solve a provided cube instead of the internal one.
     *
     * @param cube_to_solve External cubie::cube to be solved.
     * @return Sequence of moves in your internal format.
     */
    std::vector<std::vector<int>> solve(cubie::cube &cube_to_solve);

    /**
     * @brief Generate a shuffle sequence.
     *
     * Returns a sequence that:
     *  - First solves the current cube.
     *  - Then applies a random scramble sequence.
     */
    std::vector<std::vector<int>> shuffle();

    /**
     * @brief Execute a named pattern starting from the current cube.
     *
     * @param pattern Chosen pattern enum (default = EasyCheckerboard).
     * @return Combined sequence: solve-to-pattern moves, in your format.
     */
    std::vector<std::vector<int>> execute_pattern(
        Pattern pattern = Pattern::EasyCheckerboard
    );

    /**
     * @brief Print current cube facelets in 6 lines of 9 (for debugging).
     */
    void print_cubes_facelet();

    /**
     * @brief Sync internal cube state to a given facelet string
     *        (e.g., from camera/Pi).
     *
     * @param facelet 54-char or 48-char facelet representation.
     * @return Move sequence to bring the physical cube to that state,
     *         in your internal format.
     */
    std::vector<std::vector<int>> sync(std::string facelet);

    /**
     * @brief Reset internal cube back to the solved state.
     */
    void reset();

private:
    // ---------------------------------------------------------------------
    //  Underlying Solver / Cube State
    // ---------------------------------------------------------------------

    /// @brief Kociemba two-phase solving engine.
    solve::Engine solver_engine;

    /// @brief Current cube state in cubie representation.
    cubie::cube cube;

    // ---------------------------------------------------------------------
    //  Move Mapping
    // ---------------------------------------------------------------------

    /**
     * @brief Map from your move index (0..14) → cubie::cube move.
     *
     * Populated in Solver::Solver(), used by match_move().
     */
    std::unordered_map<int, cubie::cube> my_move_to_cube_move;

    /**
     * @brief Map from Kociemba move ID → your move representation.
     *
     * Format: { faceIndex, direction, angle }.
     *
     * faceIndex: 0..4 in your coordinate system
     * direction: ±1
     * angle    : 90 or 180
     */
    std::unordered_map<int, std::vector<int>> cube_move_to_my_move = {
        {13, {0, -1, 180}}, {12, {0, -1, 90}}, {14, {0,  1,  90}}, // F2, F, F'
        {1,  {1, -1, 180}}, {0,  {1, -1, 90}}, {2,  {1,  1,  90}}, // U2, U, U'
        {10, {2, -1, 180}}, {9,  {2, -1, 90}}, {11, {2,  1,  90}}, // L2, L, L'
        {4,  {3, -1, 180}}, {3,  {3, -1, 90}}, {5,  {3,  1,  90}}, // D2, D, D'
        {7,  {4, -1, 180}}, {6,  {4, -1, 90}}, {8,  {4,  1,  90}}  // R2, R, R'
    };

    // ---------------------------------------------------------------------
    //  Pattern Definitions
    // ---------------------------------------------------------------------

    /**
     * @brief Pattern data: algorithm and final facelet string.
     *
     * moves   : high-level moves in letter form (e.g., "R2", "F'", …).
     * facelet : final facelet configuration (for validation or display).
     */
    struct PatternData {
        std::vector<std::string> moves;
        std::string facelet;
    };

    /**
     * @brief Lookup table for all supported patterns.
     *
     * Key   : Pattern enum.
     * Value : PatternData containing algorithm and final facelets.
     */
    std::unordered_map<Pattern, PatternData> pattern_lookup = {
        {Pattern::Superflip, {{"F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "R2", "F", "B", "R", "B2", "R", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L'",
            "B2", "R", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "D'",
            "R2", "F", "R'", "L", "B2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "F2"},
            "UBULURUFURURFRBRDRFUFLFRFDFDFDLDRDBDLULBLFLDLBUBRBLBDB"}},

        {Pattern::EasyCheckerboard, {{"L2", "R2", "D2", "R", "L", "F2", "B2",
            "R'", "L'", "D2", "R", "L", "F2", "B2", "R'", "L'", "F2", "B2"},
            "UDUDUDUDURLRLRLRLRFBFBFBFBFDUDUDUDUDLRLRLRLRLBFBFBFBFB"}},

        {Pattern::SpeedsolvingLogo, {{"R'", "L'", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2",
            "F2", "D2", "F2", "R", "L", "B2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2",
            "B2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2"},
            "UUDDUDDUULLRRRRRLLBFBFFFBFBDDUUDUUDDRRLLLLLRRFBFBBBFBF"}},

        {Pattern::ThreeSidesSolved, {{"L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "D", "R'",
            "D", "B'", "L2", "B", "D'", "R", "L2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2",
            "B2", "L2", "B2", "D", "R2", "L2", "D", "R2"},
            "RULRULURURUULRLRLRFFFFFFFFFDDDDDDDDDUULRLULRLBBBBBBBBB"}},

        {Pattern::AllFlagsAndCrests, {{"B2", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2",
            "B'", "F'", "B2", "L2", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'",
            "F'", "B2", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "R2",
            "B2", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "L'", "R'",
            "B", "D", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "L2",
            "F2", "R'"},
            "RRRBUBLLLFUBFRBFUBDDDRFRUUULLLFDFRRRFDBFLBFDBUUULBLDDD"}},

        {Pattern::Wire, {{"R", "L", "F", "B", "R", "L", "F", "B", "R", "L", "F", "B", "R2", "B2", "L2",
            "R2", "B2", "L2"},
            "DDUUUUUDDLRRLRLRRLFFFFFFFFFUUDDDDDUURLLRLRLLRBBBBBBBBB"}},

        {Pattern::CheckerboardInTheCube, {{"B", "D", "F'", "B'", "D", "L2", "F", "B", "R2", "L2", "B'",
            "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "L", "F", "B", "R2", "L2", "B'", "F'", "D'",
            "F", "B", "L2", "R2", "B'", "F'", "B", "D'", "R", "B", "R", "D'", "R", "L'", "F", "L2", "F2",
            "B2", "R2", "D2", "R2", "F2", "B2", "L2", "D"},
            "RBURUBRRRBURBRUBBBDDDLFDFLDDFLFDLLLLFFFFLDFDLBRURBUUUU"}},

        {Pattern::PerfectScramble, {{"F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F2",
            "L'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "R2", "F'", "R2", "B'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'",
            "F'", "R", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "D'", "L2", "F2",
            "L2", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'"},
            "LFBBUDFLRBFLURBFDURBDLFLUDRBLUFDRBULFRDBLFDURURDDBUFRL"}},

        {Pattern::Emoticon, {{"F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "F2", "R'", "L'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "B", "R2", "D", "B", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "R2", "L", "B2", "D'", "B2", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'",
            "F'", "F2", "B2", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "L2", "B2", "L2"},
            "RRRDUULLLBBBLRRFFFDBUDFUDFULLLDDURRRBBBRLLFFFUBDUBDUFD"}},

        {Pattern::MinorityCross, {{"L2", "D'", "B2", "L2", "B2", "D'", "F2", "D", "F", "B", "R2", "L2",
            "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "B2", "L", "B2", "F'", "L", "B'", "F",
            "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F2",
            "F", "B", "R2", "L2", "B'", "F'", "D",  "F", "B", "L2", "R2", "B'", "F'", "L2"},
            "FDFLURFBFLFLURDLBLUDURFBULUBFBLDRBUBRBRFLDRURDLDFBUDRD"}},

        {Pattern::PerpendicularLines, {{"R2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "L2",
            "R2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "L2", "L'", "R", "L'", "R"},
            "DUDDUDDUDLRLLRLLRLFFFFFFFFFUDUUDUUDURLRRLRRLRBBBBBBBBB"}},

        {Pattern::FlippedTips, {{"R", "B", "L'", "F2", "L", "B'", "R'", "D2", "L", "F2", "L'", "D2", "L",
            "F2", "L'", "D2"},
            "UUUUUUUURFRRRRRRRRFFUFFFFFFDDDDDDLDDLLLLLLBLLBBBBBBBBD"}},

        {Pattern::PlusMinus, {{"F", "B", "R2", "L2", "B'", "F'", "D2", "F", "B", "L2", "R2", "B'", "F'",
            "R2", "L2", "F", "B", "R2", "L2", "B'", "F'", "D2", "F", "B", "L2", "R2", "B'", "F'", "R2", "L2"},
            "DUDUUUDUDLRLLRLLRLFFFFFFFFFUDUDDDUDURLRRLRRLRBBBBBBBBB"}},

        {Pattern::Tablecloth, {{"R", "L", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "F'",
            "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "D2", "R2", "L2", "F'", "D2", "F2", "D",
            "R2", "L2", "F2", "B2", "D", "B2", "L2"},
            "BRBDUDBRBRBRBRBRBRUDULFLUDUFLFUDUFLFLFLFLFLFLDUDRBRDUD"}},

        {Pattern::Deckerboard, {{"F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "R",
            "L2", "B", "D'", "R", "D2", "L", "D'", "B", "R2", "L", "F2", "B", "R2",
            "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'"},
            "BLBLULBLBLBLBRBLBLDUDUFUDUDFRFRDRFRFRFRFLFRFRUDUDBDUDU"}},

        {Pattern::SpiralPattern, {{"L'", "B'", "D", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2",
            "R2", "B'", "F'", "R", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'",
            "F'", "R'", "D2", "R2", "D", "L", "D'", "L'", "R'", "F2", "B", "R2", "L2", "B'", "F'",
            "D", "F", "B", "L2", "R2", "B'", "F'"},
            "RRURUURRRBBRBRRBBBDDDDFDFFDDLLDDLLLLFFFFLLFFLBBUUBUUUU"}},

        {Pattern::FruitBowl, {{"B2", "L2", "F2", "R2", "F2", "R2", "D", "F", "B", "R2", "L2", "B'", "F'",
            "D", "F", "B", "L2", "R2", "B'", "F'", "B2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2",
            "L2", "B'", "F'", "L'", "R'", "D'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2",
            "R2", "B'", "F'"},
            "BFBBUBBBBUUUURDUUULLLLFLLLLFFFFDFFBFDDDULDDDDRRRRBRRRR"}},

        {Pattern::Flower, {{"R2", "D2", "R2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2",
            "R2", "F2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "D2", "F2", "L2", "F2", "B2",
            "R2", "D2", "R2", "F2", "B2", "L2"},
            "UDUUUDUUURRRRRRRRRFBFBFBFBFDDDDDUDUDLLLLLLLLLBFBFBFBFB"}},

        {Pattern::VerticalStripes, {{"F2", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'",
            "R", "L2", "B", "D'", "R", "D2", "L", "D'", "B", "R2", "L", "F2", "B", "R2",
            "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'"},
            "LURLURLURDDDRRRUUUFFFFFFFFFLDRLDRLDRDDDLLLUUUBBBBBBBBB"}},

        {Pattern::GiftBox, {{"F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "B2", "R2", "B2", "L2", "F2", "R2", "D'", "F2", "L2", "B", "F'", "L", "F2", "D", "F", "B",
            "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "R2", "F'", "L'", "R'"},
            "RULRULRULDDDRRRUUUBFBFFFBFBRDLRDLRDLDDDLLLUUUFBFBBBFBF"}},

        {Pattern::OppositeCorners, {{"R", "L", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2",
            "F2", "D2", "F2", "R", "L", "F2", "D2", "B2", "D2"},
            "DUUUUUUUDRRLRRRLRRBBBBFBBBBUDDDDDDDULLRLLLRLLFFFFBFFFF"}},

        {Pattern::Cross, {{"R2", "L'", "D", "F2", "R'", "D'", "R'", "L", "F", "B", "R2", "L2", "B'", "F'",
            "D'", "F", "B", "L2", "R2", "B'", "F'", "D", "R", "D", "B2", "R'", "F", "B", "R2", "L2", "B'",
            "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "D2"},
            "BUBUUUBUBURURRRURULFLFFFLFLFDFDDDFDFDLDLLLDLDRBRBBBRBR"}},

        {Pattern::FourCrosses, {{"L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2",
            "R2", "L2", "F2", "B2", "D2", "L2", "R2", "F2", "B2"},
            "DUDUUUDUDLRLRRRLRLFFFFFFFFFUDUDDDUDURLRLLLRLRBBBBBBBBB"}},

        {Pattern::UnionJack, {{"L", "F", "B'", "D2", "L2", "D2", "F'", "B", "L2", "D2", "L"},
            "BUBUUUBUBLRLRRRLRLDFDFFFDFDFDFDDDFDFRLRLLLRLRUBUBBBUBU"}},

        {Pattern::CubeInTheCube, {{"F", "L", "F2", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2",
            "R2", "B'", "F'", "R", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "F2", "L2", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "L'",
            "B", "D'", "B'", "L2", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'"},
            "RUURUURRRBRRBRRBBBDDDFFDFFDDDLDDLLLLFFFFLLFLLBBUBBUUUU"}},

        {Pattern::CubeInACubeInACube, {{"F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'",
            "F'", "L'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F2", "R2",
            "B'", "R", "F2", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "B2",
            "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "B'", "L", "F", "B",
            "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "B", "R2", "L2", "B'",
            "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "R", "F'"},
            "BURBUUBBBURBURRUUULLLFFLDFLLDFDDFFFFDDDDLLDLFUBRBBRRRR"}},

        {Pattern::Anaconda, {{"L", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "B'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "R", "L'",
            "B", "R'", "F", "B'", "D", "R", "D'", "F'"},
            "RRRUURRURBBBRRBBRBDFDDFFDDDLLLLDDLDLFLFLLFFFFUUUUBBUBU"}},

        {Pattern::Python, {{"F2", "R'", "B'", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2",
            "B'", "F'", "R'", "L", "F'", "L", "F'", "B", "D'", "R", "B", "L2"},
            "RURUURRRRUUURRUURUBBBFFFBBBLLLLDDLDLDLDDLLDDDFBFFBFFBF"}},

        {Pattern::BlackMamba, {{"R", "D", "L", "F'", "R", "L'", "D", "R'", "F", "B", "R2", "L2", "B'", "F'",
            "D", "F", "B", "L2", "R2", "B'", "F'", "D'", "B", "F", "B", "R2", "L2", "B'", "F'", "D'", "F",
            "B", "L2", "R2", "B'", "F'", "R'", "D'"},
            "FFFUUUFFFDRDDRDDRDLLLFFLLFLBDBBDBBDBULULLUUUURRRRBBRBR"}},

        {Pattern::GreenMamba, {{"R", "D", "R", "F", "R'", "F'", "B", "D", "R'", "F", "B", "R2", "L2",
            "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "B'", "F", "B", "R2", "L2", "B'", "F'",
            "D", "F", "B", "L2", "R2", "B'", "F'", "D2"},
            "FFFUUUFFFDRDDRDDRDLLLFFLLFLBDBBDDBBBULULLUUUURRRBBBRRR"}},

        {Pattern::Tangled, {{"F", "B", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'",
            "F'", "D", "F", "B", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "D", "F", "B", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "D", "F2", "B2"},
            "UDDDUDDDULLLRRRLLLFFBBFBBFFDUUUDUUUDRRRLLLRRRBBFFBFFBB"}},

        {Pattern::FourSpots, {{"F2", "B2", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2",
            "B'", "F'", "D'", "R2", "L2", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2",
            "B'", "F'", "D'"},
            "DDDDUDDDDLLLLRLLLLFFFFFFFFFUUUUDUUUURRRRLRRRRBBBBBBBBB"}},

        {Pattern::SixSpots, {{"F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "D'", "R", "L'", "F", "B'", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2",
            "B'", "F'", "D'"},
            "RRRRURRRRBBBBRBBBBDDDDFDDDDLLLLDLLLLFFFFLFFFFUUUUBUUUU"}},

        {Pattern::Twister, {{"F", "R'", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2",
            "B'", "F'","L", "F'", "L'", "F2", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2",
            "R2", "B'", "F'","R", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "L'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "L", "F'"},
            "BUUUUUBBBURRRRRUUULLLFFFFFLDDFDDFFDFDLDDLLDLLBBRBBRRBR"}},

        {Pattern::Kilt, {{"F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "R2",
            "L2", "F2", "B2", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "R", "L", "F", "B'", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'",
            "F2", "D2", "R2", "L2", "F2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "F2",
            "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F"},
            "BRBRURBRBUBUBRBUBULDLDFDLDLFLFLDLFLFDFDFLFDFDRURUBURUR"}},

        {Pattern::Tetris, {{"L", "R", "F", "B", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2",
            "R2", "B'", "F'", "D'", "L'", "R'"},
            "RLLRULRRLBBBBRFFFFDDUDFUDUURLLRDLRRLFFFFLBBBBDDUDBUDUU"}},

        {Pattern::DontCrossLine, {{"F2", "L2", "R2", "B2", "D2",
            "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2"},
            "UDUDUDUDULRLLRLLRLBBBBFBBBBDUDUDUDUDRLRRLRRLRFFFFBFFFF"}},

        {Pattern::HiAllAround, {{"L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "R2", "F2", "L2",
            "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "D2", "F2", "L2", "L2", "F2", "B2", "R2",
            "D2", "R2", "F2", "B2", "L2"},
            "UUUDUDUUURRRLRLRRRFBFFFFFBFDDDUDUDDDLLLRLRLLLBFBBBBBFB"}},

        {Pattern::DisplacedMotif, {{"L2", "B2", "D'", "B2", "D", "L2", "F", "B", "R2", "L2", "B'", "F'",
            "D", "F", "B", "L2", "R2", "B'", "F'", "R2", "D", "R2", "B", "F", "B", "R2", "L2", "B'", "F'",
            "D", "F", "B", "L2", "R2", "B'", "F'", "R'", "F2", "R", "F", "B", "R2", "L2", "B'", "F'", "D'",
            "F", "B", "L2", "R2", "B'", "F'", "B'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2",
            "R2", "B'", "F'"},
            "RUURUURUURRRRRRUUUBFFBFFBBBLLLDDLLLLDDDDLDDLDBBFBBFFFF"}},

        {Pattern::AreYouHigh, {{"L", "R'", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "D2",
            "L'", "R", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "D2", "R2", "L2"},
            "UUUDUDUUURLRLRLRLRFFFBFBFFFDDDUDUDDDLRLRLRLRLBBBFBFBBB"}},

        {Pattern::CUAround, {{"F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "B2", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "L2", "D", "L2",
            "R2", "D'", "B'", "R", "D'", "L", "R'", "B2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2",
            "L2", "F'", "L'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'"},
            "BBBUUBBBBDDDRRDDDDRRRRFFRRRFDFFDFFFFULUULUUUULLLLBBLLL"}},

        {Pattern::OrderInChaos, {{"B", "L2", "B'", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2",
            "B", "F'", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "L",
            "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "B", "F", "B", "R2",
            "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'", "R", "F", "B", "R2", "L2", "B'", "F'",
            "D'", "F", "B", "L2", "R2", "B'", "F'", "B", "F2", "B", "R2", "L2", "B'", "F'", "D'", "F",
            "B", "L2", "R2", "B'", "F'", "R", "D", "R", "B'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F",
            "B", "L2", "R2", "B'", "F'"},
            "FRFUURFRURFUFRRUFURUFUFURFRBDBLDLDLBDLDBLBLBDLDLBBDLDB"}},

        {Pattern::EvenlyDistributed, {{"D'", "B2", "D'", "L2", "R2", "D", "B2", "L2", "D'", "B2", "L", "R'",
            "F'", "L2", "D", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "F2", "R'", "D'", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'"},
            "BRBDUDFLFUBULRLDFDLBRUFDLBRFRFUDUBLBUFURLRDBDRFLUBDRFL"}},

        {Pattern::TheHole, {{"R", "L'", "F'", "B", "D", "F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B",
            "L2", "R2", "B'", "F'"},
            "ULUUUUULUFFFFRFFFFLDLLFLLDLDRDDDDDRDBBBBLBBBBRURRBRRUR"}},

        {Pattern::NoEntry, {{"F", "B", "R2", "L2", "B'", "F'", "D'", "F", "B", "L2", "R2", "B'", "F'",
            "L'", "B", "R", "L'", "D'", "L", "B'", "F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2",
            "R2", "B'", "F'", "F2", "R2", "B2", "D2", "B2", "R2", "B2", "D", "B2"},
            "FFFFUFFFFLLLLRLLRLUUUUFUUUUBBBBDDBBBRRRLLRRRRDDDDBBDDD"}},

        {Pattern::Plus, {{"B2", "F2", "R2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "B2",
            "F2", "R2", "L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "R2", "L2", "F2", "B2",
            "R2", "D2", "R2", "F2", "B2", "L2"},
            "DUDDUUDDDLRLRRRLRLFFFFFFFFFUUUUDDUDURLRLLLRLRBBBBBBBBB"}},

        {Pattern::ThreeCThreeW, {{"D", "L", "B'", "L2", "F", "L'", "B'", "F", "B", "R2", "L2", "B'", "F'",
            "D", "F", "B", "L2", "R2", "B'", "F'", "D'", "R", "L'", "F", "B", "D", "L'"},
            "RRRRURURRBBBBRBRBBFDDFFDDFFLDDDDLDLLFFLFLLLLFUUUUBUUUB"}},

        {Pattern::Pong, {{"F", "B", "R2", "L2", "B'", "F'", "D", "F", "B", "L2", "R2", "B'", "F'", "R2",
            "F2", "B2", "L2","L2", "F2", "B2", "R2", "D2", "R2", "F2", "B2", "L2", "D",
            "R2", "F2", "B2", "L2"},
            "DUDUUUDDDLRLLRRLRLFFFFFFFFFUUUDDDUDURLRLLRRLRBBBBBBBBB"}},
    };

    /**
     * @brief Map from human-readable move string → your move format.
     *
     * Keys are moves like "D", "D'", "D2", "F", etc.
     * Values are { faceIndex, direction, angle } in your convention.
     */
    std::unordered_map<std::string, std::vector<int>> letter_to_move = {
        {"D",  {0, -1,  90}},
        {"D'", {0,  1,  90}},
        {"D2", {0, -1, 180}},

        {"F",  {1, -1,  90}},
        {"F'", {1,  1,  90}},
        {"F2", {1, -1, 180}},

        {"L",  {2, -1,  90}},
        {"L'", {2,  1,  90}},
        {"L2", {2, -1, 180}},

        {"B",  {3, -1,  90}},
        {"B'", {3,  1,  90}},
        {"B2", {3, -1, 180}},

        {"R",  {4, -1,  90}},
        {"R'", {4,  1,  90}},
        {"R2", {4, -1, 180}},
    };
};