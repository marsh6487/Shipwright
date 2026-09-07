#pragma once
#include <initializer_list>
struct RegisterShipInitFunc {
    RegisterShipInitFunc(void (*)(), std::initializer_list<const char*>) {
    }
};
