#pragma once

class UBOBase {
public:
    virtual ~UBOBase() = default;
    virtual void sync(uint32_t swapchain_idx) = 0;
};