#pragma once

#include <string>

// #include "serialization.h"

namespace pg
{
    class Archive;

    template<typename T>
    void serialize(Archive& archive, const T& element);

    enum class UnionType
    {
        FLOAT,
        DOUBLE,
        INT,
        SIZE_T,
        STRING,
        BOOL
    };

    // Todo add a none type

    /**
     * @struct ElementType
     * @brief Variant strut to hold the element type of a configuration element.
     *
     * This structure can represent a configuration element which can be any of the following types:
     * float, int, string, bool.
     *
     * This structure can be serialized.
     */
    struct ElementType
    {
    private:
        /**
         * @union U
         *
         * @brief Hold the data of the element type.
         */
        union U
        {
            /**
             * @brief Construct a new union U object.
             *
             * Need to explicitly declare ctor and dtor cause they are non POD type in the union.
             * By default the union construct itself as an int with the value of 0;
             */
            U() { i = 0; }

            /**
             * @brief Destroy the U object.
             */
            ~U() {}

            float f;        ///< Float representation of the element type.
            double d;       ///< Double representation of the element type.
            int i;          ///< Int representation of the element type.
            size_t l;       ///< Size_t representation of the element type
            std::string s;  ///< String representation of the element type.
            bool b;         ///< Bool representation of the element type.
            // Big Int bi;
        };

    public:
        /** The type of the internal representation of the union. */
        UnionType type = UnionType::INT;

        /**
         * @brief Construct a new Element Type object.
         *
         * By default it is a int set as 0.
         */
        ElementType() { this->setValue(0); emptyFlag = true; }

        /**
         * @brief Construct a new Element Type object.
         *
         * @tparam Type The type of the value to be constructed.
         * @param value The value to be constructed.
         *
         * This constructor is explicit to avoid any problems with const char* being implicitly converted to int !
         * Currently this class only support 5 types of data:
         *  -   float
         *  -   int
         *  -   size_t
         *  -   string
         *  -   bool
         */
        template <typename Type>
        ElementType(const Type& value) { this->setValue(value); }

        ElementType(const char* value) { this->setValue(value); }

        ElementType(const ElementType& other)
        {
            switch (other.type)
            {
                case UnionType::FLOAT:  this->setValue(other.data.f); break;
                case UnionType::DOUBLE: this->setValue(other.data.d); break;
                case UnionType::INT:    this->setValue(other.data.i); break;
                case UnionType::SIZE_T: this->setValue(other.data.l); break;
                case UnionType::STRING: this->setValue(other.data.s); break;
                case UnionType::BOOL:   this->setValue(other.data.b); break;
            }
        }

        ElementType(ElementType&& other) noexcept
            : type(other.type), emptyFlag(other.emptyFlag)
        {
            switch (other.type)
            {
                case UnionType::FLOAT:  data.f = other.data.f; break;
                case UnionType::DOUBLE: data.d = other.data.d; break;
                case UnionType::INT:    data.i = other.data.i; break;
                case UnionType::SIZE_T: data.l = other.data.l; break;
                case UnionType::STRING:
                    new(&data.s) std::string(std::move(other.data.s));
                    other.data.s.~basic_string();
                    other.type = UnionType::INT;
                    other.data.i = 0;
                    break;
                case UnionType::BOOL:   data.b = other.data.b; break;
            }
        }

        ~ElementType() { clearPreviousType(); }

        void operator=(const ElementType& other)
        {
            switch (other.type)
            {
                case UnionType::FLOAT:  this->setValue(other.data.f); break;
                case UnionType::DOUBLE: this->setValue(other.data.d); break;
                case UnionType::INT:    this->setValue(other.data.i); break;
                case UnionType::SIZE_T: this->setValue(other.data.l); break;
                case UnionType::STRING: this->setValue(other.data.s); break;
                case UnionType::BOOL:   this->setValue(other.data.b); break;
            }
        }

        ElementType& operator=(ElementType&& other) noexcept
        {
            if (this != &other)
            {
                clearPreviousType();

                type = other.type;
                emptyFlag = other.emptyFlag;

                switch (other.type)
                {
                    case UnionType::FLOAT:  data.f = other.data.f; break;
                    case UnionType::DOUBLE: data.d = other.data.d; break;
                    case UnionType::INT:    data.i = other.data.i; break;
                    case UnionType::SIZE_T: data.l = other.data.l; break;
                    case UnionType::STRING:
                        new(&data.s) std::string(std::move(other.data.s));
                        other.data.s.~basic_string();
                        other.type = UnionType::INT;
                        other.data.i = 0;
                        break;
                    case UnionType::BOOL:   data.b = other.data.b; break;
                }
            }
            return *this;
        }

        void operator=(const float& value)
        {
            this->setValue(value);
        }

        void operator=(const double& value)
        {
            this->setValue(value);
        }

        void operator=(const int& value)
        {
            this->setValue(value);
        }

        void operator=(const size_t& value)
        {
            this->setValue(value);
        }

        void operator=(const char* value)
        {
            this->setValue(value);
        }

        void operator=(const std::string& value)
        {
            this->setValue(value);
        }

        void operator=(const bool& value)
        {
            this->setValue(value);
        }

        void setValue(float value)
        {
            clearPreviousTypeIfNeeded(UnionType::FLOAT);

            data.f = value;
            this->type = UnionType::FLOAT;
        }

        void setValue(double value)
        {
            clearPreviousTypeIfNeeded(UnionType::DOUBLE);

            data.d = value;
            this->type = UnionType::DOUBLE;
        }

        void setValue(int value)
        {
            clearPreviousTypeIfNeeded(UnionType::INT);

            data.i = value;
            this->type = UnionType::INT;
        }

        void setValue(size_t value)
        {
            clearPreviousTypeIfNeeded(UnionType::SIZE_T);

            data.l = value;
            this->type = UnionType::SIZE_T;
        }

#ifdef __EMSCRIPTEN__
        void setValue(unsigned long long value)
        {
            clearPreviousType();

            data.l = value;
            this->type = UnionType::SIZE_T;
        }
#elif UINTPTR_MAX == 0xffFFffFF
        void setValue(unsigned long long value)
        {
            clearPreviousType();

            data.l = value;
            this->type = UnionType::SIZE_T;
        }
#endif
        void setValue(const char* value)
        {
            clearPreviousType();

            new(&data.s) std::string(value);
            this->type = UnionType::STRING;
        }

        void setValue(const std::string& value)
        {
            clearPreviousType();

            new(&data.s) std::string(value);
            this->type = UnionType::STRING;
        }

        void setValue(bool value)
        {
            clearPreviousTypeIfNeeded(UnionType::BOOL);

            data.b = value;
            this->type = UnionType::BOOL;
        }

        template <typename Type>
        inline Type get() const
        {
            return static_cast<Type>(*this);
        }

        bool isBool() const
        {
            return type == UnionType::BOOL;
        }

        bool isNumber() const
        {
            return type == UnionType::INT || type == UnionType::FLOAT || type == UnionType::DOUBLE || type == UnionType::SIZE_T;
        }

        bool isLitteral() const
        {
            return type == UnionType::STRING;
        }

        bool isTrue() const;

        std::string getTypeString() const { return enumTypeToString(type); }

        ElementType operator+(const ElementType& other) const;
        ElementType operator-(const ElementType& other) const;
        ElementType operator*(const ElementType& other) const;
        ElementType operator/(const ElementType& other) const;
        ElementType operator%(const ElementType& other) const;

        ElementType operator> (const ElementType& other) const;
        ElementType operator>=(const ElementType& other) const;
        ElementType operator< (const ElementType& other) const;
        ElementType operator<=(const ElementType& other) const;
        ElementType operator==(const ElementType& other) const;
        ElementType operator!=(const ElementType& other) const;

        template <typename Type>
        ElementType operator==(const Type& other) const { return *this == ElementType{other}; }

        ElementType operator-() const;

        std::string toString() const;

        inline static std::string getType() { return "ElementType"; }

        inline bool isEmpty() const { return emptyFlag; }

        explicit operator bool() const;

    private:
        friend void serialize<>(Archive& archive, const ElementType& element);

        void clearPreviousType()
        {
            emptyFlag = false;

            if (this->type == UnionType::STRING)
                data.s.~basic_string();

            // TODO add Big Int clear too here
        }

        void clearPreviousTypeIfNeeded(UnionType newType)
        {
            emptyFlag = false;

            // Optimization: only clear if we're changing from string to non-string
            if (this->type == UnionType::STRING && newType != UnionType::STRING)
                data.s.~basic_string();
        }

        std::string enumTypeToString(const UnionType& type) const;

        explicit operator float() const;
        explicit operator double() const;
        explicit operator int() const;
        explicit operator size_t() const;
        explicit operator std::string() const;

        U data;

        bool emptyFlag = false;
    };

    typedef std::unordered_map<std::string, ElementType> ElementMap;

    // Declare the ElementType serialize/deserialize specializations here so they are visible
    // wherever ElementType is used, preventing the generic templates from being instantiated first.
    template <>
    void serialize(Archive& archive, const ElementType& element);

    class UnserializedObject;

    template <typename Type>
    Type deserialize(const UnserializedObject& serializedString);

    template <>
    ElementType deserialize(const UnserializedObject& serializedString);
}