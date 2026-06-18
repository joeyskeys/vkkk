#pragma once

namespace vkkk
{

class UBOBase {
public:
    virtual ~UBOBase() = default;
    virtual size_t size() const = 0;
};

}