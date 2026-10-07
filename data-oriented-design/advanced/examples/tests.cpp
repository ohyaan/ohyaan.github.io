#include "generational_ids.hpp"
#include "sparse_store.hpp"
#include <iostream>
#include <map>
#include <string>
void check(bool value) { if (!value) throw std::runtime_error("optional invariant failed"); }
void generations() {
    course::reuse::Pool<2> pool, other;
    const auto old = pool.allocate(), foreign = other.allocate();
    check(!pool.contains(foreign) && !pool.contains({}));
    check(pool.release(old) && !pool.release(old));
    const auto fresh = pool.allocate();
    check(fresh.slot == old.slot && fresh.generation == 2 && !pool.contains(old));
    check(pool.release(fresh));
    const auto retired = pool.allocate(); check(retired.slot != fresh.slot);
    for (int i = 0; i < 256; ++i) pool.allocate();
    check(pool.contains(retired));
    course::reuse::Pool<1> no_reuse;
    const auto first = no_reuse.allocate(); no_reuse.release(first);
    check(no_reuse.allocate().slot != first.slot);
    course::reuse::Handle orphan;
    { course::reuse::Pool<> temporary; orphan = temporary.allocate(); }
    check(!other.contains(orphan));
}
void storage() {
    course::storage::SparseStore<course::Vec2> store(32);
    std::map<std::size_t, course::Vec2> oracle;
    for (std::size_t n = 0; n < 1024; ++n) {
        const auto slot = (n * 17) % 32 + 1;
        if (n % 3) {
            const course::Vec2 value{double(n), -double(n)};
            check(store.insert(slot, value) == oracle.emplace(slot, value).second);
        } else { check(store.erase(slot) == bool(oracle.erase(slot))); }
        check(store.size() == oracle.size());
        for (std::size_t k = 1; k <= 32; ++k) {
            const auto* value = store.find(k); const auto it = oracle.find(k);
            check(bool(value) == (it != oracle.end()));
            if (value) check(value->x == it->second.x && value->y == it->second.y);
        }
    }
    check(!store.find(0) && !store.find(33));
    bool rejected = false; try { store.insert(33, {}); } catch (const std::out_of_range&) { rejected = true; }
    check(rejected);
    // A slot is not identity. Validate the full handle and clean data before release.
    course::reuse::Pool<> pool; course::storage::SparseStore<course::Vec2> positions(4);
    const auto old = pool.allocate(); positions.insert(old.slot, {9, 0});
    positions.erase(old.slot); pool.release(old);
    const auto fresh = pool.allocate(); positions.insert(fresh.slot, {1, 0});
    check(!pool.contains(old) && pool.contains(fresh) && positions.find(fresh.slot)->x == 1);
}
int main(int argc, char** argv) {
    try {
        const std::string group = argc > 1 ? argv[1] : "all";
        if (group == "all" || group == "generations") generations();
        if (group == "all" || group == "storage") storage();
        if (group != "all" && group != "generations" && group != "storage") throw std::invalid_argument("unknown group");
        std::cout << "PASS: " << group << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
