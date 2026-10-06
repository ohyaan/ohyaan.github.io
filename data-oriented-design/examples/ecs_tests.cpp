#include "minimal_ecs.hpp"
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
    require(a.size() == b.size(), "snapshot count");
    for (std::size_t i = 0; i < a.size(); ++i)
        require(near(a[i].position.x, b[i].position.x) &&
                near(a[i].position.y, b[i].position.y) &&
                a[i].visible == b[i].visible && a[i].expiring == b[i].expiring &&
                near(a[i].remaining, b[i].remaining), "snapshot equivalence");
}
void image_equal(const std::vector<lesson::Vec2>& a,
                 const std::vector<lesson::Vec2>& b) {
    require(a.size() == b.size(), "render count");
    for (std::size_t i = 0; i < a.size(); ++i)
        require(near(a[i].x, b[i].x) && near(a[i].y, b[i].y), "render order and values");
}
void counts(const lesson::ecs::World& world, std::size_t p, std::size_t v,
            std::size_t l, std::size_t visible) {
    const auto c = world.component_counts();
    require(c.positions == p && c.velocities == v && c.lifetimes == l &&
            c.visible == visible, "component membership counts");
}
void matching_and_order() {
    // Synthetic stores deliberately include unmatched members to test the join.
    lesson::ecs::Positions p{{1, {{0, 0}}}, {2, {{10, 5}}}, {4, {{9, 1}}}};
    const lesson::ecs::Velocities v{{1, {{2, -2}}}, {3, {{100, 100}}}};
    lesson::ecs::detail::movement_system(p, v, 0.5);
    require(p.size() == 3 && near(p.at(1).value.x, 1) &&
            near(p.at(1).value.y, -1) && near(p.at(2).value.x, 10) &&
            near(p.at(4).value.x, 9), "movement requires Position AND Velocity");
    image_equal(lesson::ecs::detail::render_system(p, {1, 3, 4}), {{1, -1}, {9, 1}});
    // Observe final movement before the expired entity's components are removed.
    lesson::ecs::Lifetimes life{{1, {0.5}}};
    std::vector<lesson::ecs::Number> expired;
    expired.reserve(life.size());
    lesson::ecs::detail::lifetime_system(life, 0.5, expired);
    require(expired == std::vector<lesson::ecs::Number>{1} &&
            near(life.at(1).remaining, 0) && near(p.at(1).value.x, 1),
            "movement precedes expiration; collection does not erase stores");
}
void simulation_contract() {
    std::vector<lesson::Seed> seeds;
    for (int bits = 0; bits < 8; ++bits)
        seeds.push_back({{double(bits), 1}, {2, -1}, bool(bits & 1),
                         bool(bits & 2), bool(bits & 4), 0.5});
    seeds.push_back({{}, {}, true, true, true, 0});
    seeds.push_back({{}, {}, true, true, true, -1});
    lesson::plain::World baseline(seeds);
    lesson::identity::World identity(seeds);
    lesson::ecs::World world(seeds);
    const auto handles = world.entities();
    require(handles.size() == 8, "initially dead seeds create no entity");
    counts(world, 8, 4, 4, 4);
    const auto before = world.states();
    for (double dt : {0.0, -0.1, std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { world.step(dt); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid dt must reject");
        equal(before, world.states());
        counts(world, 8, 4, 4, 4);
    }
    for (int step = 0; step < 5; ++step) {
        equal(baseline.states(), world.states());
        equal(identity.states(), world.states());
        image_equal(lesson::render(baseline.states()), world.render());
        if (step < 2) counts(world, 8, 4, 4, 4);
        else counts(world, 4, 2, 0, 2);
        for (int bits = 0; bits < 8; ++bits) {
            const auto state = world.inspect(handles[static_cast<std::size_t>(bits)]);
            require(bool(state) == (!(bits & 4) || step < 2), "expiration by ID");
            if (state) {
                const double elapsed = step * 0.25;
                require(near(state->position.x, bits + ((bits & 1) ? 2 * elapsed : 0))
                        && near(state->position.y, 1 - ((bits & 1) ? elapsed : 0)),
                        "independent expected trajectory");
            }
        }
        baseline.step(0.25);
        identity.step(0.25);
        world.step(0.25);
    }
    require(world.spawn({{}, {}, false, false, false, 0}).number() == 9,
            "expiration does not recycle numbers");
}
void identity_and_cleanup() {
    using lesson::ecs::World;
    using lesson::ecs::Entity;
    static_assert(!std::is_copy_constructible_v<World>);
    static_assert(!std::is_move_constructible_v<World>);
    World world;
    const Entity empty;
    require(!world.contains(empty) && !world.inspect(empty) && !world.destroy(empty),
            "empty handle");
    world.spawn({{}, {}, true, true, true, 0});
    const auto first = world.spawn({{1, 2}, {3, 4}, true, true, true, 10});
    const auto selected = world.spawn({{10, 1}, {2, -2}, true, true, false, 0});
    const auto copy = selected;
    const auto saved = world.inspect(selected);
    require(first.number() == 1 && selected.number() == 2, "monotonic issuance");
    require(world.destroy(first), "explicit deletion");
    counts(world, 1, 1, 0, 1);
    for (int i = 0; i < 512; ++i) world.spawn({{}, {}, false, false, false, 0});
    world.step(0.5);
    const auto current = world.inspect(copy);
    require(current && near(current->position.x, 11) && near(current->position.y, 0),
            "selection survives other deletion and growth");
    require(saved && near(saved->position.x, 10), "snapshot remains copied");
    require(world.destroy(selected) && !world.destroy(copy) && !world.inspect(copy),
            "all copies become stale and repeated deletion is safe");
    counts(world, 512, 0, 0, 0);
    const auto replacement = world.spawn({{}, {}, true, true, true, 0.25});
    require(replacement.number() > selected.number(), "no reuse");
    world.step(0.25);
    require(!world.contains(replacement), "automatic cleanup invalidates handle");
    counts(world, 512, 0, 0, 0);
    World other;
    const auto foreign = other.spawn({{}, {}, true, true, true, 10});
    require(foreign.number() == first.number(), "numeric collision between worlds");
    const auto before = world.states();
    require(!world.contains(foreign) && !world.inspect(foreign) && !world.destroy(foreign),
            "foreign handle rejection");
    equal(before, world.states());
    Entity orphan;
    {
        World temporary;
        orphan = temporary.spawn({{}, {}, false, false, false, 0});
    }
    World recreated;
    const auto fresh = recreated.spawn({{}, {}, false, false, false, 0});
    require(fresh.number() == orphan.number() && !recreated.contains(orphan),
            "destroyed world namespace cannot recur");
    require(fresh.number() == foreign.number() && !recreated.contains(foreign) &&
            !recreated.inspect(foreign) && !recreated.destroy(foreign) &&
            recreated.contains(fresh), "foreign collision cannot target a live entity");
    const auto all = world.entities();
    for (const auto& entity : all) require(world.destroy(entity), "bulk deletion");
    counts(world, 0, 0, 0, 0);
    world.step(0.25);
    require(world.states().empty() && world.render().empty(), "empty world is safe");
}
} // namespace

int main() {
    try {
        matching_and_order();
        simulation_contract();
        identity_and_cleanup();
        std::cout << "PASS: component joins, order, equivalence, identity, and cleanup\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
