CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g

TARGET = server
OBJS = src/logsys/*.cpp src/pool/*.cpp src/timer/*.cpp \
	src/httpConnection/*.cpp src/server/*.cpp \
	src/buffer/*.cpp src/main.cpp
	
all: $(OBJS)
		$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET) -pthread -lmysqlclient
	
clean:
	rm -rf $(OBJS) $(TARGET)
