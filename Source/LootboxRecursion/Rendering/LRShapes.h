#pragma once

#include "CoreMinimal.h"

class UObject;
class UStaticMesh;

/**
 * Shapes the engine's /Engine/BasicShapes don't have, built as static meshes at runtime (so the
 * project still needs no mesh assets). They follow the basic shapes' conventions: 100cm across,
 * pivot at the centre of the bounds, one material slot, and a box for simple collision so the
 * cursor can pick them.
 */
namespace LRShapes
{
	/**
	 * A tetrahedron (a d4) standing on one face, point up: the base is an equilateral triangle
	 * with a 50cm circumradius at Z = -50, the apex at Z = +50. Scale Z by 1/sqrt(2) relative to
	 * X and Y for a regular one.
	 */
	UStaticMesh* MakeTetrahedron(UObject* Outer);
}
