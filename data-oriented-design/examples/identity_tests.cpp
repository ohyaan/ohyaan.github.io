#include "entity_identity.hpp"
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
void equal(const std::vector<lesson::Snapshot>& a,
           const std::vector<lesson::Snapshot>& b) {
    require(a.size() == b.size(), "state count");
    for (std::size_t i = 0; i < a.size(); ++i) {
        require(near(a[i].position.x, b[i].position.x) &&
                near(a[i].position.y, b[i].position.y) &&
                a[i].visible == b[i].visible &&
                a[i].expiring == b[i].expiring &&
                near(a[i].remaining, b[i].remaining), "state equivalence");
    }
}
void identity_contract() {
    using lesson::identity::Entity;
    using lesson::identity::World;
    static_assert(!std::is_copy_constructible_v<World>);
    static_assert(!std::is_move_constructible_v<World>);
    World world;
    const Entity empty;
    require(!world.contains(empty) && !world.inspect(empty) &&
            !world.destroy(empty), "default handle");
    const auto rejected = world.spawn({{}, {}, false, true, true, 0});
    require(!world.contains(rejected), "initially expired seed");
    const auto first = world.spawn({{0, 0}, {}, false, true, false, 0});
    const auto tracked = world.spawn({{10, 1}, {2, -2}, true, true, false, 0});
    require(first.number() == 1 && tracked.number() == 2, "monotonic numbers");
    const auto saved = world.inspect(tracked);
    require(bool(saved), "initial lookup");
    const auto copied_handle = tracked;
    require(world.contains(copied_handle), "copied handle");
    require(world.destroy(first), "delete before tracked entity");
    const auto old_capacity = world.storage_capacity();
    world.reserve(old_capacity + 1);
    require(world.storage_capacity() > old_capacity, "forced reallocation");
    for (int i = 0; i < 512; ++i)
        world.spawn({{double(i), 3}, {}, false, false, false, 0});
    world.step(0.5);
    const auto state = world.inspect(tracked);
    require(state && near(state->position.x, 11) && near(state->position.y, 0),
            "identity survives deletion, growth, and movement");
    require(near(saved->position.x, 10), "snapshot is a copy");
    require(world.destroy(tracked) && !world.destroy(tracked),
            "destroy is safe to repeat");
    require(!world.contains(copied_handle) && !world.inspect(tracked),
            "all copies become stale");
    const auto replacement = world.spawn({{20, 0}, {}, false, true, false, 0});
    require(replacement.number() > tracked.number() && !world.contains(tracked),
            "no ID reuse");

    World other;
    const auto foreign = other.spawn({{}, {}, false, true, false, 0});
    require(foreign.number() == first.number(), "world-local numeric collision");
    const auto before = world.states();
    require(!world.contains(foreign) && !world.inspect(foreign) &&
            !world.destroy(foreign), "reject foreign namespace");
    equal(before, world.states());
    Entity orphan;
    {
        World temporary;
        orphan = temporary.spawn({{}, {}, false, true, false, 0});
    }
    World recreated;
    const auto fresh = recreated.spawn({{}, {}, false, true, false, 0});
    require(fresh.number() == orphan.number() && !recreated.contains(orphan),
            "old world handle cannot target a new world");
}
void simulation_contract() {
    std::vector<lesson::Seed> seeds;
    for (int bits = 0; bits < 8; ++bits)
        seeds.push_back({{double(bits), 1}, {2, -1}, bool(bits & 1),
                         bool(bits & 2), bool(bits & 4), 0.5});
    seeds.push_back({{}, {}, false, true, true, -1});
    lesson::plain::World baseline(seeds);
    lesson::identity::World world(seeds);
    const auto handles = world.entities();
    require(handles.size() == 8, "one handle per initially live body");
    const auto before = world.states();
    for (double dt : {0.0, -0.1, std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { world.step(dt); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid dt rejection");
        equal(before, world.states());
    }
    for (int step = 0; step < 5; ++step) {
        equal(baseline.states(), world.states());
        require(world.states().size() == (step < 2 ? 8U : 4U),
                "expected survivors");
        const auto image = lesson::render(world.states());
        require(image.size() == (step < 2 ? 4U : 2U), "visible survivors");
        for (int bits = 0; bits < 8; ++bits) {
            const auto state = world.inspect(handles[static_cast<std::size_t>(bits)]);
            const bool alive = !(bits & 4) || step < 2;
            require(bool(state) == alive, "lifetime invalidates the correct ID");
            if (state) {
                const double elapsed = step * 0.25;
                require(near(state->position.x, bits + ((bits & 1) ? 2 * elapsed : 0))
                        && near(state->position.y, 1 - ((bits & 1) ? elapsed : 0)),
                        "independent expected trajectory");
            }
        }
        baseline.step(0.25);
        world.step(0.25);
    }
    const auto after = world.spawn({{}, {}, false, true, false, 0});
    require(after.number() == 9, "expiration does not reset the counter");
}
void exhaustion_guard() {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    require(lesson::identity::next_number(0) == 1 &&
            lesson::identity::next_number(maximum - 1) == maximum,
            "counter boundaries");
    bool rejected = false;
    try { (void)lesson::identity::next_number(maximum); }
    catch (const std::overflow_error&) { rejected = true; }
    require(rejected, "counter must not wrap to zero");
}
} // namespace

int main() {
    try {
        identity_contract();
        simulation_contract();
        exhaustion_guard();
        std::cout << "PASS: identity, namespace, lifetime, equivalence, and exhaustion\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
