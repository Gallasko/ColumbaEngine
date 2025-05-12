#pragma once

#include <variant>

#include "constant.h"

#include "coresystems.h"

#include "ECS/system.h"
#include "ECS/callable.h"

namespace pg
{
    typedef std::variant<float, constant::Vector2D, constant::Vector3D, constant::Vector4D> TweenValue;

    inline TweenValue operator-(const TweenValue& lhs, const TweenValue& rhs)
    {
        return std::visit([](auto&& left, auto&& right) -> TweenValue {
            using LType = std::decay_t<decltype(left)>;
            using RType = std::decay_t<decltype(right)>;

            if constexpr (std::is_same_v<LType, RType>)
            {
                return left - right; // Perform subtraction if types match
            }
            else
            {
                throw std::invalid_argument("Mismatched types in TweenValue subtraction");
            }
        }, lhs, rhs);
    }

    inline TweenValue operator+(const TweenValue& lhs, const TweenValue& rhs)
    {
        return std::visit([](auto&& left, auto&& right) -> TweenValue {
            using LType = std::decay_t<decltype(left)>;
            using RType = std::decay_t<decltype(right)>;

            if constexpr (std::is_same_v<LType, RType>)
            {
                return left + right; // Perform addition if types match
            }
            else
            {
                throw std::invalid_argument("Mismatched types in TweenValue addition");
            }
        }, lhs, rhs);
    }

    inline TweenValue operator*(const TweenValue& value, float scalar)
    {
        return std::visit([scalar](auto&& val) -> TweenValue {
            using ValueType = std::decay_t<decltype(val)>;

            if constexpr (std::is_arithmetic_v<ValueType>)
            {
                return val * scalar; // Scale if the type is arithmetic (e.g., float)
            }
            else if constexpr (std::is_same_v<ValueType, constant::Vector2D> or
                               std::is_same_v<ValueType, constant::Vector3D> or
                               std::is_same_v<ValueType, constant::Vector4D>)
            {
                return val * scalar; // Assume vector types support scalar multiplication
            }
            else
            {
                throw std::invalid_argument("Unsupported type in TweenValue multiplication");
            }
        }, value);
    }

    inline float TweenLinear(float value)
    {
        return value;
    }

    inline float TweenQuad(float value)
    {
        return 1 - (1 - value) * (1 - value);
    }

    struct TweenComponent
    {
        TweenValue start;
        TweenValue end;

        float duration = 0.0f; // Duration of the tween in milliseconds

        std::function<void(const TweenValue&)> onUpdateCallback; // Callback to be called on each update

        CallablePtr onCompleteCallback = nullptr; // Callback to be called when the tween completes

        int loops = 1; // Whether the tween should loop
        bool pingpong = false; // Whether the tween should ping-pong
        bool reverse = false; // Whether the tween is currently in reverse mode

        std::function<float(float)> easing = TweenLinear; // Type of tweening function to use

        bool active = true; // Whether the tween is currently active
        float elapsedTime = 0.0f; // Time elapsed since the tween started
    };

    struct TweenSystem : public System<Own<TweenComponent>, Listener<TickEvent>>
    {
        virtual std::string getSystemName() const override { return "Tween System"; }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void execute() override
        {
            if (deltaTime == 0)
                return;

            for (auto tween : view<TweenComponent>())
            {
                if (tween->active)
                {
                    tween->elapsedTime += deltaTime;

                    if (tween->elapsedTime >= tween->duration)
                    {
                        tween->elapsedTime = tween->duration;
                        tween->active = false;

                        if (tween->onCompleteCallback)
                            tween->onCompleteCallback->call(ecsRef);
                    }

                    float t = tween->elapsedTime / tween->duration;

                    // if (tween->pingpong)
                    // {
                    //     t = fmod(t, 2.0f);
                    //     t = t > 1.0f ? 2.0f - t : t;
                    // }

                    t = tween->easing(t);

                    TweenValue value = tween->start + (tween->end - tween->start) * t;

                    if (tween->onUpdateCallback)
                        tween->onUpdateCallback(value);
                }
            }

            deltaTime = 0;
        }

        uint64_t deltaTime = 0;
    };
}