#include "Camera.h"

/* -----------------------------------------------------------
   Constructor
   - Opens camera using V4L2 backend
   - Loads the 6 quads (facelet sampling regions)
   ----------------------------------------------------------- */
Camera::Camera() {
    capture.open(0, cv::CAP_V4L2);

    // Fixed 6 polygon facelet regions (manually calibrated)
    facelet_quads = {
        { {354,204}, {425,205}, {402,267}, {297,264} },  // Quad #1
        { {267,219}, {319,218}, {286,262}, {220,259} },  // Quad #2
        { {198,223}, {232,218}, {188,260}, {147,260} },  // Quad #3
        { {399,332}, {433,357}, {359,358}, {312,333} },  // Quad #4
        { {290,313}, {317,357}, {270,349}, {223,313} },  // Quad #5
        { {197,303}, {224,343}, {197,338}, {160,304} }   // Quad #6
    };
}

Camera::~Camera() {
    capture.release();
}

/* -----------------------------------------------------------
   getandshow()
   - Opens a live preview window
   - Prints clicked coordinates (for calibration)
   ----------------------------------------------------------- */
void Camera::getandshow() {
    cv::namedWindow("Frame");

    cv::setMouseCallback("Frame", [](int event, int x, int y, int, void*) {
        if (event == cv::EVENT_LBUTTONDOWN) {
            std::cout << "Clicked at: (" << x << ", " << y << ")\n";
        }
    });

    while (true) {
        capture >> frame;
        if (frame.empty()) continue;

        cv::imshow("Frame", frame);
        if (cv::waitKey(10) == 27) break;  // ESC exits
    }
}

/* -----------------------------------------------------------
   sync()
   - Draws quads on all 13 captured images
   - Extracts 54 facelet colors into cube_colors[]
   - Builds full 54-character facelet string
   - Checks cube validity
   ----------------------------------------------------------- */
bool Camera::sync() {
    // Draw classification info on each captured image
    for (cv::Mat& img : images) {
        for (const auto& quad : facelet_quads) {
            drawSquareInfo(img, quad);
        }
    }

    // ---- FACELET INDEX MAP ----
    // For each face (U/R/F/D/L/B) you define *9 indices* into cube_colors
    // Stored in solver-friendly facelet order.
    const std::vector<int> U = {6, 7, 0, 13, -1, 1, 12, 19, 2};
    const std::vector<int> R = {5, 4, 3, 28, -1, 40, 29, 34, 35};
    const std::vector<int> F = {74, 22, 21, 73, -1, 25, 45, 46, 26};
    const std::vector<int> D = {42, 49, 31, 30, -1, 43, 48, 55, 32};
    const std::vector<int> L = {17, 16, 15, 64, -1, 76, 51, 52, 53};
    const std::vector<int> B = {11, 10, 9, 37, -1, 61, 36, 58, 59};

    /* Helper to convert 9 indices → string of 9 characters.
       -1 means center sticker → use face letter */
    auto faceString = [&](const std::vector<int>& indices, char center) {
        std::string result;
        for (int idx : indices) {
            result += (idx == -1) ? std::string(1, center) : cube_colors[idx];
        }
        return result;
    };

    // Build final 54-character Kociemba-compatible facelet
    facelet =
        faceString(U, 'U') +
        faceString(R, 'R') +
        faceString(F, 'F') +
        faceString(D, 'D') +
        faceString(L, 'L') +
        faceString(B, 'B');

    std::cout << "Facelet: " << facelet << std::endl;

    return isValidCube();
}

/* -----------------------------------------------------------
   classifyColor()
   - Converts mean BGR → HSV
   - Applies manual thresholds to classify cube sticker color
   ----------------------------------------------------------- */
std::string Camera::classifyColor(const cv::Scalar& bgr) {
    cv::Mat bgr_pixel(1, 1, CV_8UC3, bgr);
    cv::Mat hsv_pixel;
    cv::cvtColor(bgr_pixel, hsv_pixel, cv::COLOR_BGR2HSV);

    cv::Vec3b hsv = hsv_pixel.at<cv::Vec3b>(0, 0);
    int h = hsv[0], s = hsv[1], v = hsv[2];

    // Manual calibrated HSV thresholds
    if (h > 170 && s > 100 && v > 100) return "U";  // Red
    if (h >= 1 && h < 20 && s > 80 && v > 40) return "D"; // Orange
    if (h >= 20 && h < 32 && s > 90 && v > 50) return "F"; // Yellow
    if (h >= 32 && h < 85 && s > 80 && v > 50) return "L"; // Green
    if (h >= 85 && h <= 135 && s > 80 && v > 50) return "R"; // Blue

    return "B"; // fallback → White
}

/* -----------------------------------------------------------
   getimage(i)
   - Sleeps briefly so motor motion stops
   - Flushes camera buffer (prevents old frames)
   - Captures the requested image index into images[i]
   ----------------------------------------------------------- */
void Camera::getimage(int i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mtx.lock();

    // Flush 5 frames to avoid stale images
    for (int j = 0; j < 5; ++j) {
        cv::Mat temp;
        capture >> temp;
    }

    // Ensure vector size
    if (i >= images.size()) {
        images.resize(i + 1);
    }

    capture >> images[i];
    mtx.unlock();
}

/* -----------------------------------------------------------
   drawSquareInfo()
   - Creates a mask for polygon
   - Computes mean color of polygon
   - Classifies color (U/R/F/D/L/B)
   - Draws polygon + label
   - Pushes classification into cube_colors[]
   ----------------------------------------------------------- */
void Camera::drawSquareInfo(cv::Mat& img, const std::vector<cv::Point>& quad) {
    cv::Mat mask = cv::Mat::zeros(img.size(), CV_8UC1);
    std::vector<std::vector<cv::Point>> contour = { quad };
    cv::fillPoly(mask, contour, cv::Scalar(255));

    cv::Scalar meanColor = cv::mean(img, mask);
    std::string colorName = classifyColor(meanColor);

    const cv::Point* pts = quad.data();
    int npts = 4;
    cv::polylines(img, &pts, &npts, 1, true, cv::Scalar(0, 255, 0), 1);

    cv::Point center = (quad[0] + quad[1] + quad[2] + quad[3]) * 0.25;
    cv::putText(img, colorName, center, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(0, 0, 0), 1);

    cube_colors.push_back(colorName);
}

/* -----------------------------------------------------------
   sync_reset()
   - Clears stored colors + images so next sync is clean
   ----------------------------------------------------------- */
void Camera::sync_reset() {
    cube_colors.clear();
    images.clear();
}

/* -----------------------------------------------------------
   isValidCube()
   - Ensures each color appears exactly 9 times
   - Ensures exactly 6 colors exist
   - Performs adjacency match checks between stickers
   ----------------------------------------------------------- */
bool Camera::isValidCube() {
    std::unordered_map<char, int> counts;

    for (char c : facelet) counts[c]++;

    // Each color must appear 9 times
    for (const auto& [color, count] : counts) {
        if (count != 9) {
            std::cout << "Invalid count for '" << color << "': got " << count << "\n";
            return false;
        }
    }

    if (counts.size() != 6) {
        std::cout << "Invalid: expected 6 unique colors, found " << counts.size() << "\n";
        return false;
    }

    // Matching helpers
    auto match = [&](int i, int j) {
        if (cube_colors[i] != cube_colors[j]) {
            std::cout << "Mismatch: cube_colors[" << i << "] != [" << j << "]\n";
            return false;
        }
        return true;
    };

    auto match3 = [&](int i, int j, int k) {
        if (cube_colors[i] != cube_colors[j] || cube_colors[j] != cube_colors[k]) {
            std::cout << "Mismatch among " << i << ", " << j << ", " << k << "\n";
            return false;
        }
        return true;
    };

    // These checks ensure correct cube adjacency
    return
        match(6, 14) && match(0, 8) && match(12, 20) && match(2, 18) &&
        match(5, 27) && match(3, 41) && match(29, 33) && match(35, 39) &&
        match(11, 38) && match(9, 60) && match(36, 57) && match(59, 62) &&
        match3(53, 71, 75) && match(52, 70) && match3(51, 65, 69) &&
        match(15, 77) && match(17, 63) &&
        match(23, 74) && match(21, 24) && match(45, 72) && match(26, 47) &&
        match(30, 44) && match3(42, 50, 68) && match(49, 67) &&
        match(32, 54) && match3(48, 56, 66);
}