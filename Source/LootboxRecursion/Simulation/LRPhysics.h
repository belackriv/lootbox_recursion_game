#pragma once

#include "CoreMinimal.h"

/**
 * Real physical constants and black hole formulas, in SI units (kg, m, s, W, K). Plain C++,
 * shared by the host black hole rules (FLRHostDef) and the HUD's readouts. See
 * docs/DESIGN.md, "Feeding the host", for where the numbers come from.
 */
namespace LRPhysics
{
	/** Newton's gravitational constant, m^3 / (kg s^2). */
	inline constexpr double G = 6.674e-11;
	/** Speed of light, m/s. */
	inline constexpr double C = 2.998e8;
	/** Standard gravity (1 g), m/s^2. */
	inline constexpr double StandardGravity = 9.80665;

	/** Hawking lifetime of a black hole of mass M is this times M^3 (seconds, M in kg). */
	inline constexpr double HawkingLifetimePerKg3 = 8.41e-17;
	/** Hawking temperature is this divided by the mass (kelvin, M in kg). */
	inline constexpr double HawkingTemperatureKg = 1.227e23;
	/** Hawking power is this divided by the mass squared (watts, M in kg). */
	inline constexpr double HawkingPowerKg2 = 3.56e32;
	/**
	 * The real Eddington accretion limit as a fraction of the black hole's mass per second:
	 * L_Edd = 6.3 W per kg (electron scattering), accreted at 10% efficiency. Its inverse is the
	 * Salpeter time, about 45 million years.
	 */
	inline constexpr double EddingtonRatePerSecond = 7.03e-16;

	inline double SchwarzschildRadius(double Mass) { return 2.0 * G * Mass / (C * C); }
	inline double HawkingTemperature(double Mass) { return Mass > 0.0 ? HawkingTemperatureKg / Mass : 0.0; }
	inline double HawkingPower(double Mass) { return Mass > 0.0 ? HawkingPowerKg2 / (Mass * Mass) : 0.0; }
	/** The distance at which a mass pulls with the given acceleration: sqrt(G M / a). */
	inline double RadiusOfGravity(double Mass, double Acceleration)
	{
		return (Mass > 0.0 && Acceleration > 0.0) ? FMath::Sqrt(G * Mass / Acceleration) : 0.0;
	}
	/** The mass whose pull reaches the given acceleration at the given radius: a r^2 / G. */
	inline double MassForGravity(double Radius, double Acceleration) { return Acceleration * Radius * Radius / G; }
}
