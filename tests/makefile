# Makefile

# 编译器
CXX = g++

# 编译选项，-g 用于调试，-Wall 显示所有警告，-std=c++11 使用 C++11 标准
CXXFLAGS = -g -Wall -std=c++11

# 链接选项：系统 spdlog 常以外部 fmt 编译，需同时链接 libfmt
LDFLAGS = -lspdlog -lfmt -lcurl -lpthread

# 源文件
SOURCES = benchmark.cpp

# 目标可执行文件
TARGET = vector_db_test

# 默认目标
all: $(TARGET)

# 构建规则
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# 清理规则
clean:
	rm -f $(TARGET)
