#pragma once

#include "CoreMinimal.h"

/**
 * Composites animated frames of the black hole backdrop from a baked lookup table
 * (Content/Cosmos/BlackHole.lrbh, made by Tools/cosmos/generate_black_hole.py).
 *
 * The expensive part - ray tracing light around the hole - happened offline. The table says,
 * for every pixel, whether the ray fell in (shadow), where it crossed the accretion disk and
 * the Doppler/gravitational shift there, and how close it passed the photon sphere. Each frame
 * we rotate the disk's turbulence pattern at Keplerian speed (inner orbits faster), so the
 * disk swirls while the lensing stays physically correct.
 *
 * Plain C++ (no UObjects) so it can be unit tested. Mirrors composite() in the Python tool.
 */
class LOOTBOXRECURSION_API FLRBlackHoleRenderer
{
public:
	bool LoadFromFile(const FString& Path, FString& OutError);
	bool LoadFromBytes(const TArray<uint8>& Bytes, FString& OutError);

	bool IsLoaded() const { return Width > 0; }
	int32 GetWidth() const { return Width; }
	int32 GetHeight() const { return Height; }

	/**
	 * Render one frame into OutPixels (Width * Height, row-major, straight alpha).
	 * RGB is the colour normalised so that RGB * A approximates the HDR radiance, sRGB-encoded
	 * (alpha stays linear); the shadow is opaque black and empty space is fully transparent.
	 */
	void Render(double TimeSeconds, TArray<FColor>& OutPixels) const;

	/** Rotation period of the disk's inner edge, in seconds. */
	float SpinSeconds = 8.f;
	/** Overall disk brightness before tone mapping. */
	float Brightness = 2.6f;

private:
	struct FTexel
	{
		uint16 DiskR = 0;   // 0 = no disk, else 1..65535 across [RInner, ROuter]
		uint16 DiskPhi = 0; // 0..65535 over 2 pi
		uint16 Shift = 0;   // g * 16384
		uint8 Shadow = 0;   // coverage 0..255
		uint8 Ring = 0;     // photon ring glow 0..255
	};

	void RenderRow(int32 Y, double TimeSeconds, FColor* OutRow) const;

	TArray<FTexel> Texels;
	TArray<uint8> Noise;
	int32 Width = 0;
	int32 Height = 0;
	int32 NoiseRadial = 0;
	int32 NoiseAngular = 0;
	float RInner = 0.f;
	float ROuter = 1.f;
};
