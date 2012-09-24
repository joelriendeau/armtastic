#pragma once

// static_singleton : allocated in static storage, always available, even if not declared anywhere
template <typename T>
class static_singleton
{
private:
    // The instance of the singleton object
    static T singleton;
public:
    // Returns the singleton as reference
    static T& get() { return singleton; }
};
template <typename T> T static_singleton<T>::singleton;

// external_singleton : allocated anywhere (stack or main, heap), set before being used
// todo for maximum safety : assert if singleton != null when built (catch multiple allocations), assert on get if singleton == null
template <typename T>
class external_singleton
{
private:
    // The instance of the singleton object
    static T* singleton;
public:
    external_singleton() { singleton = &static_cast<T&>(*this); }
    // Returns the singleton as reference
    static T& get() { return *singleton; }
};
template <typename T> T* external_singleton<T>::singleton = 0;