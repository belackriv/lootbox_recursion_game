# Materials

The game runs on engine content alone. Every visual has a **material hook**: a path to a
project material that replaces the built-in look **if the asset exists**. If it doesn't exist,
the fallback is used and nothing is logged. So you can make materials one at a time, in any
order, and each one takes effect the next time you press Play.

The code is in `Rendering/LRMaterialHooks`. The paths are Config settings on the actors, and
you can change them in `Config/DefaultGame.ini` (the defaults are listed there, commented out).

## Parameters the code sets

Name them exactly like this. All are optional: a material ignores any it doesn't have.

| Parameter | Type | Meaning |
|---|---|---|
| `Color` | Vector | Linear HDR colour. Values above 1 bloom. |
| `Opacity` | Scalar | 0 to 1. Only matters for translucent materials. |
| `Amount` | Scalar | 0 to 1: how "full" the thing is (see each hook). |
| `Texture` | Texture | The thing's image, where it has one (the black hole). |

`Color` and `Opacity` come from the data where there is some: the item's `color` and
`opacity` in `items.json`.

## The hooks

All paths are under `/Game/Materials/` (the `Content/Materials` folder).

| Asset | Used for | Blend / shading | Must tick | Gets |
|---|---|---|---|---|
| `M_GridBeam` | Grid lines, hover and selection outlines | Opaque, Unlit | Used with Instanced Static Meshes | Color |
| `M_Entity` | Solid entities: caches, machines, an irradiator's contents | Opaque, lit or unlit | | Color, Amount |
| `M_SeeThrough` | Entities with `opacity` below 1: ripples (0.15), irradiators (0.25) | Translucent, Unlit | | Color, Opacity, Amount |
| `M_Matter` | The gas disc in a cell that holds matter | Any | | Color, Amount |
| `M_SkyBackdrop` | The void behind everything | Opaque, Unlit, **Two Sided** | | Color |
| `M_Star` | Stars | Opaque, Unlit | Used with Instanced Static Meshes | Color |
| `M_BlackHole` | The black hole image | Translucent, Unlit | | Texture, Color |
| `M_Plasma` | The primordial plasma veil (fades out at recombination) | Translucent, Unlit, **Two Sided** | | Color, Opacity |
| `M_Static` | TV static over the 3D view and the chamber camera while the instruments are down | Translucent, **User Interface** domain | | Texture, Amount, Color, Opacity |

What `Amount` means:
- **Ripple:** its amplitude, as a fraction of the most it can reach.
- **Irradiator:** its cache's stacks, as a fraction of the cap (1 once observed).
- **Matter disc:** how full the cell is (0 is a small puff, 1 is the largest disc).
- **Static:** how strong it is (it fades in and out as the instruments go down and come back).

The sky is seen from inside a sphere, which is why `M_SkyBackdrop` and `M_Plasma` must be Two
Sided.

**One item's own material.** An item in `items.json` can name a material, for example
`"material": "/Game/Materials/M_Ripple"`. If that asset exists, the item uses it instead of
`M_Entity` or `M_SeeThrough`, and gets the same parameters.

## Generating them

`Tools/unreal/make_materials.py` builds some of these materials from code. Two ways to run it:
- Close the editor and run `Tools\materials.bat`, or `Tools\materials.bat M_SeeThrough` for one.
- Inside the editor, use Tools > Execute Python Script and pick the file.

So far it has `M_SeeThrough` and `M_Matter`.

- **Rebuilds replace the graph.** Running it again rebuilds the graph in place, so the
  asset's references survive but hand edits to its graph are lost.
- **Tune with parameters.** Each recipe exposes its tuning numbers as parameters (`Glow`,
  `RimOpacity`, `Softness`...). Change those in the material, or in a Material Instance,
  rather than editing the graph.
- **Needs the Python plugin.** It uses the Python Editor Script Plugin, which
  `LootboxRecursion.uproject` enables.

## Recipes

These are starting points. Each one assumes a new Material asset opened in the material
editor; the settings named come from its Details panel.

**M_GridBeam** (already in the repo): Unlit. `Color` goes into Emissive Color.

**M_SeeThrough** (scripted), the glass look that keeps ripples readable over bright matter:
1. Set Blend Mode to Translucent and Shading Model to Unlit. Leave Two Sided off.
2. `Color` × `Glow` (2) goes into Emissive Color.
3. Add a Fresnel node, with its exponent from `RimExponent` (3). Lerp from `Opacity` (the
   item's, e.g. 0.15) to `RimOpacity` (0.9) by the Fresnel, and feed the result into Opacity.
4. Feed `Amount` × `AmountRim` (0.3) into the Fresnel's Base Reflect Fraction, so deeper
   ripples get a wider rim.

**M_Entity:**
1. Use Default Lit. `Color` goes into Base Color, with Roughness 0.4.
2. For a glow, add `Color` × `Amount` × 3 into Emissive Color.

**M_Matter** (scripted), a soft puff rather than a hard pancake:
1. Set Blend Mode to Translucent and Shading Model to Unlit.
2. `Color` × `Glow` (1.5) goes into Emissive Color.
3. Opacity fades out from the middle, measured in world space (the disc is a flattened
   sphere, so its UVs don't give a radial fade):
   - `d` = the horizontal Distance between Absolute World Position and Object Position,
     divided by Object Radius. It's 0 in the middle and 1 at the edge.
   - Opacity = saturate(1 − d^`Softness`) × lerp(`MinOpacity`, `MaxOpacity`, `Amount`).
4. The code sets `Opacity` to 1 on every hook, which is why this material uses its own
   `MinOpacity` and `MaxOpacity` parameters instead.
5. Optional: pan a noise texture across it for a slow swirl.

**M_Star:**
1. Unlit, with "Used with Instanced Static Meshes" ticked. `Color` goes into Emissive Color.
2. For twinkle, multiply by `1 + 0.3 × sin(Time × 3 + PerInstanceRandom × 6.28)`.

**M_BlackHole:**
1. Set Blend Mode to Translucent and Shading Model to Unlit.
2. Sample `Texture` (a Texture Sample Parameter 2D). Its RGB × `Color` goes into Emissive
   Color, and its alpha goes into Opacity.
3. Here you can add things the baked frames can't: heat shimmer, or a hot-spot bloom boost.

**M_Plasma:**
1. Set Blend Mode to Translucent, Shading Model to Unlit, and tick Two Sided.
2. `Color` goes into Emissive Color and `Opacity` into Opacity.
3. Optional: multiply Opacity by a slow, large-scale noise, so the plasma looks like churning
   fog rather than a flat wash.

**M_Static** (drawn by the Slate HUD, so it's a UI material). Its path is on `LRHud`, not an
actor in the level.
1. Set Material Domain to User Interface and Blend Mode to Translucent.
2. Sample `Texture` (a Texture Sample Parameter 2D, sampler Clamp): it's fresh grey noise 30
   times a second. Use its RGB for Final Color, or tint it.
3. Opacity = the Vertex Color's alpha: Slate passes the strength there, with a small flicker.
   `Amount` is the strength alone, for effects that should grow with it.
4. Room to add what the plain noise can't: scanlines, a vignette, chromatic fringes, a slow
   vertical roll.

**M_SkyBackdrop:**
1. Unlit, with Two Sided ticked. `Color` goes into Emissive Color.
2. It's a good place for a faint nebula texture later.

## What stays engine-only for now

- Entity labels, which use the engine's text material.
- The Slate HUD, which isn't drawn with materials, apart from the static (`M_Static`).
