#include "Rendering/LRMaterialHooks.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Rendering/LRUnlit.h"

UMaterialInterface* LRMaterialHooks::LoadOptional(const FSoftObjectPath& Path)
{
	// Checked first so a hook nobody has made yet doesn't log a load failure every run.
	if (Path.IsNull() || !FPackageName::DoesPackageExist(Path.GetLongPackageName()))
	{
		return nullptr;
	}
	return Cast<UMaterialInterface>(Path.TryLoad());
}

UMaterialInterface* LRMaterialHooks::LoadOptional(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	FString ObjectPath = Path;
	if (!ObjectPath.Contains(TEXT(".")))
	{
		// "/Game/Materials/M_Ripple" -> "/Game/Materials/M_Ripple.M_Ripple"
		ObjectPath += TEXT(".") + FPackageName::GetShortName(ObjectPath);
	}
	return LoadOptional(FSoftObjectPath(ObjectPath));
}

UMaterialInstanceDynamic* LRMaterialHooks::Make(UMaterialInterface* Hook, UObject* Outer, const FLinearColor& Tint, UTexture* Texture)
{
	UMaterialInstanceDynamic* Material = Hook ? UMaterialInstanceDynamic::Create(Hook, Outer) : nullptr;
	if (Material)
	{
		SetColor(Material, Tint);
		if (Texture)
		{
			Material->SetTextureParameterValue(TextureParam, Texture);
		}
	}
	return Material;
}

UMaterialInstanceDynamic* LRMaterialHooks::MakeOr(UMaterialInterface* Hook, UMaterialInterface* Fallback, UObject* Outer, UTexture* Texture, const FLinearColor& Tint)
{
	if (Hook)
	{
		return Make(Hook, Outer, Tint, Texture);
	}
	return LRUnlit::MakeMaterial(Fallback, Outer, Cast<UTexture2D>(Texture), Tint);
}

void LRMaterialHooks::SetColor(UMaterialInstanceDynamic* Material, const FLinearColor& Tint)
{
	if (Material)
	{
		Material->SetVectorParameterValue(ColorParam, FLinearColor(Tint.R, Tint.G, Tint.B, 1.f));
		Material->SetScalarParameterValue(OpacityParam, Tint.A);
		LRUnlit::SetTint(Material, Tint);
	}
}

void LRMaterialHooks::SetAmount(UMaterialInstanceDynamic* Material, float Amount)
{
	if (Material)
	{
		Material->SetScalarParameterValue(AmountParam, FMath::Clamp(Amount, 0.f, 1.f));
	}
}
