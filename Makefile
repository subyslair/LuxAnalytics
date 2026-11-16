CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -Iinclude
SRCS = $(wildcard src/*.cpp)
TARGET = lux_engine

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) data/clienteling_report.csv
