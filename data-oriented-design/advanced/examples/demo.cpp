#include "generational_ids.hpp"
#include "sparse_store.hpp"
#include <iostream>
int main() {
    course::reuse::Pool<2> pool;
    const auto old = pool.allocate(); pool.release(old);
    const auto fresh = pool.allocate();
    std::cout << "reused slot=" << fresh.slot << " generation=" << fresh.generation
              << " old alive=" << pool.contains(old) << '\n';
    course::storage::SparseStore<course::Vec2> positions(4), velocities(4);
    positions.insert(1, {0, 0}); positions.insert(2, {10, 0}); velocities.insert(2, {2, 0});
    positions.erase(1); course::storage::movement(positions, velocities, 0.5);
    std::cout << "after swap erase: slot=2 x=" << positions.find(2)->x << '\n';
}
