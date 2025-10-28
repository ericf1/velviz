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
    float tickInterval;
    if (newMax <= 50) {
        tickInterval = 5;
    } else if (newMax <= 100) { // between 51 and 100
        tickInterval = 10;
    } else if (newMax <= 200) { // between 101 and 200
        tickInterval = 20;
    } else if (newMax <= 500) { // between 201 and 500
        tickInterval = 50;
    } else if (newMax <= 1000){ // between 501 and 1000
        tickInterval = 500;
    } else if (newMax <= 5000) { // between 1,001 and 5,000
        tickInterval = 1000;
    } else if (newMax <= 10000) { // between 5,001 and 10,000
        tickInterval = 5000;
    } else if (newMax <= 50000) {   // between 10,001 and 50,000
        tickInterval = 10000;
    } else if (newMax <= 100000) {  // between 50,001 and 100,000
        tickInterval = 10000;
    } else if (newMax <= 500000) { // between 100,001 and 500,000
        tickInterval = 100000;
    } else if (newMax <= 1000000) { // between 500,001 and 1,000,000
        tickInterval = 100000;
    } else if (newMax <= 5000000) { // between 1,000,001 and 5,000,000
        tickInterval = 500000;
    } else if (newMax <= 10000000) { // between 5,000,001 and 10,000,000
        tickInterval = 1000000;
    } else if (newMax <= 50000000) { // between 10,000,001 and 50,000,000
        tickInterval = 5000000;
    } else if (newMax <= 100000000) { // between 50,000,001 and 100,000,000
        tickInterval = 10000000;
    } else if (newMax <= 500000000) { // between 100,000,001 and 500,000,000
        tickInterval = 50000000;
    } else if (newMax <= 1000000000) { // between 500,000,001 and 1,000,000,000
        tickInterval = 100000000;
    } else if (newMax <= 5000000000) { // between 1,000,000,001 and 5,000,000,000
        tickInterval = 500000000;
    } else if (newMax <= 10000000000) { // between 5,000,000,001 and 10,000,000,000
        tickInterval = 1000000000;
    } else if (newMax <= 50000000000) { // between 10,000,000,001 and 50,000,000,000
        tickInterval = 5000000000;
    } else if (newMax <= 100000000000) { // between 50,000,000,001 and 100,000,000,000
        tickInterval = 10000000000;
    } else if (newMax <= 500000000000) { // between 100,000,000,001 and 500,000,000,000
        tickInterval = 50000000000;
    } else if (newMax <= 1000000000000) { // between 500,000,000,001 and 1,000,000,000,000
        tickInterval = 100000000000;
    } else if (newMax <= 5000000000000) { // between 1,000,000,000,001 and 5,000,000,000,000
        tickInterval = 500000000000;
    } else { // above 5,000,000,000,000
        tickInterval = 1000000000000;
    }

    ax->max = newMax;

    // Update tick values
    ax->tickCount = (newMax / tickInterval) + 2; // +2 for extra ticks
    for (int i = 0; i < ax->tickCount; ++i) {
        ax->tickValues[i] = i * tickInterval;
    }
}