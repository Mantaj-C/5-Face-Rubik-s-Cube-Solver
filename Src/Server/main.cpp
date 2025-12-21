#include "GPIO.h"
#include "Stepper.h"
#include "Camera.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <chrono>

// ---------------------- Pin Definitions ----------------------
// Ring gear stepper (locks/unlocks the cube clamp)
#define RING_Gear_Stepper_Enable_Pin     24
#define RING_Gear_Stepper_Direction_Pin  19
#define RING_Gear_Stepper_Step_Pin       26
#define RING_Gear_Stepper_MS1_Pin        25
#define RING_Gear_Stepper_MS2_Pin        8
#define RING_Gear_Stepper_MS3_Pin        7

// Shared microstepping pins for all turning steppers
#define Turning_Steppers_Enable_Pin      23
#define Turning_Steppers_MS1_Pin         16
#define Turning_Steppers_MS2_Pin         20
#define Turning_Steppers_MS3_Pin         21

// Individual face steppers (Bottom, North, East, South, West)
#define Bottom_Stepper_Direction_Pin     27
#define Bottom_Stepper_Step_Pin          22

#define North_Stepper_Direction_Pin      4
#define North_Stepper_Step_Pin           17

#define East_Stepper_Direction_Pin       9
#define East_Stepper_Step_Pin            10

#define South_Stepper_Direction_Pin      6
#define South_Stepper_Step_Pin           13

#define West_Stepper_Direction_Pin       11
#define West_Stepper_Step_Pin            5

// ------------------- Server / Motion Constants ----------------
#define DEFAULT_FREQUENCY 1350          // Default step pulse frequency (Hz)
#define PORT 5050                       // TCP port
#define BUFFER_SIZE 1024
#define RING_STEPPER_LOCK_DEGREES 800   // Steps for locking/unlocking clamp

// #define CAMERA_STREAM                 // Enable live camera preview
// #define NO_CLIENT_TESTING             // Enable local stdin command mode

int main() {
    // ---- Camera object ----
    Camera cam;

#ifdef CAMERA_STREAM
    // Optional: start asynchronous camera preview for calibration/debug
    std::thread t(std::bind(&Camera::getandshow, &cam));
    t.detach();
#endif

    int default_freq = DEFAULT_FREQUENCY;

    // Timer / sync state flags
    float solve_time = 0;
    bool timer_flag = false;
    bool sync_flag = false;
    bool valid_cube = true;

    auto start = std::chrono::high_resolution_clock::now();

    // Shared microstepping mode for all face steppers
    Stepper::StepMode sharedMode = Stepper::FULL;

    // ---------------------- Stepper Initialization ----------------------
    // Ring gear stepper (lock/unlock)
    Stepper Gear_Stepper(
        RING_Gear_Stepper_Enable_Pin,
        RING_Gear_Stepper_Direction_Pin,
        RING_Gear_Stepper_Step_Pin,
        RING_Gear_Stepper_MS1_Pin,
        RING_Gear_Stepper_MS2_Pin,
        RING_Gear_Stepper_MS3_Pin
    );

    // Face steppers (all share same enable + MS1/MS2/MS3)
    Stepper Bottom_Stepper(
        Turning_Steppers_Enable_Pin,
        Bottom_Stepper_Direction_Pin,
        Bottom_Stepper_Step_Pin,
        Turning_Steppers_MS1_Pin,
        Turning_Steppers_MS2_Pin,
        Turning_Steppers_MS3_Pin,
        &sharedMode
    );

    Stepper North_Stepper(
        Turning_Steppers_Enable_Pin,
        North_Stepper_Direction_Pin,
        North_Stepper_Step_Pin,
        Turning_Steppers_MS1_Pin,
        Turning_Steppers_MS2_Pin,
        Turning_Steppers_MS3_Pin,
        &sharedMode
    );

    Stepper East_Stepper(
        Turning_Steppers_Enable_Pin,
        East_Stepper_Direction_Pin,
        East_Stepper_Step_Pin,
        Turning_Steppers_MS1_Pin,
        Turning_Steppers_MS2_Pin,
        Turning_Steppers_MS3_Pin,
        &sharedMode
    );

    Stepper South_Stepper(
        Turning_Steppers_Enable_Pin,
        South_Stepper_Direction_Pin,
        South_Stepper_Step_Pin,
        Turning_Steppers_MS1_Pin,
        Turning_Steppers_MS2_Pin,
        Turning_Steppers_MS3_Pin,
        &sharedMode
    );

    Stepper West_Stepper(
        Turning_Steppers_Enable_Pin,
        West_Stepper_Direction_Pin,
        West_Stepper_Step_Pin,
        Turning_Steppers_MS1_Pin,
        Turning_Steppers_MS2_Pin,
        Turning_Steppers_MS3_Pin,
        &sharedMode
    );

    // Index mapping for "face" integer in commands (0..5)
    // 0=Bottom, 1=East, 2=North, 3=Gear, 4=West, 5=South
    std::vector<Stepper> steppers = {
        Bottom_Stepper,
        East_Stepper,
        North_Stepper,
        Gear_Stepper,
        West_Stepper,
        South_Stepper
    };

    // ---------------------- TCP Server Setup ----------------------
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;   // Bind on all interfaces
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        return 1;
    }

    std::cout << "Raspberry Pi motor server listening on port " << PORT << "...\n";

    // ---------------------- Main Accept Loop ----------------------
    while (true) {
#ifndef NO_CLIENT_TESTING
        // Wait for a client connection (Mac UI)
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }
#endif

#ifdef NO_CLIENT_TESTING
        // Testing mode: no socket, just stdin
        int client_fd = 0;
#endif

        // -------------- Per-Client Command Loop --------------
        while (true) {

#ifdef NO_CLIENT_TESTING
            // Testing mode: read command from terminal
            std::string command;
            std::cout << "Enter command: ";
            std::getline(std::cin, command);
#endif

#ifndef NO_CLIENT_TESTING
            // Normal mode: read command from TCP socket
            char buffer[BUFFER_SIZE] = {0};
            int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
            if (bytes_read <= 0) break;   // client disconnected
            std::string command(buffer, bytes_read);
#endif

            std::ostringstream reply;
            std::cout << "Received command: " << command << std::endl;

            // ---------------- BATCH COMMAND MODE ("B ...") ----------------
            if (command[0] == 'B') {
                // After 'B', we expect semicolon-separated mini-commands
                std::string sub = command.substr(1);
                std::istringstream batch(sub);
                std::string token;

                while (std::getline(batch, token, ';')) {
                    std::istringstream cmd(token);
                    std::string keyword;
                    cmd >> keyword;

                    // ---- M = Move: "M face direction amount" ----
                    if (keyword == "M") {
                        int face, direction, amount;
                        cmd >> face >> direction >> amount;

                        if (face >= 0 && face < (int)steppers.size()) {
                            auto dir = (direction == 1)
                                ? Stepper::CLOCKWISE
                                : Stepper::COUNTER_CLOCKWISE;

                            steppers[face].rotate(amount, dir, default_freq);

                            std::cout << "Rotated " << amount
                                      << " steps on face " << face
                                      << " in direction " << direction << std::endl;
                            // Optional delay here if needed for camera stability
                            // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        }

                    // ---- E = Enable: "E face" ----
                    } else if (keyword == "E") {
                        int face;
                        cmd >> face;
                        if (face >= 0 && face < (int)steppers.size()) {
                            steppers[face].enable();
                        }

                    // ---- D = Disable: "D face" ----
                    } else if (keyword == "D") {
                        int face;
                        cmd >> face;
                        if (face >= 0 && face < (int)steppers.size()) {
                            steppers[face].disable();
                        }

                    // ---- F = Set Frequency: "F freq" ----
                    } else if (keyword == "F") {
                        int freq;
                        cmd >> freq;
                        default_freq = freq;

                    // ---- T = Timer Start ----
                    // Mac expects a "T <ms>" response when solving completes.
                    } else if (keyword == "T") {
                        timer_flag = true;
                        start = std::chrono::high_resolution_clock::now();

                    // ---- MS = Microstepping: "MS<group> <mode>" ----
                    // Example: "MS0 4" → group 0 (ring), SIXTEENTH mode
                    } else if (token.substr(0, 2) == "MS") {
                        int group, mode;
                        std::istringstream ms_cmd(token.substr(2));
                        ms_cmd >> group >> mode;

                        Stepper::StepMode sm;
                        switch (mode) {
                            case 0: sm = Stepper::FULL;      break;
                            case 1: sm = Stepper::HALF;      break;
                            case 2: sm = Stepper::QUARTER;   break;
                            case 3: sm = Stepper::EIGHTH;    break;
                            case 4: sm = Stepper::SIXTEENTH; break;
                            default:
                                std::cerr << "Invalid step mode: " << mode << std::endl;
                                send(client_fd, "ERR\n", 4, 0);
                                continue;
                        }

                        // group 0 = Gear stepper, group 1 = other steppers
                        if (group == 0) Gear_Stepper.setMode(sm);
                        else Bottom_Stepper.setMode(sm);

                    // ---- S = Sync (Full cube scan + color recognition) ----
                    } else if (keyword == "S") {
                        // Only run once per command; valid_cube forces exit
                        while (valid_cube) {
                            cam.sync_reset();

                            // Capture 13 images with specific rotation sequence
                            cam.getimage(0);

                            North_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(1);

                            North_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(2);

                            North_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(3);

                            North_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);

                            West_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(4);

                            West_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(5);

                            West_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(6);

                            West_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);

                            South_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            West_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(7);

                            West_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            South_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            West_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(8);

                            West_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            South_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            West_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(9);

                            West_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            South_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);

                            East_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            North_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(10);

                            North_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            East_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            North_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(11);

                            North_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            East_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);
                            North_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            cam.getimage(12);

                            North_Stepper.rotate(180, Stepper::CLOCKWISE, default_freq / 2);
                            East_Stepper.rotate(90, Stepper::CLOCKWISE, default_freq / 2);

                            valid_cube = false;

                            // Run classification + validity check
                            bool temp = cam.sync();
                            //valid_cube = !cam.sync(); // optional loop retry
                            sync_flag = true;
                        }

                    // ---- L = Lock clamp (Ring gear forward) ----
                    } else if (keyword == "L") {
                        Gear_Stepper.rotate(RING_STEPPER_LOCK_DEGREES,
                                            Stepper::CLOCKWISE,
                                            default_freq);

                    // ---- U = Unlock clamp (Ring gear backward) ----
                    } else if (keyword == "U") {
                        Gear_Stepper.rotate(RING_STEPPER_LOCK_DEGREES,
                                            Stepper::COUNTER_CLOCKWISE,
                                            default_freq);

                    // ---- P = Increase frequency ----
                    } else if (keyword == "P") {
                        default_freq += 10;
                        std::cout << "Changed frequency to " << default_freq << std::endl;

                    // ---- G = Decrease frequency ----
                    } else if (keyword == "G") {
                        default_freq -= 10;
                        std::cout << "Changed frequency to " << default_freq << std::endl;

                    } else {
                        // Unknown keyword inside batch
                        reply << "ERR\n";
                    }
                }

                // ---- Reply selection after batch ----

                if (timer_flag) {
                    // End timing, send solve time
                    timer_flag = false;
                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double, std::milli> elapsed = end - start;
                    solve_time = elapsed.count();
                    reply << "T " << solve_time << "\n";
                } else if (sync_flag) {
                    // Send sync result (facelet string)
                    sync_flag = false;
                    valid_cube = true;

                    std::string facelet = cam.getfacelet();
                    reply << "S " << facelet << "\n";
                } else {
                    // Default: batch processed
                    reply << "DONE\n";
                }

            // ---------------- SINGLE-CHAR COMMANDS (Not in batch) ----------------
            } else {
                if (command[0] == 'Q') {
                    // Quit command from client
                    reply << "BYE\n";
                    send(client_fd, reply.str().c_str(), reply.str().size(), 0);
                    break;
                } else {
                    reply << "ERR\n";
                }
            }

            // Send accumulated reply back to client
            std::string response = reply.str();
            send(client_fd, response.c_str(), response.size(), 0);
        }

        // Client connection closed
        close(client_fd);
    }

    // Should never reach here in normal daemon mode
    close(server_fd);
    return 0;
}