"""Build the game's hook materials (docs/MATERIALS.md) in the Unreal Editor.

Run it one of two ways:
  - Headless, with the editor closed: Tools\\materials.bat [names...]
  - Inside the editor: Tools > Execute Python Script..., then pick this file (it builds all).

With no names it builds every recipe below; otherwise only the named ones (e.g. M_SeeThrough).
An existing asset is rebuilt in place: its nodes are replaced, so anything referencing it keeps
working, but hand edits to its graph are lost. Tweak the look in a Material Instance, or through
the parameters (each recipe exposes its tuning numbers as parameters), rather than in the graph.

Needs the Python Editor Script Plugin (enabled in LootboxRecursion.uproject).
"""

import sys

import unreal

MEL = unreal.MaterialEditingLibrary
ASSET_DIR = "/Game/Materials"


# ---- Asset helpers ---------------------------------------------------------------------------

def _assets():
    """The editor's asset subsystem, or the older EditorAssetLibrary where that's what exists."""
    try:
        subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        if subsystem:
            return subsystem
    except Exception:
        pass
    return unreal.EditorAssetLibrary


def get_or_create_material(name):
    path = f"{ASSET_DIR}/{name}"
    assets = _assets()
    if assets.does_asset_exist(path):
        material = assets.load_asset(path)
        MEL.delete_all_material_expressions(material)
        unreal.log(f"make_materials: rebuilding {path}")
        return material
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(name, ASSET_DIR, unreal.Material, unreal.MaterialFactoryNew())
    unreal.log(f"make_materials: created {path}")
    return material


def save(material):
    MEL.recompile_material(material)
    _assets().save_loaded_asset(material, False)


# ---- Graph helpers ---------------------------------------------------------------------------

class Graph:
    """Thin wrapper over MaterialEditingLibrary so recipes read like the node graph."""

    def __init__(self, material):
        self.material = material

    def node(self, cls, x, y, **props):
        expression = MEL.create_material_expression(self.material, cls, x, y)
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def vector(self, name, default, x, y, group="Look"):
        return self.node(unreal.MaterialExpressionVectorParameter, x, y,
                         parameter_name=name, default_value=unreal.LinearColor(*default), group=group)

    def scalar(self, name, default, x, y, group="Look"):
        return self.node(unreal.MaterialExpressionScalarParameter, x, y,
                         parameter_name=name, default_value=default, group=group)

    def link(self, source, target, pin="", source_pin=""):
        """
        Connect source's output (default: its first) to target's input pin ("" = its first).
        pin can be a tuple of names to try, for pins whose name differs between versions.
        """
        for name in (pin,) if isinstance(pin, str) else pin:
            if MEL.connect_material_expressions(source, source_pin, target, name):
                return
        unreal.log_warning(f"make_materials: couldn't connect {source.get_name()} -> {target.get_name()}.{pin}")

    def output(self, source, prop, source_pin=""):
        if not MEL.connect_material_property(source, source_pin, prop):
            unreal.log_warning(f"make_materials: couldn't connect {source.get_name()} -> {prop}")


def _translucent_unlit(material, two_sided=False):
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", two_sided)


# ---- Recipes ---------------------------------------------------------------------------------

def build_see_through(material):
    """
    M_SeeThrough: ripples and irradiators (items with opacity below 1).

    Glass with a rim: the middle shows the item's opacity (0.15 for ripples), and the edges turn
    towards RimOpacity, so the outline stays readable over bright matter. Amount (a ripple's
    amplitude, an irradiator's stacks) widens the rim.
      Emissive = Color * Glow
      Opacity  = lerp(Opacity, RimOpacity, Fresnel(RimExponent, base = Amount * AmountRim))
    """
    _translucent_unlit(material)
    g = Graph(material)

    color = g.vector("Color", (0.7, 0.55, 1.0, 1.0), -700, -200)
    glow = g.scalar("Glow", 2.0, -700, -60)
    emissive = g.node(unreal.MaterialExpressionMultiply, -400, -150)
    g.link(color, emissive, "A")
    g.link(glow, emissive, "B")
    g.output(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    opacity = g.scalar("Opacity", 0.15, -700, 80)
    rim_opacity = g.scalar("RimOpacity", 0.9, -700, 180)
    rim_exponent = g.scalar("RimExponent", 3.0, -1000, 300, group="Rim")
    amount = g.scalar("Amount", 0.0, -1000, 420, group="Rim")
    amount_rim = g.scalar("AmountRim", 0.3, -1000, 520, group="Rim")

    rim_base = g.node(unreal.MaterialExpressionMultiply, -700, 460)
    g.link(amount, rim_base, "A")
    g.link(amount_rim, rim_base, "B")

    fresnel = g.node(unreal.MaterialExpressionFresnel, -400, 320)
    g.link(rim_exponent, fresnel, ("ExponentIn", "Exponent"))
    g.link(rim_base, fresnel, ("BaseReflectFractionIn", "BaseReflectFraction"))

    blend = g.node(unreal.MaterialExpressionLinearInterpolate, -150, 120)
    g.link(opacity, blend, "A")
    g.link(rim_opacity, blend, "B")
    g.link(fresnel, blend, "Alpha")
    g.output(blend, unreal.MaterialProperty.MP_OPACITY)


def build_matter(material):
    """
    M_Matter: the gas disc in a cell holding matter (a flattened engine sphere).

    A soft puff instead of a hard pancake: opaque in the middle, fading to nothing at the edge.
    The fade is measured in world space from the disc's centre, so it doesn't depend on the
    sphere's UVs. Fuller cells (Amount) are denser. The code sets Opacity to 1 on every hook,
    so this material uses its own MinOpacity / MaxOpacity instead.
      Emissive = Color * Glow
      d        = horizontal distance from the centre / ObjectRadius   (0 middle, 1 edge)
      Opacity  = saturate(1 - d^Softness) * lerp(MinOpacity, MaxOpacity, Amount)
    """
    _translucent_unlit(material)
    g = Graph(material)

    color = g.vector("Color", (0.8, 0.85, 1.0, 1.0), -700, -250)
    glow = g.scalar("Glow", 1.5, -700, -110)
    emissive = g.node(unreal.MaterialExpressionMultiply, -400, -200)
    g.link(color, emissive, "A")
    g.link(glow, emissive, "B")
    g.output(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    world = g.node(unreal.MaterialExpressionWorldPosition, -1500, 50)
    centre = g.node(unreal.MaterialExpressionObjectPositionWS, -1500, 170)
    world_xy = g.node(unreal.MaterialExpressionComponentMask, -1300, 50, r=True, g=True, b=False, a=False)
    centre_xy = g.node(unreal.MaterialExpressionComponentMask, -1300, 170, r=True, g=True, b=False, a=False)
    g.link(world, world_xy)
    g.link(centre, centre_xy)

    distance = g.node(unreal.MaterialExpressionDistance, -1100, 100)
    g.link(world_xy, distance, "A")
    g.link(centre_xy, distance, "B")

    radius = g.node(unreal.MaterialExpressionObjectRadius, -1100, 240)
    normalized = g.node(unreal.MaterialExpressionDivide, -900, 140)
    g.link(distance, normalized, "A")
    g.link(radius, normalized, "B")

    softness = g.scalar("Softness", 1.5, -900, 300, group="Shape")
    curved = g.node(unreal.MaterialExpressionPower, -700, 160)
    g.link(normalized, curved, "Base")
    g.link(softness, curved, ("Exp", "Exponent"))
    inverted = g.node(unreal.MaterialExpressionOneMinus, -550, 160)
    g.link(curved, inverted)
    falloff = g.node(unreal.MaterialExpressionSaturate, -420, 160)
    g.link(inverted, falloff)

    min_opacity = g.scalar("MinOpacity", 0.35, -700, 360, group="Shape")
    max_opacity = g.scalar("MaxOpacity", 0.85, -700, 460, group="Shape")
    amount = g.scalar("Amount", 0.5, -700, 560, group="Shape")
    density = g.node(unreal.MaterialExpressionLinearInterpolate, -420, 420)
    g.link(min_opacity, density, "A")
    g.link(max_opacity, density, "B")
    g.link(amount, density, "Alpha")

    opacity = g.node(unreal.MaterialExpressionMultiply, -200, 250)
    g.link(falloff, opacity, "A")
    g.link(density, opacity, "B")
    g.output(opacity, unreal.MaterialProperty.MP_OPACITY)


RECIPES = {
    "M_SeeThrough": build_see_through,
    "M_Matter": build_matter,
}


def main(names):
    unknown = [name for name in names if name not in RECIPES]
    if unknown:
        unreal.log_error(f"make_materials: unknown {unknown}; known: {sorted(RECIPES)}")
        return
    for name in names or sorted(RECIPES):
        material = get_or_create_material(name)
        RECIPES[name](material)
        save(material)
        unreal.log(f"make_materials: saved {ASSET_DIR}/{name}")


if __name__ == "__main__":
    main(sys.argv[1:])
