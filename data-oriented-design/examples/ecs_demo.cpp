#include "minimal_ecs.hpp"
#include <iostream>

int main() {
    lesson::ecs::World world;
    world.spawn({{0, 0}, {2, 0}, true, true, false, 0});
    const auto transient = world.spawn({{10, 0}, {-2, 0}, true, true, true, 0.5});
    world.spawn({{5, 5}, {}, false, true, false, 0});
    for (int step = 1; step <= 2; ++step) {
        world.step(0.25);
        const auto counts = world.component_counts();
        std::cout << "after " << step * 0.25 << "s: positions=" << counts.positions
                  << " velocities=" << counts.velocities
                  << " lifetimes=" << counts.lifetimes
                  << " visible=" << counts.visible << '\n';
    }
    std::cout << "expired handle resolves=" << world.contains(transient) << '\n';
    for (const auto position : world.render())
        std::cout << "visible position: " << position.x << ',' << position.y << '\n';
}
