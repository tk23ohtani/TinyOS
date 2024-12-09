# Compiler
CXX = C:\MinGW\bin\g++.exe

# Target executable
TARGET = Debug/TinyOS.exe

# Source files
SRCS = TinyOS/TinyOS.cpp

# Compiler flags
CXXFLAGS = -Wall -g

# Build target
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# Clean target
clean:
	del $(TARGET)
