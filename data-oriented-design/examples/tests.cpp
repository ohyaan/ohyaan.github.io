#include "deferred.hpp"
#include <iostream>
#include <string>
#include <type_traits>

namespace {
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
void equal(const std::vector<course::Seed>& a, const std::vector<course::Seed>& b) {
    require(a.size() == b.size(), "survivor count");
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto& x = a[i]; const auto& y = b[i];
        require(near(x.position.x, y.position.x) && near(x.position.y, y.position.y)
                && near(x.velocity.x, y.velocity.x) && near(x.velocity.y, y.velocity.y)
                && x.moving == y.moving && x.visible == y.visible && x.expiring == y.expiring
                && near(x.remaining, y.remaining) && bool(x.acceleration) == bool(y.acceleration), "snapshot fields/order");
        if (x.acceleration) require(near(x.acceleration->x, y.acceleration->x)
                                  && near(x.acceleration->y, y.acceleration->y), "acceleration value");
    }
}
template<class World> void invalid_steps(World& world) {
    const auto before = world.states();
    for (double dt : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { world.step(dt); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid dt rejected"); equal(before, world.states());
    }
}
void baseline() {
    std::vector<course::BasicSeed> seeds;
    for (int bits = 0; bits < 8; ++bits)
        seeds.push_back({{double(bits), 1}, {2, -1}, bool(bits & 1), bool(bits & 2), bool(bits & 4), 0.5});
    seeds.push_back({{}, {}, false, true, true, 0});
    course::Baseline world(seeds);
    require(world.states().size() == 8, "initial expiration");
    world.step(0.25);
    const auto state = world.states();
    for (int bits = 0; bits < 8; ++bits) {
        require(near(state[static_cast<std::size_t>(bits)].position.x, bits + ((bits & 1) ? 0.5 : 0)), "baseline trajectory");
    }
    world.step(0.25); require(world.states().size() == 4, "baseline expiration");
    const auto before = world.states();
    for (double dt : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false; try { world.step(dt); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "baseline invalid dt");
        const auto after = world.states(); require(after.size() == before.size(), "baseline invalid dt membership");
        for (std::size_t i = 0; i < before.size(); ++i)
            require(near(after[i].position.x, before[i].position.x) && near(after[i].position.y, before[i].position.y)
                    && near(after[i].remaining, before[i].remaining), "baseline invalid dt leaves state unchanged");
    }
}
void acceleration() {
    course::RecordWorld world(course::fixture()); invalid_steps(world);
    world.step(0.5);
    require(near(world.states().at(0).velocity.x, 2) && near(world.states().at(0).position.x, 1), "acceleration before movement");
    world.step(0.5);
    require(near(world.states().at(0).velocity.x, 3) && near(world.states().at(0).position.x, 2.5)
            && world.states().size() == 2, "second trajectory and expiration");
    std::vector<course::Seed> no_acceleration;
    std::vector<course::BasicSeed> basic;
    for (auto s : course::fixture()) { s.acceleration.reset(); no_acceleration.push_back(s); basic.push_back(s); }
    course::Baseline original(basic); course::RecordWorld revised(no_acceleration);
    for (int i = 0; i < 4; ++i) {
        original.step(0.25); revised.step(0.25);
        const auto a = original.states(); const auto b = revised.states(); require(a.size() == b.size(), "old contract retained");
        for (std::size_t j = 0; j < a.size(); ++j) require(near(a[j].position.x, b[j].position.x), "no acceleration equivalence");
    }
}
void alternatives() {
    bool rejected = false;
    try { course::inheritance::AcceleratedBody invalid(course::fixture().at(2)); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "direct accelerated construction requires acceleration");
    rejected = false;
    try { course::inheritance::UniformBody invalid(course::fixture().at(0)); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "direct uniform construction rejects acceleration");
    std::vector<course::Seed> seeds;
    for (int bits = 0; bits < 16; ++bits) {
        if ((bits & 8) && !(bits & 1)) continue;
        seeds.push_back(course::seed({{double(bits), 1}, {2, -1}, bool(bits & 1), bool(bits & 2), bool(bits & 4), 0.75},
                                    (bits & 8) ? std::optional<course::Vec2>{{2, -2}} : std::nullopt));
    }
    course::RecordWorld record(seeds); course::composition::World composition(seeds);
    course::inheritance::World inheritance(seeds); course::IdentityWorld identity(seeds); course::EcsWorld ecs(seeds);
    invalid_steps(composition); invalid_steps(inheritance); invalid_steps(identity); invalid_steps(ecs);
    for (int n = 0; n < 5; ++n) {
        equal(record.states(), composition.states()); equal(record.states(), inheritance.states());
        equal(record.states(), identity.states()); equal(record.states(), ecs.states());
        std::size_t index = 0;
        for (const auto& initial : seeds) {
            if (initial.expiring && n >= 3) continue;
            const auto value = record.states().at(index++);
            const double elapsed = n * 0.25;
            const double ax = initial.acceleration ? 0.0625 * n * (n + 1) : 0;
            require(near(value.position.x, initial.position.x + (initial.moving ? 2 * elapsed + ax : 0)), "independent x expectation");
            require(near(value.position.y, initial.position.y + (initial.moving ? -elapsed - ax : 0)), "independent y expectation");
            require(near(value.velocity.x, initial.moving ? 2 + (initial.acceleration ? 0.5 * n : 0) : 0), "independent velocity expectation");
        }
        record.step(0.25); composition.step(0.25); inheritance.step(0.25); identity.step(0.25); ecs.step(0.25);
    }
}
void identity() {
    static_assert(!std::is_copy_constructible_v<course::IdentityWorld>);
    static_assert(!std::is_move_constructible_v<course::IdentityWorld>);
    course::IdentityWorld world, other;
    const auto a = world.spawn(course::fixture().at(0)), b = world.spawn(course::fixture().at(2));
    const auto foreign = other.spawn(course::fixture().at(2));
    require(a.number() == foreign.number() && !world.contains(foreign) && !world.destroy(foreign), "owner check with collision");
    require(!world.contains({}) && !world.inspect({}), "empty handle");
    const auto copy = a; const auto saved = world.inspect(b);
    world.destroy(a); require(!world.contains(copy) && !world.destroy(copy), "copied stale handle");
    for (int i = 0; i < 256; ++i) world.spawn(course::fixture().at(2));
    world.step(0.5); require(world.contains(b) && near(saved->position.x, 5), "growth, deletion, snapshot copies");
    course::Handle orphan;
    { course::IdentityWorld temporary; orphan = temporary.spawn(course::fixture().at(2)); }
    require(!other.contains(orphan), "dead world marker cannot be reused");
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    require(course::successor(maximum - 1) == maximum, "last valid number");
    bool rejected = false; try { course::successor(maximum); } catch (const std::overflow_error&) { rejected = true; }
    require(rejected, "counter does not wrap");
}
void ecs() {
    course::EcsWorld world;
    auto body = course::fixture().at(0); body.expiring = true; body.remaining = 0.5;
    const auto handle = world.spawn(body);
    std::vector<course::Trace> trace; world.step(0.5, &trace);
    require(trace.size() == 4 && trace[0].stage == course::Stage::acceleration
            && trace[1].stage == course::Stage::movement && trace[2].stage == course::Stage::lifetime
            && trace[3].stage == course::Stage::removed && near(trace[3].position.x, 1), "final movement and stage order");
    const auto c = world.counts(); require(!world.contains(handle) && c.positions == 0 && c.velocities == 0
        && c.accelerations == 0 && c.lifetimes == 0 && c.visible == 0, "every store removed");
}
void deferred() {
    course::DeferredWorld world; const auto old = world.spawn(course::fixture().at(0));
    world.step(0.5, [&](const auto& view, auto& writer) {
        auto body = *view.inspect(old); writer.destroy(old); writer.spawn(body); writer.destroy(old);
        require(view.contains(old) && near(view.inspect(old)->position.x, 1), "stable recording membership");
        bool rejected = false; try { world.destroy(old); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "reentrant mutation blocked");
    });
    auto receipt = world.take_results(); require(receipt.destroyed == 1 && receipt.ignored == 1 && receipt.created.size() == 1, "FIFO receipts");
    const auto fresh = receipt.created.at(0); require(!world.contains(old) && fresh.number() > old.number(), "no ID reuse needed");
    require(near(world.inspect(fresh)->position.x, 1), "new entity did not move in creating frame");
    world.step(0.5); require(near(world.inspect(fresh)->position.x, 2.5), "next-frame update");
    bool failed = false;
    try { world.step(0.5, [&](const auto&, auto& writer) { writer.destroy(fresh); throw std::runtime_error("decision fault"); }); }
    catch (const std::runtime_error&) { failed = true; }
    require(failed && world.pending() == 1 && near(world.inspect(fresh)->position.x, 4.5), "failure retains unapplied work after simulation");
    bool blocked = false; try { world.step(0.5); } catch (const std::logic_error&) { blocked = true; }
    require(blocked, "pending work blocks another frame"); world.retry(); require(!world.contains(fresh), "retry is flush only");
    world.take_results();
    try { world.step(0.5, [](const auto&, auto& writer) { writer.spawn(course::fixture().at(2)); throw std::runtime_error("cancel"); }); }
    catch (const std::runtime_error&) {}
    world.discard(); require(world.states().empty(), "discard affects only pending work");
}
void debug() {
    auto body = course::fixture().at(0); body.visible = false;
    const auto broken = course::broken_visibility_update(body, 0.5);
    course::RecordWorld record({body}); course::EcsWorld ecs({body});
    record.step(0.5); std::vector<course::Trace> trace; ecs.step(0.5, &trace);
    require(near(broken.position.x, 0) && !near(broken.position.x, 1), "intentional bug is reproduced, not accepted");
    require(near(record.states().at(0).position.x, 1) && trace.size() == 2
            && near(trace[0].velocity.x, 2) && near(trace[1].position.x, 1), "trace identifies writers");
    require(course::render(ecs.states()).empty(), "visibility affects rendering only");
}
void maintenance() {
    course::EcsWorld world(course::fixture()); const auto before = world.states();
    auto invalid = course::fixture().at(0); invalid.moving = false;
    bool rejected = false; try { world.spawn(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "stationary acceleration invalid"); equal(before, world.states());
    invalid = course::fixture().at(0); invalid.acceleration->x = std::numeric_limits<double>::infinity();
    rejected = false; try { world.spawn(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "nonfinite component rejected"); equal(before, world.states());
    struct Target {
        course::EcsWorld world; bool fail = true;
        course::Handle spawn(course::Seed body) {
            if (fail && body.position.x == 99) throw std::runtime_error("controlled pre-mutation fault");
            return world.spawn(body);
        }
        bool destroy(course::Handle h) { return world.destroy(h); }
    } target;
    course::Commands commands; commands.spawn(course::fixture().at(2));
    auto body = course::fixture().at(2); body.position.x = 99; commands.spawn(body);
    rejected = false; try { commands.flush(target); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected && commands.pending() == 1 && commands.results().created.size() == 1, "committed prefix and retained suffix");
    target.fail = false; commands.flush(target);
    require(commands.take_results().created.size() == 2 && target.world.states().size() == 2, "retry does not duplicate prefix");
}
} // namespace
int main(int argc, char** argv) {
    const std::string group = argc > 1 ? argv[1] : "all";
    bool found = false;
    try {
        const std::pair<const char*, void(*)()> cases[] = {{"baseline", baseline}, {"acceleration", acceleration},
            {"alternatives", alternatives}, {"identity", identity}, {"ecs", ecs}, {"deferred", deferred}, {"debug", debug}, {"maintenance", maintenance}};
        for (const auto& test : cases) if (group == "all" || group == test.first) {
            found = true; test.second(); std::cout << "PASS: " << test.first << '\n';
        }
        require(found, "unknown test group"); return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
