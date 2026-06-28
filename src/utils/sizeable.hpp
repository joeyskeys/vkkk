#pragma once

namespace vkkk
{

template <typename Derived>
class Sizeable {
public:
    size_t size() const {
        return sizeof(Derived);
    }

    const void* get_data() const {
        return static_cast<const void*>(static_cast<const Derived*>(this));
    }
};

}