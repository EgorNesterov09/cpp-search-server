#pragma once
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>
 
using namespace std::string_literals;
 
template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };
 
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);
 
    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
 
        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex)
            , ref_to_value(bucket.map[key]) {
        }
    };
 
    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count), size_(0) {
    }
 
    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return {key, bucket};
    }
 
    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            size_ += map.size();
            result.insert(map.begin(), map.end());
        }
        return result;
    }
    
    size_t Size() {
        return size_;
    }
 
    void Erase(Key key) {
        auto index = key % buckets_.size();
        buckets_[index].map.erase(key);
    }
private:
    std::vector<Bucket> buckets_;
    size_t size_;
};