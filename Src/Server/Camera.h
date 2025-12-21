#pragma once
#include <opencv2/opencv.hpp>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <string>
// NOTE: std::mutex is used below – make sure <mutex> is included somewhere.

class Camera {
public:
    /* --------------------------------------------------------
       Constructor / Destructor
       - Opens the camera device (V4L2 in your .cpp)
       - Initializes quad positions for sampling facelets
       -------------------------------------------------------- */
    Camera();
    ~Camera();

    /* --------------------------------------------------------
       getandshow()
       - Live preview mode for calibration
       - Shows a window and prints mouse click coordinates
       -------------------------------------------------------- */
    void getandshow();

    /* --------------------------------------------------------
       sync()
       - Uses captured images + facelet_quads
       - Samples each quad, classifies colors, fills cube_colors
       - Builds 54-char facelet string in Kociemba order
       - Validates cube structure (isValidCube)
       - Returns true if cube state is valid
       -------------------------------------------------------- */
    bool sync();

    /* --------------------------------------------------------
       getimage(i)
       - Captures a single image into images[i]
       - Called repeatedly while rotating the cube to each face
       - Thread-safe (uses mtx)
       -------------------------------------------------------- */
    void getimage(int i);

    /* --------------------------------------------------------
       sync_reset()
       - Clears cube_colors and images
       - Call before a new sync sequence to start clean
       -------------------------------------------------------- */
    void sync_reset();

    /* --------------------------------------------------------
       getfacelet()
       - Returns the last computed 54-character facelet string
       -------------------------------------------------------- */
    std::string getfacelet();

private:
    /* --------------------------------------------------------
       Video / frame data
       -------------------------------------------------------- */
    cv::VideoCapture capture;  // Camera handle
    cv::Mat frame;             // Single frame buffer for preview / capture

    /* --------------------------------------------------------
       Geometry for sampling
       - facelet_quads: list of 4-point polygons (same for all images)
       - facelet_quads_per_image: if you later want per-image quads
       -------------------------------------------------------- */
    std::vector<std::vector<cv::Point>> facelet_quads;
    std::vector<std::vector<cv::Point>> facelet_quads_per_image[4];

    /* --------------------------------------------------------
       Sync / capture state
       -------------------------------------------------------- */
    std::mutex mtx;                 // Protects access to capture & images
    std::vector<cv::Mat> images;    // Captured images for each cube orientation
    std::vector<std::string> cube_colors; // Classified colors, indexed globally
    std::string facelet;            // Final 54-char facelet string (URFDLB order)

    /* --------------------------------------------------------
       classifyColor()
       - Input: mean BGR/HSV color
       - Output: one-letter sticker color ("U","R","F","D","L","B")
       - Implemented with HSV thresholding in .cpp
       -------------------------------------------------------- */
    std::string classifyColor(const cv::Scalar& color);

    /* --------------------------------------------------------
       getCubeFaceROIs()
       - Returns 3x3 rectangular ROIs for a cube face
       - Used for debugging / clicking, separate from quads
       -------------------------------------------------------- */
    std::vector<cv::Rect> getCubeFaceROIs();

    /* --------------------------------------------------------
       drawSquareInfo()
       - Given an image and a quad:
         * Builds a mask
         * Computes mean color
         * Classifies sticker color
         * Draws outline + label onto the image
         * Pushes classification into cube_colors
       -------------------------------------------------------- */
    void drawSquareInfo(cv::Mat& img, const std::vector<cv::Point>& quad);

    /* --------------------------------------------------------
       onMouse()
       - Static callback for OpenCV mouse events
       - Logs clicked position and which ROI (if any) was clicked
       - Used in calibration / debug mode
       -------------------------------------------------------- */
    static void onMouse(int event, int x, int y, int, void* userdata);

    /* --------------------------------------------------------
       isValidCube()
       - Checks that:
         * Each color appears exactly 9 times
         * There are exactly 6 unique colors
         * All adjacency constraints (pairs / triples) are satisfied
       - Prints detailed mismatch/debug output on failure
       -------------------------------------------------------- */
    bool isValidCube();
};