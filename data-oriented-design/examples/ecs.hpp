#pragma once
#include "identity.hpp"
#include <map>
#include <set>

namespace course {
using Number = std::uint64_t;
enum class Stage { acceleration, movement, lifetime, removed };
inline const char* stage_name(Stage stage) {
    switch (stage) {
    case Stage::acceleration: return "acceleration";
    case Stage::movement: return "movement";
    case Stage::lifetime: return "lifetime";
    case Stage::removed: return "removed";
    }
    return "unknown";
}
struct Trace { Number number; Stage stage; Vec2 position; Vec2 velocity; };
struct Counts { std::size_t positions, velocities, accelerations, lifetimes, visible; };
class EcsWorld {
    Issuer ids_;
    std::map<Number, Vec2> positions_, velocities_, accelerations_;
    std::map<Number, double> lifetimes_;
    std::set<Number> visible_;
    void erase_all(Number number) {
        positions_.erase(number); velocities_.erase(number); accelerations_.erase(number);
        lifetimes_.erase(number); visible_.erase(number);
    }
    Seed snapshot(Number number) const {
        const auto velocity = velocities_.find(number), acceleration = accelerations_.find(number);
        const auto lifetime = lifetimes_.find(number);
        return seed({positions_.at(number), velocity == velocities_.end() ? Vec2{} : velocity->second,
                     velocity != velocities_.end(), visible_.count(number) != 0,
                     lifetime != lifetimes_.end(), lifetime == lifetimes_.end() ? 0 : lifetime->second},
                    acceleration == accelerations_.end() ? std::nullopt : std::optional<Vec2>{acceleration->second});
    }
public:
    EcsWorld() = default;
    explicit EcsWorld(const std::vector<Seed>& seeds) { for (const auto& body : seeds) spawn(body); }
    Handle spawn(const Seed& body) {
        validate_seed(body);
        if (!live(body)) return {};
        const auto handle = ids_.issue();
        const auto number = handle.number();
        try {
            positions_.emplace(number, body.position);
            if (body.moving) velocities_.emplace(number, body.velocity);
            if (body.acceleration) accelerations_.emplace(number, *body.acceleration);
            if (body.expiring) lifetimes_.emplace(number, body.remaining);
            if (body.visible) visible_.insert(number);
        } catch (...) { erase_all(number); throw; }
        return handle;
    }
    bool contains(const Handle& handle) const {
        return ids_.owns(handle) && positions_.count(handle.number()) != 0;
    }
    std::optional<Seed> inspect(const Handle& handle) const {
        return contains(handle) ? std::optional<Seed>{snapshot(handle.number())} : std::nullopt;
    }
    bool destroy(const Handle& handle) {
        if (!contains(handle)) return false;
        erase_all(handle.number()); return true;
    }
    void step(double dt, std::vector<Trace>* trace = nullptr) {
        validate_dt(dt);
        std::vector<Number> expired;
        expired.reserve(lifetimes_.size());
        // Reserve before mutation. Trace values contain no allocating strings.
        if (trace) trace->reserve(trace->size() + accelerations_.size() + velocities_.size() + 2 * lifetimes_.size());
        auto record = [&](Number number, Stage stage) {
            if (trace) {
                const auto state = snapshot(number);
                trace->push_back({number, stage, state.position, state.velocity});
            }
        };
        for (const auto& [number, acceleration] : accelerations_) {
            accelerate(velocities_.at(number), acceleration, dt);
            record(number, Stage::acceleration);
        }
        for (const auto& [number, velocity] : velocities_) {
            move(positions_.at(number), velocity, dt);
            record(number, Stage::movement);
        }
        for (auto& [number, remaining] : lifetimes_) {
            remaining -= dt; record(number, Stage::lifetime);
            if (remaining <= 0) expired.push_back(number);
        }
        for (const auto number : expired) { record(number, Stage::removed); erase_all(number); }
    }
    std::vector<Seed> states() const {
        std::vector<Seed> result;
        for (const auto& [number, position] : positions_) { (void)position; result.push_back(snapshot(number)); }
        return result;
    }
    Counts counts() const {
        return {positions_.size(), velocities_.size(), accelerations_.size(), lifetimes_.size(), visible_.size()};
    }
};
} // namespace course
