#include "Rendering/LRUnlit.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TextureResource.h"

namespace
{
	// Parameter names of the engine's Widget3DPassThrough materials (see UWidgetComponent).
	const FName TextureParam(TEXT("SlateUI"));
	const FName TintParam(TEXT("TintColorAndOpacity"));
	const FName OpacityFromTextureParam(TEXT("OpacityFromTexture"));
	const FName BackColorParam(TEXT("BackColor"));
}

UTexture2D* LRUnlit::MakeSolidTexture(const FColor& Color)
{
	UTexture2D* Texture = UTexture2D::CreateTransient(1, 1, PF_B8G8R8A8);
	if (!Texture)
	{
		return nullptr;
	}
	void* Data = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, &Color, sizeof(FColor));
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

UMaterialInstanceDynamic* LRUnlit::MakeMaterial(UMaterialInterface* Parent, UObject* Outer, UTexture2D* Texture, const FLinearColor& Tint)
{
	UMaterialInstanceDynamic* Material = Parent ? UMaterialInstanceDynamic::Create(Parent, Outer) : nullptr;
	if (Material)
	{
		Material->SetTextureParameterValue(TextureParam, Texture);
		Material->SetVectorParameterValue(TintParam, Tint);
		Material->SetVectorParameterValue(BackColorParam, FLinearColor::Black);
		Material->SetScalarParameterValue(OpacityFromTextureParam, 1.f);
	}
	return Material;
}

void LRUnlit::SetTint(UMaterialInstanceDynamic* Material, const FLinearColor& Tint)
{
	if (Material)
	{
		Material->SetVectorParameterValue(TintParam, Tint);
	}
}

void LRUnlit::ExcludeFromLighting(UPrimitiveComponent* Component)
{
	Component->bAffectDistanceFieldLighting = false;
	Component->bAffectDynamicIndirectLighting = false;
	Component->bVisibleInRayTracing = false;
	Component->SetCastShadow(false);
}
