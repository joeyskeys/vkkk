#include "concepts/curve.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <glm/common.hpp>
#include <glm/vec4.hpp>

namespace vkkk
{

namespace
{

float clamp01(float t) {
    return std::clamp(t, 0.0f, 1.0f);
}

std::vector<float> make_clamped_uniform_knots(uint32_t point_count, uint32_t degree) {
    std::vector<float> knots(static_cast<size_t>(point_count) + degree + 1, 0.0f);
    const uint32_t internal_count = point_count - degree;
    for (uint32_t index = 0; index < internal_count; ++index) {
        knots[degree + index] = static_cast<float>(index) / static_cast<float>(internal_count);
    }
    std::fill(knots.end() - static_cast<std::ptrdiff_t>(degree + 1), knots.end(), 1.0f);
    return knots;
}

uint32_t find_span(const std::vector<float>& knots, uint32_t point_count, uint32_t degree, float t) {
    if (t >= knots[point_count]) {
        return point_count - 1;
    }
    uint32_t span = degree;
    while (span < point_count - 1 && t >= knots[span + 1]) {
        ++span;
    }
    return span;
}

glm::vec3 de_boor(const std::vector<glm::vec3>& points, const std::vector<float>& knots,
    uint32_t degree, float t)
{
    const uint32_t point_count = static_cast<uint32_t>(points.size());
    const uint32_t span = find_span(knots, point_count, degree, t);
    std::vector<glm::vec3> d(degree + 1);
    for (uint32_t j = 0; j <= degree; ++j) {
        d[j] = points[span - degree + j];
    }
    for (uint32_t r = 1; r <= degree; ++r) {
        for (uint32_t j = degree; j >= r; --j) {
            const uint32_t left = span - degree + j;
            const float denom = knots[left + degree + 1 - r] - knots[left];
            const float alpha = denom == 0.0f ? 0.0f : (t - knots[left]) / denom;
            d[j] = glm::mix(d[j - 1], d[j], alpha);
        }
    }
    return d[degree];
}

glm::vec4 de_boor_weighted(const std::vector<glm::vec3>& points, const std::vector<float>& weights,
    const std::vector<float>& knots, uint32_t degree, float t)
{
    const uint32_t point_count = static_cast<uint32_t>(points.size());
    const uint32_t span = find_span(knots, point_count, degree, t);
    std::vector<glm::vec4> d(degree + 1);
    for (uint32_t j = 0; j <= degree; ++j) {
        const uint32_t index = span - degree + j;
        d[j] = glm::vec4(points[index] * weights[index], weights[index]);
    }
    for (uint32_t r = 1; r <= degree; ++r) {
        for (uint32_t j = degree; j >= r; --j) {
            const uint32_t left = span - degree + j;
            const float denom = knots[left + degree + 1 - r] - knots[left];
            const float alpha = denom == 0.0f ? 0.0f : (t - knots[left]) / denom;
            d[j] = glm::mix(d[j - 1], d[j], alpha);
        }
    }
    return d[degree];
}

} // namespace

BezierCurve::BezierCurve(std::vector<glm::vec3> points)
    : control_points(std::move(points))
{
    if (control_points.size() < 2) {
        throw std::invalid_argument("BezierCurve requires at least two control points");
    }
}

glm::vec3 BezierCurve::evaluate(float t) const {
    t = clamp01(t);
    std::vector<glm::vec3> points = control_points;
    for (size_t r = 1; r < points.size(); ++r) {
        for (size_t i = 0; i < points.size() - r; ++i) {
            points[i] = glm::mix(points[i], points[i + 1], t);
        }
    }
    return points[0];
}

BSplineCurve::BSplineCurve(std::vector<glm::vec3> points, uint32_t p)
    : control_points(std::move(points))
    , degree(p)
{
    if (degree == 0 || control_points.size() <= degree) {
        throw std::invalid_argument("BSplineCurve requires degree >= 1 and more control points than degree");
    }
    knots = make_clamped_uniform_knots(static_cast<uint32_t>(control_points.size()), degree);
}

glm::vec3 BSplineCurve::evaluate(float t) const {
    return de_boor(control_points, knots, degree, clamp01(t));
}

NurbsCurve::NurbsCurve(std::vector<glm::vec3> points, std::vector<float> w, uint32_t p)
    : control_points(std::move(points))
    , weights(std::move(w))
    , degree(p)
{
    if (degree == 0 || control_points.size() <= degree
        || weights.size() != control_points.size())
    {
        throw std::invalid_argument("NurbsCurve requires matching weights and more control points than degree");
    }
    for (const float weight : weights) {
        if (!(weight > 0.0f) || !std::isfinite(weight)) {
            throw std::invalid_argument("NurbsCurve weights must be finite and positive");
        }
    }
    knots = make_clamped_uniform_knots(static_cast<uint32_t>(control_points.size()), degree);
}

glm::vec3 NurbsCurve::evaluate(float t) const {
    const glm::vec4 homogeneous = de_boor_weighted(control_points, weights, knots, degree, clamp01(t));
    if (homogeneous.w == 0.0f) {
        return glm::vec3(0.0f);
    }
    return glm::vec3(homogeneous) / homogeneous.w;
}

CatmullRomCurve::CatmullRomCurve(std::vector<glm::vec3> p)
    : points(std::move(p))
{
    if (points.size() < 2) {
        throw std::invalid_argument("CatmullRomCurve requires at least two points");
    }
}

glm::vec3 CatmullRomCurve::evaluate(float t) const {
    t = clamp01(t);
    const uint32_t last = static_cast<uint32_t>(points.size() - 1);
    const float scaled = t * static_cast<float>(last);
    uint32_t segment = static_cast<uint32_t>(scaled);
    if (segment >= last) {
        segment = last - 1;
    }
    const float u = scaled - static_cast<float>(segment);
    const auto at = [this, last](int32_t index) {
        return points[static_cast<size_t>(std::clamp(index, 0, static_cast<int32_t>(last)))];
    };
    const glm::vec3 p0 = at(static_cast<int32_t>(segment) - 1);
    const glm::vec3 p1 = at(static_cast<int32_t>(segment));
    const glm::vec3 p2 = at(static_cast<int32_t>(segment) + 1);
    const glm::vec3 p3 = at(static_cast<int32_t>(segment) + 2);
    const float u2 = u * u;
    const float u3 = u2 * u;
    return 0.5f * ((2.0f * p1)
        + (-p0 + p2) * u
        + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u3);
}

} // namespace vkkk
