// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written, separately chained hash table. No associative containers.

#include "tools/tool_registry.h"

#include <cstdint>
#include <string>
#include <utility>

namespace {

constexpr std::size_t kInitialBucketCount = 16;

}  // namespace

ToolRegistry::ToolRegistry() : buckets_(kInitialBucketCount) {}
ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    // Use the FNV 1a offset and prime to hash each byte in order.
    std::uint64_t value = 14695981039346656037ULL;
    for (unsigned char c : key) {
        value ^= c;
        value *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(value);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;

    const std::string name = tool->name();
    if (find(name) != nullptr) return false;

    // Grow before the new entry would take the load factor above 0.75.
    if ((size_ + 1) * 4 > buckets_.size() * 3) {
        rehash(buckets_.size() * 2);
    }

    const std::size_t index = hash(name) % buckets_.size();
    buckets_[index].push_back(std::move(tool));
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const std::size_t index = hash(name) % buckets_.size();
    for (const auto& tool : buckets_[index]) {
        if (tool->name() == name) return tool.get();
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return size_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const Bucket& bucket : buckets_) {
        for (const auto& tool : bucket) fn(*tool);
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const Bucket& bucket : buckets_) {
        if (bucket.size() > longest) longest = bucket.size();
    }
    return longest;
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<Bucket> new_buckets(new_bucket_count);
    for (Bucket& bucket : buckets_) {
        for (auto& tool : bucket) {
            const std::size_t index = hash(tool->name()) % new_bucket_count;
            // Only the unique_ptr moves. The Tool stays at the same address.
            new_buckets[index].push_back(std::move(tool));
        }
    }
    buckets_.swap(new_buckets);
}
