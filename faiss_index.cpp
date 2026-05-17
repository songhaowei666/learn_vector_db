#include "faiss_index.h"
#include "logger.h"
#include "constants.h"
#include <faiss/MetaIndexes.h> // FAISS 1.7.x：IndexIDMap 在此头文件中
#include <faiss/IndexFlat.h>
#include <faiss/index_io.h> // 更正头文件
#include <iostream>
#include <vector>
#include <fstream> // 包含 <fstream> 以使用 std::ifstream

extern "C" bool roaring_bitmap_contains(const roaring_bitmap_t* r, uint32_t val);


bool RoaringBitmapIDSelector::is_member(faiss::idx_t id) const {
    return roaring_bitmap_contains(bitmap_, static_cast<uint32_t>(id));
}

FaissIndex::FaissIndex(faiss::Index* index) : index(index) {}

void FaissIndex::insert_vectors(const std::vector<float>& data, uint64_t label) {
    long id = static_cast<long>(label);
    index->add_with_ids(1, data.data(), &id);
}

void FaissIndex::remove_vectors(const std::vector<long>& ids) {
    faiss::IndexIDMap* id_map = dynamic_cast<faiss::IndexIDMap*>(index);
    if (id_map) {
        faiss::IDSelectorBatch selector(ids.size(), ids.data());
        id_map->remove_ids(selector);
    } else {
        throw std::runtime_error("Underlying Faiss index is not an IndexIDMap");
    }
}

std::pair<std::vector<long>, std::vector<float>> FaissIndex::search_vectors(const std::vector<float>& query, int k, const roaring_bitmap_t* bitmap) {
    int dim = index->d;
    int num_queries = query.size() / dim;
    std::vector<long> indices(num_queries * k);
    std::vector<float> distances(num_queries * k);

    auto* labels = reinterpret_cast<faiss::idx_t*>(indices.data());
    if (bitmap == nullptr) {
        index->search(num_queries, query.data(), k, distances.data(), labels);
    } else {
        RoaringBitmapIDSelector selector(bitmap);
        faiss::SearchParameters params;
        params.sel = &selector;
        index->search(num_queries, query.data(), k, distances.data(), labels, &params);
    }

    GlobalLogger->debug("Retrieved values:");
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] != -1) {
            GlobalLogger->debug("ID: {}, Distance: {}", indices[i], distances[i]);
        } else {
            GlobalLogger->debug("No specific value found");
        }
    }
    return {indices, distances};
}

void FaissIndex::saveIndex(const std::string& file_path) { // 添加 saveIndex 方法实现
    faiss::write_index(index, file_path.c_str());
}

void FaissIndex::loadIndex(const std::string& file_path) { // 添加 loadIndex 方法实现
    std::ifstream file(file_path); // 尝试打开文件
    if (file.good()) { // 检查文件是否存在
        file.close();
        if (index != nullptr) {
            delete index;
        }
        index = faiss::read_index(file_path.c_str());
    } else {
        GlobalLogger->warn("File not found: {}. Skipping loading index.", file_path);
    }
}