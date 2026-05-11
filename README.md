# 研究向量数据库

使用了很久的向量数据库，总想研究下它的原理。本仓库是一个面向学习的迷你向量库服务：用 **Faiss / hnswlib** 做近似检索，用 **RocksDB** 存标量与元数据，用 **NuRaft** 做多副本一致性，对外暴露 **HTTP JSON** 接口。

---

## 索引存储

- **FLAT**：基于 Faiss 的精确暴力检索（`IndexFactory::IndexType::FLAT`）。
- **HNSW**：基于 hnswlib 的图索引（`IndexFactory::IndexType::HNSW`），适合更大规模近似检索。
- **FILTER**：整型字段过滤索引（`FilterIndex`），内部用 **Roaring Bitmap** 维护「字段值 → 文档 id 集合」，与向量检索组合做标量过滤。
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

首次拉取 NuRaft 与子模块：

```bash
git clone https://github.com/eBay/NuRaft.git third_party/NuRaft
cd third_party/NuRaft && git submodule update --init
cd ../..
make clean && make
```

若本机有 `cmake`，会优先用 CMake 构建 `libnuraft.a`；否则 makefile 会用 g++ 按 NuRaft 核心源码列表手动编译静态库。

---

## 配置与运行

配置文件示例为根目录 [`conf.ini`](conf.ini)：`db_path`（RocksDB 目录）、`wal_path`（WAL 目录）、`node_id`、`endpoint`、`port`（Raft）、`http_server_address`、`http_server_port`（HTTP 服务）。

```bash
./vdb_server conf.ini
```

---

## 仓库主要源码

| 模块 | 说明 |
|------|------|
| `vdb_server.cpp` | 入口：读配置、初始化索引工厂、向量库、Raft、HTTP 服务 |
| `vector_database.*` | 向量与标量协同、WAL、快照、检索入口 |
| `scalar_storage.*` | RocksDB 标量层 |
| `index_factory.*`、`faiss_index.*`、`hnswlib_index.*`、`filter_index.*` | 索引类型与持久化 |
| `persistence.*` | WAL 与快照相关 |
| `raft_stuff.*`、`log_state_machine.*` 等 | Raft 集成 |
| `http_server.*` | HTTP 路由与 JSON 处理 |
