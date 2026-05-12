# 编译器
CXX = g++

# NuRaft 源码（生成 third_party/NuRaft/build/libnuraft.a：优先 cmake，否则 g++ 直接编译）
NURAFT_ROOT := $(CURDIR)/third_party/NuRaft
NURAFT_BUILD := $(NURAFT_ROOT)/build
NURAFT_STATIC := $(NURAFT_BUILD)/libnuraft.a
# 无 cmake 时编译 NuRaft 所需源码列表（需与 NuRaft CMakeLists.txt 中 RAFT_CORE 一致）
NURAFT_MANUAL_SRCS := \
	$(NURAFT_ROOT)/src/asio_service.cxx \
	$(NURAFT_ROOT)/src/buffer.cxx \
	$(NURAFT_ROOT)/src/buffer_serializer.cxx \
	$(NURAFT_ROOT)/src/cluster_config.cxx \
	$(NURAFT_ROOT)/src/crc32.cxx \
	$(NURAFT_ROOT)/src/error_code.cxx \
	$(NURAFT_ROOT)/src/global_mgr.cxx \
	$(NURAFT_ROOT)/src/handle_append_entries.cxx \
	$(NURAFT_ROOT)/src/handle_client_request.cxx \
	$(NURAFT_ROOT)/src/handle_custom_notification.cxx \
	$(NURAFT_ROOT)/src/handle_commit.cxx \
	$(NURAFT_ROOT)/src/handle_join_leave.cxx \
	$(NURAFT_ROOT)/src/handle_priority.cxx \
	$(NURAFT_ROOT)/src/handle_snapshot_sync.cxx \
	$(NURAFT_ROOT)/src/handle_timeout.cxx \
	$(NURAFT_ROOT)/src/handle_user_cmd.cxx \
	$(NURAFT_ROOT)/src/handle_vote.cxx \
	$(NURAFT_ROOT)/src/launcher.cxx \
	$(NURAFT_ROOT)/src/log_entry.cxx \
	$(NURAFT_ROOT)/src/peer.cxx \
	$(NURAFT_ROOT)/src/raft_server.cxx \
	$(NURAFT_ROOT)/src/snapshot.cxx \
	$(NURAFT_ROOT)/src/snapshot_sync_ctx.cxx \
	$(NURAFT_ROOT)/src/snapshot_sync_req.cxx \
	$(NURAFT_ROOT)/src/srv_config.cxx \
	$(NURAFT_ROOT)/src/stat_mgr.cxx
NURAFT_MANUAL_OBJS := $(patsubst $(NURAFT_ROOT)/src/%.cxx,$(NURAFT_BUILD)/manual/%.o,$(NURAFT_MANUAL_SRCS))
NURAFT_MAN_CPPFLAGS := -I$(NURAFT_ROOT) -I$(NURAFT_ROOT)/src -I$(NURAFT_ROOT)/include \
	-I$(NURAFT_ROOT)/include/libnuraft -I$(NURAFT_ROOT)/asio/asio/include
NURAFT_MAN_CXXFLAGS := -std=c++11 -O2 -g -fPIC -pthread -DASIO_STANDALONE -DASIO_HAS_STD_CHRONO=1 \
	-Wall -Wno-pessimizing-move -Wno-deprecated-declarations
AR := ar

# Include directories（libnuraft 头文件 + 独立 Asio 头文件）
INCLUDES = -I ./hnswlib \
	-I ./third_party/annoy/src \
	-I $(NURAFT_ROOT)/include \
	-I $(NURAFT_ROOT)/asio/asio/include

# 编译选项
CXXFLAGS = -std=c++11 -g $(INCLUDES) -DASIO_STANDALONE

# 链接选项（静态库 libnuraft 需在 -lssl -lcrypto 之前，以便解析依赖）
LDFLAGS = $(NURAFT_STATIC) -lfaiss -fopenmp -lopenblas -lpthread -lspdlog -lfmt -lrocksdb -lroaring -lssl -lcrypto -lz -ldl

# 目标文件
TARGET = vdb_server

# 源文件
SOURCES = vdb_server.cpp faiss_index.cpp http_server.cpp index_factory.cpp logger.cpp \
hnswlib_index.cpp annoy_index.cpp scalar_storage.cpp vector_database.cpp filter_index.cpp persistence.cpp \
in_memory_log_store.cpp log_state_machine.cpp raft_stuff.cpp raft_logger.cpp

# 对象文件
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

# 构建 NuRaft 静态库：有 cmake 则用 CMake；否则用 g++ 编译 RAFT_CORE 再打静态库（无需 cmake）
$(NURAFT_STATIC):
	@if [ ! -f "$(NURAFT_ROOT)/CMakeLists.txt" ]; then \
		echo "缺少 $(NURAFT_ROOT)，请先: git clone https://github.com/eBay/NuRaft.git third_party/NuRaft && cd third_party/NuRaft && git submodule update --init"; \
		exit 1; \
	fi
	@if command -v cmake >/dev/null 2>&1; then \
		mkdir -p $(NURAFT_BUILD) && cd $(NURAFT_BUILD) \
			&& cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
			&& $(MAKE); \
	else \
		echo "未检测到 cmake，使用 g++ 直接编译 NuRaft（与 CMake 的 RAFT_CORE 一致）"; \
		$(MAKE) nuraft-manual; \
	fi

# 无 cmake 时的 NuRaft 静态库（供 $(NURAFT_STATIC) 调用，也可手动 make nuraft-manual）
.PHONY: nuraft-manual
nuraft-manual: $(NURAFT_MANUAL_OBJS)
	mkdir -p $(NURAFT_BUILD)
	$(AR) rcs $(NURAFT_STATIC) $(NURAFT_MANUAL_OBJS)

$(NURAFT_BUILD)/manual/%.o: $(NURAFT_ROOT)/src/%.cxx
	mkdir -p $(NURAFT_BUILD)/manual
	$(CXX) $(NURAFT_MAN_CXXFLAGS) $(NURAFT_MAN_CPPFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) $(NURAFT_STATIC)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)