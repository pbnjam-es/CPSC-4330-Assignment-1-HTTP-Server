CXX      := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -g
LDFLAGS  :=
TARGET   := server

SRCS := main.cpp config.cpp server.cpp workerThread.cpp operatorThread.cpp helpers.cpp selectLoop.cpp
OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
