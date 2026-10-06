#include "deferred_ecs.hpp"
#include <iostream>
#include <limits>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
void equal(const std::vector<lesson::Snapshot>& a, const std::vector<lesson::Snapshot>& b) {
    require(a.size() == b.size(), "snapshot count");
    for (std::size_t i = 0; i < a.size(); ++i)
        require(near(a[i].position.x, b[i].position.x) && near(a[i].position.y, b[i].position.y)
                && a[i].visible == b[i].visible && a[i].expiring == b[i].expiring
                && near(a[i].remaining, b[i].remaining), "snapshot equivalence");
}
void baseline_contract() {
    std::vector<lesson::Seed> seeds;
    for (int bits = 0; bits < 8; ++bits)
        seeds.push_back({{double(bits), 1}, {2, -1}, bool(bits & 1), bool(bits & 2),
                         bool(bits & 4), 0.5});
    seeds.push_back({{}, {}, true, true, true, 0});
    lesson::plain::World baseline(seeds);
    lesson::generational::World<> previous(seeds);
    lesson::deferred::World world(seeds);
    const auto before = world.states();
    for (double dt : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()}) {
        bool called = false, rejected = false;
        try { world.step(dt, [&](const auto&, auto&) { called = true; }); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected && !called && world.pending() == 0, "invalid dt before callback");
        equal(before, world.states());
    }
    for (int step = 0; step < 5; ++step) {
        equal(baseline.states(), world.states()); equal(previous.states(), world.states());
        const auto expected = lesson::render(baseline.states()), actual = world.render();
        require(actual.size() == expected.size(), "render count");
        for (std::size_t i = 0; i < actual.size(); ++i)
            require(near(actual[i].x, expected[i].x) && near(actual[i].y, expected[i].y),
                    "render order and values");
        baseline.step(0.25); previous.step(0.25);
        world.step(0.25, [&](const auto& view, auto&) {
            equal(baseline.states(), view.states()); // Reads occur after intrinsic cleanup.
        });
    }
}
void boundaries_and_fifo() {
    lesson::deferred::World world;
    const auto old = world.spawn({{}, {2, 0}, true, true, false, 0});
    const auto doomed = world.spawn({{}, {}, false, true, true, 0.5});
    lesson::deferred::World other;
    const auto foreign = other.spawn({{}, {}, false, true, false, 0});
    world.step(0.5, [&](const auto& view, auto& writer) {
        require(!view.contains(doomed) && near(view.inspect(old)->position.x, 1),
                "decisions see post-movement, post-expiration state");
        for (const auto& entity : view.entities()) writer.destroy(entity);
        writer.destroy(old); writer.destroy(foreign); writer.destroy({}); writer.destroy(doomed);
        writer.spawn({{10, 0}, {4, 0}, true, true, false, 0});
        writer.destroy(old); // Targets the previous generation, even after a reused spawn.
        writer.spawn({{}, {}, true, true, true, 0});
        require(view.contains(old) && view.entities().size() == 1, "recording does not mutate");
        bool rejected = false;
        try { world.spawn({}); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "captured World cannot spawn during decisions");
        rejected = false;
        try { world.retry_flush(); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "reentrant flush is forbidden");
        rejected = false;
        try { world.step(0.25); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "reentrant frame is forbidden");
        rejected = false;
        try { world.destroy(old); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "captured World cannot delete during decisions");
        rejected = false;
        try { world.discard_pending(); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "cannot discard recording while active");
        rejected = false;
        try { (void)world.take_results(); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "cannot reset receipts while active");
    });
    auto report = world.take_results();
    require(report.created.size() == 1 && report.destroyed == 1 && report.ignored_destroy == 5
            && report.ignored_spawn == 1 && world.pending() == 0, "FIFO results and rejected targets");
    const auto fresh = report.created.at(0);
    require(fresh.slot() == old.slot() && fresh.generation() > old.generation()
            && world.contains(fresh) && !world.contains(old), "queued stale deletion protects replacement");
    require(near(world.inspect(fresh)->position.x, 10), "spawn does not move in creating frame");
    world.step(0.25);
    require(near(world.inspect(fresh)->position.x, 11), "spawn moves next frame");
    world.step(0.25, [&](const auto&, auto& writer) {
        writer.spawn({{20, 0}, {}, false, true, false, 0}); writer.destroy(fresh);
    });
    report = world.take_results();
    require(report.created.at(0).slot() != fresh.slot(), "spawn-before-destroy cannot reuse still-live slot");
    const auto counts = world.component_counts();
    require(counts.positions == 1 && counts.velocities == 0 && counts.lifetimes == 0
            && counts.visible == 1, "queued deletion clears every store");
}
void recording_failure() {
    lesson::deferred::World world;
    const auto old = world.spawn({{}, {2, 0}, true, true, false, 0});
    bool failed = false;
    try {
        world.step(0.5, [&](const auto&, auto& writer) {
            writer.destroy(old); writer.spawn({{10, 0}, {}, false, true, false, 0});
            throw std::runtime_error("decision failed");
        });
    } catch (const std::runtime_error&) { failed = true; }
    require(failed && world.pending() == 2 && world.contains(old)
            && near(world.inspect(old)->position.x, 1), "simulation committed; recording retained, not flushed");
    bool blocked = false;
    try { world.step(0.5); } catch (const std::logic_error&) { blocked = true; }
    require(blocked, "pending work blocks next simulation");
    blocked = false;
    try { (void)world.take_results(); } catch (const std::logic_error&) { blocked = true; }
    require(blocked, "pending work blocks receipt reset");
    world.retry_flush();
    require(!world.contains(old) && world.pending() == 0
            && near(world.inspect(world.results().created.at(0))->position.x, 10),
            "retry applies only recorded work, not another time step");
    world.take_results();
    try { world.step(0.25, [](const auto&, auto& writer) {
        writer.spawn({}); throw std::runtime_error("cancel this decision");
    }); } catch (const std::runtime_error&) {}
    world.discard_pending();
    require(world.pending() == 0 && world.entities().size() == 1, "discard does not spawn or roll back simulation");
    world.step(0.25); // Exception guard restored the idle phase.
}
void playback_failure() {
    // Controlled pre-mutation failure exercises queue bookkeeping, not bad_alloc.
    struct Target {
        lesson::generational::World<> world;
        bool fail = true;
        lesson::deferred::Entity spawn(const lesson::Seed& seed) {
            if (fail && seed.position.x == 99) throw std::runtime_error("injected spawn failure");
            return world.spawn(seed);
        }
        bool destroy(const lesson::deferred::Entity& entity) { return world.destroy(entity); }
    } target;
    lesson::deferred::CommandBuffer commands;
    const auto old = target.world.spawn({});
    commands.destroy(old); commands.spawn({{1, 0}, {}, false, true, false, 0});
    commands.spawn({{99, 0}, {}, false, true, false, 0}); commands.destroy(old);
    bool failed = false;
    try { commands.flush(target); } catch (const std::runtime_error&) { failed = true; }
    require(failed && commands.pending() == 2 && commands.results().destroyed == 1
            && commands.results().created.size() == 1 && target.world.entities().size() == 1,
            "successful prefix committed; failed front and suffix retained");
    target.fail = false;
    commands.flush(target);
    const auto report = commands.take_results();
    require(report.created.size() == 2 && report.destroyed == 1 && report.ignored_destroy == 1
            && commands.pending() == 0 && target.world.entities().size() == 2,
            "retry does not replay successful prefix or lose its receipts");
    require(near(target.world.inspect(report.created.at(0))->position.x, 1) &&
            near(target.world.inspect(report.created.at(1))->position.x, 99), "receipt order");
    commands.flush(target);
    require(commands.results().created.empty(), "empty flush creates nothing");
    lesson::Seed copied{{7, 0}, {}, false, true, false, 0};
    commands.spawn(copied);
    copied.position.x = 8;
    commands.spawn({{99, 0}, {}, false, true, false, 0});
    target.fail = true;
    failed = false;
    try { commands.flush(target); } catch (const std::runtime_error&) { failed = true; }
    require(failed && commands.pending() == 1 && target.world.entities().size() == 3 &&
            near(target.world.inspect(commands.results().created.at(0))->position.x, 7),
            "command owns a seed copy and commits prefix before failure");
    commands.discard_pending();
    require(commands.take_results().created.size() == 1 && target.world.entities().size() == 3,
            "discard preserves committed prefix and its receipts");
}
} // namespace
int main() {
    try {
        baseline_contract(); boundaries_and_fifo(); recording_failure(); playback_failure();
        std::cout << "PASS: frame boundaries, FIFO, handles, equivalence, and failure recovery\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
