#pragma once

#include <rocksdb/db.h>
#include <string>
#include <utility>
#include <vector>
#include <rapidjson/document.h> // 包含rapidjson头文件

// RocksDB 标量表一页扫描结果（key 为库内键，value 为 JSON 字符串）
struct ScalarKvPage {
    std::vector<std::pair<std::string, std::string>> items;
    bool has_more = false;
};

class ScalarStorage {
public:
    // 构造函数，打开RocksDB
    ScalarStorage(const std::string& db_path);

    // 析构函数，关闭RocksDB
    ~ScalarStorage();

    // 向量插入函数
    void insert_scalar(uint64_t id, const rapidjson::Document& data); // 将参数类型更改为rapidjson::Document

    // 根据ID查询向量函数
    rapidjson::Document get_scalar(uint64_t id); // 将返回类型更改为rapidjson::Document
    void put(const std::string& key, const std::string& value); // 添加 put 方法声明
    std::string get(const std::string& key); // 添加 get 方法声明

    // 在 [key_prefix, key_upper] 字典序范围内扫描（key_prefix 非空则只保留以前缀开头的 key；key_upper 空表示无上界）；page_num 从 1 起
    ScalarKvPage scan_page(const std::string& key_prefix, const std::string& key_upper, size_t page_num, size_t page_size);

private:
    // RocksDB实例
    rocksdb::DB* db_;
};