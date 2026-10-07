#pragma once
#include "baseline.hpp"
#include <memory>
#include <optional>
#include <utility>

namespace course {
struct Seed : BasicSeed { std::optional<Vec2> acceleration; };
inline Seed seed(BasicSeed basic, std::optional<Vec2> acceleration = std::nullopt) {
    Seed result; static_cast<BasicSeed&>(result) = basic; result.acceleration = acceleration;
    return result;
}
inline void validate_seed(const Seed& value) {
    validate_basic(value);
    if (value.acceleration && (!value.moving || !finite(*value.acceleration)))
        throw std::invalid_argument("acceleration requires movement and finite values");
}
inline Seed observed(Seed value) {
    static_cast<BasicSeed&>(value) = observed(static_cast<BasicSeed>(value));
    return value;
}
inline void accelerate(Vec2& velocity, Vec2 acceleration, double dt) {
    velocity.x += acceleration.x * dt; velocity.y += acceleration.y * dt;
}
inline void advance(Seed& body, double dt) {
    if (body.acceleration) accelerate(body.velocity, *body.acceleration, dt);
    if (body.moving) move(body.position, body.velocity, dt);
    if (body.expiring) body.remaining -= dt;
}
inline std::vector<Vec2> render(const std::vector<Seed>& states) {
    std::vector<Vec2> result;
    for (const auto& body : states) if (body.visible) result.push_back(body.position);
    return result;
}
inline std::vector<Seed> fixture() {
    return {seed({{0, 0}, {1, 0}, true, true, false, 0}, Vec2{2, 0}),
            seed({{10, 0}, {-1, 0}, true, false, true, 1}),
            seed({{5, 5}, {}, false, true, false, 0})};
}
class RecordWorld {
    std::vector<Seed> bodies_;
public:
    explicit RecordWorld(const std::vector<Seed>& seeds) {
        for (const auto& body : seeds) {
            validate_seed(body);
            if (live(body)) bodies_.push_back(body);
        }
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& body : bodies_) advance(body, dt);
        bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
            [](const auto& body) { return !live(body); }), bodies_.end());
    }
    std::vector<Seed> states() const {
        std::vector<Seed> result;
        for (const auto& body : bodies_) result.push_back(observed(body));
        return result;
    }
};
namespace composition {
struct Motion { Vec2 velocity; std::optional<Vec2> acceleration; };
struct Body {
    Vec2 position;
    bool visible;
    std::optional<Motion> motion;
    std::optional<double> lifetime;
};
class World {
    std::vector<Body> bodies_;
public:
    explicit World(const std::vector<Seed>& seeds) {
        for (const auto& value : seeds) {
            validate_seed(value);
            if (!live(value)) continue;
            Body body{value.position, value.visible, std::nullopt, std::nullopt};
            if (value.moving) body.motion = Motion{value.velocity, value.acceleration};
            if (value.expiring) body.lifetime = value.remaining;
            bodies_.push_back(body);
        }
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& body : bodies_) {
            if (body.motion) {
                if (body.motion->acceleration)
                    accelerate(body.motion->velocity, *body.motion->acceleration, dt);
                move(body.position, body.motion->velocity, dt);
            }
            if (body.lifetime) *body.lifetime -= dt;
        }
        bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
            [](const auto& body) { return body.lifetime && *body.lifetime <= 0; }), bodies_.end());
    }
    std::vector<Seed> states() const {
        std::vector<Seed> result;
        for (const auto& body : bodies_) {
            auto value = seed({body.position, {}, bool(body.motion), body.visible,
                               bool(body.lifetime), body.lifetime.value_or(0)});
            if (body.motion) {
                value.velocity = body.motion->velocity;
                value.acceleration = body.motion->acceleration;
            }
            result.push_back(value);
        }
        return result;
    }
};
} // namespace composition
namespace inheritance {
class Body {
protected:
    Seed state_;
    virtual void update_motion(double dt) = 0;
public:
    explicit Body(Seed value) : state_(value) { validate_seed(value); }
    virtual ~Body() = default;
    void advance(double dt) {
        update_motion(dt);
        if (state_.expiring) state_.remaining -= dt;
    }
    bool expired() const { return !live(state_); }
    Seed snapshot() const { return observed(state_); }
};
class UniformBody final : public Body {
    void update_motion(double dt) override {
        if (state_.moving) move(state_.position, state_.velocity, dt);
    }
public:
    explicit UniformBody(Seed value) : Body(value) {
        if (value.acceleration) throw std::invalid_argument("uniform motion cannot accept acceleration");
    }
};
class AcceleratedBody final : public Body {
    void update_motion(double dt) override {
        accelerate(state_.velocity, *state_.acceleration, dt);
        move(state_.position, state_.velocity, dt);
    }
public:
    explicit AcceleratedBody(Seed value) : Body(value) {
        if (!value.acceleration) throw std::invalid_argument("accelerated motion requires acceleration");
    }
};
class World {
    std::vector<std::unique_ptr<Body>> bodies_;
public:
    explicit World(const std::vector<Seed>& seeds) {
        for (const auto& body : seeds) {
            validate_seed(body);
            if (!live(body)) continue;
            if (body.acceleration) bodies_.push_back(std::make_unique<AcceleratedBody>(body));
            else bodies_.push_back(std::make_unique<UniformBody>(body));
        }
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& body : bodies_) body->advance(dt);
        bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
            [](const auto& body) { return body->expired(); }), bodies_.end());
    }
    std::vector<Seed> states() const {
        std::vector<Seed> result;
        for (const auto& body : bodies_) result.push_back(body->snapshot());
        return result;
    }
};
} // namespace inheritance
} // namespace course
