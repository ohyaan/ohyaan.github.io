#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace lesson {
struct Vec2 { double x = 0; double y = 0; };
struct Seed {
    Vec2 position;
    Vec2 velocity;
    bool moving = false;
    bool visible = false;
    bool expiring = false;
    double remaining = 0;
};
struct Snapshot {
    Vec2 position;
    bool visible;
    bool expiring;
    double remaining;
};

inline void validate_dt(double dt) {
    if (!std::isfinite(dt) || dt <= 0) {
        throw std::invalid_argument("dt must be finite and positive");
    }
}
inline bool initially_live(const Seed& seed) {
    return !seed.expiring || seed.remaining > 0;
}
inline void move(Vec2& position, Vec2 velocity, double dt) {
    position.x += velocity.x * dt;
    position.y += velocity.y * dt;
}
inline std::vector<Vec2> render(const std::vector<Snapshot>& states) {
    std::vector<Vec2> result;
    for (const auto& state : states) {
        if (state.visible) result.push_back(state.position);
    }
    return result;
}

// Chapter 2: one record per body and a direct processing loop.
namespace plain {
class World {
    std::vector<Seed> bodies_;
public:
    explicit World(const std::vector<Seed>& seeds) {
        for (const auto& seed : seeds) {
            if (initially_live(seed)) bodies_.push_back(seed);
        }
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& body : bodies_) {
            if (body.moving) move(body.position, body.velocity, dt);
            if (body.expiring) body.remaining -= dt;
        }
        bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
            [](const Seed& body) {
                return body.expiring && body.remaining <= 0;
            }), bodies_.end());
    }
    std::vector<Snapshot> states() const {
        std::vector<Snapshot> result;
        for (const auto& body : bodies_) {
            result.push_back({body.position, body.visible, body.expiring,
                              body.expiring ? body.remaining : 0});
        }
        return result;
    }
};
} // namespace plain

// Chapter 3: a polymorphic interface, not a claim about all OOP designs.
namespace inheritance {
class Body {
protected:
    Seed state_;
public:
    explicit Body(Seed seed) : state_(seed) {}
    virtual ~Body() = default;
    virtual void advance(double dt) = 0;
    bool expired() const {
        return state_.expiring && state_.remaining <= 0;
    }
    Snapshot snapshot() const {
        return {state_.position, state_.visible, state_.expiring,
                state_.expiring ? state_.remaining : 0};
    }
};
// Explicit specialisations represent the two independent behaviour axes.
// Visibility remains data; it does not require another set of subclasses.
template<bool Moving, bool Expiring>
class ConfiguredBody final : public Body {
public:
    explicit ConfiguredBody(Seed seed) : Body(seed) {}
    void advance(double dt) override {
        if constexpr (Moving) move(state_.position, state_.velocity, dt);
        if constexpr (Expiring) state_.remaining -= dt;
    }
};
inline std::unique_ptr<Body> make_body(Seed seed) {
    if (seed.moving && seed.expiring)
        return std::make_unique<ConfiguredBody<true, true>>(seed);
    if (seed.moving)
        return std::make_unique<ConfiguredBody<true, false>>(seed);
    if (seed.expiring)
        return std::make_unique<ConfiguredBody<false, true>>(seed);
    return std::make_unique<ConfiguredBody<false, false>>(seed);
}
class World {
    std::vector<std::unique_ptr<Body>> bodies_;
public:
    explicit World(const std::vector<Seed>& seeds) {
        for (auto seed : seeds) {
            if (initially_live(seed)) bodies_.push_back(make_body(seed));
        }
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& body : bodies_) body->advance(dt);
        bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
            [](const auto& body) { return body->expired(); }), bodies_.end());
    }
    std::vector<Snapshot> states() const {
        std::vector<Snapshot> result;
        for (const auto& body : bodies_) result.push_back(body->snapshot());
        return result;
    }
};
} // namespace inheritance

// Owning optional values is composition, not a component-store ECS.
namespace composition {
struct Motion { Vec2 velocity; };
struct Lifetime { double remaining; };
struct Body {
    Vec2 position;
    bool visible;
    std::optional<Motion> motion;
    std::optional<Lifetime> lifetime;
};
class World {
    std::vector<Body> bodies_;
public:
    explicit World(const std::vector<Seed>& seeds) {
        for (const auto& seed : seeds) {
            if (!initially_live(seed)) continue;
            Body body{seed.position, seed.visible, std::nullopt, std::nullopt};
            if (seed.moving) body.motion = Motion{seed.velocity};
            if (seed.expiring) body.lifetime = Lifetime{seed.remaining};
            bodies_.push_back(body);
        }
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& body : bodies_) {
            if (body.motion) move(body.position, body.motion->velocity, dt);
            if (body.lifetime) body.lifetime->remaining -= dt;
        }
        bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
            [](const Body& body) {
                return body.lifetime && body.lifetime->remaining <= 0;
            }), bodies_.end());
    }
    std::vector<Snapshot> states() const {
        std::vector<Snapshot> result;
        for (const auto& body : bodies_) {
            result.push_back({body.position, body.visible, bool(body.lifetime),
                              body.lifetime ? body.lifetime->remaining : 0});
        }
        return result;
    }
};
} // namespace composition
} // namespace lesson
