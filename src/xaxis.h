#ifndef AXIS_H
#define AXIS_H

typedef struct {
    float max;         // current max visible value
    int tickCount;  // number of ticks
    float tickValues[32];    // tick values
} Axis;

/**
 * Initialize the axis with default values.
 */
void initAxis(Axis* ax);

/**
 * Update the axis with a new max value.
 * Handles smoothing, spacing, and tick logic.
 */
void updateAxis(Axis* ax, float newMax);



#endif // AXIS_H
