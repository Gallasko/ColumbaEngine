#ifndef CONSTANT_H
#define CONSTANT_H

//TODO have no include in constant

#include <unordered_map> // Included for RefractTable
#include <string> // Included for RefractTable and Big Int
#include <vector> // Included for Big Int
#include <cmath>
#include <cstdlib>
#include <algorithm>

#include <Memory/elementtype.h>

#include "logger.h"

namespace pg
{
    inline bool areAlmostEqual(float a, float b, float epsilon = 1e-5f)
    {
        return std::fabs(a - b) <= epsilon * std::max({1.0f, std::fabs(a), std::fabs(b)});
    }

    inline bool areNotAlmostEqual(float a, float b, float epsilon = 1e-5f)
    {
        return not areAlmostEqual(a, b, epsilon);
    }

    namespace constant
    {
        //Screen Const
        constexpr int SCREEN_WIDTH = 624;
        constexpr int SCREEN_HEIGHT = 480;

        //Camera Const
        constexpr float YAW         = -90.0f;
        constexpr float PITCH       =  0.0f;
        constexpr float SPEED       =  2.5f;
        constexpr float SENSITIVITY =  1.5f;
        constexpr float ZOOM        =  45.0f;

        // Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
        enum class Camera_Movement
        {
            FORWARD,
            BACKWARD,
            LEFT,
            RIGHT
        };

        //Vector struct
        // Todo add an operator [] to vec2 vec3 and vec4 for easier acces and an operator=(float[2])
        struct Vector2D
        {
            float x = 0.0f;
            float y = 0.0f;

            inline Vector2D() {}

            inline Vector2D(double x, double y) : x(static_cast<float>(x)), y(static_cast<float>(y)) {}

            inline Vector2D(const unsigned int& x, const unsigned int& y) : x(static_cast<float>(x)), y(static_cast<float>(y)) {}

            inline Vector2D(const int& x, const int& y) : x(static_cast<float>(x)), y(static_cast<float>(y)) {}

            inline Vector2D(const float& x, const float& y) : x(x), y(y) {}

            inline Vector2D(const Vector2D& vec) : x(vec.x), y(vec.y) {}

            inline void operator=(const Vector2D &rhs)
            {
                this->x = rhs.x;
                this->y = rhs.y;
            }

            inline Vector2D operator+(const Vector2D &rhs) const
            {
                Vector2D vec;
                vec.x = this->x + rhs.x;
                vec.y = this->y + rhs.y;

                return vec;
            }

            inline Vector2D operator-(const Vector2D &rhs) const
            {
                Vector2D vec;
                vec.x = this->x - rhs.x;
                vec.y = this->y - rhs.y;

                return vec;
            }

            inline Vector2D operator*(float scalar) const
            {
                Vector2D vec;
                vec.x = this->x * scalar;
                vec.y = this->y * scalar;

                return vec;
            }

            inline Vector2D& operator+=(const Vector2D &rhs)
            {
                this->x += rhs.x;
                this->y += rhs.y;

                return *this;
            }

            inline bool operator==(const Vector2D &rhs) const
            {
                return areAlmostEqual(this->x, rhs.x) and areAlmostEqual(this->y, rhs.y);
            }

            inline bool operator!=(const Vector2D &rhs) const
            {
                return not (*this == rhs);
            }

            // Returns the Euclidean length (magnitude) of the vector.
            inline float length() const
            {
                return std::sqrt(x * x + y * y);
            }

            inline float dot(const Vector2D &rhs) const
            {
                return this->x * rhs.x + this->y * rhs.y;
            }

            // Returns a normalized copy of this vector (unit length).
            // If the vector is zero-length, returns the original (0,0).
            inline Vector2D normalized() const
            {
                float len = length();

                if (len > 0.0f)
                    return Vector2D(x / len, y / len);
                else
                    return *this;
            }

            // Normalizes this vector in-place to unit length.
            // If the vector is zero-length, it remains unchanged.
            inline void normalize()
            {
                float len = length();

                if (len > 0.0f)
                {
                    x /= len;
                    y /= len;
                }
            }
        };

        //Vector struct
        struct Vector3D
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;

            Vector3D() {}

            Vector3D(const float& x, const float& y, const float& z) : x(x), y(y), z(z) {}

            Vector3D(const Vector2D& vec2, const float& z) : x(vec2.x), y(vec2.y), z(z) {}

            Vector3D(const float& x, const Vector2D& vec2) : x(x), y(vec2.x), z(vec2.y) {}

            Vector3D(const Vector3D& vec) : x(vec.x), y(vec.y), z(vec.z) {}

            inline void operator=(const Vector3D &rhs)
            {
                this->x = rhs.x;
                this->y = rhs.y;
                this->z = rhs.z;
            }

            inline Vector3D operator+(const Vector3D &rhs) const
            {
                Vector3D vec;
                vec.x = this->x + rhs.x;
                vec.y = this->y + rhs.y;
                vec.z = this->z + rhs.z;

                return vec;
            }

            inline Vector3D operator-(const Vector3D &rhs) const
            {
                Vector3D vec;
                vec.x = this->x - rhs.x;
                vec.y = this->y - rhs.y;
                vec.z = this->z - rhs.z;

                return vec;
            }

            inline Vector3D operator*(float scalar) const
            {
                Vector3D vec;
                vec.x = this->x * scalar;
                vec.y = this->y * scalar;
                vec.z = this->z * scalar;

                return vec;
            }

            inline Vector3D& operator+=(const Vector3D &rhs)
            {
                this->x += rhs.x;
                this->y += rhs.y;
                this->z += rhs.z;

                return *this;
            }

            inline bool operator==(const Vector3D &rhs) const
            {
                return areAlmostEqual(this->x, rhs.x) and areAlmostEqual(this->y, rhs.y) and areAlmostEqual(this->z, rhs.z);
            }

            inline bool operator!=(const Vector3D &rhs) const
            {
                return not (*this == rhs);
            }
        };

        //Vector struct
        struct Vector4D
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            float w = 0.0f;

            Vector4D() {}

            Vector4D(const float& x, const float& y, const float& z, const float& w) : x(x), y(y), z(z), w(w) {}

            Vector4D(const Vector3D& vec, const float& w) : x(vec.x), y(vec.y), z(vec.z), w(w)  {}

            Vector4D(const float& x, const Vector3D& vec) : x(x), y(vec.x), z(vec.y), w(vec.z)  {}

            Vector4D(const Vector4D& vec) : x(vec.x), y(vec.y), z(vec.z), w(vec.w)  {}

            inline void operator=(const Vector4D &rhs)
            {
                this->x = rhs.x;
                this->y = rhs.y;
                this->z = rhs.z;
                this->w = rhs.w;
            }

            inline Vector4D operator+(const Vector4D &rhs) const
            {
                Vector4D vec;
                vec.x = this->x + rhs.x;
                vec.y = this->y + rhs.y;
                vec.z = this->z + rhs.z;
                vec.w = this->w + rhs.w;

                return vec;
            }

            inline Vector4D operator-(const Vector4D &rhs) const
            {
                Vector4D vec;
                vec.x = this->x - rhs.x;
                vec.y = this->y - rhs.y;
                vec.z = this->z - rhs.z;
                vec.w = this->w - rhs.w;

                return vec;
            }

            inline Vector4D operator*(float scalar) const
            {
                Vector4D vec;
                vec.x = this->x * scalar;
                vec.y = this->y * scalar;
                vec.z = this->z * scalar;
                vec.w = this->w * scalar;

                return vec;
            }

            inline Vector4D& operator+=(const Vector4D &rhs)
            {
                this->x += rhs.x;
                this->y += rhs.y;
                this->z += rhs.z;
                this->w += rhs.w;

                return *this;
            }

            inline bool operator==(const Vector4D &rhs) const
            {
                return areAlmostEqual(this->x, rhs.x) and areAlmostEqual(this->y, rhs.y) and areAlmostEqual(this->z, rhs.z) and areAlmostEqual(this->w, rhs.w);
            }

            inline bool operator!=(const Vector4D &rhs) const
            {
                return not (*this == rhs);
            }
        };

        class BigInt
        {
        public:
            BigInt() { digits.push_back(0); }
            BigInt(const BigInt& bigNum) : digits(bigNum.digits), sign(bigNum.sign) {}
            BigInt(const long long int& value) { *this = value; }
            BigInt(const std::string& value) { *this = value; }
            BigInt(const char* value) { *this = std::string(value); }

            void operator=(const BigInt& rhs)
            {
                digits = rhs.digits;
                sign = rhs.sign;
            }

            void operator=(long long int value)
            {
                digits.clear();

                sign = value > 0;
                if (sign == 0)
                    value = -value;

                for(; value > 0; value /= 1000)
                    digits.push_back(value % 1000);
            }

            void operator=(const std::string& value)
            {
                digits.clear();
                sign = 1;

                if (value.size() < 1)
                {
                    digits.push_back(0);
                    return;
                }

                int pos = 0;
                if (value[0] == '-' or value[0] == '+')
                {
                    pos++;

                    if (value.size() < 2)
                    {
                        digits.push_back(0);
                        return;
                    }

                    if (value[0] == '-')
                        sign = 0;
                }

                for (int i = value.size() - 1; i >= pos; i -= 3)
                {
                    int x = 0;
                    for (int j = std::max(pos, i - 3 + 1); j <= i; j++)
                        x = x * 10 + value[j] - '0';

                    digits.push_back(x);
                }

                trim();
            }

            BigInt operator+(const BigInt& rhs) const
            {
                if (sign == rhs.sign)
                {
                    BigInt result(rhs);
                    unsigned short carry = 0;

                    for (int i = 0; i < (int)std::max(digits.size(), rhs.digits.size()) or carry; i++)
                    {
                        if (i == (int)result.digits.size())
                            result.digits.push_back(0);

                        result.digits[i] += carry + (i < (int)digits.size() ? digits[i] : 0);

                        carry = result.digits[i] >= 1000;
                        if (carry)
                            result.digits[i] -= 1000;
                    }

                    return result;
                }

                return *this - (-rhs);
            }

            BigInt operator-() const
            {
                BigInt res = *this;
                res.sign = !sign;
                return res;
            }

            BigInt operator-(const BigInt &rhs) const
            {
                if (sign == rhs.sign)
                {
                    if (abs() >= rhs.abs())
                    {
                        BigInt res = *this;

                        for (int i = 0, carry = 0; i < (int) rhs.digits.size() || carry; i++)
                        {
                            res.digits[i] -= carry + (i < (int) rhs.digits.size() ? rhs.digits[i] : 0);

                            carry = res.digits[i] < 0;
                            if (carry)
                                res.digits[i] += 1000;
                        }

                        res.trim();
                        return res;
                    }
                    return -(rhs - *this);
                }
                return *this + (-rhs);
            }

            void operator+=(const BigInt &rhs) { *this = *this + rhs; }
            void operator-=(const BigInt &rhs) { *this = *this - rhs; }

            bool operator< (const BigInt &rhs) const
            {
                if (sign != rhs.sign)
                    return sign < rhs.sign;

                if (digits.size() != rhs.digits.size())
                    return digits.size() * sign < rhs.digits.size() * rhs.sign;

                for (int i = digits.size() - 1; i >= 0; i--)
                    if (digits[i] != rhs.digits[i])
                        return digits[i] * sign < rhs.digits[i] * sign;

                return false;
            }

            bool operator> (const BigInt &rhs) const { return rhs < *this; }
            bool operator<=(const BigInt &rhs) const { return !(rhs < *this); }
            bool operator>=(const BigInt &rhs) const { return !(*this < rhs); }
            bool operator==(const BigInt &rhs) const { return !(*this < rhs) && !(rhs < *this); }
            bool operator!=(const BigInt &rhs) const { return *this < rhs || rhs < *this; }

            std::string toString() const { std::string res = ""; if (sign == 0) res += "-"; auto it = digits.rbegin(); if (it != digits.rend()) res += std::to_string(*it++); while (it != digits.rend()) { res += numberWithLeadingZero(*it++); } return res; }

            friend std::ostream& operator<<(std::ostream& out, const BigInt& n) { return out << n.toString(); }

        private:
            std::vector<short> digits; // digit stored in base 1000 -> 1 slot == 3 digits
            bool sign = 1; // sign = 1 positiv integer, sign = 0 negativ integer

            std::string numberWithLeadingZero(short n) const
            {
                std::string value = "";
                if (n < 100)
                        value += "0";
                if (n < 10)
                        value += "0";

                value += std::to_string(n);

                return value;
            }

            BigInt abs() const
            {
                BigInt res = *this;
                res.sign = 1;
                return res;
            }

            void trim()
            {
                while (!digits.empty() && !digits.back())
                    digits.pop_back();
                if (digits.empty())
                    sign = 1;
            }

        };

        typedef std::unordered_map<std::string, ElementType> RefracTable;

        struct ModelInfo
        {
            float *vertices = nullptr;
            unsigned int *indices = nullptr;

            unsigned int nbVertices = 0;
            unsigned int nbIndices = 0;

            ModelInfo() {}

            ModelInfo(const ModelInfo &rhs)
            {
                unsigned int i = 0;
                this->vertices = new float[rhs.nbVertices];
                for (i = 0; i < rhs.nbVertices; i++)
                    this->vertices[i] = rhs.vertices[i];

                this->indices = new unsigned int[rhs.nbIndices];
                for (i = 0; i < rhs.nbIndices; i++)
                    this->indices[i] = rhs.indices[i];

                this->nbVertices = rhs.nbVertices;
                this->nbIndices = rhs.nbIndices;
            }

            virtual ~ModelInfo() { if (vertices != nullptr) delete[] vertices; if (indices!= nullptr) delete[] indices; }

            inline void operator=(const ModelInfo &rhs)
            {
                unsigned int i = 0;

                if (this->vertices != nullptr)
                    delete[] this->vertices;
                if (this->indices != nullptr)
                    delete[] this->indices;

                this->vertices = new float[rhs.nbVertices];
                for (i = 0; i < rhs.nbVertices; i++)
                    this->vertices[i] = rhs.vertices[i];

                this->indices = new unsigned int[rhs.nbIndices];
                for (i = 0; i < rhs.nbIndices; i++)
                    this->indices[i] = rhs.indices[i];

                this->nbVertices = rhs.nbVertices;
                this->nbIndices = rhs.nbIndices;
            }
        };

        struct SquareInfo : public ModelInfo
        {
            SquareInfo() : ModelInfo()
            {
                vertices = new float[20];
                //              x                     y                    z                    texpos x             texpos y
                vertices[0] =  0.0f; vertices[1] =   0.0f; vertices[2] =  1.0f; vertices[3] =  0.0f; vertices[4] =  0.0f;
                vertices[5] =  1.0f; vertices[6] =   0.0f; vertices[7] =  1.0f; vertices[8] =  1.0f; vertices[9] =  0.0f;
                vertices[10] = 0.0f; vertices[11] = -1.0f; vertices[12] = 1.0f; vertices[13] = 0.0f; vertices[14] = 1.0f;
                vertices[15] = 1.0f; vertices[16] = -1.0f; vertices[17] = 1.0f; vertices[18] = 1.0f; vertices[19] = 1.0f;

                indices = new unsigned int[6];
                indices[0] = 0; indices[1] = 1; indices[2] = 2;
                indices[3] = 1; indices[4] = 2; indices[5] = 3;

                nbVertices = 20;
                nbIndices = 6;
            }

            SquareInfo(const SquareInfo &rhs) : ModelInfo()
            {
                unsigned int i = 0;
                this->vertices = new float[rhs.nbVertices];
                for (i = 0; i < rhs.nbVertices; i++)
                    this->vertices[i] = rhs.vertices[i];

                this->indices = new unsigned int[rhs.nbIndices];
                for (i = 0; i < rhs.nbIndices; i++)
                    this->indices[i] = rhs.indices[i];

                this->nbVertices = rhs.nbVertices;
                this->nbIndices = rhs.nbIndices;
            }

            virtual ~SquareInfo() {}

            inline void operator=(const SquareInfo &rhs)
            {
                unsigned int i = 0;

                if (this->vertices != nullptr)
                    delete[] this->vertices;
                if (this->indices != nullptr)
                    delete[] this->indices;

                this->vertices = new float[rhs.nbVertices];
                for (i = 0; i < rhs.nbVertices; i++)
                    this->vertices[i] = rhs.vertices[i];

                this->indices = new unsigned int[rhs.nbIndices];
                for (i = 0; i < rhs.nbIndices; i++)
                    this->indices[i] = rhs.indices[i];

                this->nbVertices = rhs.nbVertices;
                this->nbIndices = rhs.nbIndices;
            }
        };

    }
}

#endif
