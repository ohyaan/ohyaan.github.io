#include "generational_ecs.hpp"
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
void image_equal(const std::vector<lesson::Vec2>& a, const std::vector<lesson::Vec2>& b) {
    require(a.size() == b.size(), "render count");
    for (std::size_t i = 0; i < a.size(); ++i)
        require(near(a[i].x, b[i].x) && near(a[i].y, b[i].y), "render order/values");
}
template<lesson::generational::Generation Limit>
void counts(const lesson::generational::World<Limit>& world, std::size_t p,
            std::size_t v, std::size_t l, std::size_t visible) {
    const auto c = world.component_counts();
    require(c.positions == p && c.velocities == v && c.lifetimes == l &&
            c.visible == visible, "complete component cleanup");
}
void simulation_contract() {
    std::vector<lesson::Seed> seeds;
    for (int bits = 0; bits < 8; ++bits)
        seeds.push_back({{double(bits), 1}, {2, -1}, bool(bits & 1),
                         bool(bits & 2), bool(bits & 4), 0.5});
    seeds.push_back({{}, {}, true, true, true, 0});
    lesson::plain::World baseline(seeds);
    lesson::ecs::World previous(seeds);
    lesson::generational::World<> world(seeds);
    const auto handles = world.entities();
    require(handles.size() == 8 && world.slot_counts().allocated == 8,
            "initial expiration consumes no slot");
    const auto before = world.states();
    for (double dt : {0.0, -0.1, std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { world.step(dt); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid dt rejection");
        equal(before, world.states());
        counts(world, 8, 4, 4, 4);
        require(world.slot_counts().live == 8 && world.slot_counts().reusable == 0,
                "invalid step leaves slots unchanged");
    }
    for (int step = 0; step < 5; ++step) {
        equal(baseline.states(), world.states());
        equal(previous.states(), world.states());
        image_equal(lesson::render(baseline.states()), world.render());
        if (step < 2) counts(world, 8, 4, 4, 4);
        else counts(world, 4, 2, 0, 2);
        for (int bits = 0; bits < 8; ++bits) {
            const auto state = world.inspect(handles[static_cast<std::size_t>(bits)]);
            require(bool(state) == (!(bits & 4) || step < 2), "correct handle expires");
            if (state) {
                const double elapsed = step * 0.25;
                require(near(state->position.x, bits + ((bits & 1) ? 2 * elapsed : 0))
                        && near(state->position.y, 1 - ((bits & 1) ? elapsed : 0)),
                        "independent trajectory");
            }
        }
        baseline.step(0.25); previous.step(0.25); world.step(0.25);
    }
    const auto new_entity = world.spawn({{99, 0}, {}, false, true, false, 0});
    require(new_entity.generation() == 2 && world.slot_counts().allocated == 8,
            "automatic expiration supplies reusable slots");
    for (std::size_t i = 4; i < handles.size(); ++i)
        require(!world.contains(handles[i]) && !world.destroy(handles[i]),
                "expired handles remain stale after reuse");
    require(near(world.states().back().position.x, 99), "reused slot appends to order");
}
void reuse_and_order() {
    using lesson::generational::World;
    using lesson::generational::Entity;
    static_assert(!std::is_copy_constructible_v<World<>>);
    static_assert(!std::is_move_constructible_v<World<>>);
    World<> world;
    const Entity empty;
    require(!world.contains(empty) && !world.inspect(empty) && !world.destroy(empty),
            "empty handle");
    const auto a = world.spawn({{10, 0}, {2, 0}, true, true, true, 10});
    const auto b = world.spawn({{20, 0}, {}, false, true, false, 0});
    const auto c = world.spawn({{30, 0}, {}, false, true, false, 0});
    const auto copied_a = a;
    const auto saved = world.inspect(a);
    require(world.destroy(a) && !world.destroy(copied_a) && world.destroy(c),
            "delete and repeat");
    counts(world, 1, 0, 0, 1);
    const auto d = world.spawn({{40, 0}, {}, false, false, false, 0});
    const auto e = world.spawn({{50, 0}, {}, false, true, false, 0});
    require(d.slot() == c.slot() && e.slot() == a.slot() && e.generation() == 2,
            "LIFO free list recycles distinct slots");
    require(!world.contains(a) && !world.inspect(copied_a) && !world.destroy(a) &&
            world.contains(e), "stale handle cannot delete replacement");
    require(saved && near(saved->position.x, 10), "snapshot survives as copied history");
    const auto ordered = world.entities();
    require(ordered[0].slot() == b.slot() && ordered[1].slot() == d.slot() &&
            ordered[2].slot() == e.slot(), "creation order is not slot order");
    image_equal(world.render(), {{20, 0}, {50, 0}});
    world.step(0.25);
    const auto current = world.inspect(e);
    require(current && near(current->position.x, 50), "old velocity did not leak into reuse");
    counts(world, 3, 0, 0, 2);
    const auto capacity = world.slot_capacity();
    world.reserve_slots(capacity + 1);
    require(world.slot_capacity() > capacity && world.contains(e), "metadata relocation");
    for (int i = 0; i < 512; ++i) world.spawn({{}, {}, false, false, false, 0});
    require(world.contains(b) && world.contains(d) && world.contains(e), "bulk growth");
    World<> other;
    const auto foreign = other.spawn({{}, {}, false, false, false, 0});
    // e has generation 2, so compare against a fresh live generation-1 entity too.
    World<> fresh_world;
    const auto fresh = fresh_world.spawn({{}, {}, false, true, false, 0});
    require(foreign.slot() == fresh.slot() && foreign.generation() == fresh.generation()
            && !fresh_world.contains(foreign) && !fresh_world.inspect(foreign)
            && !fresh_world.destroy(foreign) && fresh_world.contains(fresh),
            "namespace is necessary even when slot and generation match");
    Entity orphan;
    { World<> temporary; orphan = temporary.spawn({{}, {}, false, false, false, 0}); }
    require(!fresh_world.contains(orphan), "destroyed World namespace remains distinct");
    for (const auto& entity : world.entities()) require(world.destroy(entity), "bulk cleanup");
    counts(world, 0, 0, 0, 0);
    world.step(0.25);
    require(world.states().empty() && world.render().empty(), "empty World");
    const auto allocated = world.slot_counts().allocated;
    for (int cycle = 0; cycle < 2; ++cycle) {
        std::set<lesson::generational::Number> used;
        for (std::size_t i = 0; i < allocated; ++i)
            require(used.insert(world.spawn({{}, {}, false, false, false, 0}).slot()).second,
                    "free list never issues the same live slot twice");
        require(world.slot_counts().allocated == allocated &&
                world.slot_counts().live == allocated && world.slot_counts().reusable == 0,
                "reuse every available slot without allocating metadata");
        require(!world.contains(a) && !world.contains(b) && !world.contains(e),
                "older generations remain stale across full reuse cycles");
        for (const auto& entity : world.entities()) require(world.destroy(entity), "cycle cleanup");
        require(world.slot_counts().reusable == allocated, "all slots return to the free list");
        counts(world, 0, 0, 0, 0);
    }
}
void retirement_contract() {
    lesson::generational::World<2> world;
    std::vector<lesson::generational::Entity> old;
    for (std::uint64_t generation = 1; generation <= 2; ++generation) {
        const auto entity = world.spawn({{}, {}, true, true, true, 0.25});
        require(entity.slot() == 1 && entity.generation() == generation, "limit boundary");
        old.push_back(entity);
        if (generation == 1) require(world.destroy(entity), "explicit release to free list");
        else world.step(0.25); // Automatic deletion also retires at the limit.
        counts(world, 0, 0, 0, 0);
    }
    const auto replacement = world.spawn({{}, {}, false, false, false, 0});
    const auto slots = world.slot_counts();
    require(replacement.slot() == 2 && replacement.generation() == 1 &&
            slots.allocated == 2 && slots.live == 1 && slots.retired == 1 &&
            slots.reusable == 0, "exhausted slot is never reissued");
    for (const auto& entity : old)
        require(!world.contains(entity) && !world.inspect(entity) && !world.destroy(entity),
                "every retired generation stays invalid");
    lesson::generational::World<1> one;
    const auto first = one.spawn({{}, {}, false, false, false, 0});
    require(one.destroy(first), "generation one retirement");
    require(one.spawn({{}, {}, false, false, false, 0}).slot() == 2 &&
            one.slot_counts().retired == 1, "limit one cannot increment or wrap");
}
} // namespace
int main() {
    try {
        simulation_contract(); reuse_and_order(); retirement_contract();
        std::cout << "PASS: generations, reuse, retirement, cleanup, order, and equivalence\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
