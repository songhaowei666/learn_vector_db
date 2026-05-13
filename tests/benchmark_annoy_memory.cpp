// Annoy 内存基准：通过项目内 AnnoyIndex（annoy_index.cpp）插入 100 万条，观察 VmRSS / VmHWM 变化
// 维度 128；插入后先采样内存，再调用 search_vectors 触发与线上一致的 rebuild_if_needed()

#include "../annoy_index.h"
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
    constexpr int64_t k_count = 1000000;
    constexpr int k_trees = 10;

    AnnoyIndex idx(k_dim, AnnoyIndex::MetricType::L2, k_trees);
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
    print_kb_line("插入100万条后（尚未 build）", rss_after_insert, hwm_after_insert);

    std::vector<float> query(static_cast<size_t>(k_dim), 0.01f);
    idx.search_vectors(query, 1, nullptr, -1);

    long rss_after_build = read_status_kb("VmRSS");
    long hwm_after_build = read_status_kb("VmHWM");
    print_kb_line("search 触发 build 完成后（可检索态）", rss_after_build, hwm_after_build);

    const auto insert_sec =
        std::chrono::duration<double>(t_insert_end - t_insert_begin).count();

    std::cout << "--- 增量（相对插入前 VmRSS）---\n";
    std::cout << "插入100万条后 VmRSS 增量: " << (rss_after_insert - rss_base) << " kB\n";
    std::cout << "build 完成后 VmRSS 增量: " << (rss_after_build - rss_base) << " kB\n";
    std::cout << "插入阶段耗时: " << insert_sec << " s\n";

    return 0;
}
