#include "sparse_store.hpp"
#include <iostream>

int main() {
    lesson::storage::SparseStore<lesson::Vec2> positions(8), velocities(8);
    positions.insert(1, {10, 0}); positions.insert(2, {20, 0}); positions.insert(3, {30, 0});
    velocities.insert(1, {2, 0}); velocities.insert(3, {4, 0}); velocities.insert(4, {100, 0});
    positions.erase(1); // Last entry moves into the hole; its sparse index is repaired.
    positions.insert(1, {40, 0}); // Store membership alone is not a generation check.
    std::cout << "dense slots:";
    for (const auto& entry : positions.entries()) std::cout << ' ' << entry.slot;
    std::cout << '\n';
    lesson::storage::movement(positions, velocities, 0.5);
    for (const auto slot : {1U, 2U, 3U})
        std::cout << "slot=" << slot << " position=" << positions.find(slot)->x << '\n';
    std::cout << "unmatched slot 4 exists=" << positions.contains(4) << '\n';
}
