#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
class UPrimitiveComponent;
class UTexture2D;

/**
 * Flat, unshaded colour from engine content, so the project needs no material assets yet.
 *
 * Uses the Widget3DPassThrough materials that UWidgetComponent renders 3D UI with: they are
 * unlit, and their colour is the "SlateUI" texture times the "TintColorAndOpacity" tint. With a
 * 1x1 white texture the tint is the colour, and a tint above 1 is HDR, so it glows through bloom.
 */
namespace LRUnlit
{
	/** For ConstructorHelpers::FObjectFinder (engine content, present in every install). */
	inline const TCHAR* const OpaqueMaterialPath = TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque");
	inline const TCHAR* const TranslucentMaterialPath = TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent");

	/**
	 * The one-sided translucent variant (UWidgetComponent uses it for one-sided widgets). Only the
	 * near side of a closed shape draws, so its opacity is exactly the tint's alpha instead of
	 * doubling where the back faces show through. A package and object path for LoadObject;
	 * check the package exists first and fall back to TranslucentMaterialPath.
	 */
	inline const TCHAR* const TranslucentOneSidedPackage = TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided");
	inline const TCHAR* const TranslucentOneSidedObject = TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided.Widget3DPassThrough_Translucent_OneSided");

	/** A transient 1x1 texture of one colour. */
	UTexture2D* MakeSolidTexture(const FColor& Color);

	/** A dynamic instance of Parent (one of the materials above) showing Texture * Tint. */
	UMaterialInstanceDynamic* MakeMaterial(UMaterialInterface* Parent, UObject* Outer, UTexture2D* Texture, const FLinearColor& Tint);

	void SetTint(UMaterialInstanceDynamic* Material, const FLinearColor& Tint);

	/**
	 * Keep a purely decorative primitive out of Lumen, distance fields and ray tracing. Glowing
	 * unlit surfaces would otherwise light the scene (noisily), and the huge sky shapes would
	 * look like a solid shell around it.
	 */
	void ExcludeFromLighting(UPrimitiveComponent* Component);
}
