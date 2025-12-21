CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread
INCLUDES = -I/opt/homebrew/include -I/Users/mantaj/rob-twophase/src
LDFLAGS = -L/opt/homebrew/lib -L/Users/mantaj/rob-twophase/src -lsfml-graphics -lsfml-window -lsfml-system -ltwophase -framework OpenGL -framework GLUT

SRCS = solver_client.cpp Solver.cpp control_panel.cpp
OBJS = $(SRCS:.cpp=.o)

all: cube_solver

cube_solver: $(OBJS)
	$(CXX) $(CXXFLAGS) -o cube_solver $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) cube_solver