#pragma once

#include <cmath>

#include "pgconstant.h"

namespace pg
{
    /**
     * @brief Represents a 2D point in space.
     */
    struct Point2D
    {
        float x; ///< The x-coordinate of the point.
        float y; ///< The y-coordinate of the point.

        Point2D() : x(0.0f), y(0.0f) {}
        Point2D(float x, float y) : x(x), y(y) {}
        Point2D(const Point2D& other) : x(other.x), y(other.y) {}

        Point2D& operator=(const Point2D& other)
        {
            x = other.x;
            y = other.y;

            return *this;
        }

        /**
         * @brief Equality operator to compare two points.
         *
         * @param other The point to compare with.
         * @return true if both points are equal, false otherwise.
         */
        bool operator==(const Point2D& other) const
        {
            return (areAlmostEqual(x, other.x) and areAlmostEqual(y, other.y));
        }

        /**
         * @brief Inequality operator to compare two points.
         *
         * @param other The point to compare with.
         * @return true if both points are not equal, false otherwise.
         */
        bool operator!=(const Point2D& other) const
        {
            return !(*this == other);
        }

        Point2D& operator+=(const Point2D& other)
        {
            x += other.x;
            y += other.y;
            return *this;
        }
    };

    struct Segment2D
    {
        Point2D start; ///< The starting point of the segment.
        Point2D end;   ///< The ending point of the segment.

        Segment2D() : start(), end() {}
        Segment2D(const Point2D& start, const Point2D& end) : start(start), end(end) {}
        Segment2D(const Segment2D& other) : start(other.start), end(other.end) {}

        /**
         * @brief Equality operator to compare two segments.
         *
         * @param other The segment to compare with.
         * @return true if both segments are equal, false otherwise.
         */
        bool operator==(const Segment2D& other) const
        {
            return (start == other.start and end == other.end);
        }

        /**
         * @brief Inequality operator to compare two segments.
         *
         * @param other The segment to compare with.
         * @return true if both segments are not equal, false otherwise.
         */
        bool operator!=(const Segment2D& other) const
        {
            return !(*this == other);
        }

        /**
         * @brief Get the length of the segment.
         *
         * @return The length of the segment.
         */
        float length() const
        {
            return std::sqrt(std::pow(end.x - start.x, 2) + std::pow(end.y - start.y, 2));
        }

        /**
         * @brief Get the direction vector of the segment.
         *
         * @return A Point2D representing the direction vector of the segment.
         */
        Point2D direction() const
        {
            return Point2D(end.x - start.x, end.y - start.y);
        }

        /**
         * @brief Get the normal vector of the segment.
         *
         * @return A Point2D representing the normal vector of the segment.
         */
        Point2D normal() const
        {
            Point2D dir = direction();

            return Point2D(-dir.y, dir.x); // Rotate 90 degrees
        }

        /**
         * @brief Get the normalized direction vector of the segment.
         *
         * @return A Point2D representing the normalized direction vector of the segment.
         */
        Point2D normalizedDirection() const
        {
            float len = length();
            if (areAlmostEqual(len, 0.0f))
                return Point2D(0.0f, 0.0f);

            return Point2D((end.x - start.x) / len, (end.y - start.y) / len);
        }
    };
}