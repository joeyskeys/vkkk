#pragma once

namespace vkkk
{

class InstanceAttr {
public:
    virtual ~InstanceAttr() = default;
    virtual size_t size() const = 0;
};

}