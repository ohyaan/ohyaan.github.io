#include "simulation.hpp"
#include <iostream>
#include <limits>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
void equal(const std::vector<lesson::Snapshot>& a,
           const std::vector<lesson::Snapshot>& b) {
    require(a.size() == b.size(), "survivor count");
    for (std::size_t i = 0; i < a.size(); ++i) {
        require(near(a[i].position.x, b[i].position.x) &&
                near(a[i].position.y, b[i].position.y), "position");
        require(a[i].visible == b[i].visible &&
                a[i].expiring == b[i].expiring &&
                near(a[i].remaining, b[i].remaining), "capabilities/lifetime");
    }
    const auto ar = lesson::render(a), br = lesson::render(b);
    require(ar.size() == br.size(), "render count");
    for (std::size_t i = 0; i < ar.size(); ++i) {
        require(near(ar[i].x, br[i].x) && near(ar[i].y, br[i].y),
                "rendered position");
    }
}
template<class World>
void contract() {
    const std::vector<lesson::Seed> seeds{
        {{0, 0}, {2, -4}, true, true, false, 0},
        {{5, 5}, {99, 99}, false, true, false, 0},
        {{0, 1}, {0, 1}, true, false, true, 0.5},
        {{7, 7}, {}, false, true, true, 0},
        {{8, 8}, {}, false, true, true, -1}
    };
    World world(seeds);
    require(world.states().size() == 3, "initial expiration");
    world.step(0.25);
    auto states = world.states();
    require(near(states[0].position.x, 0.5) &&
            near(states[0].position.y, -1), "known trajectory");
    require(near(states[1].position.x, 5) &&
            near(states[1].position.y, 5), "stationary body");
    require(near(states[2].remaining, 0.25) &&
            near(states[2].position.y, 1.25), "lifetime decrement");
    require(lesson::render(states).size() == 2, "invisible moving body");
    for (double dt : {0.0, -0.1, std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { world.step(dt); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid dt must throw");
        equal(world.states(), states);
    }
    world.step(0.25);
    require(world.states().size() == 2, "exact lifetime removal");
    require(near(world.states()[0].position.x, 1), "second movement");
    World empty({});
    empty.step(1);
    require(empty.states().empty(), "empty world");
}
void combinations() {
    std::vector<lesson::Seed> seeds;
    for (int bits = 0; bits < 8; ++bits) {
        seeds.push_back({{double(bits), 1}, {2, -1},
                         bool(bits & 1), bool(bits & 2), bool(bits & 4), 0.5});
    }
    lesson::plain::World plain(seeds);
    lesson::inheritance::World hierarchy(seeds);
    lesson::composition::World composed(seeds);
    for (int step = 0; step < 5; ++step) {
        const auto states = plain.states();
        equal(states, hierarchy.states());
        equal(states, composed.states());
        require(states.size() == (step < 2 ? 8U : 4U), "expected survivors");
        require(lesson::render(states).size() == (step < 2 ? 4U : 2U),
                "expected visible survivors");
        // An independent expected trajectory guards against shared bugs.
        for (std::size_t i = 0; i < states.size(); ++i) {
            const int bits = static_cast<int>(i);
            const double elapsed = step * 0.25;
            require(near(states[i].position.x, bits + ((bits & 1) ? 2 * elapsed : 0)),
                    "combination trajectory");
            require(near(states[i].position.y, 1 - ((bits & 1) ? elapsed : 0)),
                    "combination y trajectory");
            require(states[i].visible == bool(bits & 2) &&
                    states[i].expiring == bool(bits & 4), "expected capabilities");
            require(near(states[i].remaining, (bits & 4) ? 0.5 - elapsed : 0),
                    "expected remaining lifetime");
        }
        plain.step(0.25);
        hierarchy.step(0.25);
        composed.step(0.25);
    }
}
} // namespace

int main() {
    try {
        contract<lesson::plain::World>();
        contract<lesson::inheritance::World>();
        contract<lesson::composition::World>();
        combinations();
        std::cout << "PASS: contract, invalid dt, and eight capability combinations\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
