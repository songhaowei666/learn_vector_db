// HNSW 索引 HTTP 压测：与 benchmark.cpp 相同，仅修改下方 kIndexType
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <random>
#include <map>
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <utility>
#include <spdlog/spdlog.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std::chrono;
using namespace spdlog;

// 指定索引类型（服务端 constants.h 中 INDEX_TYPE_HNSW）
static const char kIndexType[] = "HNSW";

enum class TestType {
    Write,
    Read,
    Both
};

std::map<std::string, std::string> readConfig(const std::string& filename) {
    std::map<std::string, std::string> config;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                config[key] = value;
                spdlog::info("Key: {} Value: {}", key, value); // 使用 spdlog 打印日志
            }
        }
    }

    return config;
}

Document createDocumentWithVector(const std::vector<double>& values) {
    Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();  // 获取分配器

    Value vector(kArrayType);
    for (double val : values) {
        vector.PushBack(Value().SetDouble(val), allocator);
    }

    doc.AddMember("vectors", vector, allocator);
    return doc;
}


std::pair<std::vector<Document>, std::vector<Document>> generateTestDataBoth(int numberOfRequests, int dim) {
    std::vector<Document> writeData;
    std::vector<Document> readData;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    for (int i = 0; i < numberOfRequests; ++i) {
        std::vector<double> readVectorValues;
        for (int j = 0; j < dim; ++j) {
            readVectorValues.push_back(dis(gen));
        }

        Document readDoc = createDocumentWithVector(readVectorValues);
        readDoc.AddMember("id", i, readDoc.GetAllocator());
        readDoc.AddMember("k", 5, readDoc.GetAllocator());
        readDoc.AddMember("indexType", kIndexType, readDoc.GetAllocator());
        readData.push_back(std::move(readDoc));

        for (int writeCount = 0; writeCount < 5; ++writeCount) {
            std::vector<double> writeVectorValues;
            for (double val : readVectorValues) {
                double offset = dis(gen) * 0.002 - 0.001;
                writeVectorValues.push_back(val + offset);
            }

            Document writeDoc = createDocumentWithVector(writeVectorValues);
            writeDoc.AddMember("id", i * 10000 + writeCount, writeDoc.GetAllocator());
            writeDoc.AddMember("indexType", kIndexType, writeDoc.GetAllocator());
            writeData.push_back(std::move(writeDoc));
        }
    }

    return std::make_pair(std::move(writeData), std::move(readData));
}

std::vector<Document> generateTestData(int numberOfRequests, int dim, TestType testType) {
    std::vector<Document> testData;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    for (int i = 0; i < numberOfRequests; ++i) {
        Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        Value vectors(kArrayType);
        for (int j = 0; j < dim; ++j) {
            vectors.PushBack(Value(dis(gen)).Move(), allocator);
        }

        if (testType == TestType::Write) {
            doc.AddMember("id", i, allocator);
            doc.AddMember("vectors", vectors, allocator);
            doc.AddMember("indexType", kIndexType, allocator);
       }

        if (testType == TestType::Read) {
            doc.AddMember("vectors", vectors, allocator);
            doc.AddMember("k", 5, allocator);
            doc.AddMember("indexType", kIndexType, allocator);
        }

        testData.push_back(std::move(doc));
    }

    return testData;
}

size_t callbackFunction(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

std::chrono::duration<double> performOperation(const Document& doc, const char* url, rapidjson::Document* responseDoc = nullptr) {
    CURL* curl = curl_easy_init();
    std::chrono::duration<double> duration(0);

    if (curl) {
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);
        std::string jsonStr = buffer.GetString();

        // 打印请求的 JSON 内容
        spdlog::info("Sending request to {}: {}", url, jsonStr);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer.GetString());

        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callbackFunction);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        auto start = std::chrono::high_resolution_clock::now();
        CURLcode res = curl_easy_perform(curl);
        auto end = std::chrono::high_resolution_clock::now();

        if (res != CURLE_OK) {
            spdlog::error("curl_easy_perform() failed: {}", curl_easy_strerror(res));
        } else if (responseDoc) {
            // 解析响应内容到 JSON 对象，如果提供了 responseDoc
            responseDoc->Parse(response.c_str());
            if (responseDoc->HasParseError()) {
                spdlog::error("Failed to parse response: {}", response);
            }
        }

        duration = end - start;

        curl_easy_cleanup(curl);
    }

    return duration;
}



void executeBenchmark(const std::vector<Document>& testData, int numThreads, TestType testType, const std::string& writeUrl, const std::string& readUrl, bool checkReadResponse = false) {
    
    std::string testTypeStr;
    switch (testType) {
        case TestType::Write:
            testTypeStr = "Write";
            break;
        case TestType::Read:
            testTypeStr = "Read";
            break;
        case TestType::Both:
            testTypeStr = "Both";
            break;
        default:
            testTypeStr = "非法枚举值";
            break;
    }
    spdlog::info("Executing benchmark with the following parameters:");
    spdlog::info("Test Type: {}", testTypeStr);
    spdlog::info("Number of Threads: {}", numThreads);
    spdlog::info("Number of Vectors: {}", testData.size());
    spdlog::info("Write URL: {}", writeUrl);
    spdlog::info("Read URL: {}", readUrl);

    std::vector<std::thread> threads;
    std::mutex mutex;
    std::vector<std::chrono::duration<double>> durations;
    std::vector<double> recallRates(numThreads, 0.0); // 每个线程的召回率

    int blockSize = testData.size() / numThreads;
    int remainder = testData.size() % numThreads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            // 计算每个线程的工作块
            int start = i * blockSize + std::min(i, remainder);
            int end = start + blockSize + (i < remainder ? 1 : 0);

            int mismatchCount = 0;
            int totalCount = 0;

            for (int j = start; j < end; ++j) {
                const Document& doc = testData[j];

                std::chrono::duration<double> duration;
                if (testType == TestType::Write) {
                    duration = performOperation(doc, writeUrl.c_str());
                }

                if (testType == TestType::Read) {
                    if (checkReadResponse) {
                        rapidjson::Document responseDoc;
                        duration = performOperation(doc, readUrl.c_str(), &responseDoc);
                        if (!responseDoc.HasParseError() && responseDoc.IsObject()) {
                            const auto& vectors = responseDoc["vectors"].GetArray();
                            int requestId = doc["id"].GetInt();

                            for (const auto& v : vectors) {
                                int returnedId = v.GetInt() / 10000;
                                if (returnedId != requestId) {
                                    mismatchCount++;
                                }
                                totalCount++;
                            }
                        }
                        
                        double recallRate = totalCount > 0 ? 1.0 - static_cast<double>(mismatchCount) / totalCount : 0.0;
                        recallRates[i] = recallRate;

                    } else {
                        duration = performOperation(doc, readUrl.c_str());
                    }
            }

                std::lock_guard<std::mutex> lock(mutex);
                durations.push_back(duration);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    double totalDuration = std::accumulate(durations.begin(), durations.end(), 0.0,
                                           [](double sum, const std::chrono::duration<double>& dur) { 
                                               return sum + dur.count(); 
                                           }) * 1000.0; // 转换为毫秒
    double averageDuration = totalDuration / durations.size();
    std::sort(durations.begin(), durations.end());
    size_t p99Index = std::max(0, std::min(static_cast<int>(durations.size() * 99 / 100) - 1, static_cast<int>(durations.size()) - 1));
    double p99Duration = durations[p99Index].count() * 1000.0; // 转换为毫秒
    double throughput = durations.size() / (totalDuration / 1000.0); // 总耗时仍以秒为单位计算吞吐量

    spdlog::info("Average duration: {} milliseconds", averageDuration);
    spdlog::info("P99 duration: {} milliseconds", p99Duration);
    spdlog::info("Throughput: {} operations per second", throughput);

    if (checkReadResponse) {
        double overallRecallRate = std::accumulate(recallRates.begin(), recallRates.end(), 0.0) / numThreads;
        spdlog::info("Overall recall rate: {}", overallRecallRate);
    }
}

int main(int argc, char* argv[]) {
    // argv[0] 是可执行程序的名字    std::cerr 标准错误流
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>  (indexType=" << kIndexType << ")\n";
        return 1;
    }

    std::string configFile = argv[1];
    auto config = readConfig(configFile);

    try {
        int testTypeInt = std::stoi(config["test_type"]);
        if (testTypeInt < 0 || testTypeInt > 2) {
            std::cerr << "test_type 须为 0(Write)、1(Read)、2(Both)，当前: " << testTypeInt << std::endl;
            return 1;
        }
        TestType testType = static_cast<TestType>(testTypeInt);
        int numThreads = std::stoi(config["num_threads"]);
        int numVectors = std::stoi(config["num_vectors"]);
        int dim = std::stoi(config["dim"]); // 读取向量的维度

        std::string writeUrl = config["write_url"];
        std::string readUrl = config["read_url"];

        spdlog::info("Starting HNSW benchmark test (indexType={})", kIndexType);

        if (testType == TestType::Both) {
            // Generate both write and read test data
            auto testDataPair = generateTestDataBoth(numVectors, dim);
            executeBenchmark(testDataPair.first, numThreads, TestType::Write, writeUrl, readUrl);
            executeBenchmark(testDataPair.second, numThreads, TestType::Read, writeUrl, readUrl, true);
        } else {
            // Generate test data based on the specified test type
            std::vector<Document> testData = generateTestData(numVectors, dim, testType);
            executeBenchmark(testData, numThreads, testType, writeUrl, readUrl);
        }

        spdlog::info("Benchmark test completed");
        return 0;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return 1;
    }
}

