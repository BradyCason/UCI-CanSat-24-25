#ifndef MADGWICK_AHRS_H
#define MADGWICK_AHRS_H

#include <math.h>

// Struct to hold the algorithm state
typedef struct {
    float beta;               // algorithm gain
    float q0, q1, q2, q3;     // quaternion of sensor frame relative to auxiliary frame
    float invSampleFreq;      // inverse of sample frequency
    float roll, pitch, yaw;   // Euler angles
    char anglesComputed;      // flag indicating if angles are computed
} MadgwickAHRS;

// Function declarations
void Madgwick_init(MadgwickAHRS *ahrs, float sampleFrequency);
void Madgwick_update(MadgwickAHRS *ahrs, float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz);
void Madgwick_updateIMU(MadgwickAHRS *ahrs, float gx, float gy, float gz, float ax, float ay, float az);
float Madgwick_getRoll(MadgwickAHRS *ahrs);
float Madgwick_getPitch(MadgwickAHRS *ahrs);
float Madgwick_getYaw(MadgwickAHRS *ahrs);

#endif
