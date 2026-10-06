#pragma once
#include "simulation.hpp"
#include <limits>
#include <type_traits>

namespace lesson::storage {
using Slot = std::size_t;
template<class T>
class SparseStore {
    static_assert(std::is_trivially_copyable_v<T> &&
                  std::is_nothrow_copy_constructible_v<T> &&
                  std::is_nothrow_copy_assignable_v<T>, "this lesson supports simple value components");
public:
    struct Entry { Slot slot; T value; };
private:
    static constexpr std::size_t absent = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> sparse_;
    std::vector<Entry> dense_;
public:
    explicit SparseStore(Slot maximum_slot) {
        if (maximum_slot == absent) throw std::length_error("slot bound cannot be incremented");
        sparse_.assign(maximum_slot + 1, absent);
    }
    SparseStore(const SparseStore&) = delete;
    SparseStore& operator=(const SparseStore&) = delete;
    SparseStore(SparseStore&&) = delete;
    SparseStore& operator=(SparseStore&&) = delete;
    bool contains(Slot slot) const {
        if (slot == 0 || slot >= sparse_.size()) return false;
        const auto index = sparse_[slot];
        return index < dense_.size() && dense_[index].slot == slot;
    }
    T* find(Slot slot) {
        return contains(slot) ? &dense_[sparse_[slot]].value : nullptr;
    }
    const T* find(Slot slot) const {
        return contains(slot) ? &dense_[sparse_[slot]].value : nullptr;
    }
    bool insert(Slot slot, T value) {
        if (slot == 0 || slot >= sparse_.size()) throw std::out_of_range("slot outside configured bound");
        if (contains(slot)) return false; // Do not silently replace existing data.
        const auto index = dense_.size();
        dense_.push_back({slot, value}); // If allocation fails, membership stays unchanged.
        sparse_[slot] = index;
        return true;
    }
    bool erase(Slot slot) {
        if (!contains(slot)) return false;
        const auto index = sparse_[slot];
        const auto last = dense_.size() - 1;
        if (index != last) {
            dense_[index] = dense_[last];
            sparse_[dense_[index].slot] = index;
        }
        dense_.pop_back();
        sparse_[slot] = absent;
        return true;
    }
    std::size_t size() const { return dense_.size(); }
    // Borrowed view: no insertion/deletion while traversing this vector.
    const std::vector<Entry>& entries() const { return dense_; }
};

inline void movement(SparseStore<Vec2>& positions, const SparseStore<Vec2>& velocities,
                     double dt) {
    validate_dt(dt);
    for (const auto& entry : velocities.entries())
        if (auto* position = positions.find(entry.slot)) move(*position, entry.value, dt);
}
} // namespace lesson::storage
