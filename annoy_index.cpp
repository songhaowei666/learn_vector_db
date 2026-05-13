#include "annoy_index.h"
#include "logger.h"
#include "third_party/annoy/src/annoylib.h"
#include "third_party/annoy/src/kissrandom.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C" bool roaring_bitmap_contains(const roaring_bitmap_t* r, uint32_t val);

namespace {
using AnnoyL2Index = Annoy::AnnoyIndex<
    int64_t,
    float,
    Annoy::Euclidean,
    Annoy::Kiss64Random,
    Annoy::AnnoyIndexSingleThreadedBuildPolicy>;
using AnnoyIPIndex = Annoy::AnnoyIndex<
    int64_t,
    float,
    Annoy::DotProduct,
    Annoy::Kiss64Random,
    Annoy::AnnoyIndexSingleThreadedBuildPolicy>;
}  // namespace

struct AnnoyIndex::Impl {
    int dim_;
    AnnoyIndex::MetricType metric_;
    int n_trees_;
    bool need_rebuild_;
    std::unordered_set<int64_t> deleted_ids_;
    std::unique_ptr<AnnoyL2Index> l2_index_;
    std::unique_ptr<AnnoyIPIndex> ip_index_;

    Impl(int dim, AnnoyIndex::MetricType metric, int n_trees)
        : dim_(dim), metric_(metric), n_trees_(n_trees), need_rebuild_(false) {
        if (dim_ <= 0) {
            throw std::invalid_argument("AnnoyIndex dim must be positive");
        }
        create_index_by_metric();
    }

    void create_index_by_metric() {
        if (metric_ == AnnoyIndex::MetricType::L2) {
            l2_index_.reset(new AnnoyL2Index(dim_));
            ip_index_.reset();
        } else {
            ip_index_.reset(new AnnoyIPIndex(dim_));
            l2_index_.reset();
        }
    }

    void rebuild_if_needed() {
        if (!need_rebuild_) {
            return;
        }
        char* error = nullptr;
        bool ok = false;
        if (metric_ == AnnoyIndex::MetricType::L2) {
            ok = l2_index_->build(n_trees_, -1, &error);
        } else {
            ok = ip_index_->build(n_trees_, -1, &error);
        }
        if (!ok) {
            throw std::runtime_error(error != nullptr ? error : "Annoy build failed");
        }
        need_rebuild_ = false;
    }

    void insert_vectors(const std::vector<float>& data, uint64_t label) {
        if (static_cast<int>(data.size()) != dim_) {
            throw std::invalid_argument("AnnoyIndex insert vector dim mismatch");
        }
        const int64_t id = static_cast<int64_t>(label);
        char* error = nullptr;
        bool ok = false;
        if (metric_ == AnnoyIndex::MetricType::L2) {
            ok = l2_index_->add_item(id, data.data(), &error);
        } else {
            ok = ip_index_->add_item(id, data.data(), &error);
        }
        if (!ok) {
            throw std::runtime_error(error != nullptr ? error : "Annoy add_item failed");
        }
        deleted_ids_.erase(id);
        need_rebuild_ = true;
    }

    void remove_vectors(const std::vector<long>& ids) {
        // Annoy 不支持原地删除，查询时按删除集合过滤
        for (long id : ids) {
            deleted_ids_.insert(static_cast<int64_t>(id));
        }
    }

    std::pair<std::vector<long>, std::vector<float>> search_vectors(
        const std::vector<float>& query,
        int k,
        const roaring_bitmap_t* bitmap,
        int search_k) {
        if (k <= 0) {
            return {{}, {}};
        }
        if (static_cast<int>(query.size()) != dim_) {
            throw std::invalid_argument("AnnoyIndex query vector dim mismatch");
        }
        rebuild_if_needed();

        const int candidate_k = std::max(k * 4, k);
        std::vector<int64_t> annoy_ids;
        std::vector<float> annoy_distances;
        if (metric_ == AnnoyIndex::MetricType::L2) {
            l2_index_->get_nns_by_vector(query.data(), static_cast<size_t>(candidate_k), search_k, &annoy_ids, &annoy_distances);
        } else {
            ip_index_->get_nns_by_vector(query.data(), static_cast<size_t>(candidate_k), search_k, &annoy_ids, &annoy_distances);
        }
        std::vector<long> ids;
        std::vector<float> distances;
        ids.reserve(static_cast<size_t>(k));
        distances.reserve(static_cast<size_t>(k));
        for (size_t i = 0; i < annoy_ids.size() && static_cast<int>(ids.size()) < k; ++i) {
            const int64_t id = annoy_ids[i];
            if (deleted_ids_.count(id) > 0) {
                continue;
            }
            if (bitmap != nullptr && !roaring_bitmap_contains(bitmap, static_cast<uint32_t>(id))) {
                continue;
            }
            ids.push_back(static_cast<long>(id));
            distances.push_back(annoy_distances[i]);
        }
        while (static_cast<int>(ids.size()) < k) {
            ids.push_back(-1);
            distances.push_back(std::numeric_limits<float>::infinity());
        }
        return {ids, distances};
    }

    void saveIndex(const std::string& file_path) {
        rebuild_if_needed();
        char* error = nullptr;
        bool ok = false;
        if (metric_ == AnnoyIndex::MetricType::L2) {
            ok = l2_index_->save(file_path.c_str(), false, &error);
        } else {
            ok = ip_index_->save(file_path.c_str(), false, &error);
        }
        if (!ok) {
            throw std::runtime_error(error != nullptr ? error : "Annoy save failed");
        }
    }

    void loadIndex(const std::string& file_path) {
        std::ifstream file(file_path);
        if (!file.good()) {
            GlobalLogger->warn("File not found: {}. Skipping loading index.", file_path);
            return;
        }
        file.close();

        create_index_by_metric();
        char* error = nullptr;
        bool ok = false;
        if (metric_ == AnnoyIndex::MetricType::L2) {
            ok = l2_index_->load(file_path.c_str(), false, &error);
        } else {
            ok = ip_index_->load(file_path.c_str(), false, &error);
        }
        if (!ok) {
            throw std::runtime_error(error != nullptr ? error : "Annoy load failed");
        }
        deleted_ids_.clear();
        need_rebuild_ = false;
    }
};

AnnoyIndex::AnnoyIndex(int dim, MetricType metric, int n_trees)
    : impl_(new Impl(dim, metric, n_trees)) {}

AnnoyIndex::~AnnoyIndex() = default;

AnnoyIndex::AnnoyIndex(AnnoyIndex&&) noexcept = default;
AnnoyIndex& AnnoyIndex::operator=(AnnoyIndex&&) noexcept = default;

void AnnoyIndex::insert_vectors(const std::vector<float>& data, uint64_t label) {
    impl_->insert_vectors(data, label);
}

void AnnoyIndex::remove_vectors(const std::vector<long>& ids) {
    impl_->remove_vectors(ids);
}

std::pair<std::vector<long>, std::vector<float>> AnnoyIndex::search_vectors(
    const std::vector<float>& query,
    int k,
    const roaring_bitmap_t* bitmap,
    int search_k) {
    return impl_->search_vectors(query, k, bitmap, search_k);
}

void AnnoyIndex::saveIndex(const std::string& file_path) {
    impl_->saveIndex(file_path);
}

void AnnoyIndex::loadIndex(const std::string& file_path) {
    impl_->loadIndex(file_path);
}
