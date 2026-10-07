#pragma once
#include "model.hpp"
#include <cstdint>
#include <limits>

namespace course {
class Issuer;
class Handle {
    struct Marker {};
    std::uint64_t number_ = 0;
    std::shared_ptr<const Marker> owner_;
    Handle(std::uint64_t number, std::shared_ptr<const Marker> owner)
        : number_(number), owner_(std::move(owner)) {}
    friend class Issuer;
public:
    Handle() = default;
    std::uint64_t number() const { return number_; }
};
inline std::uint64_t successor(std::uint64_t value) {
    if (value == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("ID space exhausted");
    return value + 1;
}
class Issuer {
    std::shared_ptr<const Handle::Marker> owner_ = std::make_shared<const Handle::Marker>();
    std::uint64_t last_ = 0;
public:
    Issuer() = default;
    Issuer(const Issuer&) = delete;
    Issuer& operator=(const Issuer&) = delete;
    Issuer(Issuer&&) = delete;
    Issuer& operator=(Issuer&&) = delete;
    Handle issue() { last_ = successor(last_); return Handle(last_, owner_); }
    bool owns(const Handle& handle) const { return handle.number_ != 0 && handle.owner_ == owner_; }
};
class IdentityWorld {
    struct Entry { std::uint64_t number; Seed body; };
    Issuer ids_;
    std::vector<Entry> entries_;
public:
    IdentityWorld() = default;
    explicit IdentityWorld(const std::vector<Seed>& seeds) { for (const auto& body : seeds) spawn(body); }
    Handle spawn(Seed body) {
        validate_seed(body);
        if (!live(body)) return {};
        const auto handle = ids_.issue();
        entries_.push_back({handle.number(), body});
        return handle;
    }
    bool contains(const Handle& handle) const {
        return ids_.owns(handle) && std::any_of(entries_.begin(), entries_.end(),
            [&](const auto& entry) { return entry.number == handle.number(); });
    }
    std::optional<Seed> inspect(const Handle& handle) const {
        if (!ids_.owns(handle)) return std::nullopt;
        for (const auto& entry : entries_)
            if (entry.number == handle.number()) return observed(entry.body);
        return std::nullopt;
    }
    bool destroy(const Handle& handle) {
        if (!ids_.owns(handle)) return false;
        const auto it = std::find_if(entries_.begin(), entries_.end(),
            [&](const auto& entry) { return entry.number == handle.number(); });
        if (it == entries_.end()) return false;
        entries_.erase(it); return true;
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& entry : entries_) advance(entry.body, dt);
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
            [](const auto& entry) { return !live(entry.body); }), entries_.end());
    }
    std::vector<Seed> states() const {
        std::vector<Seed> result;
        for (const auto& entry : entries_) result.push_back(observed(entry.body));
        return result;
    }
};
} // namespace course
