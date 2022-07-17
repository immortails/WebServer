CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp  http/httpConn.cpp server/server.cpp timer/heapTimer.cpp cJSON/cJSON.c
	$(CXX) -o debug/server  $^ $(CXXFLAGS) -lpthread 

clean:
	rm  -r debug/server