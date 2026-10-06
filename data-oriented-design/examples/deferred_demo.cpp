#include "deferred_ecs.hpp"
#include <iostream>

int main() {
    lesson::deferred::World world;
    const auto old = world.spawn({{}, {2, 0}, true, true, false, 0});
    world.step(0.5, [&](const auto& view, auto& commands) {
        const auto state = view.inspect(old);
        commands.destroy(old);
        commands.spawn({state->position, {4, 0}, true, true, false, 0});
        std::cout << "during decisions: old alive=" << view.contains(old)
                  << " position=" << state->position.x << '\n';
    });
    const auto receipts = world.take_results();
    const auto replacement = receipts.created.at(0);
    std::cout << "after flush: old alive=" << world.contains(old)
              << " replacement position=" << world.inspect(replacement)->position.x << '\n';
    std::cout << "same slot=" << (old.slot() == replacement.slot())
              << " new generation=" << replacement.generation() << '\n';
    world.step(0.25);
    std::cout << "next frame: replacement position=" << world.inspect(replacement)->position.x << '\n';
}
