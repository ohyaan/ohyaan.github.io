#include "sparse_store.hpp"
#include "generational_ecs.hpp"
#include <iostream>
#include <map>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
void mapping_contract() {
    static_assert(!std::is_copy_constructible_v<lesson::storage::SparseStore<int>>);
    static_assert(!std::is_move_constructible_v<lesson::storage::SparseStore<int>>);
    lesson::storage::SparseStore<int> store(32);
    std::map<std::size_t, int> reference;
    for (std::size_t turn = 0; turn < 1024; ++turn) {
        const auto slot = (turn * 7) % 32 + 1;
        if ((turn / 32) % 3 == 0) {
            require(store.erase(slot) == (reference.erase(slot) != 0), "erase agrees with map");
        } else {
            const int value = static_cast<int>(turn);
            require(store.insert(slot, value) == reference.emplace(slot, value).second,
                    "insert/duplicate agrees with map");
        }
        require(store.size() == reference.size(), "size matches");
        for (std::size_t key = 1; key <= 32; ++key) {
            const auto value = store.find(key); const auto expected = reference.find(key);
            require(bool(value) == (expected != reference.end()), "membership matches");
            if (value) require(*value == expected->second, "value matches");
        }
        for (const auto& entry : store.entries())
            require(reference.at(entry.slot) == entry.value, "dense reverse mapping");
    }
    for (std::size_t slot = 1; slot <= 32; ++slot) store.erase(slot);
    require(store.size() == 0 && !store.erase(1), "empty and repeat erase");
    require(!store.contains(0) && !store.find(33), "lookup outside bound");
    for (std::size_t slot : {std::size_t{0}, std::size_t{33}}) {
        bool rejected = false;
        try { store.insert(slot, 1); } catch (const std::out_of_range&) { rejected = true; }
        require(rejected && store.size() == 0, "invalid insertion changes nothing");
    }
    bool rejected = false;
    try { lesson::storage::SparseStore<int> invalid(std::numeric_limits<std::size_t>::max()); }
    catch (const std::length_error&) { rejected = true; }
    require(rejected, "bound increment cannot wrap");
    store.insert(1, 1); store.insert(2, 2); store.insert(3, 3);
    store.erase(1);
    require(store.entries().front().slot == 3 && *store.find(3) == 3, "swap repair");
    store.erase(2); store.erase(3); // Covers final-element deletion.
    require(store.insert(3, 99) && *store.find(3) == 99, "reinsertion after deletion");
    const auto& read_only = store;
    require(read_only.find(3) && *read_only.find(3) == 99, "const lookup");
}
void matching_contract() {
    lesson::storage::SparseStore<lesson::Vec2> positions(16), velocities(16);
    std::vector<lesson::Seed> seeds;
    for (std::size_t bits = 0; bits < 8; ++bits) {
        seeds.push_back({{double(bits), 1}, {2, -1}, bool(bits & 1), bool(bits & 2),
                         bool(bits & 4), 10});
        positions.insert(bits + 1, seeds.back().position);
        if (bits & 1) velocities.insert(bits + 1, seeds.back().velocity);
    }
    velocities.insert(16, {100, 100}); // Does not match any Position.
    lesson::plain::World baseline(seeds);
    for (double dt : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { lesson::storage::movement(positions, velocities, dt); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected && near(positions.find(2)->x, 1), "invalid dt before movement");
    }
    for (int step = 1; step <= 4; ++step) {
        baseline.step(0.25); lesson::storage::movement(positions, velocities, 0.25);
        const auto expected = baseline.states();
        for (std::size_t bits = 0; bits < 8; ++bits) {
            const auto value = positions.find(bits + 1);
            require(value && near(value->x, expected[bits].position.x)
                    && near(value->y, expected[bits].position.y), "baseline position projection");
            require(near(value->x, bits + ((bits & 1) ? step * 0.5 : 0)), "independent trajectory");
        }
    }
    require(positions.size() == 8 && !positions.contains(16), "join does not create missing positions");
}
void identity_boundary() {
    lesson::generational::World<> world, other;
    lesson::storage::SparseStore<lesson::Vec2> store(8);
    const auto old = world.spawn({});
    store.insert(static_cast<std::size_t>(old.slot()), {10, 0});
    require(world.destroy(old), "destroy old occupant");
    store.erase(static_cast<std::size_t>(old.slot())); // Required before any reuse.
    const auto fresh = world.spawn({});
    store.insert(static_cast<std::size_t>(fresh.slot()), {20, 0});
    const auto foreign = other.spawn({});
    auto observe = [&](const lesson::generational::Entity& entity) -> std::optional<lesson::Vec2> {
        if (!world.contains(entity) || entity.slot() > 8) return std::nullopt;
        const auto value = store.find(static_cast<std::size_t>(entity.slot()));
        return value ? std::optional<lesson::Vec2>{*value} : std::nullopt;
    };
    require(old.slot() == fresh.slot() && store.contains(static_cast<std::size_t>(old.slot()))
            && !observe(old) && !observe(foreign) && observe(fresh)->x == 20,
            "membership is not identity; validate full handle before lookup");
}
} // namespace
int main() {
    try {
        mapping_contract(); matching_contract(); identity_boundary();
        std::cout << "PASS: sparse/dense mapping, deletion, matching, and identity boundary\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
