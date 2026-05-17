#pragma once

#include <faiss/Index.h>
#include <faiss/utils/utils.h>
#include <faiss/impl/IDSelector.h>
#include <vector>

typedef struct roaring_bitmap_s roaring_bitmap_t;

// 定义 RoaringBitmapIDSelector 结构体
struct RoaringBitmapIDSelector : faiss::IDSelector {
    RoaringBitmapIDSelector(const roaring_bitmap_t* bitmap) : bitmap_(bitmap) {}

    bool is_member(faiss::idx_t id) const final;

    ~RoaringBitmapIDSelector() override {}

    const roaring_bitmap_t* bitmap_;
};

class FaissIndex {
public:
    FaissIndex(faiss::Index* index);
    void insert_vectors(const std::vector<float>& data, uint64_t label);
    void remove_vectors(const std::vector<long>& ids);
    std::pair<std::vector<long>, std::vector<float>> search_vectors(const std::vector<float>& query, int k, const roaring_bitmap_t* bitmap = nullptr);
    void saveIndex(const std::string& file_path); // 添加 saveIndex 方法声明
    void loadIndex(const std::string& file_path); // 将返回类型更改为 faiss::Index*

private:
    faiss::Index* index;
};