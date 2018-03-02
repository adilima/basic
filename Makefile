SOURCES = parser.cpp lexer.cpp main.cpp interp.cpp objects.cpp
OBJECTS = parser.o lexer.o main.o interp.o objects.o
LIBS = `llvm-config --libs` -pthread -ldl -lm -lrt -lncursesw
LDFLAGS = `llvm-config --ldflags` -L.
CFLAGS  = `llvm-config --cflags --cxxflags` \
	-std=c++17 -fexceptions \
	-O2 -fomit-frame-pointer -c

CXX = g++

TARGET = basic

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $(TARGET) $(OBJECTS) $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ $<

parser.cpp: parser.y
	bison -d parser.y

lexer.cpp: lexer.l
	flex lexer.l


clean:
	rm -fv $(TARGET) $(OBJECTS) parser.cpp parser.hpp lexer.cpp


