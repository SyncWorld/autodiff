FLAGS=-Wall -g -std=c++11 -O3 -I.

# MAIN BUILD
main: all
	g++ $(FLAGS) *.o -o main
all: src/expression.cpp src/var.cpp src/utils.cpp
	g++ $(FLAGS) -c src/*.cpp 

# RUN
test: utils-test var-test expression-test utils-test
	build/var-test
	build/expression-test
	build/utils-test

# SRC BUILD
var.o: src/var.cpp
	g++ $(FLAGS) -c src/var.cpp -o build/var.o
expression.o: src/expression.cpp
	g++ $(FLAGS) -c src/expression.cpp -o build/expression.o

# TEST BUILD
main-test.o: test/main-test.cpp
	g++ $(FLAGS) -c test/main-test.cpp -o build/main-test.o
var-test: test/var-test.cpp main-test.o
	g++ $(FLAGS) build/main-test.o \
		test/var-test.cpp \
		src/var.cpp \
		-o build/var-test
expression-test: test/expression-test.cpp src/expression.cpp src/var.cpp main-test.o
	g++ $(FLAGS) build/main-test.o \
		test/expression-test.cpp \
		src/expression.cpp \
		src/var.cpp \
		-o build/expression-test
utils-test: test/utils-test.cpp test/expression-test.cpp src/expression.cpp src/var.cpp src/utils.cpp main-test.o
	g++ $(FLAGS) build/main-test.o \
		test/utils-test.cpp \
		src/utils.cpp \
		src/expression.cpp \
		src/var.cpp \
		-o build/utils-test

# CLEAN/DIST
clean:
	rm -rf build/* 
	rm -rf *.o
