CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

Linux: codes/http_server.cpp codes/console.cpp
	$(CXX) codes/http_server.cpp -o http_server $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
	$(CXX) codes/console.cpp -o console.cgi $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

Windows: codes/windows_cgi_server.cpp
	g++ codes/windows_cgi_server.cpp -o windows_cgi_server.exe -lws2_32 -lwsock32 -std=c++14

clean: clean_Linux clean_Windows

clean_Linux:
	rm http_server console.cgi 
clean_Windows:
	windows_cgi_server.exe