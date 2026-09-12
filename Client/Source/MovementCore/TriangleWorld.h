#pragma once

#include "MovementCore.h"
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace hhv::movement
{
struct Triangle
{
	Vec3 a, b, c;
};

namespace detail
{
inline Vec3 closestPoint(Vec3 p, const Triangle& t)
{
	Vec3 ab = t.b - t.a, ac = t.c - t.a, ap = p - t.a;
	float d1 = dot(ab, ap), d2 = dot(ac, ap);

	if (d1 <= 0 && d2 <= 0)
		return t.a;
	Vec3 bp = p - t.b;
	float d3 = dot(ab, bp), d4 = dot(ac, bp);

	if (d3 >= 0 && d4 <= d3)
		return t.b;
	float vc = d1 * d4 - d3 * d2;

	if (vc <= 0 && d1 >= 0 && d3 <= 0)
		return t.a + ab * (d1 / (d1 - d3));
	Vec3 cp = p - t.c;
	float d5 = dot(ab, cp), d6 = dot(ac, cp);

	if (d6 >= 0 && d5 <= d6)
		return t.c;
	float vb = d5 * d2 - d1 * d6;

	if (vb <= 0 && d2 >= 0 && d6 <= 0)
		return t.a + ac * (d2 / (d2 - d6));
	float va = d3 * d6 - d5 * d4;

	if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
		return t.b + (t.c - t.b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
	float denom = 1.f / (va + vb + vc);
	return t.a + ab * (vb * denom) + ac * (vc * denom);
}

inline void closestSegments(Vec3 p, Vec3 q, Vec3 a, Vec3 b, Vec3& onAxis, Vec3& onEdge)
{
	Vec3 d = q - p, e = b - a, r = p - a;
	float dd = dot(d, d), ee = dot(e, e), de = dot(d, e), dr = dot(d, r), er = dot(e, r);
	float s = 0, t = 0;

	if (dd < 1e-8f)
		t = ee > 1e-8f ? std::clamp(er / ee, 0.f, 1.f) : 0;
	else if (ee < 1e-8f)
		s = std::clamp(-dr / dd, 0.f, 1.f);
	else
	{
		float denom = dd * ee - de * de;

		if (denom > 1e-8f)
			s = std::clamp((de * er - dr * ee) / denom, 0.f, 1.f);
		t = (de * s + er) / ee;

		if (t < 0)
		{
			t = 0;
			s = std::clamp(-dr / dd, 0.f, 1.f);
		}
		else if (t > 1)
		{
			t = 1;
			s = std::clamp((de - dr) / dd, 0.f, 1.f);
		}
	}
	onAxis = p + d * s;
	onEdge = a + e * t;
}

inline Vec3 separation(Vec3 p, Vec3 q, const Triangle& t)
{
	Vec3 best = p - closestPoint(p, t), other = q - closestPoint(q, t);

	if (dot(other, other) < dot(best, best))
		best = other;
	Vec3 n = normalized(cross(t.b - t.a, t.c - t.a));
	float den = dot(n, q - p);

	if (std::abs(den) > 1e-7f)
	{
		float f = dot(n, t.a - p) / den;

		if (f >= 0 && f <= 1)
		{
			Vec3 v = p + (q - p) * f;

			if (length(v - closestPoint(v, t)) < .001f)
				return {};
		}
	}
	Vec3 verts[3] = {t.a, t.b, t.c};

	for (int i = 0; i < 3; ++i)
	{
		Vec3 axis, edge;
		closestSegments(p, q, verts[i], verts[(i + 1) % 3], axis, edge);
		other = axis - edge;

		if (dot(other, other) < dot(best, best))
			best = other;
	}

	return best;
}

struct Bounds
{
	Vec3 min{1e30f, 1e30f, 1e30f}, max{-1e30f, -1e30f, -1e30f};

	void add(Vec3 p)
	{
		min = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
		max = {std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z)};
	}

	bool overlaps(const Bounds& b) const
	{
		return min.x <= b.max.x && max.x >= b.min.x && min.y <= b.max.y && max.y >= b.min.y &&
		       min.z <= b.max.z && max.z >= b.min.z;
	}
};

} // namespace detail

// Immutable after build. Both runtimes use this exact capsule/triangle implementation.
class TriangleWorld final : public CollisionWorld
{
	struct Node
	{
		detail::Bounds bounds;
		int left = -1, right = -1;
		std::size_t begin = 0, end = 0;
	};

	std::vector<Triangle> triangles_;
	std::vector<std::size_t> indices_;
	std::vector<Node> nodes_;
	std::uint64_t hash_ = 14695981039346656037ull;

	// Use the same world-space tolerance as capsule contact detection. Comparing
	// only fractions of a sweep made shared edges depend on the sweep's length.
	static constexpr float ContactTolerance = .001f;

	int buildNode(std::size_t begin, std::size_t end)
	{
		Node node;
		node.begin = begin;
		node.end = end;

		for (std::size_t i = begin; i < end; ++i)
		{
			const auto& t = triangles_[indices_[i]];
			node.bounds.add(t.a);
			node.bounds.add(t.b);
			node.bounds.add(t.c);
		}
		int id = static_cast<int>(nodes_.size());
		nodes_.push_back(node);

		if (end - begin > 8)
		{
			Vec3 extent = node.bounds.max - node.bounds.min;
			int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);
			auto center = [&](std::size_t i)
			{
				Vec3 v = triangles_[i].a + triangles_[i].b + triangles_[i].c;
				return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
			};
			std::size_t mid = (begin + end) / 2;
			std::nth_element(indices_.begin() + begin, indices_.begin() + mid, indices_.begin() + end,
			                 [&](auto a, auto b)
			                 {
				                 float ca = center(a), cb = center(b);
				                 return ca == cb ? a < b : ca < cb;
			                 });
			nodes_[id].left = buildNode(begin, mid);
			nodes_[id].right = buildNode(mid, end);
		}

		return id;
	}

	void query(int id, const detail::Bounds& bounds, Vec3 start, Vec3 delta, float r, float h,
	           float minimumNormalZ, float timeTolerance, Hit& result, std::size_t& bestId) const
	{
		const Node& n = nodes_[id];

		if (!n.bounds.overlaps(bounds))
			return;

		if (n.left >= 0)
		{
			query(n.left, bounds, start, delta, r, h, minimumNormalZ, timeTolerance, result, bestId);
			query(n.right, bounds, start, delta, r, h, minimumNormalZ, timeTolerance, result, bestId);
			return;
		}

		for (std::size_t i = n.begin; i < n.end; ++i)
		{
			std::size_t triId = indices_[i];
			const Triangle& tri = triangles_[triId];
			float time = 0;

			for (int iteration = 0; iteration < 32; ++iteration)
			{
				Vec3 center = start + delta * time, axis{0, 0, std::max(0.f, h - r)};
				Vec3 sep = detail::separation(center - axis, center + axis, tri);
				float distance = length(sep);
				Vec3 normal =
				    distance > 1e-5f ? sep / distance : normalized(cross(tri.b - tri.a, tri.c - tri.a));

				if (distance <= 1e-5f && dot(normal, center - tri.a) < 0)
					normal = normal * -1;
				Vec3 surface = normalized(cross(tri.b - tri.a, tri.c - tri.a));

				if (dot(surface, normal) < 0)
					surface = surface * -1;
				float gap = distance - r, closing = -dot(delta, normal);

				if (gap <= ContactTolerance)
				{
					const float penetration = std::max(0.f, -gap);

					const bool floorQuery = minimumNormalZ > 0;
					const bool acceptedFace = !floorQuery || (surface.z >= minimumNormalZ && normal.z > .001f);

					if (acceptedFace && (closing > 1e-6f || penetration > .01f))
					{
						const bool earlierContact = time < result.time - timeTolerance;
						const bool sameContact = std::abs(time - result.time) <= timeTolerance;
						const bool flatterFace = surface.z > result.surfaceNormal.z + .001f;
						const bool sameFacing = std::abs(surface.z - result.surfaceNormal.z) < .001f;

						if (!result.blocking || earlierContact ||
						    (sameContact && (flatterFace || (sameFacing && triId < bestId))))
						{
							result = {time, normal, true, penetration, surface};
							bestId = triId;
						}
					}
					break;
				}

				if (closing <= 1e-6f)
					break;
				time += gap / closing;

				// Do not discard a neighbouring face before the shared-edge comparison.
				if (time > result.time + timeTolerance || time > 1)
					break;
			}
		}
	}

public:
	const std::vector<Triangle>& triangles() const
	{
		return triangles_;
	}

	void build(std::vector<Triangle> triangles)
	{
		triangles_.clear();
		nodes_.clear();
		hash_ = 14695981039346656037ull;

		for (const auto& t : triangles)
		{
			if (!finite(t.a) || !finite(t.b) || !finite(t.c) || length(cross(t.b - t.a, t.c - t.a)) < 1e-5f)
				continue;
			triangles_.push_back(t);

			for (Vec3 v : {t.a, t.b, t.c})
				for (float f : {v.x, v.y, v.z})
				{
					std::uint32_t bits;
					std::memcpy(&bits, &f, sizeof(bits));

					for (int b = 0; b < 4; ++b)
					{
						hash_ ^= (bits >> (b * 8)) & 255;
						hash_ *= 1099511628211ull;
					}
				}
		}
		indices_.resize(triangles_.size());
		std::iota(indices_.begin(), indices_.end(), 0);

		if (!triangles_.empty())
			buildNode(0, triangles_.size());
	}

	// Versioned canonical geometry: centimetres, Z up, no implicit origin offset.
	// Failed loads leave the previous immutable world intact.
	bool load(std::istream& stream)
	{
		stream.imbue(std::locale::classic());
		std::string magic;
		unsigned version = 0;
		std::size_t count = 0;
		std::uint64_t expectedHash = 0;

		if (!(stream >> magic >> version >> count >> expectedHash) || magic != "HHVCOLLISION" ||
		    version != 1 || count == 0 || count > 2000000)
			return false;
		std::vector<Triangle> triangles;
		triangles.reserve(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			Triangle t;

			if (!(stream >> t.a.x >> t.a.y >> t.a.z >> t.b.x >> t.b.y >> t.b.z >> t.c.x >> t.c.y >> t.c.z) ||
			    !finite(t.a) || !finite(t.b) || !finite(t.c))
				return false;
			triangles.push_back(t);
		}
		stream >> std::ws;

		if (!stream.eof())
			return false;
		TriangleWorld candidate;
		candidate.build(std::move(triangles));

		if (candidate.size() != count || candidate.hash() != expectedHash)
			return false;
		*this = std::move(candidate);
		return true;
	}

	bool save(std::ostream& stream) const
	{
		stream.imbue(std::locale::classic());
		stream << "HHVCOLLISION 1 " << size() << ' ' << hash() << '\n';
		stream << std::setprecision(std::numeric_limits<float>::max_digits10);

		for (const auto& t : triangles_)
			stream << t.a.x << ' ' << t.a.y << ' ' << t.a.z << ' ' << t.b.x << ' ' << t.b.y << ' ' << t.b.z
			       << ' ' << t.c.x << ' ' << t.c.y << ' ' << t.c.z << '\n';
		return bool(stream) && size() > 0;
	}

	std::uint64_t hash() const
	{
		return hash_;
	}

	std::size_t size() const
	{
		return triangles_.size();
	}

	Hit sweep(Vec3 start, Vec3 delta, float radius, float halfHeight) const override
	{
		return sweepCapsule(start, delta, radius, halfHeight, -2.f);
	}

	Hit sweepFloor(Vec3 start, float distance, float radius, float halfHeight,
	               float minimumNormalZ) const override
	{
		return sweepCapsule(start, {0, 0, -distance}, radius, halfHeight, minimumNormalZ);
	}

private:
	Hit sweepCapsule(Vec3 start, Vec3 delta, float radius, float halfHeight, float minimumNormalZ) const
	{
		Hit result;

		if (nodes_.empty())
			return result;
		detail::Bounds bounds;
		Vec3 extent{radius + .01f, radius + .01f, halfHeight + .01f};
		bounds.add(start - extent);
		bounds.add(start + extent);
		bounds.add(start + delta - extent);
		bounds.add(start + delta + extent);
		std::size_t bestId = triangles_.size();
		const float timeTolerance = ContactTolerance / std::max(length(delta), ContactTolerance);
		query(0, bounds, start, delta, radius, halfHeight, minimumNormalZ, timeTolerance, result, bestId);
		return result;
	}
};

} // namespace hhv::movement
