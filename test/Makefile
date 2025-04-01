CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = test
OBJS = ../src/logsys/*.cpp ../src/pool/*.cpp ../src/timer/*.cpp \
       ../src/httpConnection/*.cpp ../src/server/*.cpp \
       ../src/buffer/*.cpp ../src/test.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf $(OBJS) $(TARGET)