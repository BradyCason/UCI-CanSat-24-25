#include "MadgwickAHRS.h"

#define SAMPLE_FREQ_DEF  512.0f   // sample frequency in Hz
#define BETA_DEF         0.1f     // 2 * proportional gain

// Inverse square root approximation (fast)
static float invSqrt(float x) {
    return 1.0f / sqrtf(x);
}

void Madgwick_init(MadgwickAHRS *ahrs, float sampleFrequency) {
    ahrs->beta = BETA_DEF;
    ahrs->q0 = 1.0f;
    ahrs->q1 = 0.0f;
    ahrs->q2 = 0.0f;
    ahrs->q3 = 0.0f;
    ahrs->invSampleFreq = 1.0f / sampleFrequency;
    ahrs->anglesComputed = 0;
}

void Madgwick_update(MadgwickAHRS *ahrs, float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz) {
//    float recipNorm;
//    float s0, s1, s2, s3;
//    float qDot1, qDot2, qDot3, qDot4;
//    float hx, hy;
//    float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz, _4bx, _4bz, _2q0, _2q1, _2q2, _2q3, _2q0q2, _2q2q3, q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;
//
//    // Use IMU algorithm if magnetometer measurement invalid
//    if ((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) {
//        Madgwick_updateIMU(ahrs, gx, gy, gz, ax, ay, az);
//        return;
//    }
//
//    // Convert gyroscope degrees/sec to radians/sec
//    gx *= 0.0174533f;
//    gy *= 0.0174533f;
//    gz *= 0.0174533f;
//
//    // Rate of change of quaternion from gyroscope
//    qDot1 = 0.5f * (-ahrs->q1 * gx - ahrs->q2 * gy - ahrs->q3 * gz);
//    qDot2 = 0.5f * (ahrs->q0 * gx + ahrs->q2 * gz - ahrs->q3 * gy);
//    qDot3 = 0.5f * (ahrs->q0 * gy - ahrs->q1 * gz + ahrs->q3 * gx);
//    qDot4 = 0.5f * (ahrs->q0 * gz + ahrs->q1 * gy - ahrs->q2 * gx);
//
//    // Compute feedback only if accelerometer measurement valid
//    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
//        recipNorm = invSqrt(ax * ax + ay * ay + az * az);
//        ax *= recipNorm;
//        ay *= recipNorm;
//        az *= recipNorm;
//
//        recipNorm = invSqrt(mx * mx + my * my + mz * mz);
//        mx *= recipNorm;
//        my *= recipNorm;
//        mz *= recipNorm;
//
//        // Auxiliary variables to avoid repeated arithmetic
//        _2q0mx = 2.0f * ahrs->q0 * mx;
//        _2q0my = 2.0f * ahrs->q0 * my;
//        _2q0mz = 2.0f * ahrs->q0 * mz;
//        _2q1mx = 2.0f * ahrs->q1 * mx;
//        _2q0 = 2.0f * ahrs->q0;
//        _2q1 = 2.0f * ahrs->q1;
//        _2q2 = 2.0f * ahrs->q2;
//        _2q3 = 2.0f * ahrs->q3;
//
//        // More calculations here...
//    }
//
//    // Integrate rate of change of quaternion
//    ahrs->q0 += qDot1 * ahrs->invSampleFreq;
//    ahrs->q1 += qDot2 * ahrs->invSampleFreq;
//    ahrs->q2 += qDot3 * ahrs->invSampleFreq;
//    ahrs->q3 += qDot4 * ahrs->invSampleFreq;
//
//    // Normalise quaternion
//    recipNorm = invSqrt(ahrs->q0 * ahrs->q0 + ahrs->q1 * ahrs->q1 + ahrs->q2 * ahrs->q2 + ahrs->q3 * ahrs->q3);
//    ahrs->q0 *= recipNorm;
//    ahrs->q1 *= recipNorm;
//    ahrs->q2 *= recipNorm;
//    ahrs->q3 *= recipNorm;
//    ahrs->anglesComputed = 0;


	float q0 = ahrs->q0, q1 = ahrs->q1, q2 = ahrs->q2, q3 = ahrs->q3;
	float recipNorm;
	float s0, s1, s2, s3;
	float qDot1, qDot2, qDot3, qDot4;
	float hx, hy;
	float _2q0mx, _2q0my, _2q0mz, _2q1mx;
	float _2q0 = 2.0f * q0;
	float _2q1 = 2.0f * q1;
	float _2q2 = 2.0f * q2;
	float _2q3 = 2.0f * q3;
	float _2q0q2 = 2.0f * q0 * q2;
	float _2q2q3 = 2.0f * q2 * q3;
	float q0q0 = q0 * q0;
	float q0q1 = q0 * q1;
	float q0q2 = q0 * q2;
	float q0q3 = q0 * q3;
	float q1q1 = q1 * q1;
	float q1q2 = q1 * q2;
	float q1q3 = q1 * q3;
	float q2q2 = q2 * q2;
	float q2q3 = q2 * q3;
	float q3q3 = q3 * q3;

	// Use IMU algorithm if magnetometer measurement invalid
	if ((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) {
		Madgwick_updateIMU(ahrs, gx, gy, gz, ax, ay, az);
		return;
	}

	// Convert gyroscope degrees/sec to radians/sec
	gx *= 0.0174533f;
	gy *= 0.0174533f;
	gz *= 0.0174533f;

	// Normalize accelerometer
	recipNorm = invSqrt(ax * ax + ay * ay + az * az);
	ax *= recipNorm;
	ay *= recipNorm;
	az *= recipNorm;

	// Normalize magnetometer
	recipNorm = invSqrt(mx * mx + my * my + mz * mz);
	mx *= recipNorm;
	my *= recipNorm;
	mz *= recipNorm;

	// Reference direction of Earth's magnetic field
	_2q0mx = 2.0f * q0 * mx;
	_2q0my = 2.0f * q0 * my;
	_2q0mz = 2.0f * q0 * mz;
	_2q1mx = 2.0f * q1 * mx;
	hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;
	hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;
	float _2bx = sqrt(hx * hx + hy * hy);
	float _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;
	float _4bx = 2.0f * _2bx;
	float _4bz = 2.0f * _2bz;

	// Gradient descent algorithm corrective step
	s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax) + _2q1 * (2.0f * q0q1 + _2q2q3 - ay) - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx)
		 + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my)
		 + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
	s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax) + _2q0 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az)
		 + _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx)
		 + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my)
		 + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
	s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax) + _2q3 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az)
		 + (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx)
		 + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my)
		 + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
	s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax) + _2q2 * (2.0f * q0q1 + _2q2q3 - ay)
		 + (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx)
		 + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my)
		 + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

	recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3); // normalize step magnitude
	s0 *= recipNorm;
	s1 *= recipNorm;
	s2 *= recipNorm;
	s3 *= recipNorm;

	// Apply feedback step
	qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - ahrs->beta * s0;
	qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy) - ahrs->beta * s1;
	qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx) - ahrs->beta * s2;
	qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx) - ahrs->beta * s3;

	// Integrate quaternion rate and normalize
	q0 += qDot1 * ahrs->invSampleFreq;
	q1 += qDot2 * ahrs->invSampleFreq;
	q2 += qDot3 * ahrs->invSampleFreq;
	q3 += qDot4 * ahrs->invSampleFreq;

	recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
	ahrs->q0 = q0 * recipNorm;
	ahrs->q1 = q1 * recipNorm;
	ahrs->q2 = q2 * recipNorm;
	ahrs->q3 = q3 * recipNorm;

	ahrs->anglesComputed = 0;
}

void Madgwick_updateIMU(MadgwickAHRS *ahrs, float gx, float gy, float gz, float ax, float ay, float az) {
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3;
    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;

    // Normalize accelerometer measurement
    recipNorm = invSqrt(ax * ax + ay * ay + az * az);
    ax *= recipNorm;
    ay *= recipNorm;
    az *= recipNorm;

    // Rate of change of quaternion from gyroscope
    qDot1 = 0.5f * (-ahrs->q1 * gx - ahrs->q2 * gy - ahrs->q3 * gz);
    qDot2 = 0.5f * (ahrs->q0 * gx + ahrs->q2 * gz - ahrs->q3 * gy);
    qDot3 = 0.5f * (ahrs->q0 * gy - ahrs->q1 * gz + ahrs->q3 * gx);
    qDot4 = 0.5f * (ahrs->q0 * gz + ahrs->q1 * gy - ahrs->q2 * gx);

    // Normalize the accelerometer measurements
    _2q0 = 2.0f * ahrs->q0;
    _2q1 = 2.0f * ahrs->q1;
    _2q2 = 2.0f * ahrs->q2;
    _2q3 = 2.0f * ahrs->q3;

    // Use accelerometer to compute error
    float axTemp = ax - (2 * ahrs->q1 * ahrs->q3 - 2 * ahrs->q0 * ahrs->q2);
    float ayTemp = ay - (2 * ahrs->q0 * ahrs->q1 + 2 * ahrs->q2 * ahrs->q3);
    float azTemp = az - (1 - 2 * ahrs->q1 * ahrs->q1 - 2 * ahrs->q2 * ahrs->q2);

    // Calculate feedback and apply correction
    s0 = _2q0 * axTemp + _2q1 * ayTemp + _2q2 * azTemp;
    s1 = _2q0 * ayTemp - _2q1 * axTemp - _2q3 * azTemp;
    s2 = _2q0 * azTemp - _2q2 * axTemp - _2q3 * ayTemp;
    s3 = _2q1 * azTemp - _2q2 * ayTemp - _2q3 * axTemp;

    // Apply feedback
    qDot1 -= ahrs->beta * s0;
    qDot2 -= ahrs->beta * s1;
    qDot3 -= ahrs->beta * s2;
    qDot4 -= ahrs->beta * s3;

    // Integrate rate of change of quaternion
    ahrs->q0 += qDot1 * ahrs->invSampleFreq;
    ahrs->q1 += qDot2 * ahrs->invSampleFreq;
    ahrs->q2 += qDot3 * ahrs->invSampleFreq;
    ahrs->q3 += qDot4 * ahrs->invSampleFreq;

    // Normalize quaternion
    recipNorm = invSqrt(ahrs->q0 * ahrs->q0 + ahrs->q1 * ahrs->q1 + ahrs->q2 * ahrs->q2 + ahrs->q3 * ahrs->q3);
    ahrs->q0 *= recipNorm;
    ahrs->q1 *= recipNorm;
    ahrs->q2 *= recipNorm;
    ahrs->q3 *= recipNorm;

    ahrs->anglesComputed = 0;
}

float Madgwick_getRoll(MadgwickAHRS *ahrs) {
    if (!ahrs->anglesComputed) {
        // Compute angles
    }
    return ahrs->roll * 57.29578f;
}

float Madgwick_getPitch(MadgwickAHRS *ahrs) {
    if (!ahrs->anglesComputed) {
        // Compute angles
    }
    return ahrs->pitch * 57.29578f;
}

float Madgwick_getYaw(MadgwickAHRS *ahrs) {
    if (!ahrs->anglesComputed) {
        // Compute angles from quaternion
        ahrs->yaw = atan2(2.0f * (ahrs->q0 * ahrs->q3 + ahrs->q1 * ahrs->q2), 1.0f - 2.0f * (ahrs->q2 * ahrs->q2 + ahrs->q3 * ahrs->q3)) * 57.29578f + 180.0f;
        ahrs->anglesComputed = 1;  // Mark angles as computed
    }
    return ahrs->yaw;  // Convert from radians to degrees and adjust
}
