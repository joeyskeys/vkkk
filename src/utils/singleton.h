#pragma once

// Code copied from appleseed/src/appleseed/foundation/core/concepts/singleton.h

namespace vkkk
{

template <typename T>
class Singleton {
public:
    using ObjectType = T;

    static ObjectType& instance() {
        static ObjectType object;
        return object;
    }

protected:
    Singleton() {}
    virtual ~Singleton() {}
};

}