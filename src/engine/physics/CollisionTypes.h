#pragma once

#include "pch/Pch.h"

enum class CollisionEventType {
    Enter,
    Exit
};

struct CollisionEvent {
    CollisionEventType type;
    GameObject* obj1;
    GameObject* obj2;
};

