#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

typedef struct roaring_bitmap_s roaring_bitmap_t;

// Annoy 向量索引封装（实现见 annoy_index.cpp）
class AnnoyIndex {
public:
    enum class MetricType {
        L2,
        IP
    };

    explicit AnnoyIndex(int dim, MetricType metric, int n_trees = 10);
    ~AnnoyIndex();

    AnnoyIndex(AnnoyIndex&&) noexcept;
    AnnoyIndex& operator=(AnnoyIndex&&) noexcept;
    AnnoyIndex(const AnnoyIndex&) = delete;
    AnnoyIndex& operator=(const AnnoyIndex&) = delete;

    void insert_vectors(const std::vector<float>& data, uint64_t label);
    void remove_vectors(const std::vector<long>& ids);
    std::pair<std::vector<long>, std::vector<float>> search_vectors(
        const std::vector<float>& query,
        int k,
        const roaring_bitmap_t* bitmap = nullptr,
        int search_k = -1);
    void saveIndex(const std::string& file_path);
    void loadIndex(const std::string& file_path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
