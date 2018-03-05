
SOURCES = parser.cpp lexer.cpp interp.cpp main.cpp basic.cpp if_stmt.cpp
OBJECTS = parser.o lexer.o interp.o main.o basic.o if_stmt.o

LIBS    = -pthread -ldl -lm -lrt -lncursesw `llvm-config --libs`
LDFLAGS = `llvm-config --ldflags` -L.

CFLAGS  = `llvm-config --cflags --cxxflags` -O2 -fexceptions -fomit-frame-pointer -std=c++17 -c

CXX     = g++

TARGET  = basic

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
	rm -fv $(TARGET) $(OBJECTS)
	rm -fv parser.{cpp,hpp} lexer.cpp

