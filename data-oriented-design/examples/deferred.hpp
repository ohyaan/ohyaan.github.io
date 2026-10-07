#pragma once
#include "ecs.hpp"
#include <deque>
#include <functional>
#include <type_traits>
#include <variant>

namespace course {
struct Results { std::vector<Handle> created; std::size_t destroyed = 0, ignored = 0; };
class Commands {
    struct Spawn { Seed body; };
    struct Destroy { Handle handle; };
    std::deque<std::variant<Spawn, Destroy>> queue_;
    Results results_;
public:
    Commands() = default;
    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;
    void spawn(Seed body) { validate_seed(body); queue_.push_back(Spawn{body}); }
    void destroy(Handle handle) { queue_.push_back(Destroy{std::move(handle)}); }
    std::size_t pending() const { return queue_.size(); }
    const Results& results() const { return results_; }
    // Trusted target operations must throw before publishing a mutation.
    template<class Target> void flush(Target& world) {
        static_assert(std::is_nothrow_copy_constructible_v<Handle>);
        if (queue_.size() > results_.created.max_size() - results_.created.size())
            throw std::length_error("too many receipts");
        results_.created.reserve(results_.created.size() + queue_.size());
        while (!queue_.empty()) {
            std::visit([&](const auto& command) {
                using T = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<T, Spawn>) {
                    const auto handle = world.spawn(command.body);
                    if (handle.number()) results_.created.push_back(handle); else ++results_.ignored;
                } else {
                    if (world.destroy(command.handle)) ++results_.destroyed; else ++results_.ignored;
                }
            }, queue_.front());
            queue_.pop_front();
        }
    }
    void discard() { queue_.clear(); }
    Results take_results() {
        if (pending()) throw std::logic_error("resolve pending commands first");
        auto result = std::move(results_); results_ = {}; return result;
    }
};
class Decisions {
    Commands& commands_;
public:
    explicit Decisions(Commands& commands) : commands_(commands) {}
    Decisions(const Decisions&) = delete;
    Decisions& operator=(const Decisions&) = delete;
    void spawn(Seed body) { commands_.spawn(body); }
    void destroy(Handle handle) { commands_.destroy(std::move(handle)); }
};
class DeferredWorld {
    EcsWorld world_;
    Commands commands_;
    bool active_ = false;
    struct Guard { bool& active; explicit Guard(bool& a) : active(a) { active = true; } ~Guard() { active = false; } };
    void idle() const { if (active_) throw std::logic_error("frame or flush is active"); }
    void ready() const { idle(); if (commands_.pending()) throw std::logic_error("retry or discard pending commands"); }
public:
    DeferredWorld() = default;
    explicit DeferredWorld(const std::vector<Seed>& seeds) : world_(seeds) {}
    Handle spawn(Seed body) { ready(); return world_.spawn(body); }
    bool destroy(Handle handle) { ready(); return world_.destroy(handle); }
    using Callback = std::function<void(const EcsWorld&, Decisions&)>;
    void step(double dt, const Callback& decide = {}) {
        ready(); validate_dt(dt); Guard guard(active_);
        world_.step(dt);
        if (decide) { Decisions writer(commands_); decide(world_, writer); }
        commands_.flush(world_);
    }
    void retry() { idle(); Guard guard(active_); commands_.flush(world_); }
    void discard() { idle(); commands_.discard(); }
    Results take_results() { idle(); return commands_.take_results(); }
    std::size_t pending() const { return commands_.pending(); }
    bool contains(Handle handle) const { return world_.contains(handle); }
    std::optional<Seed> inspect(Handle handle) const { return world_.inspect(handle); }
    std::vector<Seed> states() const { return world_.states(); }
};
// Deliberately broken specimen for Chapter 8: visibility must not gate movement.
inline Seed broken_visibility_update(Seed body, double dt) {
    validate_seed(body); validate_dt(dt);
    if (body.visible) {
        if (body.acceleration) accelerate(body.velocity, *body.acceleration, dt);
        if (body.moving) move(body.position, body.velocity, dt);
    }
    if (body.expiring) body.remaining -= dt;
    return body;
}
} // namespace course
