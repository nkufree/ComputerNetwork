CXXFLAGS = -MMD -std=c++17 -O2 -lwsock32 -finput-charset=UTF-8 -fexec-charset=GBK -fdiagnostics-color=always -g -pthread -static
CXX = g++
SRC = $(wildcard *.cpp)
TEMP = ./build/
OBJ = $(patsubst %cpp, $(TEMP)%o, $(SRC))
TARGET = main
-include *.d

.PHONY: all create_dir clean

all: create_dir $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS)

$(TEMP)%.o:%.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS)

create_dir:
	@if not exist $(TEMP) mkdir "$(TEMP)"

clean:
	del /s /q *.o *.d

# 变 量 	含 义
# $* 		表示目标文件的名称，不包含目标文件的扩展名
# $+ 		表示所有的依赖文件，这些依赖文件之间以空格分开，按照出现的先后为顺序，其中可能 包含重复的依赖文件
# $< 		表示依赖项中第一个依赖文件的名称
# $? 		依赖项中，所有比目标文件时间戳晚的依赖文件，依赖文件之间以空格分开
# $@ 		表示目标文件的名称，包含文件扩展名
# $^ 		依赖项中，所有不重复的依赖文件，这些文件之间以空格分开
