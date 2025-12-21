# 🤖 Rubik’s Cube Solver – 5-Face Camera System

This project is a fully custom **5-face Rubik’s Cube solver** built from scratch to improve my skills in:

- C++ software architecture  
- OpenCV computer vision  
- SolidWorks mechanical design  
- PCB design  
- Stepper motor control  
- TCP/IP networking  

The system uses a **client–server architecture**:

- 🍓 **Raspberry Pi → Client**  
  Handles motors, microstepping, camera capture, OpenCV color detection.

- 💻 **MacBook → Server**  
  Handles solving, graphical interface, 3D cube rendering, and control panel.

---

## 🎥 Demo Videos

**System Demo:**  
https://youtu.be/CzUOWeb3go8  

**SolidWorks Breakdown:**  
https://youtu.be/Gd5gCoYmTEs  

---

## 📂 Project Structure
RubiksCubeSolver/
│
├── PCB Files/              # PCB schematics & board design
├── Solidworks Files/       # Full CAD of the solver mechanism
│
└── Src/
├── Client/             # Raspberry Pi: motors + camera
└── Server/             # Mac: solver + UI + 3D viewer

---

# 🏗 System Overview

## 💻 Server (Mac)
Responsible for:
- Running the **rob-twophase** solver  
- Managing the **SFML + OpenGL** graphical control panel  
- Rendering a **3D cube** with animations  
- Sending commands to the Pi  
- Parsing responses  
- Timing, batching moves, and UI updates  

Technologies used:
- C++20  
- SFML  
- OpenGL  
- TCP/IP sockets  

---

## 🍓 Client (Raspberry Pi)
Responsible for:
- Driving 6 stepper motors  
- Applying microstepping (Full → Sixteenth)  
- Handling acceleration & deceleration curves  
- Capturing images with OpenCV  
- Detecting colors with HSV processing  
- Validating cube color layout  
- Sending the final **54-character facelet string** to the server  

Technologies used:
- C++  
- OpenCV  
- pigpio  
- Multithreading  
- TCP/IP sockets  

---

# 🌐 TCP/IP Communication

The architecture uses a **simple text-based custom protocol** over TCP sockets.

## Server → Client Commands
B M 0 -1 90;     # Rotate face 0, CCW, 90°
B F 1000;        # Set step frequency
B S;             # Start sync (camera scan)
B T;             # Start solve timer
Q                # Quit

## Client → Server Responses
T 1523.4         # Solve time (ms)
S UUU…BBB      # 54-char facelet string
DONE             # No special condition

---

# ⭐ Key Features

### 🖼 5-Face Camera Scanning
- Captures 13 images  
- Uses polygon masks to isolate facelets  
- Computes average HSV color  
- Maps HSV ranges → Rubik’s cube colors  
- Performs cube validity checks  

### ⚙️ Stepper Motor System
- Custom acceleration/deceleration curve  
- Shared microstepping modes  
- Ring-gear stepper for cube rotation  
- 5 turning steppers for each face  
- PWM + GPIO control via pigpio  

### 🧠 Solving Logic
- Uses **rob-twophase** solver  
- Converts solver moves → motor moves  
- Supports patterns, shuffles, syncing  

### 🖥 Graphical UI
- Built with **SFML + OpenGL**  
- Real-time visual cube  
- Move animations  
- Pattern selection  
- Manual motor controls  
- Timers & solve history  

### 🛠 Mechanical + PCB Design
- Entirely designed in SolidWorks  
- Custom PCB for stepper drivers  
- Gear system + mounting structure  
- Designed for stability, alignment, and lighting  

---

# 📘 What I Learned

This project helped me grow in:

- C++ class design, multithreading, and architecture  
- Real-time image processing with OpenCV  
- Hardware–software integration  
- Stepper motor tuning and microstepping  
- TCP/IP networking and protocol design  
- SFML & OpenGL graphics  
- SolidWorks mechanical design  
- PCB layout and driver electronics  
- Full system debugging across multiple platforms  

This was one of my most complete end-to-end engineering builds, combining software, mechanical, electrical, and computer vision into a single cohesive system.
