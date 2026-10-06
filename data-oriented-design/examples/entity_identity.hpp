#pragma once
#include "simulation.hpp"
#include <cstdint>
#include <limits>
#include <utility>

namespace lesson::identity {
struct OwnerTag {};
class World;

class Entity {
    std::uint64_t number_ = 0;
    std::shared_ptr<const OwnerTag> owner_;
    Entity(std::uint64_t number, std::shared_ptr<const OwnerTag> owner)
        : number_(number), owner_(std::move(owner)) {}
    friend class World;
public:
    Entity() = default;
    // Diagnostic number, not a globally unique or serialisable identity.
    std::uint64_t number() const { return number_; }
};

inline std::uint64_t next_number(std::uint64_t last) {
    if (last == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("entity numbers exhausted");
    return last + 1;
}

class World {
    struct Entry {
        std::uint64_t number;
        Seed body;
    };
    std::shared_ptr<const OwnerTag> owner_ = std::make_shared<const OwnerTag>();
    std::uint64_t last_number_ = 0;
    std::vector<Entry> entries_;

    bool owns(const Entity& entity) const {
        return entity.number_ != 0 && entity.owner_ == owner_;
    }
    std::vector<Entry>::const_iterator find(const Entity& entity) const {
        if (!owns(entity)) return entries_.end();
        return std::find_if(entries_.begin(), entries_.end(),
            [&](const Entry& entry) { return entry.number == entity.number_; });
    }
public:
    World() = default;
    explicit World(const std::vector<Seed>& seeds) {
        for (const auto& seed : seeds) spawn(seed);
    }
    // Copying or transferring a namespace needs a policy, not a default copy.
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    Entity spawn(Seed seed) {
        if (!initially_live(seed)) return {};
        const auto number = next_number(last_number_);
        entries_.push_back({number, seed});
        last_number_ = number;
        return Entity(number, owner_);
    }
    bool contains(const Entity& entity) const {
        return find(entity) != entries_.end();
    }
    std::optional<Snapshot> inspect(const Entity& entity) const {
        const auto it = find(entity);
        if (it == entries_.end()) return std::nullopt;
        const auto& body = it->body;
        return Snapshot{body.position, body.visible, body.expiring,
                        body.expiring ? body.remaining : 0};
    }
    bool destroy(const Entity& entity) {
        if (!owns(entity)) return false;
        const auto it = std::find_if(entries_.begin(), entries_.end(),
            [&](const Entry& entry) { return entry.number == entity.number_; });
        if (it == entries_.end()) return false;
        entries_.erase(it);
        return true;
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& entry : entries_) {
            auto& body = entry.body;
            if (body.moving) move(body.position, body.velocity, dt);
            if (body.expiring) body.remaining -= dt;
        }
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
            [](const Entry& entry) {
                return entry.body.expiring && entry.body.remaining <= 0;
            }), entries_.end());
    }
    std::vector<Snapshot> states() const {
        std::vector<Snapshot> result;
        for (const auto& entry : entries_) {
            const auto& body = entry.body;
            result.push_back({body.position, body.visible, body.expiring,
                              body.expiring ? body.remaining : 0});
        }
        return result;
    }
    std::vector<Entity> entities() const {
        std::vector<Entity> result;
        for (const auto& entry : entries_)
            result.push_back(Entity(entry.number, owner_));
        return result;
    }
    // Capacity is exposed only to make storage relocation experiments explicit.
    std::size_t storage_capacity() const { return entries_.capacity(); }
    void reserve(std::size_t count) { entries_.reserve(count); }
};
} // namespace lesson::identity
