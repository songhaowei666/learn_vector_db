#ifndef IN_MEM_VECTOR_SIMILARITY_H
#define IN_MEM_VECTOR_SIMILARITY_H

#include <vector>
#include <queue>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <algorithm> 

class VectorSimilarity {
public:
    // 枚举以确定相似度度量类型
    enum class MetricType { COSINE, DOT_PRODUCT, EUCLIDEAN };

    // 计算两个向量之间的相似度
    static float compute_similarity(const std::vector<float>& v1, const std::vector<float>& v2, MetricType metric) {
        if (metric == MetricType::COSINE) {
            return cosine_similarity(v1, v2);
        } else if (metric == MetricType::DOT_PRODUCT) {
            return dot_product(v1, v2);
        } else if (metric == MetricType::EUCLIDEAN) {
            return -euclidean_distance(v1, v2); // 使用负值，因为优先队列是最大堆
        }
        throw std::invalid_argument("Unknown metric type");
    }

    // 寻找与目标向量最相似的k个向量
    static std::vector<std::pair<float, int>> find_top_k_similar_vectors(
        const std::vector<float>& target_vector,
        const std::vector<std::vector<float>>& vectors,
        int k,
        MetricType metric
    ) {
        std::priority_queue<std::pair<float, int>> pq;

        for (int i = 0; i < vectors.size(); ++i) {
            float similarity = compute_similarity(target_vector, vectors[i], metric);
            pq.push(std::make_pair(similarity, i));
            if (pq.size() > k) {
                pq.pop();
            }
        }

        std::vector<std::pair<float, int>> top_k;
        while (!pq.empty()) {
            top_k.push_back(pq.top());
            pq.pop();
        }
        std::reverse(top_k.begin(), top_k.end());
        return top_k;
    }

private:
    static float cosine_similarity(const std::vector<float>& v1, const std::vector<float>& v2) {
        float dot = 0.0, denom_a = 0.0, denom_b = 0.0;
        for (unsigned int i = 0; i < v1.size(); ++i) {
            dot += v1[i] * v2[i];
            denom_a += v1[i] * v1[i];
            denom_b += v2[i] * v2[i];
        }
        return dot / (sqrt(denom_a) * sqrt(denom_b));
    }

    static float dot_product(const std::vector<float>& v1, const std::vector<float>& v2) {
        float sum = 0.0;
        for (unsigned int i = 0; i < v1.size(); ++i) {
            sum += v1[i] * v2[i];
        }
        return sum;
    }

    static float euclidean_distance(const std::vector<float>& v1, const std::vector<float>& v2) {
        float sum = 0.0;
        for (unsigned int i = 0; i < v1.size(); ++i) {
            sum += (v1[i] - v2[i]) * (v1[i] - v2[i]);
        }
        return sqrt(sum);
    }
};

#endif // IN_MEM_VECTOR_SIMILARITY_H
