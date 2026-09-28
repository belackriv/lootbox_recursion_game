#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
class UTexture;

/**
 * Optional project materials ("hooks"). Every visual the game draws has a material path; when
 * that asset exists it replaces the built-in look, otherwise the engine-only fallback (see
 * LRUnlit) is used, so the game runs before any art exists. docs/MATERIALS.md lists every hook.
 *
 * The code sets these parameters on a hook material. All are optional: a material that
 * doesn't have one simply ignores it.
 *   - Color   (vector)  linear HDR colour; values above 1 bloom
 *   - Opacity (scalar)  0 to 1, for translucent materials
 *   - Amount  (scalar)  0 to 1, how "full" the thing is (a ripple's amplitude, a cell's matter,
 *                       an irradiator's stacks), for materials that want to react to it
 *   - Texture (texture) where the thing has an image (the black hole's frames)
 */
namespace LRMaterialHooks
{
	inline const FName ColorParam(TEXT("Color"));
	inline const FName OpacityParam(TEXT("Opacity"));
	inline const FName AmountParam(TEXT("Amount"));
	inline const FName TextureParam(TEXT("Texture"));

	/** The material at Path if its package exists, else nullptr (without logging a load failure). */
	UMaterialInterface* LoadOptional(const FSoftObjectPath& Path);

	/** As above, from a string such as "/Game/Materials/M_Ripple" (the ".M_Ripple" object name is optional). */
	UMaterialInterface* LoadOptional(const FString& Path);

	/** A dynamic instance of Hook with Color = Tint.rgb, Opacity = Tint.a and Texture set, or nullptr if Hook is null. */
	UMaterialInstanceDynamic* Make(UMaterialInterface* Hook, UObject* Outer, const FLinearColor& Tint, UTexture* Texture = nullptr);

	/** Make(Hook) if there is a hook, else an LRUnlit instance of Fallback showing Texture * Tint. */
	UMaterialInstanceDynamic* MakeOr(UMaterialInterface* Hook, UMaterialInterface* Fallback, UObject* Outer, UTexture* Texture, const FLinearColor& Tint);

	/**
	 * Update the colour and opacity (Tint.a) of an instance made by any of the above, or of the
	 * engine's BasicShapeMaterial: sets Color, Opacity and LRUnlit's tint parameter.
	 */
	void SetColor(UMaterialInstanceDynamic* Material, const FLinearColor& Tint);

	void SetAmount(UMaterialInstanceDynamic* Material, float Amount);
}
