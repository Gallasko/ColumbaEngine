#pragma once

#include <cstdint>
#include <mutex>

namespace pg
{
    /** A 64bits unsigned value used for identifiers */
    // Todo fix this for 32 bits systems (Emscripten)
    // #ifdef __EMSCRIPTEN__
    // typedef size_t _unique_id;
    // #else
    // typedef uint_fast64_t _unique_id;
    // #endif

    typedef uint_fast64_t _unique_id;

    /** A fast 32bits unsigned value */
    typedef uint_fast32_t _uint32;

    /**
     * @brief Generator of unique identifiers
     *
     * This class generates unique identifier in a non-thread and thread safe manner.
     *
     * The first valid id is 3 as 0 is reserved for NONE, 1 is reserved for Ecs ID and 2 is reserved for Ecs Name,
     * which are the basic id of the ecs.
     */
    class UniqueIdGenerator
    {
    public:
        /**
         * @brief A simple holder struct for a list of unique identifiers
         *
         */
        struct UniqueIdList
        {
            /** First unique identifier */
            const _unique_id start;
            /** Last unique identifier */
            const _unique_id end;
            /** Number of unique identifier */
            const _unique_id length;
        };

    public:
        /**
         * @brief Generate an unique identifier
         *
         * @tparam ThreadSafe Set to true to execute in a thread safe manner
         *
         * @return _unique_id An unique 64bit identifier
         *
         * This function generates an unique identifier each time it is called.
         * Can be called in a thread safe manner but no non thread safe generateId should be call during that time !
         */
        template <bool ThreadSafe = false>
        _unique_id generateId()
        {
            _unique_id id;

            if constexpr (ThreadSafe)
            {
                std::lock_guard lock(m);
                id = currentId++;
            }
            else
                id = currentId++;

            if (id == 0)
                throw std::overflow_error("Out of entity IDs—wraparound detected");

            return id;
        }

        /**
         * @brief Generate an unique identifier
         *
         * @tparam ThreadSafe Set to true to execute in a thread safe manner
         *
         * @return UniqueIdList A start and end index of the generated Id
         *
         * This function generates a list of unique identifier each time it is called.
         * Can be called in a thread safe manner but no non thread safe generateIdList should be call during that time !
         */
        template <bool ThreadSafe = false>
        UniqueIdList generateIdList(_unique_id size) noexcept
        {
            if constexpr (ThreadSafe)
            {
                std::lock_guard lock(m);
                return {currentId, (currentId += size) - 1, size};
            }
            else
                return {currentId, (currentId += size) - 1, size};
        }

    private:
        /** Next id to be generated */
        _unique_id currentId = 3;

        /** Mutex for concurrent access */
        std::mutex m;
    };
}