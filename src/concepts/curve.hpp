#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <glm/vec3.hpp>

#include "concepts/line.h"

namespace vkkk
{

// CRTP base for CPU-side curves. Derived must provide:
// glm::vec3 evaluate(float t) const; where t is in [0, 1].
template <typename Derived>
class Curve {
public:
    Lines generate_lines(uint32_t segment_count) const {
        static_assert(requires(const Derived& curve, float t) {
            { curve.evaluate(t) } -> std::convertible_to<glm::vec3>;
        }, "Curve derived type must implement glm::vec3 evaluate(float t) const");

        if (segment_count == 0
            || segment_count > std::numeric_limits<uint32_t>::max() / (3 * sizeof(float)) - 1)
        {
            throw std::invalid_argument("Curve line segment count is invalid");
        }

        const auto& curve = static_cast<const Derived&>(*this);
        std::vector<float> vertices;
        vertices.reserve(static_cast<size_t>(segment_count + 1) * 3);
        for (uint32_t index = 0; index <= segment_count; ++index) {
            const float t = static_cast<float>(index) / static_cast<float>(segment_count);
            const glm::vec3 position = curve.evaluate(t);
            vertices.insert(vertices.end(), {position.x, position.y, position.z});
        }

        std::vector<uint32_t> indices;
        indices.reserve(static_cast<size_t>(segment_count) * 2);
        for (uint32_t index = 0; index < segment_count; ++index) {
            indices.insert(indices.end(), {index, index + 1});
        }

        Lines lines({VERTEX});
        lines.load(segment_count + 1, reinterpret_cast<const char*>(vertices.data()),
            static_cast<uint32_t>(vertices.size() * sizeof(float)),
            static_cast<uint32_t>(indices.size()), reinterpret_cast<const char*>(indices.data()),
            static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
        return lines;
    }
};

class BezierCurve : public Curve<BezierCurve> {
public:
    explicit BezierCurve(std::vector<glm::vec3> control_points);
    glm::vec3 evaluate(float t) const;

    std::vector<glm::vec3> control_points;
};

class BSplineCurve : public Curve<BSplineCurve> {
public:
    BSplineCurve(std::vector<glm::vec3> control_points, uint32_t degree = 3);
    glm::vec3 evaluate(float t) const;

    std::vector<glm::vec3> control_points;
    std::vector<float> knots;
    uint32_t degree = 3;
};

class NurbsCurve : public Curve<NurbsCurve> {
public:
    NurbsCurve(std::vector<glm::vec3> control_points, std::vector<float> weights, uint32_t degree = 3);
    glm::vec3 evaluate(float t) const;

    std::vector<glm::vec3> control_points;
    std::vector<float> weights;
    std::vector<float> knots;
    uint32_t degree = 3;
};

class CatmullRomCurve : public Curve<CatmullRomCurve> {
public:
    explicit CatmullRomCurve(std::vector<glm::vec3> points);
    glm::vec3 evaluate(float t) const;

    std::vector<glm::vec3> points;
};

} // namespace vkkk
