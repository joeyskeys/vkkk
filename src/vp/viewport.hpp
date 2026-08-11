#pragma once

#include <algorithm>
#include <cstdint>
#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "vp/feature.hpp"
#include "vk_ins/context.hpp"

namespace vkkk::vp
{

template <typename Feature>
struct FeatureHandle {
    uint64_t id = 0;
};

template <typename Feature>
struct FeatureEntry {
    uint64_t id = 0;
    Feature feature;
};

template <typename Feature>
struct FeatureStore {
    std::vector<FeatureEntry<Feature>> entries;
};

template <typename Feature>
concept ViewportFeatureType = requires(Feature feature, Context& context,
    vk::Extent2D extent, const Context::Frame& frame, vk::raii::CommandBuffer& cmd,
    uint32_t image_index)
{
    { Feature::phase } -> std::convertible_to<ViewportPhase>;
    feature.on_attach(context, extent);
    feature.on_resize(context, extent);
    feature.on_update(context, frame);
    feature.on_record(context, cmd, image_index);
};

// Non-owning viewport orchestration layer over an initialized Context.
// Per frame call: begin_frame -> update -> record_frame -> end_frame.
template <ViewportFeatureType... FeatureTypes>
class Viewport {
public:
    explicit Viewport(Context& context);

    bool begin_frame(Context::Frame& frame);
    void update(const Context::Frame& frame);
    void record_frame(const Context::Frame& frame);
    void end_frame(const Context::Frame& frame);

    template <ViewportFeatureType Feature, typename... Args>
        requires ((std::same_as<Feature, FeatureTypes> || ...))
    FeatureHandle<Feature> add_feature(Args&&... args);

    template <ViewportFeatureType Feature>
        requires ((std::same_as<Feature, FeatureTypes> || ...))
    Feature* find_feature(FeatureHandle<Feature> handle);

    template <ViewportFeatureType Feature>
        requires ((std::same_as<Feature, FeatureTypes> || ...))
    bool remove_feature(FeatureHandle<Feature> handle);

    vk::Extent2D extent() const { return viewport_extent; }
    Context& context() { return ctx; }
    const Context& context() const { return ctx; }

private:
    template <typename Feature>
    FeatureStore<Feature>& store() {
        return std::get<FeatureStore<Feature>>(feature_stores);
    }

    template <ViewportPhase Phase>
    void record_phase(vk::raii::CommandBuffer& cmd, uint32_t image_index);

    void resize_all();
    void update_all(const Context::Frame& frame);

    Context& ctx;
    vk::Extent2D viewport_extent{};
    uint64_t next_feature_id = 1;
    std::tuple<FeatureStore<FeatureTypes>...> feature_stores;
};

template <ViewportFeatureType... FeatureTypes>
Viewport<FeatureTypes...>::Viewport(Context& context)
    : ctx(context)
    , viewport_extent(context.extent())
{
}

template <ViewportFeatureType... FeatureTypes>
bool Viewport<FeatureTypes...>::begin_frame(Context::Frame& frame) {
    if (!ctx.begin_frame(frame)) {
        return false;
    }

    const auto current_extent = ctx.extent();
    if (current_extent != viewport_extent) {
        viewport_extent = current_extent;
        resize_all();
    }
    return true;
}

template <ViewportFeatureType... FeatureTypes>
void Viewport<FeatureTypes...>::update(const Context::Frame& frame) {
    update_all(frame);
}

template <ViewportFeatureType... FeatureTypes>
void Viewport<FeatureTypes...>::record_frame(const Context::Frame& frame) {
    ctx.record_frame(frame, [this](vk::raii::CommandBuffer& cmd, uint32_t image_index) {
        constexpr bool has_screen_overlay =
            ((FeatureTypes::phase == ViewportPhase::ScreenOverlay) || ...);

        PassDesc scene_pass{};
        scene_pass.present = !has_screen_overlay;
        ctx.begin_pass(cmd, image_index, scene_pass);
        record_phase<ViewportPhase::Scene>(cmd, image_index);
        ctx.end_pass(cmd, image_index, scene_pass);

        if constexpr (has_screen_overlay) {
            PassDesc overlay_pass{};
            overlay_pass.colors.front().load = PassLoadOp::Load;
            overlay_pass.depth_index = kNoDepth;
            ctx.begin_pass(cmd, image_index, overlay_pass);
            record_phase<ViewportPhase::ScreenOverlay>(cmd, image_index);
            ctx.end_pass(cmd, image_index, overlay_pass);
        }
    });
}

template <ViewportFeatureType... FeatureTypes>
void Viewport<FeatureTypes...>::end_frame(const Context::Frame& frame) {
    ctx.end_frame(frame);
}

template <ViewportFeatureType... FeatureTypes>
template <ViewportFeatureType Feature, typename... Args>
    requires ((std::same_as<Feature, FeatureTypes> || ...))
FeatureHandle<Feature> Viewport<FeatureTypes...>::add_feature(Args&&... args) {
    const FeatureHandle<Feature> handle{next_feature_id++};
    auto& entries = store<Feature>().entries;
    entries.push_back(FeatureEntry<Feature>{
        .id = handle.id,
        .feature = Feature{std::forward<Args>(args)...},
    });

    auto& feature = entries.back().feature;
    feature.on_attach(ctx, viewport_extent);
    feature.on_resize(ctx, viewport_extent);
    return handle;
}

template <ViewportFeatureType... FeatureTypes>
template <ViewportFeatureType Feature>
    requires ((std::same_as<Feature, FeatureTypes> || ...))
Feature* Viewport<FeatureTypes...>::find_feature(FeatureHandle<Feature> handle) {
    auto& entries = store<Feature>().entries;
    const auto it = std::find_if(entries.begin(), entries.end(),
        [handle](const auto& entry) { return entry.id == handle.id; });
    return it == entries.end() ? nullptr : &it->feature;
}

template <ViewportFeatureType... FeatureTypes>
template <ViewportFeatureType Feature>
    requires ((std::same_as<Feature, FeatureTypes> || ...))
bool Viewport<FeatureTypes...>::remove_feature(FeatureHandle<Feature> handle) {
    auto& entries = store<Feature>().entries;
    const auto it = std::find_if(entries.begin(), entries.end(),
        [handle](const auto& entry) { return entry.id == handle.id; });
    if (it == entries.end()) {
        return false;
    }
    entries.erase(it);
    return true;
}

template <ViewportFeatureType... FeatureTypes>
template <ViewportPhase Phase>
void Viewport<FeatureTypes...>::record_phase(vk::raii::CommandBuffer& cmd, uint32_t image_index) {
    ([&] {
        if constexpr (FeatureTypes::phase == Phase) {
            for (auto& entry : store<FeatureTypes>().entries) {
                entry.feature.on_record(ctx, cmd, image_index);
            }
        }
    }(), ...);
}

template <ViewportFeatureType... FeatureTypes>
void Viewport<FeatureTypes...>::resize_all() {
    ([&] {
        for (auto& entry : store<FeatureTypes>().entries) {
            entry.feature.on_resize(ctx, viewport_extent);
        }
    }(), ...);
}

template <ViewportFeatureType... FeatureTypes>
void Viewport<FeatureTypes...>::update_all(const Context::Frame& frame) {
    ([&] {
        for (auto& entry : store<FeatureTypes>().entries) {
            entry.feature.on_update(ctx, frame);
        }
    }(), ...);
}

} // namespace vkkk::vp
