#include "UI/SLRStaticNoise.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RHI.h"
#include "Rendering/DrawElements.h"
#include "Rendering/LRMaterialHooks.h"
#include "TextureResource.h"
#include "UI/LRHudStyle.h"
#include "UObject/Package.h"

void SLRStaticNoise::Construct(const FArguments& InArgs)
{
	Intensity = InArgs._Intensity;
	Resolution = FIntPoint(FMath::Max(8, InArgs._Resolution.X), FMath::Max(8, InArgs._Resolution.Y));
	Pixels.SetNumZeroed(Resolution.X * Resolution.Y);
	NoiseState = FPlatformTime::Cycles() | 1u; // each screen gets its own static (xorshift needs a non-zero seed)
	SetClipping(EWidgetClipping::ClipToBounds); // the rolling band runs past the edges

	if (UTexture2D* NewTexture = UTexture2D::CreateTransient(Resolution.X, Resolution.Y, PF_B8G8R8A8))
	{
		NewTexture->SRGB = true;
		NewTexture->Filter = TF_Nearest;
		NewTexture->AddressX = TA_Wrap; // for tiling
		NewTexture->AddressY = TA_Wrap;
		NewTexture->UpdateResource();
		Texture.Reset(NewTexture);
	}

	UObject* Resource = Texture.Get();
	if (InArgs._Material && Texture.IsValid())
	{
		if (UMaterialInstanceDynamic* Instance = LRMaterialHooks::Make(InArgs._Material, GetTransientPackage(), FLinearColor::White, Texture.Get()))
		{
			Material.Reset(Instance);
			Resource = Instance;
		}
	}
	Brush.SetResourceObject(Resource);
	const float PixelSize = InArgs._PixelSize;
	const float Scale = PixelSize > 0.f ? PixelSize : 1.f;
	Brush.ImageSize = FVector2f(Resolution.X * Scale, Resolution.Y * Scale);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = PixelSize > 0.f ? ESlateBrushTileType::Both : ESlateBrushTileType::NoTile;
}

FVector2D SLRStaticNoise::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D::ZeroVector; // it fills whatever it's put in
}

uint32 SLRStaticNoise::NextRandom()
{
	NoiseState ^= NoiseState << 13;
	NoiseState ^= NoiseState >> 17;
	NoiseState ^= NoiseState << 5;
	return NoiseState;
}

void SLRStaticNoise::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SLeafWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Clock = InCurrentTime;
	if (Intensity.Get() <= 0.f || !Texture.IsValid() || InCurrentTime < NextFrameAt)
	{
		return;
	}
	NextFrameAt = InCurrentTime + 1.0 / FramesPerSecond;
	Regenerate();
}

void SLRStaticNoise::Regenerate()
{
	// Grey specks, each row a little brighter or darker than the next (the set losing its sync),
	// with a faint blue cast like a CRT.
	for (int32 Y = 0; Y < Resolution.Y; ++Y)
	{
		const uint32 RowGain = 160u + (NextRandom() % 96u); // 160..255, out of 256
		FColor* Row = Pixels.GetData() + Y * Resolution.X;
		for (int32 X = 0; X < Resolution.X; X += 4)
		{
			uint32 Bits = NextRandom();
			for (int32 Lane = 0; Lane < 4 && X + Lane < Resolution.X; ++Lane, Bits >>= 8)
			{
				const uint8 Value = static_cast<uint8>(((Bits & 0xFFu) * RowGain) >> 8);
				Row[X + Lane] = FColor(static_cast<uint8>(Value * 7 / 8), static_cast<uint8>(Value * 15 / 16), Value, 255);
			}
		}
	}
	Flicker = 0.85f + 0.15f * static_cast<float>(NextRandom() % 1000u) / 1000.f;

	// UpdateTextureRegions copies on the render thread later, so hand it its own buffer.
	const int32 Pitch = Resolution.X * static_cast<int32>(sizeof(FColor));
	const int32 Bytes = Pitch * Resolution.Y;
	uint8* Copy = new uint8[Bytes];
	FMemory::Memcpy(Copy, Pixels.GetData(), Bytes);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Resolution.X, Resolution.Y);
	Texture->UpdateTextureRegions(0, 1, Region, Pitch, sizeof(FColor), Copy,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}

int32 SLRStaticNoise::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const float Amount = FMath::Clamp(Intensity.Get(), 0.f, 1.f);
	if (Amount <= 0.f || !Brush.GetResourceObject())
	{
		return LayerId;
	}
	if (Material.IsValid())
	{
		Material->SetScalarParameterValue(LRMaterialHooks::AmountParam, Amount);
	}

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &Brush, ESlateDrawEffect::None,
		FLinearColor(1.f, 1.f, 1.f, Amount * Flicker));

	// A brighter band rolling slowly down the picture.
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const float BandHeight = Size.Y * 0.12f;
	const float Top = FMath::Frac(static_cast<float>(Clock * 0.25)) * (Size.Y + BandHeight) - BandHeight;
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
		AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, BandHeight), FSlateLayoutTransform(1.f, FVector2f(0.f, Top))),
		&FLRHudStyle::Get().WhiteBrush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.08f * Amount));
	return LayerId + 1;
}
