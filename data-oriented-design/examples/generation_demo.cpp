#include "generational_ecs.hpp"
#include <iostream>

int main() {
    lesson::generational::World<> world;
    const auto old = world.spawn({{10, 0}, {2, 0}, true, true, true, 0.25});
    world.spawn({{20, 0}, {}, false, true, false, 0});
    world.step(0.25);
    const auto replacement = world.spawn({{30, 0}, {}, false, true, false, 0});
    std::cout << "old: slot=" << old.slot() << " generation=" << old.generation() << '\n';
    std::cout << "new: slot=" << replacement.slot()
              << " generation=" << replacement.generation() << '\n';
    std::cout << "old handle resolves=" << world.contains(old) << '\n';
    std::cout << "stale delete succeeds=" << world.destroy(old) << '\n';
    for (const auto position : world.render())
        std::cout << "visible position: " << position.x << ',' << position.y << '\n';
    // A small test limit exercises the real retirement path, not counter wrap.
    lesson::generational::World<2> small;
    auto handle = small.spawn({{}, {}, false, false, false, 0});
    small.destroy(handle);
    handle = small.spawn({{}, {}, false, false, false, 0});
    small.destroy(handle);
    handle = small.spawn({{}, {}, false, false, false, 0});
    std::cout << "after retirement: slot=" << handle.slot()
              << " generation=" << handle.generation()
              << " retired=" << small.slot_counts().retired << '\n';
}
