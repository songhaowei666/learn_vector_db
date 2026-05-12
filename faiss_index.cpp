#include "faiss_index.h"
#include "logger.h"
#include "constants.h"
#include <faiss/MetaIndexes.h> // FAISS 1.7.x：IndexIDMap 在此头文件中
#include <faiss/IndexFlat.h>
#include <faiss/index_io.h> // 更正头文件
#include <iostream>
#include <vector>
#include <fstream> // 包含 <fstream> 以使用 std::ifstream
#include <limits>

extern "C" bool roaring_bitmap_contains(const roaring_bitmap_t* r, uint32_t val);


// bool RoaringBitmapIDSelector::is_member(int64_t id) const {
//     return roaring_bitmap_contains(bitmap_, static_cast<uint32_t>(id));
// }

bool RoaringBitmapIDSelector::is_member(int64_t id) const{
    bool is_member = roaring_bitmap_contains(bitmap_, static_cast<uint32_t>(id)); // 获取 is_member 结果
    GlobalLogger->debug("RoaringBitmapIDSelector::is_member id: {}, is_member: {}", id, is_member); // 打印 id 和 is_member 结果

    return is_member;
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

    // FAISS 1.7.x 无 SearchParameters::sel，带 bitmap 时通过扩大 k 再按位图过滤实现
    if (bitmap == nullptr) {
        index->search(num_queries, query.data(), k, distances.data(), indices.data());
    } else {
        faiss::Index::idx_t ntotal = index->ntotal;
        if (ntotal == 0) {
            std::fill(indices.begin(), indices.end(), -1);
            std::fill(distances.begin(), distances.end(), std::numeric_limits<float>::infinity());
        } else {
            std::vector<float> cand_dist;
            std::vector<faiss::Index::idx_t> cand_lab;
            for (int qi = 0; qi < num_queries; ++qi) {
                const float* q = query.data() + qi * static_cast<size_t>(dim);
                faiss::Index::idx_t k_fetch = std::min(ntotal, static_cast<faiss::Index::idx_t>(k));
                int found = 0;
                while (found < k) {
                    cand_dist.resize(static_cast<size_t>(k_fetch));
                    cand_lab.resize(static_cast<size_t>(k_fetch));
                    index->search(1, q, k_fetch, cand_dist.data(), cand_lab.data());
                    for (size_t j = 0; j < cand_lab.size() && found < k; ++j) {
                        if (cand_lab[j] < 0) {
                            break;
                        }
                        if (roaring_bitmap_contains(bitmap, static_cast<uint32_t>(cand_lab[j]))) {
                            indices[static_cast<size_t>(qi) * k + static_cast<size_t>(found)] =
                                    static_cast<long>(cand_lab[j]);
                            distances[static_cast<size_t>(qi) * k + static_cast<size_t>(found)] = cand_dist[j];
                            ++found;
                        }
                    }
                    if (found >= k) {
                        break;
                    }
                    if (k_fetch >= ntotal) {
                        break;
                    }
                    faiss::Index::idx_t k_next = std::min(ntotal, k_fetch * 2);
                    if (k_next <= k_fetch) {
                        k_next = ntotal;
                    }
                    k_fetch = k_next;
                }
                for (; found < k; ++found) {
                    indices[static_cast<size_t>(qi) * k + static_cast<size_t>(found)] = -1;
                    distances[static_cast<size_t>(qi) * k + static_cast<size_t>(found)] =
                            std::numeric_limits<float>::infinity();
                }
            }
        }
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