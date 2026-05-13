# 研究向量数据库

使用了很久的向量数据库，总想研究下它的原理。本仓库是一个面向学习的迷你向量库服务：用 **Faiss / hnswlib / Annoy** 做向量检索实验，用 **RocksDB** 存标量与元数据，用 **NuRaft** 做多副本一致性，对外暴露 **HTTP JSON** 接口。

---

## 索引存储

- **FLAT**：基于 Faiss 的精确暴力检索（`IndexFactory::IndexType::FLAT`）。
- **HNSW**：基于 hnswlib 的图索引（`IndexFactory::IndexType::HNSW`），适合更大规模近似检索。
- **FILTER**：整型字段过滤索引（`FilterIndex`），内部用 **Roaring Bitmap** 维护「字段值 → 文档 id 集合」，与向量检索组合做标量过滤。
- **ANNOY（实验中）**：已引入 `annoy_index.cpp` 与 Annoy 头文件依赖，当前用于独立索引能力验证（插入、查询、保存、加载）。
- 索引由 `IndexFactory` 统一管理，可与快照流程一起 **save / load** 到磁盘（与标量存储协同）。

---

## 标量存储

- 使用 **RocksDB**（`ScalarStorage`）持久化每条向量的 JSON 形态元数据（如自定义整型字段等）。
- 支持按 id 读写；管理端提供按 key 前缀与上界的 **分页扫描**（`GET /admin/getScalar`），便于排查与运维。

---

## 混合存储

- **向量** 落在内存索引（Faiss / HNSW）；**属性与过滤字段** 落在 RocksDB。
- `upsert` 时同时更新向量索引与标量库，并维护 `FILTER` 索引中的位图。
- `search` 可在请求中带 `filter`（如整型字段 `=` / `!=`），先由位图得到候选 id，再与向量检索结果求交，实现「向量 + 标量条件」的混合查询。

---

## 分布式

- 基于 **NuRaft**（`RaftStuff`、状态机与日志）实现多节点复制与选主。
- **WAL**（`Persistence`）记录操作日志；**快照**用于压缩日志与恢复状态。
- HTTP 管理接口支持 **设主、加从、列节点、触发快照** 等（见下文「管理页面」）。

---

## 集成 SDK

- 未单独发布语言 SDK；客户端可直接使用 **HTTP**（如 `curl`、任意语言的 HTTP 库）调用 JSON 接口。
- 业务接口示例：`POST /search`、`POST /insert`、`POST /upsert`、`POST /query`。
- 更完整的 curl 示例见仓库内 [`docs/接口实例.md`](docs/接口实例.md)。

---

## 管理页面

- **无独立 Web 管理前端**；运维与调试通过 **`/admin/*` HTTP 接口** 完成，例如：
  - `POST /admin/setLeader`、`POST /admin/addFollower`
  - `GET /admin/listNode`、`GET /admin/getNode`
  - `POST /admin/snapshot`
  - `GET /admin/getScalar`（分页扫描 RocksDB 标量）

---

## 依赖与编译

系统需安装开发包（名称因发行版略有差异），链接阶段依赖包括但不限于：**Faiss、OpenBLAS、RocksDB、CRoaring、spdlog、fmt、OpenSSL、zlib**，以及 **NuRaft**（见子模块说明）。

Annoy 在本项目中以 **header-only** 方式使用，头文件位于 `third_party/annoy/src/`（`annoylib.h`、`kissrandom.h`、`mman.h`）。

首次拉取 NuRaft 与子模块：

```bash
git clone https://github.com/eBay/NuRaft.git third_party/NuRaft
cd third_party/NuRaft && git submodule update --init
cd ../..
make clean && make
```

若本机有 `cmake`，会优先用 CMake 构建 `libnuraft.a`；否则 makefile 会用 g++ 按 NuRaft 核心源码列表手动编译静态库。

Annoy 头文件下载（网络不稳定时可单独执行）：

```bash
mkdir -p third_party/annoy/src
curl -L "https://raw.githubusercontent.com/spotify/annoy/master/src/annoylib.h" -o "third_party/annoy/src/annoylib.h"
curl -L "https://raw.githubusercontent.com/spotify/annoy/master/src/kissrandom.h" -o "third_party/annoy/src/kissrandom.h"
curl -L "https://raw.githubusercontent.com/spotify/annoy/master/src/mman.h" -o "third_party/annoy/src/mman.h"
```

---

## 配置与运行

配置文件示例为根目录 [`conf.ini`](conf.ini)：`db_path`（RocksDB 目录）、`wal_path`（WAL 目录）、`node_id`、`endpoint`、`port`（Raft）、`http_server_address`、`http_server_port`（HTTP 服务）。

```bash
./vdb_server conf.ini
```

---

## 测试与压测

### HTTP 压测（依赖已启动的 vdb_server）

`tests/benchmark.cpp` 提供一个 HTTP 压测程序，用于验证写入、检索以及写入后检索的混合场景。测试前需先启动 `vdb_server`，并确保 `tests/conf.ini` 中的 `write_url`、`read_url` 指向当前服务地址。

```bash
cd tests
make
./vector_db_test ./conf.ini
```

`tests/conf.ini` 中主要参数如下：

| 参数 | 说明 |
|------|------|
| `test_type` | `0` 表示写入测试，`1` 表示读取测试，`2` 表示先写入再读取并计算召回率 |
| `num_threads` | 并发线程数 |
| `num_vectors` | 生成的请求数量 |
| `dim` | 向量维度 |
| `write_url` | 写入接口地址，默认示例为 `/upsert` |
| `read_url` | 查询接口地址，默认示例为 `/search` |

压测输出包括平均延迟、P99 延迟、系统吞吐量；当 `test_type=2` 时，还会输出整体召回率。示例输出记录见 `tests/记录.txt`。

### 单机内存基准（不启动 vdb_server）

在 `tests` 目录下编译并运行，用于观察 **128 维、100 万条** 向量插入前后进程的 **VmRSS / VmHWM**（读取 `/proc/self/status`）。依赖本机已安装 **spdlog、fmt、CRoaring** 等与主工程一致的开发库。

| Makefile 目标 | 源文件 | 说明 |
|-----------------|--------|------|
| `annoy_mem_bench` | `benchmark_annoy_memory.cpp` + `annoy_index.cpp`、`logger.cpp` | 使用封装类 `AnnoyIndex`：插入 100 万条后采样内存，再 `search_vectors` 触发与线上一致的 `build`。 |
| `hnsw_mem_bench` | `benchmark_hnsw_memory.cpp` + `hnswlib_index.cpp`、`logger.cpp` | 使用 `HNSWLibIndex`（默认 `M=16`、`ef_construction=200`）：插入 100 万条后采样，再执行一次 `search_vectors`。 |

```bash
cd tests
make annoy_mem_bench && ./annoy_mem_bench
make hnsw_mem_bench && ./hnsw_mem_bench
```

`make clean` 会删除 `vector_db_test`、`annoy_mem_bench`、`hnsw_mem_bench`。

---

## 仓库主要源码

| 模块 | 说明 |
|------|------|
| `vdb_server.cpp` | 入口：读配置、初始化索引工厂、向量库、Raft、HTTP 服务 |
| `vector_database.*` | 向量与标量协同、WAL、快照、检索入口 |
| `scalar_storage.*` | RocksDB 标量层 |
| `index_factory.*`、`faiss_index.*`、`hnswlib_index.*`、`annoy_index.cpp`、`filter_index.*` | 索引类型与持久化 |
| `persistence.*` | WAL 与快照相关 |
| `raft_stuff.*`、`log_state_machine.*` 等 | Raft 集成 |
| `http_server.*` | HTTP 路由与 JSON 处理 |
