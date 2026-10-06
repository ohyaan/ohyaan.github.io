#pragma once
#include "minimal_ecs.hpp"

namespace lesson::generational {
using Number = ecs::Number;
using Generation = std::uint64_t;
struct OwnerTag {};
template<Generation Limit> class World;

class Entity {
    Number slot_ = 0;
    Generation generation_ = 0;
    std::shared_ptr<const OwnerTag> owner_;
    Entity(Number slot, Generation generation, std::shared_ptr<const OwnerTag> owner)
        : slot_(slot), generation_(generation), owner_(std::move(owner)) {}
    template<Generation> friend class World;
public:
    Entity() = default;
    // Diagnostic fields; neither is a complete identity on its own.
    Number slot() const { return slot_; }
    Generation generation() const { return generation_; }
};

struct SlotCounts {
    std::size_t allocated;
    std::size_t live;
    std::size_t reusable;
    std::size_t retired;
};

template<Generation Limit = std::numeric_limits<Generation>::max()>
class World {
    static_assert(Limit >= 1, "at least one generation is required");
    static_assert(std::numeric_limits<std::size_t>::digits <=
                  std::numeric_limits<Number>::digits, "metadata indices must fit slot numbers");
    struct Slot {
        Generation generation = 1;
        bool alive = false;
        bool retired = false;
        Number next_free = 0;
    };
    std::shared_ptr<const OwnerTag> owner_ = std::make_shared<const OwnerTag>();
    std::vector<Slot> slots_;
    Number free_head_ = 0;
    std::vector<Number> order_;
    ecs::Positions positions_;
    ecs::Velocities velocities_;
    ecs::Lifetimes lifetimes_;
    ecs::Visible visible_;

    Slot& slot_at(Number number) { return slots_.at(static_cast<std::size_t>(number - 1)); }
    void erase_components(Number number) {
        velocities_.erase(number);
        lifetimes_.erase(number);
        visible_.erase(number);
        positions_.erase(number);
    }
    void release(Number number) {
        erase_components(number);
        order_.erase(std::find(order_.begin(), order_.end(), number));
        auto& slot = slot_at(number);
        slot.alive = false;
        if (slot.generation == Limit) {
            slot.retired = true; // Never wrap and resurrect an old identity.
        } else {
            ++slot.generation;
            slot.next_free = free_head_;
            free_head_ = number;
        }
    }
    Snapshot snapshot(Number number) const {
        const auto lifetime = lifetimes_.find(number);
        return {positions_.at(number).value, visible_.count(number) != 0,
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
        // Reserve before publishing any new entity or removing a free-list head.
        order_.reserve(order_.size() + 1);
        const bool reuse = free_head_ != 0;
        const auto number = reuse ? free_head_
            : identity::next_number(static_cast<Number>(slots_.size()));
        if (!reuse) slots_.push_back(Slot{});
        try {
            positions_.emplace(number, ecs::Position{seed.position});
            if (seed.moving) velocities_.emplace(number, ecs::Velocity{seed.velocity});
            if (seed.expiring) lifetimes_.emplace(number, ecs::Lifetime{seed.remaining});
            if (seed.visible) visible_.insert(number);
        } catch (...) {
            erase_components(number);
            if (!reuse) slots_.pop_back();
            throw;
        }
        auto& slot = slot_at(number);
        if (reuse) free_head_ = slot.next_free;
        slot.next_free = 0;
        slot.alive = true;
        order_.push_back(number); // Capacity was reserved; Number is trivial.
        return Entity(number, slot.generation, owner_);
    }
    bool contains(const Entity& entity) const {
        if (entity.owner_ != owner_ || entity.slot_ == 0 ||
            entity.slot_ > slots_.size()) return false;
        const auto& slot = slots_[static_cast<std::size_t>(entity.slot_ - 1)];
        return slot.alive && slot.generation == entity.generation_;
    }
    std::optional<Snapshot> inspect(const Entity& entity) const {
        if (!contains(entity)) return std::nullopt;
        return snapshot(entity.slot_);
    }
    bool destroy(const Entity& entity) {
        if (!contains(entity)) return false;
        release(entity.slot_);
        return true;
    }
    void step(double dt) {
        validate_dt(dt);
        std::vector<Number> expired;
        expired.reserve(lifetimes_.size());
        ecs::detail::movement_system(positions_, velocities_, dt);
        ecs::detail::lifetime_system(lifetimes_, dt, expired);
        for (const auto number : expired) release(number);
    }
    std::vector<Snapshot> states() const {
        std::vector<Snapshot> result;
        for (const auto number : order_) result.push_back(snapshot(number));
        return result;
    }
    std::vector<Vec2> render() const {
        std::vector<Vec2> result;
        for (const auto number : order_)
            if (visible_.count(number)) result.push_back(positions_.at(number).value);
        return result;
    }
    std::vector<Entity> entities() const {
        std::vector<Entity> result;
        for (const auto number : order_) {
            const auto& slot = slots_[static_cast<std::size_t>(number - 1)];
            result.push_back(Entity(number, slot.generation, owner_));
        }
        return result;
    }
    ecs::ComponentCounts component_counts() const {
        return {positions_.size(), velocities_.size(), lifetimes_.size(), visible_.size()};
    }
    SlotCounts slot_counts() const {
        SlotCounts counts{slots_.size(), 0, 0, 0};
        for (const auto& slot : slots_) {
            if (slot.alive) ++counts.live;
            else if (slot.retired) ++counts.retired;
            else ++counts.reusable;
        }
        return counts;
    }
    // A teaching hook for proving metadata relocation does not change identity.
    std::size_t slot_capacity() const { return slots_.capacity(); }
    void reserve_slots(std::size_t capacity) { slots_.reserve(capacity); }
};
} // namespace lesson::generational
