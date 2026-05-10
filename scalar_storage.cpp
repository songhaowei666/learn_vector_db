#include "scalar_storage.h"
#include "logger.h"
#include <rocksdb/db.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h> // 包含rapidjson/stringbuffer.h头文件
#include <rapidjson/writer.h>
#include <memory>
#include <vector>

ScalarStorage::ScalarStorage(const std::string& db_path) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_);
    if (!status.ok()) {
        GlobalLogger->error("Failed to open RocksDB: {}", status.ToString());
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }
}

ScalarStorage::~ScalarStorage() {
    delete db_;
}

void ScalarStorage::insert_scalar(uint64_t id, const rapidjson::Document& data) { // 将参数类型更改为rapidjson::Document
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    data.Accept(writer);
    std::string value = buffer.GetString();

    rocksdb::Status status = db_->Put(rocksdb::WriteOptions(), std::to_string(id), value);
    if (!status.ok()) {
        GlobalLogger->error("Failed to insert scalar: {}", status.ToString()); // 使用GlobalLogger打印错误日志
    }
}

rapidjson::Document ScalarStorage::get_scalar(uint64_t id) { // 将返回类型更改为rapidjson::Document
    std::string value;
    rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), std::to_string(id), &value);
    if (!status.ok()) {
        return rapidjson::Document(); // 返回一个空的rapidjson::Document对象
    }

    rapidjson::Document data;
    data.Parse(value.c_str());

    // 打印从ScalarStorage获取的数据和rocksdb::Status status
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    data.Accept(writer);
    GlobalLogger->debug("Data retrieved from ScalarStorage: {}, RocksDB status: {}", buffer.GetString(), status.ToString()); // 添加rocksdb::Status status

    return data;
}

void ScalarStorage::put(const std::string& key, const std::string& value) {
    rocksdb::Status status = db_->Put(rocksdb::WriteOptions(), key, value);
    if (!status.ok()) {
        GlobalLogger->error("Failed to put key-value pair: {}", status.ToString());
    }
}

std::string ScalarStorage::get(const std::string& key) {
    std::string value;
    rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);
    if (!status.ok()) {
        //GlobalLogger->error("Failed to get value for key {}: {}", key, status.ToString());
        return "";
    }
    return value;
}

ScalarKvPage ScalarStorage::scan_page(const std::string& key_prefix, const std::string& key_upper, size_t page_num, size_t page_size) {
    ScalarKvPage page;
    if (page_size == 0 || page_num == 0) {
        return page;
    }

    auto key_in_range = [&](const std::string& k) -> bool {
        if (!key_prefix.empty()) {
            if (k.size() < key_prefix.size() || k.compare(0, key_prefix.size(), key_prefix) != 0) {
                return false;
            }
        }
        if (!key_upper.empty() && k > key_upper) {
            return false;
        }
        return true;
    };

    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));
    if (!key_prefix.empty()) {
        it->Seek(key_prefix);
    } else {
        it->SeekToFirst();
    }

    size_t skip = (page_num - 1) * page_size;
    while (it->Valid()) {
        std::string k = it->key().ToString();
        if (!key_in_range(k)) {
            break;
        }
        if (skip > 0) {
            --skip;
            it->Next();
            continue;
        }
        if (page.items.size() < page_size) {
            page.items.emplace_back(std::move(k), it->value().ToString());
            it->Next();
            continue;
        }
        page.has_more = true;
        break;
    }

    if (page.items.size() == page_size && !page.has_more && it->Valid()) {
        std::string k = it->key().ToString();
        page.has_more = key_in_range(k);
    }

    return page;
}