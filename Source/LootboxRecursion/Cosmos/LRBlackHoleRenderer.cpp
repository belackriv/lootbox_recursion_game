#include "Cosmos/LRBlackHoleRenderer.h"

#include "Async/ParallelFor.h"
#include "Misc/FileHelper.h"

namespace
{
	constexpr uint32 FileVersion = 1;
	constexpr int32 HeaderBytes = 4 + 5 * 4 + 2 * 4;
	constexpr int32 TexelBytes = 8;

	uint32 ReadU32(const uint8* Data)
	{
		return static_cast<uint32>(Data[0]) | (static_cast<uint32>(Data[1]) << 8)
			| (static_cast<uint32>(Data[2]) << 16) | (static_cast<uint32>(Data[3]) << 24);
	}

	uint16 ReadU16(const uint8* Data)
	{
		return static_cast<uint16>(Data[0] | (Data[1] << 8));
	}

	float ReadF32(const uint8* Data)
	{
		const uint32 Bits = ReadU32(Data);
		float Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	struct FRgb
	{
		float R, G, B;
	};

	FRgb LerpRgb(const FRgb& A, const FRgb& B, float T)
	{
		return { A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T };
	}

	// Temperature ramp across the disk (inner white-gold -> orange -> deep red).
	const FRgb InnerColor{ 1.0f, 0.86f, 0.55f };
	const FRgb MidColor{ 1.0f, 0.42f, 0.06f };
	const FRgb OuterColor{ 0.55f, 0.07f, 0.02f };
	const FRgb BeamedColor{ 1.0f, 0.95f, 0.85f };
	const FRgb RingColor{ 1.0f, 0.8f, 0.55f };

	/** Linear 0..1 -> sRGB byte (the texture is sampled as sRGB, so encode to match). */
	uint8 ToSrgbByte(float Linear)
	{
		const float Clamped = FMath::Clamp(Linear, 0.f, 1.f);
		const float Encoded = Clamped <= 0.0031308f ? Clamped * 12.92f : 1.055f * FMath::Pow(Clamped, 1.f / 2.4f) - 0.055f;
		return static_cast<uint8>(FMath::Clamp(Encoded, 0.f, 1.f) * 255.f + 0.5f);
	}
}

bool FLRBlackHoleRenderer::LoadFromFile(const FString& Path, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutError = FString::Printf(TEXT("could not read %s"), *Path);
		return false;
	}
	return LoadFromBytes(Bytes, OutError);
}

bool FLRBlackHoleRenderer::LoadFromBytes(const TArray<uint8>& Bytes, FString& OutError)
{
	if (Bytes.Num() < HeaderBytes || FMemory::Memcmp(Bytes.GetData(), "LRBH", 4) != 0)
	{
		OutError = TEXT("not a black hole table (bad header)");
		return false;
	}
	const uint8* Data = Bytes.GetData();
	const uint32 Version = ReadU32(Data + 4);
	const int32 InWidth = static_cast<int32>(ReadU32(Data + 8));
	const int32 InHeight = static_cast<int32>(ReadU32(Data + 12));
	const int32 InNoiseRadial = static_cast<int32>(ReadU32(Data + 16));
	const int32 InNoiseAngular = static_cast<int32>(ReadU32(Data + 20));
	const float InRInner = ReadF32(Data + 24);
	const float InROuter = ReadF32(Data + 28);

	const int64 Expected = HeaderBytes + static_cast<int64>(InWidth) * InHeight * TexelBytes + static_cast<int64>(InNoiseRadial) * InNoiseAngular;
	if (Version != FileVersion || InWidth <= 0 || InHeight <= 0 || InNoiseRadial <= 0 || InNoiseAngular <= 0
		|| InROuter <= InRInner || Bytes.Num() != Expected)
	{
		OutError = FString::Printf(TEXT("unsupported or truncated black hole table (version %u, %dx%d, %d bytes)"),
			Version, InWidth, InHeight, Bytes.Num());
		return false;
	}

	Width = InWidth;
	Height = InHeight;
	NoiseRadial = InNoiseRadial;
	NoiseAngular = InNoiseAngular;
	RInner = InRInner;
	ROuter = InROuter;

	Texels.SetNum(Width * Height);
	const uint8* Cursor = Data + HeaderBytes;
	for (FTexel& Texel : Texels)
	{
		Texel.DiskR = ReadU16(Cursor);
		Texel.DiskPhi = ReadU16(Cursor + 2);
		Texel.Shift = ReadU16(Cursor + 4);
		Texel.Shadow = Cursor[6];
		Texel.Ring = Cursor[7];
		Cursor += TexelBytes;
	}
	Noise.SetNum(NoiseRadial * NoiseAngular);
	FMemory::Memcpy(Noise.GetData(), Cursor, Noise.Num());
	return true;
}

void FLRBlackHoleRenderer::Render(double TimeSeconds, TArray<FColor>& OutPixels) const
{
	OutPixels.SetNumUninitialized(Width * Height);
	FColor* Pixels = OutPixels.GetData();
	ParallelFor(Height, [this, TimeSeconds, Pixels](int32 Y)
	{
		RenderRow(Y, TimeSeconds, Pixels + Y * Width);
	});
}

void FLRBlackHoleRenderer::RenderRow(int32 Y, double TimeSeconds, FColor* OutRow) const
{
	const float TwoPi = 2.f * UE_PI;
	const float BaseOmega = TwoPi / FMath::Max(SpinSeconds, 0.1f);
	const float Pulse = 0.85f + 0.15f * FMath::Sin(static_cast<float>(TimeSeconds) * 2.1f);

	for (int32 X = 0; X < Width; ++X)
	{
		const FTexel& Texel = Texels[Y * Width + X];
		FRgb Color{ 0.f, 0.f, 0.f };

		if (Texel.DiskR > 0)
		{
			const float RNorm = (Texel.DiskR - 1) / 65534.f;
			const float Radius = RInner + RNorm * (ROuter - RInner);
			const float Phi = Texel.DiskPhi / 65535.f * TwoPi;
			const float Shift = Texel.Shift / 16384.f;

			// Keplerian differential rotation: omega ~ r^-1.5, so the pattern shears into spirals.
			const float Omega = FMath::Pow(RInner / Radius, 1.5f) * BaseOmega;
			float Psi = FMath::Fmod(Phi - Omega * static_cast<float>(FMath::Fmod(TimeSeconds, 100000.0)), TwoPi);
			if (Psi < 0.f)
			{
				Psi += TwoPi;
			}
			const int32 NoiseRow = FMath::Clamp(static_cast<int32>(RNorm * (NoiseRadial - 1)), 0, NoiseRadial - 1);
			const int32 NoiseCol = static_cast<int32>(Psi / TwoPi * NoiseAngular) % NoiseAngular;
			const float Turbulence = Noise[NoiseRow * NoiseAngular + NoiseCol] / 255.f;
			const float Pattern = 0.35f + 0.65f * FMath::Pow(Turbulence, 1.5f);

			const float Profile = FMath::Pow(RInner / Radius, 2.f) * FMath::Clamp(RNorm / 0.04f, 0.f, 1.f)
				* FMath::Clamp((1.f - RNorm) / 0.3f, 0.f, 1.f);
			// Relativistic beaming: the side orbiting toward us is much brighter.
			const float Intensity = Profile * Pattern * Shift * Shift * Shift * Brightness;

			const FRgb Ramp = RNorm < 0.35f ? LerpRgb(InnerColor, MidColor, RNorm / 0.35f) : LerpRgb(MidColor, OuterColor, (RNorm - 0.35f) / 0.65f);
			const FRgb Tinted = LerpRgb(Ramp, BeamedColor, FMath::Clamp(Shift - 1.f, 0.f, 1.f) * 0.45f);
			Color = { Tinted.R * Intensity, Tinted.G * Intensity, Tinted.B * Intensity };
		}

		const float Ring = Texel.Ring / 255.f * 0.9f * Pulse;
		Color.R += RingColor.R * Ring;
		Color.G += RingColor.G * Ring;
		Color.B += RingColor.B * Ring;

		const float Shadow = Texel.Shadow / 255.f;
		const float Luminance = FMath::Max3(Color.R, Color.G, Color.B);
		const float Alpha = FMath::Clamp(FMath::Max(Shadow, Luminance * 3.f), 0.f, 1.f);

		// Straight alpha: RGB * A ~= radiance (clamped to what 8 bits can hold). RGB is sRGB
		// encoded because the texture is sampled as sRGB; alpha stays linear.
		const float Scale = Alpha > UE_KINDA_SMALL_NUMBER ? 1.f / Alpha : 0.f;
		OutRow[X] = FColor(ToSrgbByte(Color.R * Scale), ToSrgbByte(Color.G * Scale), ToSrgbByte(Color.B * Scale),
			static_cast<uint8>(Alpha * 255.f + 0.5f));
	}
}
