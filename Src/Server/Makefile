CXX = g++
CXXFLAGS = -Wall -std=c++17 -I../rob-twophase/src -pthread `pkg-config --cflags opencv4`
LIBS = -lpigpio -lrt -pthread -L../rob-twophase -ltwophase  `pkg-config --libs opencv4`
TARGET = cube_solver

SRCS = main.cpp GPIO.cpp Stepper.cpp Camera.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(TARGET) *.o