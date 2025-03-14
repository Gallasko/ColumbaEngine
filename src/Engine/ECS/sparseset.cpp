#include "sparseset.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Sparse Set";
    }

    /**
     * @brief Construct a new Sparse Set object
     * 
     * Initialize all the internal arrays
     * Component list should always strictly match the dense array
     */
    SparseSet::SparseSet()
    {
        LOG_THIS_MEMBER(DOM);

        dense = new _unique_id[denseCapacity];

        sparse = new size_t[sparseCapacity];
    }

    /**
     * @brief Destroy the Sparse Set object
     * 
     * Clear all the component in the component list
     * and destroy the internal arrays
     * 
     * @todo Check the cost of this virtual function
     */
    SparseSet::~SparseSet()
    {
        LOG_THIS_MEMBER(DOM);

        clear();

        delete[] dense;

        delete[] sparse;
    }

    /**
     * @brief Add an id inside of the set
     * 
     * @param id The id to add to the set
     *  
     * @return size_t The position of the added id in the set
     * 
     * This function add an id to the set with respect to the dense <-> sparse linkage
     * It also allocate new memory if the set is too small
     */
    size_t SparseSet::add(const _unique_id& id)
    {
        LOG_THIS_MEMBER(DOM);

        // All the entity should always be greater than 0 as 0 is the value of empty in the system
        if (id < 1)
        {
            LOG_ERROR(DOM, "Invalid entity id, must be greater than 0");
            return 0;
        }

        const size_t currentSize = size++;

        // If the size of the list is too small allocate more space
        if (denseCapacity <= currentSize)
        {
            LOG_MILE(DOM, "Dense array is too small (" << denseCapacity << ") to fit the element: " << currentSize << ", proceed to double the capacity");
            addDenseCapacity(currentSize);
        }

        // Link the entity id with the component id through the dense <-> sparse mechanism
        dense[currentSize] = id;

        // Todo implement paging for the sparse array
        if (sparseCapacity <= id)
        {
            LOG_MILE(DOM, "Sparse array is too small (" << sparseCapacity << ") to fit the element: " << id << ", proceed to increase the capacity");
            addSparseCapacity(id);
        }

        // Link the entity id with the component id through the dense <-> sparse mechanism
        sparse[id] = currentSize;

        return currentSize;
    }

    /**
     * @brief Remove an id in the set
     * 
     * @param id the id to be removed
     * @return size_t The index of the removed element
     */
    size_t SparseSet::remove(const _unique_id& id)
    {
        LOG_THIS_MEMBER(DOM);

        // Todo check this and that we need to see before hand that the id is in the list
        const size_t currentSize = size--;

        // Check if the id has a component
        if (currentSize < 1 && !has(id))
            return 0;
        
        const auto index = sparse[id];

        LOG_MILE(DOM, "Removing component of entity: " << id << " at index " << index << " " << currentSize);

        // Update the index of the vector accordingly.

        const auto lastElement = dense[currentSize - 1];

        dense[index] = lastElement;
        sparse[lastElement] = index;
        return index;
    }


    // /**
    //  * @brief Remove a component by component index
    //  * 
    //  * @param index The index of the component to remove.
    //  * 
    //  * @todo Tell the management system that this entity (dense[index]) as lost this component to update all the other "archtype" using it
    //  * @todo Must implement a mutex for each component and entity for multithreaded use !
    //  */
    // void SparseSet::removeAt(const size_t& index)
    // {
    //     LOG_THIS_MEMBER(DOM);

    //     if(size < 1 && index >= size) 
    //         return;

    //     // TODO make a sparse set implementation that doesn't delete components on remove but instead reuse dead memory
    //     // Delete the component
    //     delete componentList[dense[index]];
        
    //     // Swap the last component in the place of the component to be removed
    //     componentList[dense[index]] = componentList[dense[size - 1]];

    //     // Put the last value where the removed value is, to keep a contiguous dense array
    //     // and update the sparse array accordingly.  
    //     sparse[dense[index]] = sparse[dense[size -1]];
    //     dense[index] = dense[size - 1];

    //     // Decrease the size of the list
    //     size--;
    // }

    /**
     * @brief Internal helper function used to expend the dense and the component list
     * 
     * This helper function double both the size of the dense and the component list to store up to size_t amount of data 
     */
    void SparseSet::addDenseCapacity(const size_t& size)
    {
        LOG_THIS_MEMBER(DOM);

        if (denseCapacity > size)
            return;
        
        if (size > SIZE_MAX)
        {
            LOG_ERROR(DOM, "Entity id is too large to fit into sparse set");
            return;
        }

        size_t targetCapacity = denseCapacity;

        // This small loop make it so denseCapacity stays as a multiple of 2
        while(targetCapacity <= size)
        {
            targetCapacity *= 2;
        }

        // Create the doubled size containers
        _unique_id* tempDense = new _unique_id[targetCapacity];
        
        // Copy the current data inside of the newly created containers
        memcpy(tempDense, dense, denseCapacity * sizeof(_unique_id));
        
        // Delete old data to not leak memory
        delete[] dense;
        
        // Set the new containers as the list container
        dense = tempDense;

        // Update the capacity of the list
        denseCapacity = targetCapacity;
    }

    /**
     * @brief Internal helper function used to expend the sparse list
     * 
     * @param[in] id The id to fit inside of the sparse array
     * 
     * This helper function double the size of the sparse to store up to size_t amount of data 
     */
    void SparseSet::addSparseCapacity(const _unique_id& id)
    {
        LOG_THIS_MEMBER(DOM);

        if (sparseCapacity > id)
            return;

        // If the entity is bigger than SIZE_MAX then it can't be expressed as a size_t and so it can't be stored in the array.
        if (id > SIZE_MAX)
        {
            LOG_ERROR(DOM, "Entity id is too large to fit into sparse set");
            return;
        }

        size_t targetCapacity = sparseCapacity;

        // This small loop make it so sparseCapacity stays as a multiple of 2
        while (targetCapacity <= id)
        {
            targetCapacity *= 2;
        }

        // sparse.reserve(targetCapacity);

        // Create a bigger size container
        size_t* tempSparse = new size_t[targetCapacity];

        // Copy the current data inside of the newly created container
        memcpy(tempSparse, sparse, sparseCapacity * sizeof(size_t));

        // Delete old data to not leak memory
        delete[] sparse;

        // Set the new container as the list container
        sparse = tempSparse;

        // Update the capacity of the list
        sparseCapacity = targetCapacity;
    }
}