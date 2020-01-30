//
// Created by mb4416 on 15/01/2020.
//

//
// Created by mb4416 on 15/01/2020.
//

#include <iostream>
#include <memory>
#include <functional>

/**
 * Generic class for object pools.
 */
template <class T, int SZ, class Initialiser, class Releaser>
class StackObjectPool {
public:
    using pointer = T*;
    using PoolType = StackObjectPool<T,SZ,Initialiser, Releaser>;

    StackObjectPool() {}
    StackObjectPool(const PoolType& orig) = delete;
    ~StackObjectPool() {}

    /**
     * Acquires an object not being currently used
     * @return pointer to the acquired object
     * @throw std::out_of_range if all the objects inside the pool are being used
     */
    T* acquire() {
        unsigned int index = 0; // look for the first free object
        while(m_occupied_registry[index]) ++index;
        if(index >= SZ) throw std::out_of_range("Pool exceeded its size");
        m_occupied_registry[index] = true; // mark it as currently in use
        m_initialiser(&m_objects[index]); // initialise it
        //printf("acquiring %p %p %d\n", this,  &m_objects[index], index);
        //return an unique_ptr that calls release when reset
        return &m_objects[index];
    }

    void release(T* element) {
        unsigned int index = element - m_objects;
        m_releaser(element); // call release functor
        //printf("releasing %p %p %d\n", this, element, index);
        m_occupied_registry[index] = false; // mark the released element as free
    }
    private:

    Initialiser m_initialiser;
    Releaser m_releaser;
    bool m_occupied_registry[SZ] {0};
    T m_objects[SZ];
};