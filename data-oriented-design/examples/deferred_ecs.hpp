#pragma once
#include "generational_ecs.hpp"
#include <deque>
#include <functional>
#include <type_traits>
#include <variant>

namespace lesson::deferred {
using Entity = generational::Entity;
struct Results {
    std::vector<Entity> created;
    std::size_t destroyed = 0;
    std::size_t ignored_destroy = 0;
    std::size_t ignored_spawn = 0;
};
class CommandBuffer {
    struct Spawn { Seed seed; };
    struct Destroy { Entity entity; };
    std::deque<std::variant<Spawn, Destroy>> commands_;
    Results results_;
public:
    CommandBuffer() = default;
    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) = delete;
    CommandBuffer& operator=(CommandBuffer&&) = delete;
    void spawn(Seed seed) { commands_.push_back(Spawn{seed}); }
    void destroy(Entity entity) { commands_.push_back(Destroy{std::move(entity)}); }
    std::size_t pending() const { return commands_.size(); }
    const Results& results() const { return results_; }
    // Target operations must fail before mutation; receipts cannot throw after spawn.
    template<class Target>
    void flush(Target& target) {
        static_assert(std::is_nothrow_copy_constructible_v<Entity>);
        if (commands_.size() > results_.created.max_size() - results_.created.size())
            throw std::length_error("too many creation receipts");
        results_.created.reserve(results_.created.size() + commands_.size());
        while (!commands_.empty()) {
            std::visit([&](const auto& command) {
                using T = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<T, Spawn>) {
                    const auto entity = target.spawn(command.seed);
                    if (entity.slot() != 0) results_.created.push_back(entity);
                    else ++results_.ignored_spawn;
                } else {
                    if (target.destroy(command.entity)) ++results_.destroyed;
                    else ++results_.ignored_destroy;
                }
            }, commands_.front());
            // Pop only after application and receipt recording succeed.
            commands_.pop_front();
        }
    }
    void discard_pending() { commands_.clear(); }
    Results take_results() {
        if (pending()) throw std::logic_error("resolve pending commands before taking results");
        Results result = std::move(results_);
        results_ = Results{};
        return result;
    }
};

class ReadView {
    const generational::World<>& world_;
public:
    explicit ReadView(const generational::World<>& world) : world_(world) {}
    bool contains(const Entity& entity) const { return world_.contains(entity); }
    std::optional<Snapshot> inspect(const Entity& entity) const { return world_.inspect(entity); }
    std::vector<Entity> entities() const { return world_.entities(); }
    std::vector<Snapshot> states() const { return world_.states(); }
};
class World;
class Writer {
    CommandBuffer& buffer_;
    explicit Writer(CommandBuffer& buffer) : buffer_(buffer) {}
    friend class World;
public:
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    void spawn(Seed seed) { buffer_.spawn(seed); }
    void destroy(Entity entity) { buffer_.destroy(std::move(entity)); }
};

class World {
    generational::World<> world_;
    CommandBuffer buffer_;
    bool active_ = false;
    struct ActiveGuard {
        bool& active;
        explicit ActiveGuard(bool& flag) : active(flag) { active = true; }
        ~ActiveGuard() { active = false; }
    };
    void idle() const {
        if (active_) throw std::logic_error("operation is not allowed during a frame or flush");
    }
    void ready() const {
        idle();
        if (buffer_.pending()) throw std::logic_error("retry or discard pending commands first");
    }
public:
    World() = default;
    explicit World(const std::vector<Seed>& seeds) : world_(seeds) {}
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;
    Entity spawn(Seed seed) { ready(); return world_.spawn(seed); }
    bool destroy(const Entity& entity) { ready(); return world_.destroy(entity); }
    using Decisions = std::function<void(const ReadView&, Writer&)>;
    void step(double dt, const Decisions& decisions = {}) {
        ready();
        validate_dt(dt);
        ActiveGuard guard(active_);
        world_.step(dt); // Movement -> lifetime -> intrinsic expiration cleanup.
        if (decisions) {
            const ReadView view(world_);
            Writer writer(buffer_);
            decisions(view, writer); // Reads and recording only; no structural playback.
        }
        buffer_.flush(world_); // FIFO structural changes at the explicit boundary.
    }
    void retry_flush() {
        idle();
        ActiveGuard guard(active_);
        buffer_.flush(world_); // Does not simulate another time step.
    }
    void discard_pending() { idle(); buffer_.discard_pending(); }
    Results take_results() { idle(); return buffer_.take_results(); }
    std::size_t pending() const { return buffer_.pending(); }
    const Results& results() const { return buffer_.results(); }
    bool contains(const Entity& entity) const { return world_.contains(entity); }
    std::optional<Snapshot> inspect(const Entity& entity) const { return world_.inspect(entity); }
    std::vector<Entity> entities() const { return world_.entities(); }
    std::vector<Snapshot> states() const { return world_.states(); }
    std::vector<Vec2> render() const { return world_.render(); }
    ecs::ComponentCounts component_counts() const { return world_.component_counts(); }
};
} // namespace lesson::deferred
