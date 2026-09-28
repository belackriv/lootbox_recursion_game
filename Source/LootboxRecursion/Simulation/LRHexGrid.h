#pragma once

#include "CoreMinimal.h"

/**
 * Geometry of the pocket universe's grid: hexagonal cells, stacked in layers. Plain C++ with
 * no engine dependencies beyond math types, so the simulation (reach radius, drift) and the
 * world view share one definition and it can be tested.
 *
 * A cell is FIntVector(Q, R, Layer): axial hex coordinates plus the layer. Hexagons are
 * "pointy-top" in the grid's XY plane: +Q runs along +X, and +R runs at 60 degrees from it
 * (towards +Y). Spacing is the distance between neighbouring centres, which is also the
 * distance across a hexagon's flat sides. Layers are Spacing apart, like hexagonal prisms.
 *
 * Why hexagons: all six neighbours are equally far away, so a radius is a round ring and
 * flow has six clean directions, with no diagonals.
 *
 * Going truly 3D later (rhombic dodecahedra): every hexagon here is the middle slice of a
 * rhombic dodecahedron standing on a three-faced vertex, so the flat game stays the same.
 * Keep (Q, R, Layer) and change only the stacking:
 *   - Shift layer k in the plane by k * (1/3, 1/3) in axial units. That puts each cell over a
 *     dimple between three cells of the layer below, like stacked oranges, and the pattern
 *     repeats every three layers (up to a whole-cell shift).
 *   - Space layers Spacing * sqrt(2/3) apart instead of Spacing.
 *   - Each cell then has 12 neighbours, all Spacing away: its 6 in-layer neighbours, 3 above
 *     at (Q, R, k+1), (Q-1, R, k+1), (Q, R-1, k+1), and 3 below at (Q, R, k-1),
 *     (Q+1, R, k-1), (Q, R+1, k-1).
 *   - Distance, radius and picking become the 3D versions (face-centred cubic lattice).
 * Anything written against the functions below, rather than raw coordinates, carries over.
 */
struct LOOTBOXRECURSION_API FLRHexGrid
{
	/** The six in-layer neighbour offsets (Q, R, 0), counter-clockwise starting at +Q (+X). */
	static const FIntVector Directions[6];

	/** Steps between two cells: hex steps within the layer plus the layer difference. */
	static int32 Distance(const FIntVector& A, const FIntVector& B);

	/** The six neighbours in the same layer, in Directions order. */
	static TArray<FIntVector> Neighbors(const FIntVector& Cell);

	/** Every cell in Center's layer within Radius steps of it, Center included, ring by ring. */
	static TArray<FIntVector> CellsInRadius(const FIntVector& Center, int32 Radius);

	/** How many cells CellsInRadius returns: 1, 7, 19, 37... */
	static int32 CountInRadius(int32 Radius) { return Radius < 0 ? 0 : 1 + 3 * Radius * (Radius + 1); }

	/** Centre of a cell relative to the grid's origin (Z is the layer's floor height). */
	static FVector ToLocal(const FIntVector& Cell, double Spacing);

	/** The cell whose hexagon contains a point in the grid's plane (the point's Z is ignored). */
	static FIntVector FromLocal(const FVector& Point, double Spacing, int32 Layer);

	/** Corner Index (any integer, taken modulo 6) of a hexagon, relative to its centre. Corner i sits between the edges facing Directions[i - 1] and Directions[i]. */
	static FVector Corner(int32 Index, double Spacing);

	/** A hexagon's circumradius (centre to corner), which is also its edge length. */
	static double CornerRadius(double Spacing);

	/** The nearest cell to fractional axial coordinates. */
	static FIntPoint Round(double Q, double R);
};
