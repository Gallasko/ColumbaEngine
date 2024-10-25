#pragma once

/**
 * @file sparseset.h
 * @author Pigeon Codeur (pigeoncodeur@gmail.com)
 * @brief Definition of the sparse set container class
 * @version 0.1
 * @date 2022-08-02
 * 
 * @copyright Copyright (c) 2022
 */

#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

#include "entity.h"

#include "Memory/memorypool.h"

#include "logger.h"

namespace pg
{
    // TODO make doc
    class SparseSet
    {
    private:
        // Todo make doc
        class SparseSetList
        {
        friend class SparseSet;
        public:
            // TODO make doc
            class Iterator
            {
            friend class SparseSet;
            friend class SparseSetList;
                // Public interface
            public:
                /** 
                 * @brief Overload of the pre increment operator
                 * 
                 * @return The current iterator with the next indice
                 */
                inline Iterator& operator++() { index++; return *this; }
                
                /**
                 * @brief Overload of the post increment operator
                 * 
                 * @return A new iterator with the next indice
                 */
                inline Iterator operator++(int) { Iterator old = *this; index++; return old; }

                /**
                 * @brief Overload of the equal operator
                 * 
                 * @param rhs Value to compare to
                 * 
                 * @return true if the value are equal
                 * @return false otherwise
                 */
                inline bool operator==(const Iterator& rhs) const { return index == rhs.index; } 
                
                /**
                 * @brief Overload of the not equal operator
                 * 
                 * @param rhs Value to compare to
                 * 
                 * @return true if the value are not equal
                 * @return false otherwise
                 */
                inline bool operator!=(const Iterator& rhs) const { return index != rhs.index; } 

                /**
                 * @brief Overload of the * operator
                 * 
                 * @return _unique_id A unique identifier in the list
                 */
                inline _unique_id operator*() const { return dense[index]; }

                // Protected constructor
            protected:
                Iterator(const size_t& pos, _unique_id *dense) : index(pos), dense(dense) { LOG_THIS_MEMBER("Sparse Set List Iterator"); }

                // Private variables
            private:
                size_t index = 1;

                _unique_id *dense;
            };

            // Public interface
        public:
            // Todo make doc
            _unique_id operator[](const size_t& index) const { LOG_THIS_MEMBER("Sparse Set List"); return dense[index]; }

            /**
             * @brief Get the head iterator
             * 
             * @return constexpr Iterator An iterator at the head of the dense list
             */
            inline Iterator begin() const { LOG_THIS_MEMBER("Sparse Set List"); return head; }

            /**
             * @brief Get the tail iterator
             * 
             * @return constexpr Iterator An iterator at the tail of the dense list
             */
            inline Iterator end() const { LOG_THIS_MEMBER("Sparse Set List"); return tail; }

            // Public constructor
        public:
            /**
             * @brief Construct a copy of a Sparse Set List object
             * 
             * @param other The Sparse Set List to copy
             */
            SparseSetList(const SparseSetList& other) : head(other.head), tail(other.head), dense(other.dense) { LOG_THIS_MEMBER("Sparse Set List"); }

            // Protected constructor
        protected:
            // Todo make doc
            SparseSetList(const size_t& size, _unique_id *dense) : head(1, dense), tail(size, dense), dense(dense) { LOG_THIS_MEMBER("Sparse Set List"); }

            // Private variables
        private:
            /** An iterator at the beginning of the list */
            const Iterator head;
            /** An iterator at the end of the list */
            const Iterator tail;

            /** The dense list to iterate over */
            _unique_id *dense;      
        };
        
        // Public constructors
    public:
        /** Construct a new Sparse Set object */
        SparseSet();

        /** Sets can't be copied as they own data */
        SparseSet(const SparseSet&) = delete;

        /** Destroy the Sparse Set object */
        virtual ~SparseSet();
    
        // Public interface
    public:
        /**
         * @brief A check function to know if an id is in this sparse set
         * 
         * @param id The id to check if it is in the sparse set
         * @return true if the id is in the set
         * @return false otherwise
         * 
         * This function uses one of the main properties of the sparse set, the reciprocity of the id in the dense and sparse array
         * This operation is O(1) as it only need 2 indirections and 3 checks to know if an id is in the list and this is true whatever the size of the array
         */
        inline bool has(const _unique_id& id) const { return id < sparseCapacity and sparse[id] < size and dense[sparse[id]] == id; };

        /**
         * @brief Get the id at a given index of the set
         * 
         * @param index The index to look for in the set
         * @return _unique_id The id or 0 if the index is not inside of the set
         */
        inline _unique_id at(const size_t& index) const
        {
            LOG_THIS_MEMBER("Sparse Set");

            if (index >= size)
                return 0;
            
            return dense[index];
        }

        /**
         * @brief Get the index of a given id if it is present in the set
         * 
         * @param id The id to store in the set
         * @return size_t The index of the id or 0 if the index is not inside of the set
         */
        inline size_t find(const _unique_id& id) const
        {
            if (has(id))
                return sparse[id];

            return 0;
        }

        /** Add an id inside of the set */
        size_t add(const _unique_id& id);

        /** Remove an id in the set */
        size_t remove(const _unique_id& id);

        /** Clear the entire list */
        inline virtual void clear()
        {
            LOG_THIS_MEMBER("Sparse Set");

            size = 1;
        }

        /**
         * @brief Get the current size of the list
         * 
         * @return constexpr size_t The current size of the list
         * 
         * The list start at index 1 to nbElement()
         * This function is used to ensure that the bound of the set are respected
         */
        inline size_t nbElements() const { LOG_THIS_MEMBER("Sparse Set"); return size; }

        inline SparseSetList view() const
        {
            LOG_THIS_MEMBER("Sparse Set");
            
            return SparseSetList(nbElements(), dense);
        }

        // Private interface
    private:
        /** Internal helper function used to expend the dense and the component list */
        void addDenseCapacity(const size_t& size);

        /** Internal helper function used to expend the sparse list */
        void addSparseCapacity(const _unique_id& id);

        // Private variables
    private:
        /** The current size of the sparse set */
        size_t size = 1;
        
        /** An interal array to hold the link componend id -> entity id */
        _unique_id* dense;

        /** An interal array to hold the link entity id -> component id */
        // size_t* sparse;
        size_t* sparse;

        /** The capacity of the dense array */
        size_t denseCapacity = 2;

        /** The capacity of the sparse array */
        size_t sparseCapacity = 2;
    };

    // 
    /**
     * @brief A container object used to store components
     * 
     * This container is used to store components and the entity id they are associated with.
     * As entities ids as well as components id are unsigned value. The indice start at 1,
     * so that 0 indicates that the component doesn't exist or no entity was found.
     * 
     * This container as:
     * - O(1) time complexity for both inserting and removing components
     * - O(1) time complexity for lookup
     * - O(n) time complexity for clear (as it delete all the components in memory)
     * - O(n) time complexity for iteration over components
     * 
     * It also stores in memory the list of components of a given type
     * 
     * @warning Never delete a ComponentSet through a SparseSet pointer
     * 
     * @todo Make a sparse set implementation that doesn't delete components on remove but instead reuse dead memory
     */
    template <typename Comp>
    class ComponentSet : public SparseSet
    {
    private:
        typedef typename std::aligned_storage<sizeof(Comp), alignof(Comp)>::type CompStorage;

    public:
        /**
         * @brief List representation of the component of the component set
         * 
         * This helper class is used to iterate through the whole component list of the component set
         * It provides a basic [] interface as well as an iterator to support range based for loop
         */
        class ComponentSetList
        {
        friend class ComponentSet;
        public:
            /**
             * @brief An Iterator for iterating over the elements of a component set
             * 
             * This iterator is a read only iterator that iterates over the elements ot the component set
             * Using the operator-> you can obtain a pointer to the component 
             */
            class Iterator
            {
            friend class ComponentSet;
            friend class ComponentSetList;
                // Public interface
            public:
                /** 
                 * @brief Overload of the pre increment operator
                 * 
                 * @return The current iterator with the next indice
                 */
                inline Iterator& operator++() { index++; return *this; }
                
                /**
                 * @brief Overload of the post increment operator
                 * 
                 * @return A new iterator with the next indice
                 */
                inline Iterator operator++(int) { Iterator old = *this; index++; return old; }

                /**
                 * @brief Overload of the equal operator
                 * 
                 * @param rhs Value to compare to
                 * 
                 * @return true if the value are equal
                 * @return false otherwise
                 */
                inline bool operator==(const Iterator& rhs) const { return index == rhs.index; } 
                
                /**
                 * @brief Overload of the not equal operator
                 * 
                 * @param rhs Value to compare to
                 * 
                 * @return true if the value are not equal
                 * @return false otherwise
                 */
                inline bool operator!=(const Iterator& rhs) const { return index != rhs.index; } 

                /**
                 * @brief Overload of the * operator
                 * 
                 * @return Comp* A pointer to the component stored recasted into the actual component
                 */
                inline Comp* operator*() { return componentList[index]; }

                /**
                 * @brief Overload of the * operator
                 * 
                 * @return Comp* A pointer to the component stored recasted into the actual component
                 */
                inline const Comp* operator*() const { return componentList[index]; }

                /**
                 * @brief Overload of the * operator
                 * 
                 * @return Comp* A pointer to the component stored recasted into the actual component
                 */
                inline Comp* operator[](size_t i) { return componentList[i];}

                /**
                 * @brief Overload of the * operator
                 * 
                 * @return Comp* A pointer to the component stored recasted into the actual component
                 */
                inline const Comp* operator[](size_t i) const { return componentList[i];}

                // Protected constructor
            protected:
                /**
                 * @brief Construct a new Iterator object
                 * 
                 * @param pos The starting position in the component list array of the iterator
                 * @param componentList The component list to iterate over
                 * 
                 * This object can only be created from a ComponentSet List inside of a ComponentSet Object
                 */
                Iterator(const size_t& pos, Comp **componentList) : index(pos), componentList(componentList) { LOG_THIS_MEMBER("Component Set List Iterator"); }

                // Private variables
            private:
                /** Index of the current position in the componentList */
                size_t index = 1;
                /** An array of component pointers */
                Comp **componentList;
            };

            // Public interface
        public:
            /**
             * @brief Overload of the [] operator
             * 
             * @param index Position in the component list
             * @return Comp* The pointer requested from the component list
             * 
             * This helper operator is used to provide access to a component inside of the component list.
             * Be careful as the operator doesn't not check the bound of the list, this can throw an out of bound exception
             * Use with nbElement of the sparse set to be in bound
             */
            Comp* operator[](const size_t& index) const { return componentList[index]; }

            /**
             * @brief Get the head iterator
             * 
             * @return constexpr Iterator An iterator at the head of the component list
             */
            inline Iterator begin() const { LOG_THIS_MEMBER("Component Set List"); return head; }

            /**
             * @brief Get the tail iterator
             * 
             * @return constexpr Iterator An iterator at the tail of the component list
             */
            inline Iterator end() const { LOG_THIS_MEMBER("Component Set List"); return tail; }

            // Public constructor
        public:
            /**
             * @brief Construct a copy of a Sparse Set List object
             * 
             * @param other The Sparse Set List to copy
             */
            ComponentSetList(const ComponentSetList& other) : head(other.head), tail(other.head), componentList(other.componentList) { LOG_THIS_MEMBER("Component Set List"); }

            /**
             * @brief Get the number of components in the list
             * 
             * @return size_t the number of components in the list
             */
            size_t nbComponents() const { LOG_THIS_MEMBER("Component Set List"); return tail.index; }

            // Protected constructor
        protected:
            /**
             * @brief Construct a new Sparse Set List object
             * 
             * @param size The current size of the component list
             * @param componentList The component list to iterate over
             * 
             * This object can only be created from a SparseSet Object
             */
            ComponentSetList(const size_t& size, Comp **componentList) : head(1, componentList), tail(size, componentList), componentList(componentList) { LOG_THIS_MEMBER("Component Set List"); }

            // Private variables
        private:
            /** An iterator at the beginning of the list */
            Iterator head;
            /** An iterator at the end of the list */
            Iterator tail;

            /** The component list to iterate over */
            Comp **componentList;      
        };

    public:
        ComponentSet() : SparseSet()
        {
            LOG_THIS_MEMBER("Component Set");

            LOG_INFO("Component Set", "Creating component set for: " << typeid(Comp).name());

            componentList = new Comp*[componentCapacity];

            // Set the first element as nullptr as it shouldn't be a valid component ever
            componentList[0] = nullptr;
        };

        virtual ~ComponentSet()
        {
            LOG_THIS_MEMBER("Component Set");

            LOG_INFO("Component Set", "Removing component set for: " << typeid(Comp).name());

            for(size_t i = 1; i < nbComponents; i++)
                pool.release(componentList[i]);

            delete[] componentList;
        }

        /**
         * @brief Overload of the [] operator
         * 
         * @param index Position in the component list
         * @return Comp* The pointer requested from the component list
         * 
         * This helper operator is used to provide access to a component inside of the component list.
         * Be careful as the operator doesn't not check the bound of the list, this can throw an out of bound exception
         * Use with nbElement of the sparse set to be in bound
         */
        Comp* operator[](const size_t& index) const { return componentList[index]; }

        /**
         * @brief Get a component from the entity id
         * 
         * @param id Id of the entity
         * @return Comp* A pointer to the associated component
         */
        inline Comp* atEntity(_unique_id id) const { auto pos = find(id); return pos != 0 ? componentList[pos] : nullptr; }

        /**
         * @brief Reserve enough space in the set to hold the requested number of objects
         * 
         * @param size The needed size of the set
         */
        void reserve(const size_t& size)
        {
            LOG_THIS_MEMBER("Component Set");

            if (size < componentCapacity)
                return;

            size_t targetCapacity = componentCapacity;

            // This small loop make it so sparseCapacity stays as a multiple of 2
            while (targetCapacity <= size)
            {
                targetCapacity *= 2;
            }

            Comp** tempComponentList = new Comp*[targetCapacity];

            memcpy(tempComponentList, componentList, componentCapacity * sizeof(Comp*));
            delete[] componentList;
            componentList = tempComponentList;
            componentCapacity = targetCapacity;
            
            pool.reserve(size);
        }

        template <typename... Args>
        Comp* addComponent(_unique_id id, Args&&... args)
        {
            LOG_THIS_MEMBER("Component Set");

            if (has(id))
            {
                LOG_INFO("Component Set", "Attaching an already existing component to id " << id);

                const auto index = find(id);

                pool.release(componentList[index]);

                auto component = pool.allocate(std::forward<Args>(args)...);

                componentList[index] = component;

                return component;
            }

            const auto index = add(id);

            if (index == 0)
            {
                LOG_ERROR("Component Set", "Invalid index, entity was not added");
                return nullptr;
            }

            if (index >= componentCapacity)
            {
                LOG_MILE("Component Set", "Increasing size of the component set");

                this->reserve(index);
            }

            lastEntityIndex = index;
            
            // Todo: Test if allocating memory in a pool is faster than direct memory allocation with new
            auto component = pool.allocate(std::forward<Args>(args)...);

            componentList[nbComponents++] = component;

            return component;
        }
        
        template <typename... Args>
        inline Comp* addComponent(const Entity* entity, Args&&... args)
        {
            LOG_THIS_MEMBER("Component Set");

            return addComponent(entity->id, std::forward<Args>(args)...);
        }

        void removeComponent(_unique_id id)
        {
            LOG_THIS_MEMBER("Component Set");

            const auto index = remove(id);
            
            if (index == 0)
            {
                LOG_ERROR("Component Set", "Invalid index, entity was not removed");
                return;
            }

            pool.release(componentList[index]);

            // Swap the last component in the place of the component to be removed
            componentList[index] = componentList[--nbComponents];

            if (nbComponents <= 1)
                nbComponents = 1; 
        }

        // TODO make a sparse set implementation that doesn't delete components on remove but instead reuse dead memory
        inline void removeComponent(const Entity* entity)
        {
            LOG_THIS_MEMBER("Component Set");

            removeComponent(entity->id);
        }

        /**
         * @brief Expose a view of the component list to a system
         * 
         * @tparam Comp The type of the component to cast the component stored in this list
         * @return ComponentSetList A view of the component list
         * 
         * @warning Any operation on this view is invalid if the component list is updated
         */
        inline ComponentSetList viewComponents() const
        {
            LOG_THIS_MEMBER("Component Set");

            return ComponentSetList(nbComponents, componentList);
        }

        // Todo reimplement clear to correctly free components

    private:
        /** The component list holding the data of all the component of this sparse set */
        Comp** componentList;

        /** The allocator pool that store all the component memory in a packed manner */
        AllocatorPool<Comp> pool;

        /** Number of component actually allocated */
        size_t nbComponents = 1;

        /** Current capacity of component in the set */
        size_t componentCapacity = 2;

        size_t lastEntityIndex = 0;
    };

    /**
     * @brief A container object used to store components
     * 
     * This container is used to store components and the entity id they are associated with.
     * As entities ids as well as components id are unsigned value. The indice start at 1,
     * so that 0 indicates that the component doesn't exist or no entity was found.
     * 
     * This container as:
     * - O(1) time complexity for both inserting and removing components
     * - O(1) time complexity for lookup
     * - O(n) time complexity for clear (as it delete all the components in memory)
     * - O(n) time complexity for iteration over components
     * 
     * It also stores in memory the list of components of a given type
     * 
     * @warning Never delete a ComponentSet through a SparseSet pointer
     * 
     * @todo Make a sparse set implementation that doesn't delete components on remove but instead reuse dead memory
     */
    template <typename Comp>
    class GroupSet : public ComponentSet<Comp>
    {

    };
}