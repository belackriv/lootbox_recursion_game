#!/usr/bin/env python3
"""Ray-trace the black hole backdrop and bake it into a lookup table for the game.

Traces light rays around a Schwarzschild black hole (units: G = c = M = 1) from a camera
looking at it, and records for every pixel:

  * whether the ray fell into the hole (the shadow),
  * where it first crossed the accretion disk (radius + angle) and the Doppler/gravitational
    shift there, which the game uses to *animate* the disk (Keplerian rotation, beaming),
  * how close it skimmed the photon sphere (r = 3M), which lights up the photon ring.

The game (FLRBlackHoleRenderer) composites frames from this table at runtime, so the disk
rotates while the lensing stays physically correct. Run it after changing any constant:

    pip install numpy pillow
    python Tools/cosmos/generate_black_hole.py            # writes Content/Cosmos/BlackHole.lrbh
    python Tools/cosmos/generate_black_hole.py --preview  # also writes preview images to docs/images
    python Tools/cosmos/generate_black_hole.py --noise-only --preview  # new turbulence, no re-trace

File format (little-endian), shared with Source/LootboxRecursion/Cosmos/LRBlackHoleRenderer.cpp:
    char[4] "LRBH", uint32 version(1), uint32 width, uint32 height,
    uint32 noiseRadial, uint32 noiseAngular, float32 rInner, float32 rOuter
    width*height * { uint16 diskR, uint16 diskPhi, uint16 shift, uint8 shadow, uint8 ring }
    noiseRadial*noiseAngular * uint8 turbulence
diskR: 0 = no disk, else 1..65535 across [rInner, rOuter]; diskPhi: 0..65535 over 2pi;
shift: g * 16384 (g = redshift * Doppler factor); shadow/ring: coverage 0..255.
"""
import argparse
import math
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT_FILE = ROOT / "Content" / "Cosmos" / "BlackHole.lrbh"
PREVIEW_DIR = ROOT / "docs" / "images"

SIZE = 512            # output resolution (square)
SUPERSAMPLE = 2       # rays per pixel side, for smooth shadow / ring edges
R_OBSERVER = 40.0     # camera distance, in M
HALF_FOV_DEG = 26.0   # half field of view of the baked image
R_INNER = 4.0         # accretion disk inner edge (the ISCO is 6M; a bit inside looks better)
R_OUTER = 17.0        # disk outer edge
INCLINATION_DEG = 84  # angle between the line of sight and the disk normal (90 = edge-on)
DPHI = 0.0035         # integration step in orbital angle
NOISE_RADIAL, NOISE_ANGULAR = 64, 512


def trace(size):
    """Returns per-ray arrays: captured, disk_r, disk_phi, shift, ring (float32)."""
    n = size
    tan_fov = math.tan(math.radians(HALF_FOV_DEG))
    coords = (np.arange(n) + 0.5) / n * 2.0 - 1.0
    px, py = np.meshgrid(coords, -coords)          # image x right, y up
    px, py = px.ravel() * tan_fov, py.ravel() * tan_fov

    # Camera on +x looking at the hole (-x). Screen right = +y, up = +z.
    d = np.stack([-np.ones_like(px), px, py], axis=1)
    d /= np.linalg.norm(d, axis=1, keepdims=True)
    e1 = np.array([1.0, 0.0, 0.0])
    d_r = d @ e1                                   # radial component (negative = inward)
    t = d - d_r[:, None] * e1
    t_len = np.linalg.norm(t, axis=1)
    t_len = np.maximum(t_len, 1e-9)
    e2 = t / t_len[:, None]

    incl = math.radians(INCLINATION_DEG)
    normal = np.array([math.cos(incl), 0.0, math.sin(incl)])   # disk normal; n.x = cos(incl)
    a1 = e1 - (e1 @ normal) * normal
    a1 /= np.linalg.norm(a1)
    a2 = np.cross(normal, a1)

    count = d.shape[0]
    u = np.full(count, 1.0 / R_OBSERVER)
    du = -u * d_r / t_len                          # Binet: u = 1/r, du/dphi
    phi = np.zeros(count)
    active = np.ones(count, bool)
    captured = np.zeros(count, bool)
    disk_r = np.zeros(count)
    disk_phi = np.zeros(count)
    shift = np.zeros(count)
    min_r = np.full(count, R_OBSERVER)

    def pos(p, uu):
        r = 1.0 / np.maximum(uu, 1e-9)
        return (np.cos(p)[:, None] * e1 + np.sin(p)[:, None] * e2) * r[:, None]

    prev_side = pos(phi, u) @ normal
    h = DPHI
    for _ in range(int(4 * math.pi / h)):
        if not active.any():
            break
        # RK4 on u'' = -u + 3u^2 (Schwarzschild null geodesic, M = 1)
        def acc(uu):
            return -uu + 3.0 * uu * uu
        k1u, k1v = du, acc(u)
        k2u, k2v = du + 0.5 * h * k1v, acc(u + 0.5 * h * k1u)
        k3u, k3v = du + 0.5 * h * k2v, acc(u + 0.5 * h * k2u)
        k4u, k4v = du + h * k3v, acc(u + h * k3u)
        nu = u + h / 6.0 * (k1u + 2 * k2u + 2 * k3u + k4u)
        ndu = du + h / 6.0 * (k1v + 2 * k2v + 2 * k3v + k4v)
        nphi = phi + h

        u = np.where(active, nu, u)
        du = np.where(active, ndu, du)
        phi = np.where(active, nphi, phi)
        r = 1.0 / np.maximum(u, 1e-9)
        min_r = np.where(active, np.minimum(min_r, r), min_r)

        # Disk crossing (first one wins; later ones are higher-order images behind it)
        x = pos(phi, u)
        side = x @ normal
        crossed = active & (np.sign(side) != np.sign(prev_side)) & (disk_r == 0)
        prev_side = side
        if crossed.any():
            idx = np.nonzero(crossed)[0]
            rr = r[idx]
            ok = (rr >= R_INNER) & (rr <= R_OUTER)
            idx = idx[ok]
            if idx.size:
                xp = x[idx]
                rr = r[idx]
                disk_r[idx] = rr
                disk_phi[idx] = np.arctan2(xp @ a2, xp @ a1) % (2 * math.pi)
                # Photon direction (disk -> camera) is the reverse of our tracing direction.
                p_i, u_i, du_i = phi[idx], u[idx], du[idx]
                tangent = ((-np.sin(p_i))[:, None] * e1 + np.cos(p_i)[:, None] * e2[idx]) / u_i[:, None] \
                    - (np.cos(p_i)[:, None] * e1 + np.sin(p_i)[:, None] * e2[idx]) * (du_i / (u_i * u_i))[:, None]
                k = -tangent / np.linalg.norm(tangent, axis=1, keepdims=True)
                # Keplerian orbit, prograde around the normal; speed seen by a static observer
                vdir = np.cross(normal, xp / rr[:, None])
                vdir /= np.linalg.norm(vdir, axis=1, keepdims=True)
                beta = np.sqrt(1.0 / np.maximum(rr - 2.0, 1e-3))
                beta = np.minimum(beta, 0.99)
                gamma = 1.0 / np.sqrt(1 - beta * beta)
                cos_t = np.einsum("ij,ij->i", vdir, k)
                doppler = 1.0 / (gamma * (1.0 - beta * cos_t))
                shift[idx] = np.sqrt(1.0 - 2.0 / rr) * doppler
                active[idx] = False

        fell = active & (r < 2.0)
        captured |= fell
        active &= ~fell
        escaped = active & (du < 0) & (r > R_OBSERVER * 1.5)
        active &= ~escaped

    captured |= active & (r < 3.0)   # still orbiting after 2 turns: effectively captured
    ring = np.exp(-((min_r - 3.0) / 0.35) ** 2) * (~captured) * (disk_r == 0)
    return captured, disk_r, disk_phi, shift, ring


def bake():
    n = SIZE * SUPERSAMPLE
    print(f"Tracing {n * n:,} rays...", flush=True)
    captured, disk_r, disk_phi, shift, ring = trace(n)
    s = SUPERSAMPLE

    def down(a):
        return a.reshape(SIZE, s, SIZE, s).mean(axis=(1, 3))

    shadow = down(captured.astype(np.float32))
    ring_d = down(ring.astype(np.float32))
    # Disk values: take the supersample with a hit nearest the pixel centre (no averaging of angles).
    dr = disk_r.reshape(SIZE, s, SIZE, s)
    dp = disk_phi.reshape(SIZE, s, SIZE, s)
    ds = shift.reshape(SIZE, s, SIZE, s)
    hit = dr > 0
    coverage = hit.mean(axis=(1, 3))
    first = np.argmax(hit.reshape(SIZE, s, SIZE, s).transpose(0, 2, 1, 3).reshape(SIZE, SIZE, s * s), axis=2)
    def pick(a):
        flat = a.transpose(0, 2, 1, 3).reshape(SIZE, SIZE, s * s)
        return np.take_along_axis(flat, first[..., None], axis=2)[..., 0]
    disk_r_px, disk_phi_px, shift_px = pick(dr), pick(dp), pick(ds)
    has_disk = coverage > 0

    r_norm = np.where(has_disk, 1 + np.round((disk_r_px - R_INNER) / (R_OUTER - R_INNER) * 65534), 0).astype(np.uint16)
    phi_q = np.round(disk_phi_px / (2 * math.pi) * 65535).astype(np.uint16)
    shift_q = np.clip(np.round(shift_px * 16384), 0, 65535).astype(np.uint16)
    shadow_q = np.clip(np.round(shadow * 255), 0, 255).astype(np.uint8)
    ring_q = np.clip(np.round(ring_d * 255), 0, 255).astype(np.uint8)

    noise_q = make_noise()
    write_table(r_norm, phi_q, shift_q, shadow_q, ring_q, noise_q)
    return r_norm, phi_q, shift_q, shadow_q, ring_q, noise_q


# Turbulence octaves (bumps around the orbit, weight). Fine streaks with strong weights keep the
# swirl visible where the disk turns slowly: its outer edge takes (R_OUTER/R_INNER)^1.5 times
# longer than the inner edge.
NOISE_OCTAVES = ((8, 1.0), (16, 0.8), (32, 0.6), (64, 0.45))


def make_noise():
    """Turbulence: smooth random bands, stretched along the orbit."""
    rng = np.random.default_rng(1234)
    noise = np.zeros((NOISE_RADIAL, NOISE_ANGULAR))
    for octave, amp in NOISE_OCTAVES:
        coarse = rng.random((NOISE_RADIAL // 4 + 1, octave))
        rows = np.linspace(0, coarse.shape[0] - 1, NOISE_RADIAL)
        cols = np.linspace(0, octave, NOISE_ANGULAR, endpoint=False)
        r0 = np.floor(rows).astype(int); r1 = np.minimum(r0 + 1, coarse.shape[0] - 1); fr = (rows - r0)[:, None]
        c0 = np.floor(cols).astype(int) % octave; c1 = (c0 + 1) % octave; fc = (cols - np.floor(cols))[None, :]
        fr = fr * fr * (3 - 2 * fr); fc = fc * fc * (3 - 2 * fc)
        top = coarse[r0][:, c0] * (1 - fc) + coarse[r0][:, c1] * fc
        bot = coarse[r1][:, c0] * (1 - fc) + coarse[r1][:, c1] * fc
        noise += amp * (top * (1 - fr) + bot * fr)
    noise = (noise - noise.min()) / (noise.max() - noise.min())
    return np.round(noise * 255).astype(np.uint8)


def write_table(r_norm, phi_q, shift_q, shadow_q, ring_q, noise_q):
    OUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_FILE, "wb") as f:
        f.write(b"LRBH")
        f.write(struct.pack("<5I2f", 1, SIZE, SIZE, NOISE_RADIAL, NOISE_ANGULAR, R_INNER, R_OUTER))
        rec = np.zeros(SIZE * SIZE, dtype=[("r", "<u2"), ("phi", "<u2"), ("shift", "<u2"), ("shadow", "u1"), ("ring", "u1")])
        rec["r"] = r_norm.ravel(); rec["phi"] = phi_q.ravel(); rec["shift"] = shift_q.ravel()
        rec["shadow"] = shadow_q.ravel(); rec["ring"] = ring_q.ravel()
        f.write(rec.tobytes())
        f.write(noise_q.tobytes())
    print(f"Wrote {OUT_FILE} ({OUT_FILE.stat().st_size:,} bytes)")


# ---- Reference compositor (mirrors FLRBlackHoleRenderer::Render) --------------------------
SPIN_SECONDS = 3.0    # rotation period at the inner edge (FLRBlackHoleRenderer::SpinSeconds)

def composite(r_norm, phi_q, shift_q, shadow_q, ring_q, noise_q, time):
    has = r_norm > 0
    rn = (r_norm.astype(np.float32) - 1) / 65534.0
    r = R_INNER + rn * (R_OUTER - R_INNER)
    phi = phi_q.astype(np.float32) / 65535.0 * 2 * math.pi
    g = shift_q.astype(np.float32) / 16384.0
    omega = (R_INNER / r) ** 1.5 * (2 * math.pi / SPIN_SECONDS)
    psi = (phi - omega * time) % (2 * math.pi)
    ni = np.clip((rn * (NOISE_RADIAL - 1)).astype(int), 0, NOISE_RADIAL - 1)
    nj = (psi / (2 * math.pi) * NOISE_ANGULAR).astype(int) % NOISE_ANGULAR
    turb = noise_q[ni, nj].astype(np.float32) / 255.0
    pattern = 0.35 + 0.65 * turb ** 1.5
    profile = (R_INNER / r) ** 2.0 * np.clip(rn / 0.04, 0, 1) * np.clip((1 - rn) / 0.3, 0, 1)
    intensity = profile * pattern * g ** 3 * 2.6
    # temperature ramp: inner white-gold -> orange -> deep red; beaming shifts toward white
    t = np.clip(rn, 0, 1)[..., None]
    inner = np.array([1.0, 0.86, 0.55]); mid = np.array([1.0, 0.42, 0.06]); outer = np.array([0.55, 0.07, 0.02])
    col = np.where(t < 0.35, inner + (mid - inner) * (t / 0.35), mid + (outer - mid) * ((t - 0.35) / 0.65))
    col = col + (np.array([1.0, 0.95, 0.85]) - col) * np.clip(g - 1.0, 0, 1)[..., None] * 0.45
    disk_rgb = col * intensity[..., None] * has[..., None]
    pulse = 0.85 + 0.15 * math.sin(time * 2.1)
    ring = (ring_q.astype(np.float32) / 255.0)[..., None] * np.array([1.0, 0.8, 0.55]) * 0.9 * pulse
    rgb = disk_rgb + ring
    shadow = shadow_q.astype(np.float32) / 255.0
    lum = rgb.max(axis=2)
    alpha = np.clip(np.maximum(shadow, lum * 3.0), 0, 1)
    return rgb, alpha


def load():
    """Read the baked file back (for --preview-only)."""
    raw = OUT_FILE.read_bytes()
    assert raw[:4] == b"LRBH"
    _, w, h, nr, na = struct.unpack_from("<5I", raw, 4)
    offset = 4 + 5 * 4 + 2 * 4
    rec = np.frombuffer(raw, dtype=[("r", "<u2"), ("phi", "<u2"), ("shift", "<u2"), ("shadow", "u1"), ("ring", "u1")],
                        count=w * h, offset=offset).reshape(h, w)
    noise = np.frombuffer(raw, dtype=np.uint8, count=nr * na, offset=offset + w * h * 8).reshape(nr, na)
    return rec["r"], rec["phi"], rec["shift"], rec["shadow"], rec["ring"], noise


def preview(data):
    from PIL import Image
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(7)
    stars = np.zeros((SIZE, SIZE, 3))
    for _ in range(900):
        y, x = rng.integers(0, SIZE, 2)
        stars[y, x] = rng.uniform(0.3, 1.0)
    frames = []
    for i in range(48):
        rgb, alpha = composite(*data, time=i * SPIN_SECONDS / 48)
        img = stars * (1 - alpha[..., None]) + rgb
        img = 1 - np.exp(-img * 1.8)                     # simple tone map
        frames.append(Image.fromarray((np.clip(img, 0, 1) ** (1 / 1.8) * 255).astype(np.uint8)))
    frames[0].save(PREVIEW_DIR / "black_hole_preview.png")
    # Real time: the GIF shows the disk turning at the speed the game does.
    frame_ms = round(SPIN_SECONDS / 48 * 1000)
    frames[0].save(PREVIEW_DIR / "black_hole_preview.gif", save_all=True, append_images=frames[1:], duration=frame_ms, loop=0)
    print(f"Wrote previews to {PREVIEW_DIR}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--preview", action="store_true", help="also render preview images")
    parser.add_argument("--preview-only", action="store_true", help="re-render previews from the existing file")
    parser.add_argument("--noise-only", action="store_true", help="regenerate the turbulence in the existing file without re-tracing")
    args = parser.parse_args()
    if args.preview_only:
        preview(load())
    elif args.noise_only:
        r_norm, phi_q, shift_q, shadow_q, ring_q, _ = load()
        noise_q = make_noise()
        write_table(r_norm.reshape(SIZE, SIZE), phi_q.reshape(SIZE, SIZE), shift_q.reshape(SIZE, SIZE),
                    shadow_q.reshape(SIZE, SIZE), ring_q.reshape(SIZE, SIZE), noise_q)
        if args.preview:
            preview(load())
    else:
        baked = bake()
        if args.preview:
            preview(baked)
