// VectorSimilarity 单元测试；在 tests 目录: make test_in_mem_similarity && ./test_in_mem_similarity

#include "../in_mem_vector_similarity.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Metric = VectorSimilarity::MetricType;

int g_failures = 0;

void check(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "FAIL " << file << ":" << line << " " << expr << "\n";
        ++g_failures;
    }
}

#define EXPECT_TRUE(cond) check((cond), #cond, __FILE__, __LINE__)

void expect_near(float actual, float expected, float eps, const char* msg) {
    if (std::fabs(actual - expected) > eps) {
        std::cerr << "FAIL " << msg << " actual=" << actual << " expected=" << expected << "\n";
        ++g_failures;
    }
}

void test_dot_product_similarity() {
    const std::vector<float> a{1.f, 0.f};
    const std::vector<float> b{2.f, 0.f};
    const std::vector<float> c{0.f, 1.f};
    expect_near(VectorSimilarity::compute_similarity(a, b, Metric::DOT_PRODUCT), 2.f, 1e-5f,
                "dot parallel");
    expect_near(VectorSimilarity::compute_similarity(a, c, Metric::DOT_PRODUCT), 0.f, 1e-5f,
                "dot orthogonal");
}

void test_cosine_similarity() {
    const std::vector<float> a{1.f, 0.f};
    const std::vector<float> b{3.f, 0.f};
    const std::vector<float> c{0.f, 1.f};
    expect_near(VectorSimilarity::compute_similarity(a, b, Metric::COSINE), 1.f, 1e-5f,
                "cosine same direction");
    expect_near(VectorSimilarity::compute_similarity(a, c, Metric::COSINE), 0.f, 1e-5f,
                "cosine orthogonal");
}

void test_euclidean_similarity() {
    const std::vector<float> a{0.f, 0.f};
    const std::vector<float> b{3.f, 4.f};
    expect_near(VectorSimilarity::compute_similarity(a, a, Metric::EUCLIDEAN), 0.f, 1e-5f,
                "euclidean identical");
    expect_near(VectorSimilarity::compute_similarity(a, b, Metric::EUCLIDEAN), -5.f, 1e-5f,
                "euclidean neg distance");
}

// k 等于向量个数时不会触发堆弹出，结果为按相似度升序排列
void test_find_top_k_all_vectors_dot() {
    const std::vector<float> target{1.f, 0.f};
    const std::vector<std::vector<float>> vectors{
        {1.f, 0.f},
        {0.f, 1.f},
        {2.f, 0.f},
    };
    const int k = static_cast<int>(vectors.size());
    auto top_k = VectorSimilarity::find_top_k_similar_vectors(target, vectors, k, Metric::DOT_PRODUCT);

    EXPECT_TRUE(static_cast<int>(top_k.size()) == k);
    EXPECT_TRUE(top_k[0].second == 1);
    EXPECT_TRUE(top_k[1].second == 0);
    EXPECT_TRUE(top_k[2].second == 2);
    expect_near(top_k[0].first, 0.f, 1e-5f, "rank0 score");
    expect_near(top_k[1].first, 1.f, 1e-5f, "rank1 score");
    expect_near(top_k[2].first, 2.f, 1e-5f, "rank2 score");
}

void test_find_top_k_cosine() {
    const std::vector<float> target{1.f, 0.f};
    const std::vector<std::vector<float>> vectors{
        {0.f, 1.f},
        {1.f, 0.f},
        {-1.f, 0.f},
    };
    const int k = static_cast<int>(vectors.size());
    auto top_k = VectorSimilarity::find_top_k_similar_vectors(target, vectors, k, Metric::COSINE);

    EXPECT_TRUE(static_cast<int>(top_k.size()) == k);
    EXPECT_TRUE(top_k[0].second == 2);
    EXPECT_TRUE(top_k[1].second == 0);
    EXPECT_TRUE(top_k[2].second == 1);
    expect_near(top_k[0].first, -1.f, 1e-5f, "cosine rank0");
    expect_near(top_k[1].first, 0.f, 1e-5f, "cosine rank1");
    expect_near(top_k[2].first, 1.f, 1e-5f, "cosine rank2");
}

}  // namespace

int main() {
    test_dot_product_similarity();
    test_cosine_similarity();
    test_euclidean_similarity();
    test_find_top_k_all_vectors_dot();
    test_find_top_k_cosine();

    if (g_failures == 0) {
        std::cout << "All VectorSimilarity tests passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << g_failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
