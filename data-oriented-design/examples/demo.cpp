#include "simulation.hpp"
#include <iostream>

int main() {
    const std::vector<lesson::Seed> seeds{
        {{0, 0}, {2, 0}, true, true, false, 0},
        {{5, 5}, {}, false, true, false, 0},
        {{0, 1}, {0, 1}, true, false, true, 0.5}
    };
    lesson::plain::World world(seeds);
    world.step(0.25);
    std::cout << "after 0.25s: alive=" << world.states().size()
              << " visible=" << lesson::render(world.states()).size() << '\n';
    world.step(0.25);
    std::cout << "after 0.50s: alive=" << world.states().size()
              << " visible=" << lesson::render(world.states()).size() << '\n';
    for (auto position : lesson::render(world.states())) {
        std::cout << "visible position: " << position.x << ',' << position.y << '\n';
    }
}
