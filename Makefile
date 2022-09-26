run: a.out
		./a.out
a.out: QuickLogger.hpp benchmark.cpp
		g++ -O2 -std=c++17 benchmark.cpp -lfmt -lpthread
clean:
		rm a.out
		rm -r logs
