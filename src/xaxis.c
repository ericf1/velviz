#include "xaxis.h"

void initAxis(Axis* ax) {
    ax->max = 10.0f;
    ax->tickCount = 5;
    for (int i = 0; i < ax->tickCount; ++i) {
        ax->tickValues[i] = i * 2; // Example tick values
    }
}

void updateAxis(Axis* ax, float newMax) {
    // if our newMax <= 50, ticks are every 5 units
    // if our newMax <= 100, ticks are every 10 units
    // if our newMax <= 200, ticks are every 20 units

    // we need to have two extra ticks beyond the max for padding
    int tickInterval;
    if (newMax <= 50) {
        tickInterval = 5;
    } else if (newMax <= 100) {
        tickInterval = 10;
    } else if (newMax <= 200) {
        tickInterval = 20;
    } else if (newMax <= 500) {
        tickInterval = 50;
    } else {
        tickInterval = 500000;
    }

    ax->max = newMax;

    // Update tick values
    ax->tickCount = (newMax / tickInterval) + 2; // +2 for extra ticks
    for (int i = 0; i < ax->tickCount; ++i) {
        ax->tickValues[i] = i * tickInterval;
    }
}