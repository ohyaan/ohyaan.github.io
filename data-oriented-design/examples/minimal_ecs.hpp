#pragma once
#include "entity_identity.hpp"
#include <map>
#include <set>

namespace lesson::ecs {
using Number = std::uint64_t;
struct Position { Vec2 value; };
struct Velocity { Vec2 value; };
struct Lifetime { double remaining; };
// Visibility is a tag: membership alone carries its meaning.
using Positions = std::map<Number, Position>;
using Velocities = std::map<Number, Velocity>;
using Lifetimes = std::map<Number, Lifetime>;
using Visible = std::set<Number>;

struct OwnerTag {};
class World;
class Entity {
    Number number_ = 0;
    std::shared_ptr<const OwnerTag> owner_;
    Entity(Number number, std::shared_ptr<const OwnerTag> owner)
        : number_(number), owner_(std::move(owner)) {}
    friend class World;
public:
    Entity() = default;
    Number number() const { return number_; }
};

namespace detail {
// World validates dt and reserves the removal list before running systems.
inline void movement_system(Positions& positions, const Velocities& velocities,
                            double dt) {
    for (const auto& [number, velocity] : velocities) {
        const auto position = positions.find(number);
        if (position != positions.end())
            move(position->second.value, velocity.value, dt);
    }
}
inline void lifetime_system(Lifetimes& lifetimes, double dt,
                            std::vector<Number>& expired) {
    for (auto& [number, lifetime] : lifetimes) {
        lifetime.remaining -= dt;
        if (lifetime.remaining <= 0) expired.push_back(number);
    }
}
inline std::vector<Vec2> render_system(const Positions& positions,
                                       const Visible& visible) {
    std::vector<Vec2> result;
    for (const auto number : visible) {
        const auto position = positions.find(number);
        if (position != positions.end()) result.push_back(position->second.value);
    }
    return result;
}
} // namespace detail

struct ComponentCounts {
    std::size_t positions;
    std::size_t velocities;
    std::size_t lifetimes;
    std::size_t visible;
};

class World {
    std::shared_ptr<const OwnerTag> owner_ = std::make_shared<const OwnerTag>();
    Number last_number_ = 0;
    Positions positions_;
    Velocities velocities_;
    Lifetimes lifetimes_;
    Visible visible_;

    bool owns(const Entity& entity) const {
        return entity.number_ != 0 && entity.owner_ == owner_;
    }
    void erase_components(Number number) {
        velocities_.erase(number);
        lifetimes_.erase(number);
        visible_.erase(number);
        positions_.erase(number);
    }
    Snapshot snapshot(Number number, const Position& position) const {
        const auto lifetime = lifetimes_.find(number);
        return {position.value, visible_.count(number) != 0,
                lifetime != lifetimes_.end(),
                lifetime != lifetimes_.end() ? lifetime->second.remaining : 0};
    }
public:
    World() = default;
    explicit World(const std::vector<Seed>& seeds) {
        for (const auto& seed : seeds) spawn(seed);
    }
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    Entity spawn(const Seed& seed) {
        if (!initially_live(seed)) return {};
        const auto number = identity::next_number(last_number_);
        // Roll back partial insertion if a component allocation fails.
        try {
            positions_.emplace(number, Position{seed.position});
            if (seed.moving) velocities_.emplace(number, Velocity{seed.velocity});
            if (seed.expiring) lifetimes_.emplace(number, Lifetime{seed.remaining});
            if (seed.visible) visible_.insert(number);
        } catch (...) {
            erase_components(number);
            throw;
        }
        last_number_ = number;
        return Entity(number, owner_);
    }
    bool contains(const Entity& entity) const {
        return owns(entity) && positions_.count(entity.number_) != 0;
    }
    std::optional<Snapshot> inspect(const Entity& entity) const {
        if (!owns(entity)) return std::nullopt;
        const auto position = positions_.find(entity.number_);
        if (position == positions_.end()) return std::nullopt;
        return snapshot(entity.number_, position->second);
    }
    bool destroy(const Entity& entity) {
        if (!contains(entity)) return false;
        erase_components(entity.number_);
        return true;
    }
    void step(double dt) {
        validate_dt(dt);
        std::vector<Number> expired;
        expired.reserve(lifetimes_.size());
        detail::movement_system(positions_, velocities_, dt);
        detail::lifetime_system(lifetimes_, dt, expired);
        // Never erase from a store while its system is traversing it.
        for (const auto number : expired) erase_components(number);
    }
    std::vector<Snapshot> states() const {
        std::vector<Snapshot> result;
        for (const auto& [number, position] : positions_)
            result.push_back(snapshot(number, position));
        return result;
    }
    std::vector<Vec2> render() const {
        return detail::render_system(positions_, visible_);
    }
    std::vector<Entity> entities() const {
        std::vector<Entity> result;
        for (const auto& [number, position] : positions_) {
            (void)position;
            result.push_back(Entity(number, owner_));
        }
        return result;
    }
    // Counts expose storage membership without borrowing mutable components.
    ComponentCounts component_counts() const {
        return {positions_.size(), velocities_.size(), lifetimes_.size(),
                visible_.size()};
    }
};
} // namespace lesson::ecs
