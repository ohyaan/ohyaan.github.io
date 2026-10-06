#include "entity_identity.hpp"
#include <iostream>

int main() {
    lesson::identity::World world;
    const auto first = world.spawn({{0, 0}, {}, false, true, false, 0});
    const auto tracked = world.spawn({{10, 0}, {2, 0}, true, true, false, 0});
    world.destroy(first);
    world.reserve(world.storage_capacity() + 1);
    world.step(0.5);
    const auto snapshot = world.inspect(tracked);
    if (!snapshot) return 1;
    std::cout << "tracked number=" << tracked.number()
              << " position=" << snapshot->position.x << ','
              << snapshot->position.y << '\n';
    world.destroy(tracked);
    const auto replacement = world.spawn({{20, 0}, {}, false, true, false, 0});
    std::cout << "deleted handle resolves=" << world.contains(tracked) << '\n';
    std::cout << "replacement number=" << replacement.number() << '\n';
    lesson::identity::World other;
    const auto foreign = other.spawn({{99, 0}, {}, false, true, false, 0});
    std::cout << "foreign handle resolves=" << world.contains(foreign) << '\n';
}
