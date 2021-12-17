DEBUG ?= 0

ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
endif

SRC = ./tools_func/tools_func.cpp ./lst_timer/lst_timer.cpp ./http_connection/http_connection.cpp ./inactive_handler/inactive_handler.cpp ./web_server/web_server.cpp main.cpp

server:$(SRC)
	g++ -o server -pthread $(SRC) $(CXXFLAGS)

clean:
	rm -r server