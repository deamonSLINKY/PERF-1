# The M-VAVE FM-1 in Blender, built from the emulator's panel layout (src/Panel.jsx: 1000 x 640 panel units, 1 unit = 0.25 mm).
#   blender -b -P blender/fm1.py -- --screen out/blender/scr_theory.png --out out/blender/still.png [--keys teal|black] [--cam hero|top|close]
#     [--samples 128] [--res 1920x1080] [--save out/blender/fm1.blend] [--anim shot.json]
# --anim: a JSON from driver/blendshot.mjs (per frame: the screen PNG, held keys and buttons, knob angles), rendered as a PNG sequence to --out (a folder).
import bpy, bmesh, math, sys, json, os
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
A = {"keys": "teal", "cam": "hero", "samples": "128", "res": "1920x1080", "screen": "", "out": "", "save": "", "anim": "", "frames": ""}
for i in range(0, len(argv) - 1, 2): A[argv[i].lstrip("-")] = argv[i + 1]

S = 0.00025                                   # metres per panel unit
def P(x, y): return ((x - 500) * S, (320 - y) * S)
MM = 0.001
Z_TOP = 22 * MM                               # the case's top face

def srgb(h, a=1.0):
    h = h.lstrip("#"); c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple([(v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4) for v in c] + [a])

# ---------- scene ----------
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def mat(name, col, rough=0.5, spec=0.5, coat=0.0, emit=None, estr=0.0, sss=0.0):
    m = bpy.data.materials.new(name)
    try: m.use_nodes = True
    except Exception: pass
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = srgb(col)
    b.inputs["Roughness"].default_value = rough
    if "Specular IOR Level" in b.inputs: b.inputs["Specular IOR Level"].default_value = spec
    if coat: b.inputs["Coat Weight"].default_value = coat; b.inputs["Coat Roughness"].default_value = 0.08
    if emit: b.inputs["Emission Color"].default_value = srgb(emit); b.inputs["Emission Strength"].default_value = estr
    if sss: b.inputs["Subsurface Weight"].default_value = sss; b.inputs["Subsurface Radius"].default_value = (0.004, 0.006, 0.006); b.inputs["Subsurface Scale"].default_value = 1.0
    return m

def bump(m, scale=900.0, strength=0.08):       # a fine grain on plastic and silicone
    nt = m.node_tree; b = nt.nodes.get("Principled BSDF")
    tc = nt.nodes.new("ShaderNodeTexCoord"); nz = nt.nodes.new("ShaderNodeTexNoise"); bp = nt.nodes.new("ShaderNodeBump")
    nz.inputs["Scale"].default_value = scale; nz.inputs["Detail"].default_value = 2.0
    bp.inputs["Strength"].default_value = strength; bp.inputs["Distance"].default_value = 0.0002
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"]); nt.links.new(nz.outputs["Fac"], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m

TEAL = A["keys"] == "teal"
M_CASE = bump(mat("case", "#1c1d20", rough=0.55, spec=0.4), 1400, 0.05)
M_SIL = bump(mat("silicone", "#4fcfba" if TEAL else "#26282b", rough=0.62, spec=0.35, sss=0.06 if TEAL else 0.0), 600, 0.06)
M_SIL2 = bump(mat("silicone_mat", "#46bda9" if TEAL else "#1f2124", rough=0.7, spec=0.3, sss=0.04 if TEAL else 0.0), 600, 0.05)
M_KNOB = mat("knob", "#141517", rough=0.38, spec=0.5)
M_RING = mat("ring", "#0e0f10", rough=0.5)
M_BEZEL = mat("bezel", "#0b0b0c", rough=0.08, spec=0.6, coat=1.0)
M_PRINT = mat("print", "#e8e8ea", rough=0.6)
M_PRINT_K = mat("print_dark", "#17322e" if TEAL else "#cfd0d3", rough=0.6)
M_RED = mat("red", "#d8262e", rough=0.4)
M_FLOOR = mat("floor", "#0f1013", rough=0.35, spec=0.4)
M_PORT = mat("port", "#050505", rough=0.6)

def link(o):
    sc.collection.objects.link(o); return o

def rrect(name, x, y, w, h, r, z0, z1, m, bev=0.6 * MM, seg=4, n=12):
    """A rounded rectangle (panel units, top-left x, y) extruded from z0 to z1 (metres), its edges softened."""
    r = min(r, w / 2, h / 2); me = bpy.data.meshes.new(name); bm = bmesh.new(); vs = []
    cs = [(x + w - r, y + r, -90), (x + w - r, y + h - r, 0), (x + r, y + h - r, 90), (x + r, y + r, 180)]
    for cx, cy, a0 in cs:
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n); px, py = P(cx + r * math.cos(a), cy + r * math.sin(a))
            vs.append(bm.verts.new((px, py, z0)))
    f = bm.faces.new(vs)
    ex = bmesh.ops.extrude_face_region(bm, geom=[f])
    for v in ex["geom"]:
        if isinstance(v, bmesh.types.BMVert): v.co.z = z1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = True
    o = link(bpy.data.objects.new(name, me)); o.data.materials.append(m)
    if bev > 0:
        md = o.modifiers.new("bevel", "BEVEL"); md.width = bev; md.segments = seg; md.limit_method = "ANGLE"; md.angle_limit = math.radians(40)
        md.harden_normals = True
    return o

def text(s, x, y, size, z, m, align="CENTER", font=None, body_w=None):
    cu = bpy.data.curves.new("t_" + s, "FONT"); cu.body = s; cu.align_x = align; cu.align_y = "CENTER"; cu.size = size * S
    if font: cu.font = font
    o = link(bpy.data.objects.new("t_" + s, cu)); px, py = P(x, y); o.location = (px, py, z); o.data.materials.append(m)
    return o

FONT = None
for fp in (r"C:\Windows\Fonts\bahnschrift.ttf", r"C:\Windows\Fonts\ARIALN.TTF"):
    if os.path.exists(fp): FONT = bpy.data.fonts.load(fp); break

# ---------- the unit ----------
case = rrect("case", 4, 4, 992, 632, 44, 2 * MM, Z_TOP, M_CASE, bev=1.6 * MM, seg=5)
rrect("foot_band", 14, 14, 972, 612, 38, 0, 2.2 * MM, M_PORT, bev=0.3 * MM, seg=2)

# knobs (Panel.jsx KNOBS), knurled, with a white pointer
KNOBS = [("MASTER", 105, 100, 24), ("SELECT", 215, 100, 24), ("PRESETS", 105, 205, 24), ("ALGORITHM", 215, 205, 24)] + [(f"KNOB{i + 1}", 610 + i * 104, 100, 22) for i in range(4)]
KNOB_OBJ = {}
for nm, x, y, r in KNOBS:
    px, py = P(x, y)
    ring = bpy.data.meshes.new("ring"); bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=64, radius1=(r + 6) * S, radius2=(r + 5) * S, depth=0.8 * MM)
    bm.to_mesh(ring); bm.free(); ro = link(bpy.data.objects.new("ring_" + nm, ring)); ro.location = (px, py, Z_TOP + 0.4 * MM); ro.data.materials.append(M_RING)
    me = bpy.data.meshes.new("knob_" + nm); bm = bmesh.new(); N = 96; H = 11 * MM; R = r * S
    bot, top = [], []
    for i in range(N):
        a = 2 * math.pi * i / N; rr = R * (0.965 if (i // 2) % 2 else 1.0)
        bot.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), 0))); top.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), H)))
    for i in range(N):
        j = (i + 1) % N; bm.faces.new((bot[i], bot[j], top[j], top[i]))
    bm.faces.new(list(reversed(bot))); bm.faces.new(top)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = True
    ko = link(bpy.data.objects.new("knob_" + nm, me)); ko.location = (px, py, Z_TOP); ko.data.materials.append(M_KNOB)
    md = ko.modifiers.new("bevel", "BEVEL"); md.width = 0.9 * MM; md.segments = 4; md.limit_method = "ANGLE"; md.angle_limit = math.radians(50); md.harden_normals = True
    pm = bpy.data.meshes.new("ptr"); bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0); bm.to_mesh(pm); bm.free()
    po = link(bpy.data.objects.new("ptr_" + nm, pm)); po.parent = ko; po.scale = (0.7 * MM, R * 0.45, 0.2 * MM); po.location = (0, R * 0.55, H + 0.05 * MM); po.data.materials.append(M_PRINT)
    KNOB_OBJ[nm] = ko
    text(nm, x, y - r - 13, 14, Z_TOP + 0.02 * MM, M_PRINT, font=FONT)

# the screen: a glossy black bezel, the LCD as an emissive image (nearest-neighbour, so the pixels stay sharp)
rrect("bezel", 300, 44, 236, 236, 26, Z_TOP, Z_TOP + 0.9 * MM, M_BEZEL, bev=0.35 * MM, seg=3)
lcd_me = bpy.data.meshes.new("lcd"); bm = bmesh.new()
x0, y0 = P(318, 62); x1, y1 = P(518, 262); zl = Z_TOP + 0.92 * MM
vv = [bm.verts.new((x0, y1, zl)), bm.verts.new((x1, y1, zl)), bm.verts.new((x1, y0, zl)), bm.verts.new((x0, y0, zl))]
f = bm.faces.new(vv); uv = bm.loops.layers.uv.new("UVMap")
for lp, c in zip(f.loops, [(0, 0), (1, 0), (1, 1), (0, 1)]): lp[uv].uv = c
bm.to_mesh(lcd_me); bm.free()
LCD = link(bpy.data.objects.new("lcd", lcd_me))
M_LCD = bpy.data.materials.new("lcd")
try: M_LCD.use_nodes = True
except Exception: pass
nt = M_LCD.node_tree; nt.nodes.clear()
out = nt.nodes.new("ShaderNodeOutputMaterial"); em = nt.nodes.new("ShaderNodeEmission"); gl = nt.nodes.new("ShaderNodeBsdfGlossy"); add = nt.nodes.new("ShaderNodeAddShader")
LCD_TEX = nt.nodes.new("ShaderNodeTexImage"); LCD_TEX.interpolation = "Closest"
gl.inputs["Roughness"].default_value = 0.05; gl.inputs["Color"].default_value = (0.015, 0.015, 0.017, 1)
em.inputs["Strength"].default_value = 1.15
nt.links.new(LCD_TEX.outputs["Color"], em.inputs["Color"]); nt.links.new(em.outputs[0], add.inputs[0]); nt.links.new(gl.outputs[0], add.inputs[1]); nt.links.new(add.outputs[0], out.inputs[0])
LCD.data.materials.append(M_LCD)
if A["screen"]: LCD_TEX.image = bpy.data.images.load(os.path.abspath(A["screen"]))

rrect("red_tab", 548, 40, 34, 22, 3, Z_TOP, Z_TOP + 0.6 * MM, M_RED, bev=0.2 * MM, seg=2)
text("M-VAVE", 382, 312, 30, Z_TOP + 0.02 * MM, M_PRINT, font=FONT)

# OCT buttons, the function buttons and the keyboard: silicone on a silicone mat
rrect("oct_mat", 58, 258, 210, 58, 14, Z_TOP, Z_TOP + 1.2 * MM, M_SIL2, bev=0.6 * MM)
rrect("btn_mat", 568, 160, 400, 140, 18, Z_TOP, Z_TOP + 1.2 * MM, M_SIL2, bev=0.6 * MM)
rrect("key_mat", 36, 340, 928, 270, 18, Z_TOP, Z_TOP + 1.2 * MM, M_SIL2, bev=0.6 * MM)
ZM = Z_TOP + 1.2 * MM
BTN = {"FX": 0, "SEL": 1, "ENV": 2, "LFO": 3, "EDIT": 4, "GLO": 5, "HOME": 6, "SAVE": 7, "ARP": 8, "SEQ": 9, "PLAY": 10, "REC": 11, "OCT-": 12, "OCT+": 13}
BTN_LAY = {}
for i, n in enumerate(["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"]): BTN_LAY[n] = (582 + i * 63, 174, 54, 48)
for i, n in enumerate(["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]): BTN_LAY[n] = (582 + i * 63, 236, 54, 48)
BTN_LAY["OCT-"] = (72, 268, 82, 38); BTN_LAY["OCT+"] = (170, 268, 82, 38)
BTN_OBJ = {}
for n, (x, y, w, h) in BTN_LAY.items():
    o = rrect("btn_" + n, x, y, w, h, 9, ZM, ZM + 3.0 * MM, M_SIL, bev=1.1 * MM, seg=5)
    lab = "PLAY\nSTOP" if n == "PLAY" else n
    t = text(lab, x + w / 2, y + h / 2, 12 if n == "PLAY" else 14, 3.0 * MM + 0.03 * MM, M_PRINT_K, font=FONT)
    t.location = (t.location.x - o.location.x, t.location.y - o.location.y, ZM + 3.03 * MM); t.parent = o
    BTN_OBJ[BTN[n]] = o

WHITE_K = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26]
BLACK_K = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25]
BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]
pitch = (928 - 24) / 16; kw = pitch * 0.78
KEY_OBJ = {}
for i, k in enumerate(WHITE_K):
    KEY_OBJ[k] = rrect(f"key_{k}", 48 + i * pitch + (pitch - kw) / 2, 340 + 270 * 0.43, kw, 270 * 0.52, kw / 2, ZM, ZM + 3.6 * MM, M_SIL, bev=1.4 * MM, seg=6)
for i, k in enumerate(BLACK_K):
    KEY_OBJ[k] = rrect(f"key_{k}", 48 + (BLACK_AFTER[i] + 1) * pitch - kw / 2, 340 + 270 * 0.06, kw, 270 * 0.33, kw / 2, ZM, ZM + 3.6 * MM, M_SIL, bev=1.4 * MM, seg=6)

# the back edge: USB-C and two jacks
for nm, x, w, h in (("usb", 420, 36, 14), ("jack_out", 520, 26, 26), ("jack_in", 580, 26, 26)):
    px, _ = P(x, 0); pm = bpy.data.meshes.new(nm); bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0); bm.to_mesh(pm); bm.free()
    po = link(bpy.data.objects.new(nm, pm)); po.scale = (w * S, 1.2 * MM, h * S); po.location = (px, 4 * S + 0.3 * MM, 11 * MM); po.data.materials.append(M_PORT)

# ---------- the stage ----------
fl = bpy.data.meshes.new("floor"); bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=3.0); bm.to_mesh(fl); bm.free()
link(bpy.data.objects.new("floor", fl)).data.materials.append(M_FLOOR)

w = bpy.data.worlds.new("world"); sc.world = w
try: w.use_nodes = True
except Exception: pass
bg = w.node_tree.nodes.get("Background"); bg.inputs["Color"].default_value = srgb("#0d0e11"); bg.inputs["Strength"].default_value = 0.12

def area(name, loc, size, power, col, aim=(0, 0, 0)):
    ld = bpy.data.lights.new(name, "AREA"); ld.size = size; ld.energy = power; ld.color = srgb(col)[:3]
    o = link(bpy.data.objects.new(name, ld)); o.location = loc
    o.rotation_euler = (Vector(aim) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    o.visible_camera = False
    if name == "rim": o.visible_glossy = False         # warm edges on the case, but no orange glare on the screen
    return o
area("key", (-0.35, -0.2, 0.55), 0.6, 9, "#fff4ea")
area("rim", (0.10, 0.34, 0.22), 0.10, 0.9, "#ff8a2a", aim=(0.0, 0.06, 0.03))
area("fill", (0.5, -0.4, 0.25), 0.5, 1.5, "#9fdcff")

# ---------- camera ----------
tgt = link(bpy.data.objects.new("target", None))
cam_d = bpy.data.cameras.new("cam"); CAM = link(bpy.data.objects.new("cam", cam_d)); sc.camera = CAM
tc = CAM.constraints.new("TRACK_TO"); tc.target = tgt; tc.track_axis = "TRACK_NEGATIVE_Z"; tc.up_axis = "UP_Y"
cam_d.dof.use_dof = True; cam_d.dof.focus_object = tgt
sx, sy = P(418, 162)
SHOTS = {
    "hero": dict(loc=(-0.16, -0.34, 0.26), tgt=(sx + 0.01, sy - 0.035, Z_TOP), lens=50, f=5.6),
    "top": dict(loc=(0.0, -0.02, 0.62), tgt=(0.0, 0.0, Z_TOP), lens=50, f=11),
    "close": dict(loc=(sx - 0.05, sy - 0.16, 0.13), tgt=(sx, sy, Z_TOP + 1 * MM), lens=60, f=2.8),
}
def shot(k):
    s = SHOTS[k]; CAM.location = s["loc"]; tgt.location = s["tgt"]; cam_d.lens = s["lens"]; cam_d.dof.aperture_fstop = s["f"]
shot(A["cam"])

# ---------- render ----------
sc.render.engine = "CYCLES"
pr = bpy.context.preferences.addons["cycles"].preferences
for dt in ("OPTIX", "CUDA"):
    try:
        pr.compute_device_type = dt; pr.get_devices()
        devs = [d for d in pr.devices if d.type == dt]
        if devs:
            for d in pr.devices: d.use = d.type == dt
            sc.cycles.device = "GPU"; print("render device:", dt, [d.name for d in devs]); break
    except Exception as e: print("no", dt, e)
sc.cycles.samples = int(A["samples"]); sc.cycles.use_denoising = True
rw, rh = map(int, A["res"].split("x")); sc.render.resolution_x = rw; sc.render.resolution_y = rh; sc.render.resolution_percentage = 100
sc.view_settings.view_transform = "Standard"; sc.view_settings.look = "None"
sc.render.image_settings.file_format = "PNG"

if A["anim"]:
    J = json.load(open(A["anim"])); base = os.path.dirname(os.path.abspath(A["anim"]))
    sc.render.fps = J.get("fps", 30); n = len(J["frames"]); sc.frame_start = 1; sc.frame_end = n
    img = bpy.data.images.load(os.path.join(base, J["frames"][0]["png"])); img.source = "SEQUENCE"; LCD_TEX.image = img
    iu = LCD_TEX.image_user; iu.frame_duration = n; iu.frame_start = 1; iu.frame_offset = 0; iu.use_auto_refresh = True
    rest = {o: o.location.z for o in list(KEY_OBJ.values()) + list(BTN_OBJ.values())}
    prev_k = prev_b = -1
    for fi, fr in enumerate(J["frames"], start=1):
        if fr["keys"] != prev_k or fr["btn"] != prev_b or fi == 1:
            for k, o in KEY_OBJ.items():
                o.location.z = rest[o] - (1.4 * MM if fr["keys"] >> k & 1 else 0); o.keyframe_insert("location", index=2, frame=fi)
            for b, o in BTN_OBJ.items():
                o.location.z = rest[o] - (1.2 * MM if fr["btn"] >> b & 1 else 0); o.keyframe_insert("location", index=2, frame=fi)
            prev_k, prev_b = fr["keys"], fr["btn"]
        for nm, deg in fr.get("ang", {}).items():
            ko = KNOB_OBJ.get(nm)
            if ko: ko.rotation_euler.z = -math.radians(deg); ko.keyframe_insert("rotation_euler", index=2, frame=fi)
    for o in list(KEY_OBJ.values()) + list(BTN_OBJ.values()) + list(KNOB_OBJ.values()):
        ad = o.animation_data
        if ad and ad.action:
            try:
                for fc in ad.action.fcurves:
                    for kp in fc.keyframe_points: kp.interpolation = "CONSTANT"
            except Exception: pass
    cm = J.get("camera")
    if cm:
        for key in cm:                                     # [{"f": frame, "loc": [..], "tgt": [..], "lens": 50}]
            CAM.location = key["loc"]; tgt.location = key["tgt"]
            CAM.keyframe_insert("location", frame=key["f"]); tgt.keyframe_insert("location", frame=key["f"])
            if "lens" in key: cam_d.lens = key["lens"]; cam_d.keyframe_insert("lens", frame=key["f"])
    fly = J.get("flyin")
    if fly:                                                # a fly-in that ends straight down on the LCD: {"end": n, "lcd_px": 1054, "lcd_top": 25}
        from mathutils import Quaternion
        CAM.constraints.clear(); CAM.rotation_mode = "QUATERNION"
        endf = fly.get("end", n); lens0, lens1 = fly.get("lens", [38, 50])
        lc = Vector((*P(418, 162), Z_TOP + 0.92 * MM))     # the LCD's centre
        view_h = 50 * MM * rh / fly.get("lcd_px", rh)          # the view's height at the LCD: the LCD is lcd_px tall in the frame
        off_px = (fly.get("lcd_top", 0) + fly.get("lcd_px", rh) / 2) - rh / 2     # how far below the frame's centre its centre sits
        sens_h = 36.0 * rh / rw
        p2 = lc + Vector((0, off_px / fly.get("lcd_px", rh) * 50 * MM, view_h * lens1 / sens_h))
        # an orbit round the LCD: the tilt from straight down (th), the heading (ps) and the distance ease from the start to straight down,
        # so the screen stays centred all the way in; the camera's rotation is Euler (th, 0, ps): ps 0 = from the front, image-up = the panel's top
        p0 = Vector(fly.get("start", [-0.30, -0.44, 0.30])); v0 = lc - p0; d0 = v0.length; v0.normalize()
        th0 = math.acos(-v0.z); ps0 = math.atan2(-v0.x, v0.y)
        d1 = (p2 - lc).z; off = Vector((0, (p2 - lc).y, 0))
        sm = lambda x: 0.0 if x <= 0 else 1.0 if x >= 1 else x * x * x * (x * (6 * x - 15) + 10)
        CAM.rotation_mode = "XYZ"
        from mathutils import Euler
        for fi in range(1, n + 1):
            u = sm((fi - 1) / max(1, endf - 1))
            th = th0 * (1 - sm(u ** 0.8)); ps = ps0 * (1 - u); d = d0 * (d1 / d0) ** u
            rot = Euler((th, 0.0, ps), "XYZ"); view = rot.to_matrix() @ Vector((0, 0, -1))
            CAM.location = lc - d * view + off * u; CAM.rotation_euler = rot; cam_d.lens = lens0 + (lens1 - lens0) * u
            tgt.location = lc
            CAM.keyframe_insert("rotation_euler", frame=fi)
            CAM.keyframe_insert("location", frame=fi); cam_d.keyframe_insert("lens", frame=fi)
        cam_d.dof.aperture_fstop = fly.get("fstop", 4.0)
    if "lcd_emit" in J: em.inputs["Strength"].default_value = J["lcd_emit"]
    if A["frames"]: a, b = map(int, A["frames"].split("-")); sc.frame_start, sc.frame_end = a, b
    sc.render.filepath = os.path.abspath(A["out"]).rstrip("\\/") + os.sep + "f_"
    if A["save"]: bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(A["save"]))
    bpy.ops.render.render(animation=True)
else:
    if A["save"]: bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(A["save"]))
    if A["out"]:
        sc.render.filepath = os.path.abspath(A["out"]); bpy.ops.render.render(write_still=True); print("saved", A["out"])
