#pragma once

// This is the 'gamma' singleton. it is efficient since the object is statically allocated, and each access
// does not need to check if the object exists or not. It also does not use the heap as a standard singleton would.
// However, static objects are created/destructed in the order the compiler pleases. So, those singleton should
// not reference each other in their constructor/destructor, and you should call an init()/destroy() method
// on them in your programs initialization and termination, thus specifying the order.
// As an alternative, the 'meyer' singleton defines the static object internally to the get() method and does not
// suffer from order of instantiation/destruction problems. However, each access needs to check whether the object
// is initialized or not and is less efficient.
template <typename T>
class static_singleton
{
private:
    /* The instance of the singleton object */
    static T singleton;

public:
    /* Returns the singleton as reference */
    static T& get()
    {
        return singleton;
    }
};

template <typename T> T static_singleton<T>::singleton;
