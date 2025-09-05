CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g
LDFLAGS = -lcrypto -lssl -pthread -lmysqlclient

TARGET = server
SRCDIR = src
SOURCES = $(wildcard $(SRCDIR)/logsys/*.cpp) \
          $(wildcard $(SRCDIR)/pool/*.cpp) \
          $(wildcard $(SRCDIR)/timer/*.cpp) \
          $(wildcard $(SRCDIR)/httpConnection/*.cpp) \
          $(wildcard $(SRCDIR)/server/*.cpp) \
          $(wildcard $(SRCDIR)/buffer/*.cpp) \
          $(SRCDIR)/main.cpp \
          $(SRCDIR)/websocket_handler.cpp

OBJS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Compilation completed. Cleaning up object files..."
	@$(MAKE) clean_objs

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

clean_objs:
	@echo "Removing object files..."
	@rm -f $(OBJS)
	@echo "Object files cleaned up."

clean:
	rm -rf $(OBJS) $(TARGET)

.PHONY: all clean clean_objs
