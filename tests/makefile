# Makefile

# 编译器
CXX = g++

# 编译选项，-g 用于调试，-Wall 显示所有警告，-std=c++11 使用 C++11 标准
CXXFLAGS = -g -Wall -std=c++11

# Annoy 内存基准使用 C++14（chrono 等），独立链接无需 spdlog
ANN_MEM_CXXFLAGS = -g -Wall -std=c++14 -O2

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

# Annoy 100万条插入内存变化基准（维度 128，链接项目内 annoy_index / logger）；在 tests 目录: make annoy_mem_bench && ./annoy_mem_bench
annoy_mem_bench: benchmark_annoy_memory.cpp ../annoy_index.cpp ../logger.cpp
	$(CXX) $(ANN_MEM_CXXFLAGS) -I.. -o $@ benchmark_annoy_memory.cpp ../annoy_index.cpp ../logger.cpp -lspdlog -lfmt -lroaring

# HNSW 100万条插入内存变化基准（维度 128，链接 hnswlib_index / logger）；在 tests 目录: make hnsw_mem_bench && ./hnsw_mem_bench
hnsw_mem_bench: benchmark_hnsw_memory.cpp ../hnswlib_index.cpp ../logger.cpp
	$(CXX) $(ANN_MEM_CXXFLAGS) -I.. -I../hnswlib -o $@ benchmark_hnsw_memory.cpp ../hnswlib_index.cpp ../logger.cpp -lspdlog -lfmt -lroaring -fopenmp

# 清理规则
clean:
	rm -f $(TARGET) annoy_mem_bench hnsw_mem_bench
