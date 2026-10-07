#include "deferred.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "acceleration";
    if (mode == "baseline") {
        course::Baseline world({{{0, 0}, {2, 0}, true, true, false, 0},
                                {{10, 0}, {-1, 0}, true, false, true, 1}});
        world.step(0.5);
        std::cout << "baseline: bodies=" << world.states().size()
                  << " first x=" << world.states().at(0).position.x << '\n';
    } else if (mode == "acceleration" || mode == "alternatives" || mode == "ecs") {
        course::RecordWorld record(course::fixture());
        course::composition::World composition(course::fixture());
        course::inheritance::World inheritance(course::fixture());
        course::EcsWorld ecs(course::fixture());
        for (int i = 1; i <= 2; ++i) {
            record.step(0.5); composition.step(0.5); inheritance.step(0.5); ecs.step(0.5);
            const auto body = record.states().at(0);
            std::cout << "t=" << i * 0.5 << " x=" << body.position.x
                      << " vx=" << body.velocity.x << " survivors=" << record.states().size() << '\n';
        }
        std::cout << "alternatives x: " << composition.states().at(0).position.x << ' '
                  << inheritance.states().at(0).position.x << ' ' << ecs.states().at(0).position.x << '\n';
    } else if (mode == "identity") {
        course::IdentityWorld world;
        const auto a = world.spawn(course::fixture().at(0)), b = world.spawn(course::fixture().at(2));
        world.destroy(a); world.step(0.5);
        std::cout << "selected id=" << b.number() << " x=" << world.inspect(b)->position.x
                  << " deleted resolves=" << world.contains(a) << '\n';
    } else if (mode == "deferred") {
        course::DeferredWorld world;
        const auto old = world.spawn(course::fixture().at(0));
        world.step(0.5, [&](const auto& view, auto& commands) {
            const auto body = *view.inspect(old);
            commands.destroy(old); commands.spawn(body);
            std::cout << "during decisions: old alive=" << view.contains(old) << " x=" << body.position.x << '\n';
        });
        const auto created = world.take_results().created.at(0);
        std::cout << "after flush: old alive=" << world.contains(old) << " new x=" << world.inspect(created)->position.x << '\n';
        world.step(0.5);
        std::cout << "next frame: new x=" << world.inspect(created)->position.x << '\n';
    } else if (mode == "debug") {
        auto body = course::fixture().at(0); body.visible = false;
        const auto broken = course::broken_visibility_update(body, 0.5);
        course::EcsWorld correct;
        const auto selected = correct.spawn(body);
        std::vector<course::Trace> trace;
        correct.step(0.5, &trace);
        std::cout << "intentional broken specimen: x=" << broken.position.x << " expected=1 FAIL\n";
        for (const auto& event : trace)
            std::cout << "id=" << event.number << ' ' << course::stage_name(event.stage)
                      << " x=" << event.position.x << " vx=" << event.velocity.x << '\n';
        std::cout << "correct x=" << correct.inspect(selected)->position.x
                  << " rendered=" << course::render(correct.states()).size() << '\n';
    } else if (mode == "maintenance") {
        course::EcsWorld world;
        const auto selected = world.spawn(course::fixture().at(0));
        auto invalid = course::fixture().at(0); invalid.moving = false;
        try { world.spawn(invalid); } catch (const std::invalid_argument&) {
            std::cout << "invalid acceleration rejected; bodies=" << world.counts().positions << '\n';
        }
        world.destroy(selected);
        const auto counts = world.counts();
        std::cout << "after cleanup: positions=" << counts.positions << " velocities=" << counts.velocities
                  << " accelerations=" << counts.accelerations << '\n';
    } else { std::cerr << "unknown scenario\n"; return 2; }
}
