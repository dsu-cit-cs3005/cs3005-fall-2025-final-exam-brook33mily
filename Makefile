CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic

OBJS = Arena.o RobotBase.o

all: RobotWarz

RobotBase.o: RobotBase.cpp RobotBase.h
	$(CXX) $(CXXFLAGS) -c RobotBase.cpp

Arena.o: Arena.cpp Arena.h
	$(CXX) $(CXXFLAGS) -c Arena.cpp

RobotWarz.o: RobotWarz.cpp Arena.h
	$(CXX) $(CXXFLAGS) -c RobotWarz.cpp

RobotWarz: RobotWarz.o Arena.o RobotBase.o
	$(CXX) $(CXXFLAGS) -ldl RobotWarz.o Arena.o RobotBase.o -o RobotWarz

clean:
	rm -f *.o *.so RobotWarz

