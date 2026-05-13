// HNSW 内存基准：通过项目内 HNSWLibIndex（hnswlib_index.cpp）插入 100 万条，观察 VmRSS / VmHWM 变化
// 维度 128；参数与 index_factory 中 HNSW 默认一致（M=16, ef_construction=200）

#include "../hnswlib_index.h"
#include "../logger.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

long read_status_kb(const char* key) {
    std::ifstream in("/proc/self/status");
    if (!in) {
        return -1;
    }
    const std::string prefix = std::string(key) + ":";
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < prefix.size() || line.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        std::istringstream iss(line.substr(prefix.size()));
        long kb = 0;
        iss >> kb;
        return kb;
    }
    return -1;
}

void print_kb_line(const char* tag, long rss_kb, long hwm_kb) {
    std::cout << tag << " VmRSS=" << rss_kb << " kB, VmHWM=" << hwm_kb << " kB\n";
}
}  // namespace

int main() {
    init_global_logger();

    constexpr int k_dim = 128;
    constexpr int k_max_elements = 1000000;
    constexpr int64_t k_count = 1000000;
    constexpr int k_m = 16;
    constexpr int k_ef_construction = 200;

    HNSWLibIndex idx(k_dim, k_max_elements, IndexFactory::MetricType::L2, k_m, k_ef_construction);
    long rss_base = read_status_kb("VmRSS");
    long hwm_base = read_status_kb("VmHWM");
    print_kb_line("插入前（空索引已创建）", rss_base, hwm_base);

    std::vector<float> vec(static_cast<size_t>(k_dim));
    for (int i = 0; i < k_dim; ++i) {
        vec[static_cast<size_t>(i)] = static_cast<float>((i * 13 + 7) % 1000) * 0.001f;
    }

    const auto t_insert_begin = std::chrono::steady_clock::now();
    for (int64_t i = 0; i < k_count; ++i) {
        idx.insert_vectors(vec, static_cast<uint64_t>(i));
    }
    const auto t_insert_end = std::chrono::steady_clock::now();

    long rss_after_insert = read_status_kb("VmRSS");
    long hwm_after_insert = read_status_kb("VmHWM");
    print_kb_line("插入100万条后", rss_after_insert, hwm_after_insert);

    std::vector<float> query(static_cast<size_t>(k_dim), 0.01f);
    idx.search_vectors(query, 1, nullptr, 50);

    long rss_after_search = read_status_kb("VmRSS");
    long hwm_after_search = read_status_kb("VmHWM");
    print_kb_line("检索 k=1 后（与线上一致走 search 路径）", rss_after_search, hwm_after_search);

    const auto insert_sec =
        std::chrono::duration<double>(t_insert_end - t_insert_begin).count();

    std::cout << "--- 增量（相对插入前 VmRSS）---\n";
    std::cout << "插入100万条后 VmRSS 增量: " << (rss_after_insert - rss_base) << " kB\n";
    std::cout << "检索后 VmRSS 增量: " << (rss_after_search - rss_base) << " kB\n";
    std::cout << "插入阶段耗时: " << insert_sec << " s\n";

    return 0;
}
