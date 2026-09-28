#include "Simulation/LRHexGrid.h"

namespace
{
	constexpr double Sqrt3 = 1.7320508075688772;
}

const FIntVector FLRHexGrid::Directions[6] = {
	FIntVector(1, 0, 0),
	FIntVector(0, 1, 0),
	FIntVector(-1, 1, 0),
	FIntVector(-1, 0, 0),
	FIntVector(0, -1, 0),
	FIntVector(1, -1, 0),
};

int32 FLRHexGrid::Distance(const FIntVector& A, const FIntVector& B)
{
	const int32 DQ = A.X - B.X;
	const int32 DR = A.Y - B.Y;
	return (FMath::Abs(DQ) + FMath::Abs(DR) + FMath::Abs(DQ + DR)) / 2 + FMath::Abs(A.Z - B.Z);
}

TArray<FIntVector> FLRHexGrid::Neighbors(const FIntVector& Cell)
{
	TArray<FIntVector> Out;
	Out.Reserve(6);
	for (const FIntVector& Direction : Directions)
	{
		Out.Add(Cell + Direction);
	}
	return Out;
}

TArray<FIntVector> FLRHexGrid::CellsInRadius(const FIntVector& Center, int32 Radius)
{
	TArray<FIntVector> Out;
	Out.Reserve(CountInRadius(Radius));
	if (Radius < 0)
	{
		return Out;
	}
	Out.Add(Center);
	// Ring K starts K steps out along Directions[4] and walks K steps along each direction.
	for (int32 Ring = 1; Ring <= Radius; ++Ring)
	{
		FIntVector Cell = Center + Directions[4] * Ring;
		for (int32 Side = 0; Side < 6; ++Side)
		{
			for (int32 Step = 0; Step < Ring; ++Step)
			{
				Out.Add(Cell);
				Cell += Directions[Side];
			}
		}
	}
	return Out;
}

FVector FLRHexGrid::ToLocal(const FIntVector& Cell, double Spacing)
{
	// Pointy-top axial layout: Q along X, R at 60 degrees.
	return FVector(
		Spacing * (Cell.X + 0.5 * Cell.Y),
		Spacing * (0.5 * Sqrt3) * Cell.Y,
		Spacing * Cell.Z);
}

FIntVector FLRHexGrid::FromLocal(const FVector& Point, double Spacing, int32 Layer)
{
	// Inverse of ToLocal in the plane, then round to the nearest hexagon.
	const double R = 2.0 * Point.Y / (Sqrt3 * Spacing);
	const double Q = Point.X / Spacing - 0.5 * R;
	const FIntPoint Cell = Round(Q, R);
	return FIntVector(Cell.X, Cell.Y, Layer);
}

FVector FLRHexGrid::Corner(int32 Index, double Spacing)
{
	// Corners sit at -30, 30, 90... degrees, so the edge between corners i and i+1 faces
	// Directions[i] (at i * 60 degrees).
	const int32 Wrapped = ((Index % 6) + 6) % 6;
	const double Angle = FMath::DegreesToRadians(60.0 * Wrapped - 30.0);
	const double Radius = CornerRadius(Spacing);
	return FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.0);
}

double FLRHexGrid::CornerRadius(double Spacing)
{
	return Spacing / Sqrt3;
}

FIntPoint FLRHexGrid::Round(double Q, double R)
{
	// Cube rounding: round all three cube coordinates (q + r + s = 0), then recompute the one
	// that moved furthest from the other two.
	const double S = -Q - R;
	double RoundedQ = FMath::RoundToDouble(Q);
	double RoundedR = FMath::RoundToDouble(R);
	const double RoundedS = FMath::RoundToDouble(S);
	const double ErrorQ = FMath::Abs(RoundedQ - Q);
	const double ErrorR = FMath::Abs(RoundedR - R);
	const double ErrorS = FMath::Abs(RoundedS - S);
	if (ErrorQ > ErrorR && ErrorQ > ErrorS)
	{
		RoundedQ = -RoundedR - RoundedS;
	}
	else if (ErrorR > ErrorS)
	{
		RoundedR = -RoundedQ - RoundedS;
	}
	return FIntPoint(static_cast<int32>(RoundedQ), static_cast<int32>(RoundedR));
}
