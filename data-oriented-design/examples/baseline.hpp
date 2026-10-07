#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace course {
struct Vec2 { double x = 0; double y = 0; };
struct BasicSeed {
    Vec2 position;
    Vec2 velocity;
    bool moving = false;
    bool visible = false;
    bool expiring = false;
    double remaining = 0;
};
inline bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
inline void validate_dt(double dt) {
    if (!std::isfinite(dt) || dt <= 0) throw std::invalid_argument("dt must be finite and positive");
}
inline void validate_basic(const BasicSeed& seed) {
    if (!finite(seed.position) || !finite(seed.velocity) ||
        (seed.expiring && !std::isfinite(seed.remaining)))
        throw std::invalid_argument("seed values must be finite");
}
inline bool live(const BasicSeed& seed) { return !seed.expiring || seed.remaining > 0; }
inline void move(Vec2& position, Vec2 velocity, double dt) {
    position.x += velocity.x * dt; position.y += velocity.y * dt;
}
inline BasicSeed observed(BasicSeed seed) {
    if (!seed.moving) seed.velocity = {};
    if (!seed.expiring) seed.remaining = 0;
    return seed;
}
class Baseline {
    std::vector<BasicSeed> bodies_;
public:
    explicit Baseline(const std::vector<BasicSeed>& seeds) {
        for (const auto& seed : seeds) {
            validate_basic(seed);
            if (live(seed)) bodies_.push_back(seed);
        }
    }
    void step(double dt) {
        validate_dt(dt);
        for (auto& body : bodies_) {
            if (body.moving) move(body.position, body.velocity, dt);
            if (body.expiring) body.remaining -= dt;
        }
        bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(),
            [](const auto& body) { return !live(body); }), bodies_.end());
    }
    std::vector<BasicSeed> states() const {
        std::vector<BasicSeed> result;
        for (const auto& body : bodies_) result.push_back(observed(body));
        return result;
    }
};
} // namespace course
