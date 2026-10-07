#pragma once
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace course::reuse {
struct Marker {};
struct Handle {
    std::size_t slot = 0;
    std::uint64_t generation = 0;
    std::shared_ptr<const Marker> owner;
};
// Metadata experiment only: the caller must remove component data before release.
template<std::uint64_t Limit = std::numeric_limits<std::uint64_t>::max()>
class Pool {
    static_assert(Limit > 0);
    struct Slot { std::uint64_t generation = 1; std::size_t next = 0; bool alive = false; };
    std::shared_ptr<const Marker> owner_ = std::make_shared<Marker>();
    std::vector<Slot> slots_{Slot{}}; // Zero is never allocated.
    std::size_t free_ = 0;
public:
    Pool() = default;
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;
    Handle allocate() {
        std::size_t index = free_;
        if (index) {
            free_ = slots_[index].next;
            slots_[index].alive = true;
        } else {
            index = slots_.size();
            slots_.push_back({1, 0, true});
        }
        return {index, slots_[index].generation, owner_};
    }
    bool contains(const Handle& handle) const {
        return handle.owner == owner_ && handle.slot > 0 && handle.slot < slots_.size()
            && slots_[handle.slot].alive && slots_[handle.slot].generation == handle.generation;
    }
    bool release(const Handle& handle) {
        if (!contains(handle)) return false;
        auto& slot = slots_[handle.slot]; slot.alive = false;
        if (slot.generation < Limit) {
            ++slot.generation; slot.next = free_; free_ = handle.slot;
        } // At the limit, retire rather than wrap and revive an old handle.
        return true;
    }
};
} // namespace course::reuse
