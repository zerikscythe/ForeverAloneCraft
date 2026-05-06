#!/usr/bin/env python3
"""
LivingWorld CP — Addon UI Editor
Parses LivingWorld.xml (and inlines dynamic Lua-created gear slots) then
renders the full control-panel layout on a zoomable tkinter canvas.
Click any frame to select it and edit its properties; Save writes changes
back to LivingWorld.xml.
"""

import os
import re
import sys
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import xml.etree.ElementTree as ET

# ── Paths ──────────────────────────────────────────────────────────────────────
ADDON_DIR = os.path.join(os.path.dirname(__file__))
XML_FILE  = os.path.join(ADDON_DIR, "LivingWorld.xml")
LUA_FILE  = os.path.join(ADDON_DIR, "LivingWorld.lua")

# ── Layout constants ───────────────────────────────────────────────────────────
SCALE    = 1.5          # WoW pixels → canvas pixels
CANVAS_W = 530
CANVAS_H = 760

PANEL_W  = 290          # right-side properties panel width
CODE_W   = 340

# UIParent: the simulated WoW screen in canvas space
UIPARENT = (0.0, 0.0, float(CANVAS_W), float(CANVAS_H))

# ── Anchor math ────────────────────────────────────────────────────────────────
_AP = {
    "TOPLEFT":    (0.0, 0.0), "TOP":    (0.5, 0.0), "TOPRIGHT":    (1.0, 0.0),
    "LEFT":       (0.0, 0.5), "CENTER": (0.5, 0.5), "RIGHT":       (1.0, 0.5),
    "BOTTOMLEFT": (0.0, 1.0), "BOTTOM": (0.5, 1.0), "BOTTOMRIGHT": (1.0, 1.0),
}

def _pt(l, t, w, h, pt):
    """Canvas position of anchor point on a frame rect."""
    fx, fy = _AP.get(pt, (0.5, 0.5))
    return l + w * fx, t + h * fy

def _origin(ax, ay, pt, w, h):
    """Frame top-left given its anchor point's canvas position."""
    fx, fy = _AP.get(pt, (0.5, 0.5))
    return ax - w * fx, ay - h * fy

# ── Colour scheme ──────────────────────────────────────────────────────────────
_FILL   = {"Frame":"#162032","Button":"#0f2d1a","EditBox":"#1e0f32","FontString":"#2a1800"}
_BORDER = {"Frame":"#4a9fd4","Button":"#5cd484","EditBox":"#a070e0","FontString":"#e08040"}
_PREVIEW_FILL = {"Frame":"#0f0f12","Button":"#545a63","EditBox":"#07090d","FontString":""}
_PREVIEW_BORDER = {"Frame":"#8d7448","Button":"#b9b1a1","EditBox":"#8d7448","FontString":""}
_PREVIEW_TEXT = {"Frame":"#c7b58a","Button":"#f3f0e6","EditBox":"#e5dcc5","FontString":"#ffd36b"}
_HLSEL  = "#f59e0b"   # selected frame border
_HLHOV  = "#60a5fa"   # hover border
_HANDLE_FILL = "#f59e0b"
HANDLE_SIZE = 8

# ── Tab → page mapping ─────────────────────────────────────────────────────────
DEFAULT_TABS = ["Bots", "Combat", "Gear", "Bags", "Settings"]
ANCHOR_POINTS = [
    "TOPLEFT", "TOP", "TOPRIGHT",
    "LEFT", "CENTER", "RIGHT",
    "BOTTOMLEFT", "BOTTOM", "BOTTOMRIGHT",
]
SCRIPT_BLOCK_RE = re.compile(r"^\[(?P<name>[^\]]+)\]\s*$", re.MULTILINE)
LUA_FUNC_START_RE = re.compile(r"^\s*(local\s+)?function\s+([A-Za-z_][A-Za-z0-9_:\.]*)\s*\(")
GEAR_LAYOUT_MARKER_START = "-- BEGIN LWCP_GEAR_SLOT_LAYOUT"
GEAR_LAYOUT_MARKER_END = "-- END LWCP_GEAR_SLOT_LAYOUT"
BAGS_TAB_LAYOUT_MARKER_START = "-- BEGIN LWCP_BAGS_TAB_LAYOUT"
BAGS_TAB_LAYOUT_MARKER_END = "-- END LWCP_BAGS_TAB_LAYOUT"
GEAR_SLOT_LAYOUT_ENTRY_RE = re.compile(
    r"^\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*\{\s*"
    r"x\s*=\s*(?P<x>-?\d+(?:\.\d+)?)\s*,\s*"
    r"y\s*=\s*(?P<y>-?\d+(?:\.\d+)?)\s*,\s*"
    r"w\s*=\s*(?P<w>-?\d+(?:\.\d+)?)\s*,\s*"
    r"h\s*=\s*(?P<h>-?\d+(?:\.\d+)?)\s*\}\s*,?\s*$",
    re.MULTILINE,
)

# ── Gear slot layout (from LWCP_InitGearPage in LivingWorld.lua) ───────────────
# SetPoint("TOP", parent, "TOP", -90 + col*32, yBase - row*33)  size 30×30
# LWCPGearSlot1-19  → yBase=-66, grid 7 cols
# LWCPBagEquipSlot1-4 → fixed row at y=-180
# LWCPBagSlot1-14   → yBase=-228, grid 7 cols, size 30×28
GEAR_SLOT_NAMES = [
    "Head","Neck","Shoulders","Back","Chest","Shirt","Tabard",
    "Wrist","Hands","Waist","Legs","Feet","Finger1","Finger2",
    "Trinket1","Trinket2","MainHand","OffHand","Ranged",
]

def _default_gear_slot_layouts() -> dict[str, dict[str, float]]:
    layouts: dict[str, dict[str, float]] = {}
    for i in range(1, 20):
        col = (i - 1) % 7
        row = (i - 1) // 7
        layouts[f"LWCPGearSlot{i}"] = {
            "x": -90.0 + col * 32.0,
            "y": -66.0 - row * 33.0,
            "w": 30.0,
            "h": 30.0,
        }
    return layouts

DEFAULT_GEAR_SLOT_LAYOUTS = _default_gear_slot_layouts()
GEAR_LAYOUT_ORDER = tuple(DEFAULT_GEAR_SLOT_LAYOUTS.keys())

def _default_bags_tab_layouts() -> dict[str, dict[str, float]]:
    layouts: dict[str, dict[str, float]] = {
        "LWCPBagContainerBtn0": {"x": -88.0, "y": -54.0, "w": 30.0, "h": 30.0},
        "LWCPBagContainerBtn1": {"x": -52.0, "y": -54.0, "w": 30.0, "h": 30.0},
        "LWCPBagContainerBtn2": {"x": -16.0, "y": -54.0, "w": 30.0, "h": 30.0},
        "LWCPBagContainerBtn3": {"x": 20.0, "y": -54.0, "w": 30.0, "h": 30.0},
        "LWCPBagContainerBtn4": {"x": 56.0, "y": -54.0, "w": 30.0, "h": 30.0},
        "LWCPBagGearBtn": {"x": 102.0, "y": -50.0, "w": 46.0, "h": 22.0},
    }
    for i in range(1, 33):
        col = (i - 1) % 8
        row = (i - 1) // 8
        layouts[f"LWCPBagItemSlot{i}"] = {
            "x": -105.0 + col * 30.0,
            "y": -112.0 - row * 30.0,
            "w": 28.0,
            "h": 28.0,
        }
    return layouts

DEFAULT_BAGS_TAB_LAYOUTS = _default_bags_tab_layouts()
BAGS_TAB_LAYOUT_ORDER = tuple(DEFAULT_BAGS_TAB_LAYOUTS.keys())

# ── Data model ─────────────────────────────────────────────────────────────────
class FNode:
    _ctr = 0
    __slots__ = ("uid","name","ftype","hidden","w","h","text",
                 "a_pt","a_relto","a_relpt","a_ox","a_oy",
                 "children","cx_l","cx_t","on_layer","_scripts","_synthetic","_xml_path")

    def __init__(self):
        FNode._ctr += 1
        self.uid       = FNode._ctr
        self.name      = ""
        self.ftype     = "Frame"
        self.hidden    = False
        self.w         = 100.0
        self.h         = 30.0
        self.text      = ""
        self.a_pt      = "CENTER"
        self.a_relto   = ""
        self.a_relpt   = ""
        self.a_ox      = 0.0
        self.a_oy      = 0.0
        self.children: list["FNode"] = []
        self.cx_l      = 0.0
        self.cx_t      = 0.0
        self.on_layer  = ""
        self._scripts  = {}   # event → code snippet
        self._synthetic = False  # True for Lua-created nodes
        self._xml_path = None

    @property
    def eff_relpt(self):
        return self.a_relpt or self.a_pt

    @property
    def cx_r(self): return self.cx_l + self.w * SCALE
    @property
    def cx_b(self): return self.cx_t + self.h * SCALE

    def all_descendants(self):
        yield self
        for c in self.children:
            yield from c.all_descendants()

ELEMENT_DEFAULTS = {
    "Frame": {"w": 120.0, "h": 60.0, "text": ""},
    "Button": {"w": 90.0, "h": 24.0, "text": "New Button"},
    "EditBox": {"w": 140.0, "h": 20.0, "text": ""},
    "FontString": {"w": 140.0, "h": 16.0, "text": "New Label"},
    "CheckButton": {"w": 24.0, "h": 24.0, "text": ""},
    "Slider": {"w": 140.0, "h": 16.0, "text": ""},
}
SNAP_VALUES = ("1", "2", "2.5", "5", "10")
UNDO_LIMIT = 5
LAYER_LEVELS = ("BACKGROUND", "BORDER", "ARTWORK", "OVERLAY", "HIGHLIGHT")
LAYER_RANK = {name: idx for idx, name in enumerate(LAYER_LEVELS)}

# ── XML parser ─────────────────────────────────────────────────────────────────
_FRAME_TAGS = {"Frame","Button","EditBox","FontString","CheckButton","Slider"}

def _ltag(e):
    t = e.tag
    return t.split("}")[-1] if "}" in t else t

def _parse_elem(elem, _parent, current_layer="", xml_path=()) -> list[FNode]:
    tag = _ltag(elem)
    if tag not in _FRAME_TAGS:
        out = []
        for idx, ch in enumerate(list(elem)):
            out.extend(_parse_elem(ch, _parent, current_layer, xml_path + (idx,)))
        return out

    n = FNode()
    n.ftype  = tag
    n.name   = elem.get("name", "")
    n.hidden = elem.get("hidden", "false").lower() == "true"
    n.text   = elem.get("text", "")
    n.on_layer = current_layer
    n._xml_path = xml_path

    for sub in elem:
        stag = _ltag(sub)
        if stag == "Size":
            n.w = float(sub.get("x", n.w))
            n.h = float(sub.get("y", n.h))
        elif stag == "Anchors":
            for anch in sub:
                if _ltag(anch) == "Anchor":
                    n.a_pt    = anch.get("point", "CENTER")
                    n.a_relto = anch.get("relativeTo", "")
                    n.a_relpt = anch.get("relativePoint", "")
                    for off in anch:
                        if _ltag(off) == "Offset":
                            for dim in off:
                                if _ltag(dim) == "AbsDimension":
                                    n.a_ox = float(dim.get("x", 0))
                                    n.a_oy = float(dim.get("y", 0))
                    break
        elif stag == "Frames":
            for idx, ch in enumerate(list(sub)):
                n.children.extend(_parse_elem(ch, n, "", xml_path + (list(elem).index(sub), idx)))
        elif stag == "Layers":
            sub_children = list(sub)
            sub_index = list(elem).index(sub)
            for layer_index, layer in enumerate(sub_children):
                layer_level = layer.get("level", "")
                for idx, ch in enumerate(list(layer)):
                    n.children.extend(_parse_elem(ch, n, layer_level, xml_path + (sub_index, layer_index, idx)))
        elif stag == "Scripts":
            for script in sub:
                n._scripts[_ltag(script)] = (script.text or "").strip()

    return [n]

def parse_xml(path) -> list[FNode]:
    tree = ET.parse(path)
    nodes = []
    for idx, ch in enumerate(list(tree.getroot())):
        nodes.extend(_parse_elem(ch, None, "", (idx,)))
    return nodes

# ── Synthetic gear-slot nodes (from LivingWorld.lua LWCP_InitGearPage) ─────────
def _make_gear_slot(name, parent_node, layout: dict[str, float], label="") -> FNode:
    n = FNode()
    n.name      = name
    n.ftype     = "Button"
    n.text      = label
    n.w, n.h    = float(layout.get("w", 30.0)), float(layout.get("h", 30.0))
    n.a_pt      = "TOP"
    n.a_relto   = parent_node.name
    n.a_relpt   = "TOP"
    n.a_ox      = float(layout.get("x", 0.0))
    n.a_oy      = float(layout.get("y", 0.0))
    n._synthetic = True
    return n

def load_gear_slot_layouts(path: str) -> dict[str, dict[str, float]]:
    layouts = {name: values.copy() for name, values in DEFAULT_GEAR_SLOT_LAYOUTS.items()}
    if not os.path.exists(path):
        return layouts

    text = open(path, "r", encoding="utf-8").read()
    start = text.find(GEAR_LAYOUT_MARKER_START)
    end = text.find(GEAR_LAYOUT_MARKER_END, start + len(GEAR_LAYOUT_MARKER_START))
    if start == -1 or end == -1:
        return layouts

    block = text[start + len(GEAR_LAYOUT_MARKER_START):end]
    for match in GEAR_SLOT_LAYOUT_ENTRY_RE.finditer(block):
        name = match.group("name")
        if name not in layouts:
            continue
        layouts[name] = {
            "x": float(match.group("x")),
            "y": float(match.group("y")),
            "w": float(match.group("w")),
            "h": float(match.group("h")),
        }
    return layouts

def _format_gear_slot_layout_block(layouts: dict[str, dict[str, float]]) -> str:
    lines = [GEAR_LAYOUT_MARKER_START, "local LW_GearSlotLayout = {"]
    for name in GEAR_LAYOUT_ORDER:
        layout = layouts.get(name, DEFAULT_GEAR_SLOT_LAYOUTS[name])
        lines.append(
            f"    {name} = {{ x = {_fmt_num(layout['x'])}, y = {_fmt_num(layout['y'])}, "
            f"w = {_fmt_num(layout['w'])}, h = {_fmt_num(layout['h'])} }},"
        )
    lines.append("}")
    lines.append(GEAR_LAYOUT_MARKER_END)
    return "\n".join(lines)

def save_gear_slot_layouts(path: str, named: dict[str, FNode]):
    layouts = {name: values.copy() for name, values in DEFAULT_GEAR_SLOT_LAYOUTS.items()}
    for name in GEAR_LAYOUT_ORDER:
        node = named.get(name)
        if node is None:
            continue
        layouts[name] = {
            "x": node.a_ox,
            "y": node.a_oy,
            "w": node.w,
            "h": node.h,
        }

    block = _format_gear_slot_layout_block(layouts)
    text = open(path, "r", encoding="utf-8").read()
    pattern = re.compile(
        re.escape(GEAR_LAYOUT_MARKER_START) + r".*?" + re.escape(GEAR_LAYOUT_MARKER_END),
        re.S,
    )
    if pattern.search(text):
        text = pattern.sub(block, text, count=1)
    else:
        insert_at = text.find("function LWCP_InitGearPage(frame)")
        if insert_at == -1:
            raise ValueError("Could not find LWCP_InitGearPage in LivingWorld.lua")
        prefix = text[:insert_at].rstrip() + "\n\n"
        suffix = text[insert_at:]
        text = prefix + block + "\n\n" + suffix

    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)

def load_bags_tab_layouts(path: str) -> dict[str, dict[str, float]]:
    layouts = {name: values.copy() for name, values in DEFAULT_BAGS_TAB_LAYOUTS.items()}
    if not os.path.exists(path):
        return layouts

    text = open(path, "r", encoding="utf-8").read()
    start = text.find(BAGS_TAB_LAYOUT_MARKER_START)
    end = text.find(BAGS_TAB_LAYOUT_MARKER_END, start + len(BAGS_TAB_LAYOUT_MARKER_START))
    if start == -1 or end == -1:
        return layouts

    block = text[start + len(BAGS_TAB_LAYOUT_MARKER_START):end]
    for match in GEAR_SLOT_LAYOUT_ENTRY_RE.finditer(block):
        name = match.group("name")
        if name not in layouts:
            continue
        layouts[name] = {
            "x": float(match.group("x")),
            "y": float(match.group("y")),
            "w": float(match.group("w")),
            "h": float(match.group("h")),
        }
    return layouts

def _format_bags_tab_layout_block(layouts: dict[str, dict[str, float]]) -> str:
    lines = [BAGS_TAB_LAYOUT_MARKER_START, "local LW_BagsTabLayout = {"]
    for name in BAGS_TAB_LAYOUT_ORDER:
        layout = layouts.get(name, DEFAULT_BAGS_TAB_LAYOUTS[name])
        lines.append(
            f"    {name} = {{ x = {_fmt_num(layout['x'])}, y = {_fmt_num(layout['y'])}, "
            f"w = {_fmt_num(layout['w'])}, h = {_fmt_num(layout['h'])} }},"
        )
    lines.append("}")
    lines.append(BAGS_TAB_LAYOUT_MARKER_END)
    return "\n".join(lines)

def save_bags_tab_layouts(path: str, named: dict[str, FNode]):
    layouts = {name: values.copy() for name, values in DEFAULT_BAGS_TAB_LAYOUTS.items()}
    for name in BAGS_TAB_LAYOUT_ORDER:
        node = named.get(name)
        if node is None:
            continue
        layouts[name] = {
            "x": node.a_ox,
            "y": node.a_oy,
            "w": node.w,
            "h": node.h,
        }

    block = _format_bags_tab_layout_block(layouts)
    text = open(path, "r", encoding="utf-8").read()
    pattern = re.compile(
        re.escape(BAGS_TAB_LAYOUT_MARKER_START) + r".*?" + re.escape(BAGS_TAB_LAYOUT_MARKER_END),
        re.S,
    )
    if pattern.search(text):
        text = pattern.sub(block, text, count=1)
    else:
        insert_at = text.find(GEAR_LAYOUT_MARKER_START)
        if insert_at == -1:
            insert_at = text.find("function LWCP_InitBagsPage(frame)")
        if insert_at == -1:
            raise ValueError("Could not find Bags layout insertion point in LivingWorld.lua")
        prefix = text[:insert_at].rstrip() + "\n\n"
        suffix = text[insert_at:]
        text = prefix + block + "\n\n" + suffix

    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)

def inject_gear_slots(named: dict, layouts: dict[str, dict[str, float]]) -> None:
    gear_page = named.get("LWCPPageGear")
    if gear_page is None:
        return

    slots: list[FNode] = []

    # Equipment slots 1-19
    for i in range(1, 20):
        label = GEAR_SLOT_NAMES[i - 1] if i <= len(GEAR_SLOT_NAMES) else str(i)
        name = f"LWCPGearSlot{i}"
        s = _make_gear_slot(name, gear_page, layouts.get(name, DEFAULT_GEAR_SLOT_LAYOUTS[name]), label=label)
        slots.append(s)

    gear_page.children.extend(slots)
    for s in slots:
        named[s.name] = s

def prune_legacy_gear_page_nodes(named: dict[str, FNode]) -> list[tuple[str | None, tuple | None]]:
    gear_page = named.get("LWCPPageGear")
    if gear_page is None:
        return []

    removed: list[tuple[str | None, tuple | None]] = []
    kept_equipped_label = False
    keep_children: list[FNode] = []
    remove_names = {"LWCPGearPrevBtn", "LWCPGearNextBtn", "LWCPGearPageLabel"}
    remove_texts = {"Bags  (click to retrieve):", "Equipped Bags:"}

    for child in gear_page.children:
        drop = False
        if child.name in remove_names:
            drop = True
        elif child.ftype == "FontString":
            if child.text == "Equipped  (click to retrieve):":
                if kept_equipped_label:
                    drop = True
                else:
                    kept_equipped_label = True
            elif child.text in remove_texts:
                drop = True
        if drop:
            removed.append(_node_identity(child))
            if child.name and named.get(child.name) is child:
                del named[child.name]
        else:
            keep_children.append(child)

    gear_page.children = keep_children
    return removed

def inject_bags_tab_controls(named: dict, layouts: dict[str, dict[str, float]]) -> None:
    bags_page = named.get("LWCPPageBags")
    if bags_page is None:
        return

    slots: list[FNode] = []
    for bag_idx in range(5):
        name = f"LWCPBagContainerBtn{bag_idx}"
        label = "Pack" if bag_idx == 0 else f"Bag{bag_idx}"
        slots.append(_make_gear_slot(name, bags_page, layouts.get(name, DEFAULT_BAGS_TAB_LAYOUTS[name]), label=label))

    slots.append(_make_gear_slot("LWCPBagGearBtn", bags_page,
                                 layouts.get("LWCPBagGearBtn", DEFAULT_BAGS_TAB_LAYOUTS["LWCPBagGearBtn"]),
                                 label="Gear"))

    for i in range(1, 33):
        name = f"LWCPBagItemSlot{i}"
        slots.append(_make_gear_slot(name, bags_page, layouts.get(name, DEFAULT_BAGS_TAB_LAYOUTS[name]), label=str(i)))

    bags_page.children.extend(slots)
    for s in slots:
        named[s.name] = s

# ── Layout resolver ─────────────────────────────────────────────────────────────
def _ref_rect(rel_to, named, par_rect):
    if not rel_to:
        return par_rect
    lo = rel_to.lower()
    if lo == "uiparent":
        return UIPARENT
    if lo == "minimap":
        return (CANVAS_W - 65.0, 55.0, 42.0, 42.0)
    if rel_to in named:
        ref = named[rel_to]
        return (ref.cx_l, ref.cx_t, ref.w * SCALE, ref.h * SCALE)
    return par_rect

def _resolve(node: FNode, named: dict, par_rect: tuple):
    rl, rt, rw, rh = _ref_rect(node.a_relto, named, par_rect)
    ax, ay = _pt(rl, rt, rw, rh, node.eff_relpt)
    ax += node.a_ox * SCALE
    ay -= node.a_oy * SCALE          # WoW y+ = up → canvas y+ = down
    nw, nh = node.w * SCALE, node.h * SCALE
    node.cx_l, node.cx_t = _origin(ax, ay, node.a_pt, nw, nh)
    child_rect = (node.cx_l, node.cx_t, nw, nh)
    for ch in node.children:
        _resolve(ch, named, child_rect)

def resolve_all(nodes: list[FNode], named: dict):
    for n in nodes:
        _resolve(n, named, UIPARENT)

# ── XML save helper ────────────────────────────────────────────────────────────
def _find_by_name(root_elem, name):
    for e in root_elem.iter():
        if e.get("name") == name:
            return e
    return None

def _find_by_path(root_elem, path):
    elem = root_elem
    for idx in path or ():
        children = list(elem)
        if idx < 0 or idx >= len(children):
            return None
        elem = children[idx]
    return elem

def _find_element_for_node(root_elem, node: FNode):
    if node.name:
        elem = _find_by_name(root_elem, node.name)
        if elem is not None:
            return elem
    if node._xml_path is not None:
        return _find_by_path(root_elem, node._xml_path)
    return None

def _xml_ns(root_elem):
    return root_elem.tag.split("}")[0] + "}" if "}" in root_elem.tag else ""

def _xe(root_elem, tag, attrib=None, text=None):
    elem = ET.Element(f"{_xml_ns(root_elem)}{tag}", attrib or {})
    if text is not None:
        elem.text = text
    return elem

def _direct_child(elem, tag):
    for child in elem:
        if _ltag(child) == tag:
            return child
    return None

def _ensure_path(root_elem, elem, tags):
    cur = elem
    for tag in tags:
        nxt = _direct_child(cur, tag)
        if nxt is None:
            nxt = _xe(root_elem, tag)
            cur.append(nxt)
        cur = nxt
    return cur

def _fmt_num(val):
    return str(int(val) if val == int(val) else val)

def _find_parent_node(nodes: list[FNode], target: FNode) -> FNode | None:
    for root in nodes:
        for node in root.all_descendants():
            for child in node.children:
                if child is target:
                    return node
    return None

def _clone_node(node: FNode) -> FNode:
    cloned = FNode()
    cloned.uid = node.uid
    cloned.name = node.name
    cloned.ftype = node.ftype
    cloned.hidden = node.hidden
    cloned.w = node.w
    cloned.h = node.h
    cloned.text = node.text
    cloned.a_pt = node.a_pt
    cloned.a_relto = node.a_relto
    cloned.a_relpt = node.a_relpt
    cloned.a_ox = node.a_ox
    cloned.a_oy = node.a_oy
    cloned.cx_l = node.cx_l
    cloned.cx_t = node.cx_t
    cloned.on_layer = node.on_layer
    cloned._scripts = dict(node._scripts)
    cloned._synthetic = node._synthetic
    cloned._xml_path = node._xml_path
    cloned.children = [_clone_node(child) for child in node.children]
    return cloned

def _clone_nodes(nodes: list[FNode]) -> list[FNode]:
    return [_clone_node(node) for node in nodes]

def _node_identity(node: FNode) -> tuple[str | None, tuple | None]:
    return (node.name or None, tuple(node._xml_path) if node._xml_path is not None else None)

def _rebuild_named(nodes: list[FNode]) -> dict[str, FNode]:
    named: dict[str, FNode] = {}
    max_uid = 0
    for root in nodes:
        for node in root.all_descendants():
            max_uid = max(max_uid, node.uid)
            if node.name:
                named[node.name] = node
    FNode._ctr = max(FNode._ctr, max_uid)
    return named

def _build_xml_element_for_node(root_elem, node: FNode):
    elem = _xe(root_elem, node.ftype)
    if node.name:
        elem.set("name", node.name)
    if node.text and node.ftype in {"Button", "FontString"}:
        elem.set("text", node.text)
    if node.hidden:
        elem.set("hidden", "true")
    if node.ftype == "Button":
        elem.set("inherits", "UIPanelButtonTemplate")
    elif node.ftype == "EditBox":
        elem.set("inherits", "InputBoxTemplate")
        elem.set("autoFocus", "false")
    size = _xe(root_elem, "Size", {"x": _fmt_num(node.w), "y": _fmt_num(node.h)})
    elem.append(size)
    anchors = _xe(root_elem, "Anchors")
    anchor = _xe(root_elem, "Anchor", {"point": node.a_pt})
    if node.a_relto:
        anchor.set("relativeTo", node.a_relto)
    if node.a_relpt:
        anchor.set("relativePoint", node.a_relpt)
    offset = _xe(root_elem, "Offset")
    dim = _xe(root_elem, "AbsDimension", {"x": _fmt_num(node.a_ox), "y": _fmt_num(node.a_oy)})
    offset.append(dim)
    anchor.append(offset)
    anchors.append(anchor)
    elem.append(anchors)
    if node._scripts:
        scripts = _xe(root_elem, "Scripts")
        for tag_name, code in node._scripts.items():
            scripts.append(_xe(root_elem, tag_name, text=code))
        elem.append(scripts)
    if node.ftype == "Frame":
        elem.append(_xe(root_elem, "Frames"))
    return elem

def _ensure_xml_node_exists(root_elem, nodes: list[FNode], node: FNode):
    existing = _find_by_name(root_elem, node.name) if node.name else None
    if existing is not None:
        return existing
    parent_node = _find_parent_node(nodes, node)
    if parent_node is None:
        raise ValueError(f"Could not find parent for new node '{node.name or node.ftype}'")
    parent_elem = _find_by_name(root_elem, parent_node.name) if parent_node.name else None
    if parent_elem is None:
        raise ValueError(f"Could not find XML parent '{parent_node.name}' for new node '{node.name or node.ftype}'")
    elem = _build_xml_element_for_node(root_elem, node)
    return _place_node_elem(root_elem, parent_elem, elem, node)

def _find_layer_container(root_elem, parent_elem, level: str):
    layers = _ensure_path(root_elem, parent_elem, ["Layers"])
    for child in layers:
        if _ltag(child) == "Layer" and child.get("level", "") == level:
            return child
    layer = _xe(root_elem, "Layer", {"level": level})
    layers.append(layer)
    return layer

def _place_node_elem(root_elem, parent_elem, elem, node: FNode):
    existing_parent = None
    for candidate in parent_elem.iter():
        for child in list(candidate):
            if child is elem:
                existing_parent = candidate
                candidate.remove(child)
                break
        if existing_parent is not None:
            break

    if node.ftype != "Frame" and node.on_layer:
        container = _find_layer_container(root_elem, parent_elem, node.on_layer)
    elif node.ftype == "FontString":
        container = _find_layer_container(root_elem, parent_elem, node.on_layer or "OVERLAY")
    else:
        container = _ensure_path(root_elem, parent_elem, ["Frames"])
    container.append(elem)
    return elem

def _tab_button_metrics(tabs: list[str]):
    widths = [max(52, min(96, 24 + len(tab) * 7)) for tab in tabs]
    gap = 6
    total = sum(widths) + gap * max(0, len(widths) - 1)
    left = -(total / 2.0)
    out = []
    for tab, width in zip(tabs, widths):
        center_x = left + (width / 2.0)
        out.append((tab, width, center_x))
        left += width + gap
    return out

def load_tabs_from_lua(path: str) -> list[str]:
    if not os.path.exists(path):
        return DEFAULT_TABS[:]
    text = open(path, "r", encoding="utf-8").read()
    match = re.search(r'local\s+LW_Tabs\s*=\s*\{(?P<body>.*?)\}', text, re.S)
    if not match:
        return DEFAULT_TABS[:]
    tabs = re.findall(r'"([^"]+)"', match.group("body"))
    return tabs or DEFAULT_TABS[:]

def save_tabs_to_lua(path: str, tabs: list[str]):
    text = open(path, "r", encoding="utf-8").read()
    new_decl = 'local LW_Tabs = { ' + ", ".join(f'"{tab}"' for tab in tabs) + " }"
    text, count = re.subn(r'local\s+LW_Tabs\s*=\s*\{.*?\}', new_decl, text, count=1, flags=re.S)
    if count != 1:
        raise ValueError("Could not find LW_Tabs declaration in LivingWorld.lua")
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)

def _strip_lua_for_keywords(line: str) -> str:
    line = re.sub(r"--.*$", "", line)
    line = re.sub(r'"(?:\\.|[^"\\])*"', '""', line)
    line = re.sub(r"'(?:\\.|[^'\\])*'", "''", line)
    return line

def _lua_block_delta(line: str) -> int:
    stripped = _strip_lua_for_keywords(line)
    function_count = len(re.findall(r"\bfunction\b", stripped))
    if_count = len(re.findall(r"\bif\b.*\bthen\b", stripped))
    for_count = len(re.findall(r"\bfor\b.*\bdo\b", stripped))
    while_count = len(re.findall(r"\bwhile\b.*\bdo\b", stripped))
    repeat_count = len(re.findall(r"\brepeat\b", stripped))
    do_count = len(re.findall(r"\bdo\b", stripped)) - for_count - while_count
    end_count = len(re.findall(r"\bend\b", stripped))
    until_count = len(re.findall(r"\buntil\b", stripped))
    return function_count + if_count + for_count + while_count + repeat_count + do_count - end_count - until_count

def parse_lua_functions(text: str) -> list[dict]:
    lines = text.splitlines(keepends=True)
    funcs: list[dict] = []
    i = 0
    while i < len(lines):
        match = LUA_FUNC_START_RE.match(lines[i])
        if not match:
            i += 1
            continue

        name = match.group(2)
        depth = 0
        end_idx = i
        for j in range(i, len(lines)):
            depth += _lua_block_delta(lines[j])
            if depth <= 0:
                end_idx = j
                break

        code = "".join(lines[i:end_idx + 1]).rstrip() + "\n"
        funcs.append({
            "name": name,
            "start": i,
            "end": end_idx,
            "code": code,
        })
        i = end_idx + 1
    return funcs

def sync_tabs_in_xml(path: str, tabs: list[str]):
    tree = ET.parse(path)
    root_elem = tree.getroot()
    main_frame = _find_by_name(root_elem, "LWCPFrame")
    if main_frame is None:
        raise ValueError("Could not find LWCPFrame in LivingWorld.xml")
    frames_elem = _direct_child(main_frame, "Frames")
    scripts_elem = _direct_child(main_frame, "Scripts")
    if frames_elem is None or scripts_elem is None:
        raise ValueError("LWCPFrame is missing Frames or Scripts sections")

    tab_buttons = {child.get("name"): child for child in list(frames_elem)
                   if child.get("name", "").startswith("LWCPTab")}
    tab_pages = {child.get("name"): child for child in list(frames_elem)
                 if child.get("name", "").startswith("LWCPPage")}

    keep_button_names = {f"LWCPTab{tab}" for tab in tabs}
    keep_page_names = {f"LWCPPage{tab}" for tab in tabs}

    for child in list(frames_elem):
        name = child.get("name", "")
        if name.startswith("LWCPTab") and name not in keep_button_names:
            frames_elem.remove(child)
        if name.startswith("LWCPPage") and name not in keep_page_names:
            frames_elem.remove(child)

    for tab, width, center_x in _tab_button_metrics(tabs):
        btn_name = f"LWCPTab{tab}"
        btn = _find_by_name(root_elem, btn_name)
        if btn is None:
            btn = _xe(root_elem, "Button", {
                "name": btn_name,
                "inherits": "UIPanelButtonTemplate",
                "text": tab,
            })
            frames_elem.append(btn)
        btn.set("text", tab)

        size = _ensure_path(root_elem, btn, ["Size"])
        size.set("x", _fmt_num(width))
        size.set("y", "22")

        anchor = _ensure_path(root_elem, btn, ["Anchors", "Anchor"])
        anchor.set("point", "TOP")
        anchor.attrib.pop("relativeTo", None)
        anchor.attrib.pop("relativePoint", None)
        dim = _ensure_path(root_elem, btn, ["Anchors", "Anchor", "Offset", "AbsDimension"])
        dim.set("x", _fmt_num(center_x))
        dim.set("y", "-72")

        scripts = _ensure_path(root_elem, btn, ["Scripts"])
        onclick = _direct_child(scripts, "OnClick")
        if onclick is None:
            onclick = _xe(root_elem, "OnClick")
            scripts.append(onclick)
        onclick.text = f'LWCP_ShowTab("{tab}");'

        page_name = f"LWCPPage{tab}"
        page = _find_by_name(root_elem, page_name)
        if page is None:
            page = _xe(root_elem, "Frame", {"name": page_name, "hidden": "true"})
            size = _xe(root_elem, "Size", {"x": "240", "y": "300"})
            page.append(size)
            anchors = _xe(root_elem, "Anchors")
            anchor = _xe(root_elem, "Anchor", {"point": "TOP"})
            offset = _xe(root_elem, "Offset")
            dim = _xe(root_elem, "AbsDimension", {"x": "0", "y": "-100"})
            offset.append(dim)
            anchor.append(offset)
            anchors.append(anchor)
            page.append(anchors)
            layers = _xe(root_elem, "Layers")
            layer = _xe(root_elem, "Layer", {"level": "OVERLAY"})
            fs = _xe(root_elem, "FontString", {
                "inherits": "GameFontNormal",
                "text": f"-- {tab} --",
            })
            fs.append(_xe(root_elem, "Size", {"x": "216", "y": "16"}))
            fs_anchors = _xe(root_elem, "Anchors")
            fs_anchor = _xe(root_elem, "Anchor", {"point": "CENTER", "relativePoint": "TOP"})
            fs_offset = _xe(root_elem, "Offset")
            fs_dim = _xe(root_elem, "AbsDimension", {"x": "0", "y": "-12"})
            fs_offset.append(fs_dim)
            fs_anchor.append(fs_offset)
            fs_anchors.append(fs_anchor)
            fs.append(fs_anchors)
            layer.append(fs)
            layers.append(layer)
            page.append(layers)
            page.append(_xe(root_elem, "Frames"))
            frames_elem.append(page)
        page.set("hidden", "false" if tab == tabs[0] else "true")

    onshow = _direct_child(scripts_elem, "OnShow")
    if onshow is None:
        onshow = _xe(root_elem, "OnShow")
        scripts_elem.append(onshow)
    onshow.text = f'LWCP_RefreshRoster(); LWCP_ShowTab("{tabs[0]}");'

    try:
        ET.indent(tree, space="    ")
    except AttributeError:
        pass
    tree.write(path, xml_declaration=False, encoding="unicode")

def _remove_xml_element(root_elem, elem) -> bool:
    for parent in root_elem.iter():
        for child in list(parent):
            if child is elem:
                parent.remove(child)
                return True
    return False

def save_xml(nodes: list[FNode], named: dict, path: str, deleted_nodes: list[tuple[str | None, tuple | None]] | None = None):
    tree = ET.parse(path)
    root_elem = tree.getroot()

    for deleted_name, deleted_path in deleted_nodes or []:
        elem = _find_by_name(root_elem, deleted_name) if deleted_name else None
        if elem is None and deleted_path is not None:
            elem = _find_by_path(root_elem, deleted_path)
        if elem is not None:
            _remove_xml_element(root_elem, elem)

    for root in nodes:
        for node in root.all_descendants():
            if node._synthetic:
                continue
            elem = _find_element_for_node(root_elem, node)
            if elem is None:
                elem = _ensure_xml_node_exists(root_elem, nodes, node)
                node._xml_path = None
            parent_node = _find_parent_node(nodes, node)
            if parent_node and parent_node.name:
                parent_elem = _find_by_name(root_elem, parent_node.name)
                if parent_elem is not None:
                    _place_node_elem(root_elem, parent_elem, elem, node)
            if node.ftype in {"Button", "FontString"}:
                elem.set("text", node.text)
            elif elem.get("text") is not None and not node.text:
                elem.set("text", "")
            size = _ensure_path(root_elem, elem, ["Size"])
            size.set("x", _fmt_num(node.w))
            size.set("y", _fmt_num(node.h))
            anch = _ensure_path(root_elem, elem, ["Anchors", "Anchor"])
            anch.set("point", node.a_pt)
            if node.a_relto:
                anch.set("relativeTo", node.a_relto)
            else:
                anch.attrib.pop("relativeTo", None)
            if node.a_relpt:
                anch.set("relativePoint", node.a_relpt)
            else:
                anch.attrib.pop("relativePoint", None)
            dim = _ensure_path(root_elem, elem, ["Anchors", "Anchor", "Offset", "AbsDimension"])
            dim.set("x", _fmt_num(node.a_ox))
            dim.set("y", _fmt_num(node.a_oy))
            if node.on_layer and node.ftype != "Frame":
                node.on_layer = node.on_layer.upper()
            if node.hidden:
                elem.set("hidden", "true")
            elif "hidden" in elem.attrib:
                elem.set("hidden", "false")
            scripts = _direct_child(elem, "Scripts")
            if node._scripts:
                if scripts is None:
                    scripts = _xe(root_elem, "Scripts")
                    elem.append(scripts)
                existing = {_ltag(child): child for child in list(scripts)}
                for tag_name, code in node._scripts.items():
                    script_elem = existing.get(tag_name)
                    if script_elem is None:
                        script_elem = _xe(root_elem, tag_name)
                        scripts.append(script_elem)
                    script_elem.text = code
                for child in list(scripts):
                    if _ltag(child) not in node._scripts:
                        scripts.remove(child)
            elif scripts is not None:
                for child in list(scripts):
                    scripts.remove(child)

    try:
        ET.indent(tree, space="    ")
    except AttributeError:
        pass  # Python < 3.9

    tree.write(path, xml_declaration=False, encoding="unicode")

# ── Editor application ─────────────────────────────────────────────────────────
class LWEditor:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("LivingWorld CP — UI Editor")
        root.configure(bg="#0d1117")
        root.resizable(True, True)

        self.nodes:    list[FNode]    = []
        self.named:    dict[str, FNode] = {}
        self.selected: FNode | None   = None
        self.tabs                     = DEFAULT_TABS[:]
        self.active_tab               = self.tabs[0]
        self._cid_node: dict[int, FNode] = {}  # canvas item id → FNode

        # Drag state
        self._press_cx    = 0.0
        self._press_cy    = 0.0
        self._press_node: FNode | None = None
        self._drag_active = False
        self._drag_orig_ox = 0.0
        self._drag_orig_oy = 0.0
        self._drag_lock_axis = None
        self._resize_active = False
        self._resize_handle = None
        self._resize_handles: dict[int, str] = {}
        self._resize_orig = {}
        self._pan_active = False
        self._snap_enabled = tk.BooleanVar(value=False)
        self._snap_value = tk.StringVar(value="1")
        self._preview_mode = tk.BooleanVar(value=False)
        self._splitter_initialized = False
        self._undo_stack: list[dict] = []
        self._deleted_nodes: list[tuple[str | None, tuple | None]] = []

        self._build_ui()
        self.root.bind_all("<Control-z>", self._on_undo_shortcut)
        self._load()

    # ── UI construction ────────────────────────────────────────────────────────
    def _build_ui(self):
        toolbar = tk.Frame(self.root, bg="#1f2937", height=48)
        toolbar.pack(fill="x", side="top")
        self._build_toolbar(toolbar)

        body = ttk.Panedwindow(self.root, orient="horizontal")
        body.pack(fill="both", expand=True)
        self.body = body

        code_frame = tk.Frame(body, bg="#111827", width=CODE_W)
        code_frame.pack_propagate(False)
        self._build_code_panel(code_frame)
        body.add(code_frame, weight=2)

        cvs_frame = tk.Frame(body, bg="#0d1117")
        self._build_canvas(cvs_frame)
        body.add(cvs_frame, weight=5)

        prop_frame = tk.Frame(body, bg="#1f2937", width=PANEL_W)
        prop_frame.pack_propagate(False)
        self._build_props(prop_frame)
        body.add(prop_frame, weight=2)
        self.root.after_idle(self._schedule_splitter_init)

    def _build_toolbar(self, tb):
        _btn = dict(bg="#374151", fg="white", relief="flat", padx=10, pady=5,
                    cursor="hand2", font=("Segoe UI", 9),
                    activebackground="#4b5563", activeforeground="white")

        tk.Label(tb, text="LW CP Editor", bg="#1f2937", fg="#f59e0b",
                 font=("Segoe UI", 11, "bold")).pack(side="left", padx=12, pady=8)

        tk.Button(tb, text="↺  Reload", command=self._load, **_btn
                  ).pack(side="left", padx=4, pady=8)
        tk.Button(tb, text="💾  Save XML", command=self._save, **_btn
                  ).pack(side="left", padx=4, pady=8)

        tk.Label(tb, text="│", bg="#1f2937", fg="#374151").pack(side="left", padx=6)
        tk.Label(tb, text="Tab:", bg="#1f2937", fg="#9ca3af",
                 font=("Segoe UI", 9)).pack(side="left", padx=(0, 4), pady=8)

        self._tab_btns: dict[str, tk.Button] = {}
        self._tab_btn_style = _btn
        self._tab_bar = tk.Frame(tb, bg="#1f2937")
        self._tab_bar.pack(side="left", pady=8)
        tk.Button(tb, text="+ Tab", command=self._add_tab, **_btn).pack(side="left", padx=(6, 2), pady=8)
        tk.Button(tb, text="- Tab", command=self._delete_tab, **_btn).pack(side="left", padx=2, pady=8)

        self._scale_var = tk.DoubleVar(value=SCALE)
        tk.Label(tb, text="│", bg="#1f2937", fg="#374151").pack(side="left", padx=6)
        tk.Label(tb, text="Zoom:", bg="#1f2937", fg="#9ca3af",
                 font=("Segoe UI", 9)).pack(side="left", padx=(0, 2))
        tk.Scale(tb, variable=self._scale_var, from_=0.8, to=2.5,
                 resolution=0.1, orient="horizontal", length=90,
                 bg="#1f2937", fg="white", troughcolor="#374151",
                 highlightthickness=0, command=self._on_zoom,
                 sliderlength=16).pack(side="left", pady=6)

        self._undo_btn = tk.Button(tb, text="Undo", command=self._undo, **_btn)
        self._undo_btn.pack(side="left", padx=4, pady=8)

        tk.Label(tb, text="â”‚", bg="#1f2937", fg="#374151").pack(side="left", padx=6)
        tk.Checkbutton(tb, text="Snap", variable=self._snap_enabled,
                       bg="#1f2937", fg="white", selectcolor="#374151",
                       activebackground="#1f2937", activeforeground="white",
                       font=("Segoe UI", 9), highlightthickness=0
                       ).pack(side="left", pady=8)
        self._snap_combo = ttk.Combobox(tb, textvariable=self._snap_value,
                                        state="readonly", font=("Segoe UI", 8),
                                        values=SNAP_VALUES, width=6)
        self._snap_combo.pack(side="left", padx=(6, 0), pady=8)
        tk.Checkbutton(tb, text="Preview", variable=self._preview_mode,
                       command=self._render, bg="#1f2937", fg="white",
                       selectcolor="#374151", activebackground="#1f2937",
                       activeforeground="white", font=("Segoe UI", 9),
                       highlightthickness=0).pack(side="left", padx=(8, 0), pady=8)
        tk.Button(tb, text="Center X", command=lambda: self._center_selected_to_parent("x"), **_btn
                  ).pack(side="left", padx=(6, 2), pady=8)
        tk.Button(tb, text="Center Y", command=lambda: self._center_selected_to_parent("y"), **_btn
                  ).pack(side="left", padx=2, pady=8)

        self._refresh_toolbar_tabs()
        self._refresh_undo_button()

    def _build_code_panel(self, parent):
        tk.Label(parent, text="Code", bg="#111827", fg="#f59e0b",
                 font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=12, pady=(12, 4))
        ttk.Separator(parent, orient="horizontal").pack(fill="x", padx=8)

        nb = ttk.Notebook(parent)
        nb.pack(fill="both", expand=True, padx=10, pady=8)

        element_tab = tk.Frame(nb, bg="#111827")
        lua_tab = tk.Frame(nb, bg="#111827")
        nb.add(element_tab, text="Element Scripts")
        nb.add(lua_tab, text="Lua Functions")

        self._code_info_var = tk.StringVar(value="Select a frame to inspect its scripts.")
        tk.Label(element_tab, textvariable=self._code_info_var, bg="#111827", fg="#9ca3af",
                 justify="left", anchor="w", font=("Segoe UI", 8)).pack(fill="x", pady=(4, 6))
        script_wrap = tk.Frame(element_tab, bg="#111827")
        script_wrap.pack(fill="both", expand=True)
        self._script_text = tk.Text(script_wrap, bg="#0b1220", fg="#86efac",
                                    relief="flat", font=("Consolas", 9), wrap="none")
        script_y = ttk.Scrollbar(script_wrap, orient="vertical", command=self._script_text.yview)
        script_x = ttk.Scrollbar(script_wrap, orient="horizontal", command=self._script_text.xview)
        self._script_text.configure(yscrollcommand=script_y.set, xscrollcommand=script_x.set)
        script_y.pack(side="right", fill="y")
        script_x.pack(side="bottom", fill="x")
        self._script_text.pack(fill="both", expand=True)
        tk.Label(element_tab, text="Use [OnClick], [OnShow], etc. as section headers.",
                 bg="#111827", fg="#6b7280", font=("Segoe UI", 8)).pack(anchor="w", pady=(6, 4))
        tk.Button(element_tab, text="Save Script Changes", command=self._save_script_changes,
                  bg="#d97706", fg="white", relief="flat", padx=10, pady=6,
                  cursor="hand2", font=("Segoe UI", 9, "bold"),
                  activebackground="#b45309").pack(anchor="e")

        self._lua_func_var = tk.StringVar()
        self._lua_func_info_var = tk.StringVar(value="Pick a function to inspect its block.")
        lua_select = tk.Frame(lua_tab, bg="#111827")
        lua_select.pack(fill="x", pady=(4, 6))
        tk.Label(lua_select, text="Function:", bg="#111827", fg="#9ca3af",
                 font=("Segoe UI", 8)).pack(side="left", padx=(0, 6))
        self._lua_func_combo = ttk.Combobox(lua_select, textvariable=self._lua_func_var,
                                            state="readonly", font=("Segoe UI", 8))
        self._lua_func_combo.pack(side="left", fill="x", expand=True)
        self._lua_func_combo.bind("<<ComboboxSelected>>", self._on_lua_function_selected)
        tk.Button(lua_select, text="Reload", command=self._load_lua_text,
                  bg="#374151", fg="white", relief="flat", padx=10, pady=5,
                  cursor="hand2", font=("Segoe UI", 9),
                  activebackground="#4b5563").pack(side="left", padx=(6, 0))
        tk.Button(lua_select, text="Save Function", command=self._save_lua_function,
                  bg="#d97706", fg="white", relief="flat", padx=10, pady=5,
                  cursor="hand2", font=("Segoe UI", 9, "bold"),
                  activebackground="#b45309").pack(side="left", padx=(6, 0))
        tk.Label(lua_tab, textvariable=self._lua_func_info_var, bg="#111827", fg="#9ca3af",
                 justify="left", anchor="w", font=("Segoe UI", 8)).pack(fill="x", pady=(0, 6))
        lua_wrap = tk.Frame(lua_tab, bg="#111827")
        lua_wrap.pack(fill="both", expand=True)
        self._lua_func_text = tk.Text(lua_wrap, bg="#0b1220", fg="#e5e7eb",
                                      relief="flat", font=("Consolas", 9), wrap="none")
        lua_y = ttk.Scrollbar(lua_wrap, orient="vertical", command=self._lua_func_text.yview)
        lua_x = ttk.Scrollbar(lua_wrap, orient="horizontal", command=self._lua_func_text.xview)
        self._lua_func_text.configure(yscrollcommand=lua_y.set, xscrollcommand=lua_x.set)
        lua_y.pack(side="right", fill="y")
        lua_x.pack(side="bottom", fill="x")
        self._lua_func_text.pack(fill="both", expand=True)

    def _refresh_toolbar_tabs(self):
        for child in self._tab_bar.winfo_children():
            child.destroy()
        self._tab_btns.clear()
        for tab in self.tabs:
            btn = tk.Button(self._tab_bar, text=tab,
                            command=lambda t=tab: self._switch_tab(t),
                            **self._tab_btn_style)
            btn.pack(side="left", padx=2)
            self._tab_btns[tab] = btn
        self._update_tab_btns()

    def _capture_undo_state(self) -> dict:
        return {
            "nodes": _clone_nodes(self.nodes),
            "active_tab": self.active_tab,
            "selected_name": self.selected.name if self.selected and self.selected.name else None,
            "selected_uid": self.selected.uid if self.selected else None,
            "deleted_nodes": list(self._deleted_nodes),
        }

    def _push_undo_state(self):
        if not self.nodes:
            return
        self._undo_stack.append(self._capture_undo_state())
        if len(self._undo_stack) > UNDO_LIMIT:
            self._undo_stack = self._undo_stack[-UNDO_LIMIT:]
        self._refresh_undo_button()

    def _refresh_undo_button(self):
        if hasattr(self, "_undo_btn"):
            self._undo_btn.configure(state=("normal" if self._undo_stack else "disabled"))

    def _restore_undo_state(self, state: dict):
        self.nodes = _clone_nodes(state["nodes"])
        self.named = _rebuild_named(self.nodes)
        self._deleted_nodes = list(state.get("deleted_nodes", []))
        self.active_tab = state.get("active_tab", self.active_tab)
        selected_name = state.get("selected_name")
        selected_uid = state.get("selected_uid")
        self.selected = self.named.get(selected_name) if selected_name else None
        if self.selected is None and selected_uid is not None:
            for root in self.nodes:
                for node in root.all_descendants():
                    if node.uid == selected_uid:
                        self.selected = node
                        break
                if self.selected is not None:
                    break
        self._update_tab_btns()
        self._refresh_relto_options()
        self._refresh_tool_parent_options()
        self._relayout()
        self._render()
        if self.selected:
            self._show_props(self.selected)
        else:
            self._clear_props()

    def _undo(self):
        if not self._undo_stack:
            return
        state = self._undo_stack.pop()
        self._restore_undo_state(state)
        self._refresh_undo_button()

    def _on_undo_shortcut(self, _event=None):
        if self.root.focus_get() is not self.canvas:
            return
        self._undo()
        return "break"

    def _load_lua_text(self):
        self._lua_source = ""
        if os.path.exists(LUA_FILE):
            with open(LUA_FILE, "r", encoding="utf-8") as fh:
                self._lua_source = fh.read()
        self._lua_functions = parse_lua_functions(self._lua_source)
        names = [entry["name"] for entry in self._lua_functions]
        self._lua_func_combo.configure(values=names)
        if names:
            current = self._lua_func_var.get()
            target = current if current in names else names[0]
            self._lua_func_var.set(target)
            self._show_lua_function(target)
        else:
            self._lua_func_var.set("")
            self._lua_func_text.delete("1.0", "end")
            self._lua_func_info_var.set("No top-level Lua functions were found.")

    def _on_lua_function_selected(self, _event=None):
        self._show_lua_function(self._lua_func_var.get())

    def _show_lua_function(self, name: str):
        self._lua_func_text.delete("1.0", "end")
        for entry in getattr(self, "_lua_functions", []):
            if entry["name"] == name:
                self._lua_func_text.insert("1.0", entry["code"])
                line_span = entry["end"] - entry["start"] + 1
                self._lua_func_info_var.set(f"{name}  lines {entry['start'] + 1}-{entry['end'] + 1} ({line_span} lines)")
                return
        self._lua_func_info_var.set("Pick a function to inspect its block.")

    def _save_lua_function(self):
        name = self._lua_func_var.get().strip()
        if not name:
            messagebox.showwarning("No function", "Select a Lua function first.")
            return
        updated_block = self._lua_func_text.get("1.0", "end-1c").rstrip() + "\n"
        lines = self._lua_source.splitlines(keepends=True)
        for entry in getattr(self, "_lua_functions", []):
            if entry["name"] != name:
                continue
            new_lines = updated_block.splitlines(keepends=True)
            lines[entry["start"]:entry["end"] + 1] = new_lines
            self._lua_source = "".join(lines)
            with open(LUA_FILE, "w", encoding="utf-8", newline="") as fh:
                fh.write(self._lua_source)
            self.tabs = load_tabs_from_lua(LUA_FILE)
            if self.active_tab not in self.tabs:
                self.active_tab = self.tabs[0]
            self._refresh_toolbar_tabs()
            self._load_lua_text()
            self._lua_func_var.set(name)
            self._show_lua_function(name)
            messagebox.showinfo("Saved", f"Updated function in:\n{LUA_FILE}")
            return
        messagebox.showerror("Save error", f"Could not find function '{name}' in LivingWorld.lua.")

    def _refresh_relto_options(self):
        relto = self._field_widgets.get("a_relto")
        if isinstance(relto, ttk.Combobox):
            values = ["", "UIParent", "Minimap"] + sorted(self.named.keys())
            relto.configure(values=values)

    def _refresh_tool_parent_options(self):
        values = ["Selected Element", "Active Tab Page"] + sorted(self.named.keys())
        if hasattr(self, "_tool_parent_combo"):
            self._tool_parent_combo.configure(values=values)
            if self._tool_parent_var.get() not in values:
                self._tool_parent_var.set("Selected Element" if self.selected else "Active Tab Page")

    def _on_tool_type_changed(self, _event=None):
        elem_type = self._tool_type_var.get()
        defaults = ELEMENT_DEFAULTS.get(elem_type, ELEMENT_DEFAULTS["Frame"])
        self._tool_text_var.set(defaults["text"])
        if not self._tool_name_var.get().strip():
            self._tool_name_var.set(self._suggest_element_name(elem_type))

    def _quick_add_element(self, elem_type: str):
        self._tool_type_var.set(elem_type)
        self._on_tool_type_changed()
        self._add_new_element()

    def _suggest_element_name(self, elem_type: str) -> str:
        base = f"LWCPNew{elem_type}"
        idx = 1
        name = f"{base}{idx}"
        while name in self.named:
            idx += 1
            name = f"{base}{idx}"
        return name

    def _get_active_page_node(self) -> FNode | None:
        return self.named.get(f"LWCPPage{self.active_tab}")

    def _resolve_tool_parent(self) -> FNode | None:
        choice = self._tool_parent_var.get()
        if choice == "Selected Element" and self.selected is not None:
            return self.selected
        if choice == "Active Tab Page":
            return self._get_active_page_node()
        return self.named.get(choice) or self._get_active_page_node()

    def _add_new_element(self):
        elem_type = self._tool_type_var.get().strip() or "Frame"
        if elem_type not in ELEMENT_DEFAULTS:
            messagebox.showerror("Invalid type", f"Unsupported element type '{elem_type}'.")
            return
        parent = self._resolve_tool_parent()
        if parent is None:
            messagebox.showerror("No parent", "Could not determine a parent for the new element.")
            return
        if parent._synthetic:
            messagebox.showerror("Invalid parent", "Synthetic Lua-created nodes cannot be used as XML parents.")
            return
        name = self._tool_name_var.get().strip() or self._suggest_element_name(elem_type)
        if name in self.named:
            messagebox.showerror("Duplicate name", f"An element named '{name}' already exists.")
            return

        self._push_undo_state()
        defaults = ELEMENT_DEFAULTS[elem_type]
        node = FNode()
        node.name = name
        node.ftype = elem_type
        node.w = defaults["w"]
        node.h = defaults["h"]
        node.text = self._tool_text_var.get() if elem_type in {"Button", "FontString"} else ""
        node.on_layer = "OVERLAY" if elem_type == "FontString" else ""
        node.a_pt = "TOPLEFT"
        node.a_relto = parent.name
        node.a_relpt = "TOPLEFT"
        node.a_ox = 12.0
        node.a_oy = -12.0

        parent.children.append(node)
        self.named[node.name] = node
        self._refresh_relto_options()
        self._refresh_tool_parent_options()
        self._relayout()
        self._render()
        self.selected = node
        self._draw_selection(node)
        self._show_props(node)
        self._tool_name_var.set(self._suggest_element_name(elem_type))
        self._tool_info_var.set(f"Added {elem_type} '{name}' under {parent.name or self.active_tab}. Save XML to persist it.")

    def _format_scripts(self, node: FNode) -> str:
        if node._synthetic:
            return "-- Synthetic node from LWCP_InitGearPage; edit LivingWorld.lua for behavior.\n"
        if not node._scripts:
            return ""
        parts = []
        for evt, code in node._scripts.items():
            parts.append(f"[{evt}]\n{code}".rstrip())
        return "\n\n".join(parts) + "\n"

    def _parse_script_blocks(self, text: str) -> dict[str, str]:
        scripts: dict[str, str] = {}
        matches = list(SCRIPT_BLOCK_RE.finditer(text))
        if not matches:
            if text.strip():
                raise ValueError("Scripts need section headers like [OnClick] or [OnShow].")
            return scripts
        for idx, match in enumerate(matches):
            name = match.group("name").strip()
            start = match.end()
            end = matches[idx + 1].start() if idx + 1 < len(matches) else len(text)
            block = text[start:end].strip("\n")
            scripts[name] = block.strip()
        return scripts

    def _save_script_changes(self):
        if not self.selected:
            messagebox.showwarning("No selection", "Select a frame before saving scripts.")
            return
        if self.selected._synthetic:
            messagebox.showinfo("Lua-managed", "Synthetic gear-slot nodes are created in LivingWorld.lua.")
            return
        try:
            self.selected._scripts = self._parse_script_blocks(self._script_text.get("1.0", "end-1c"))
            save_xml(self.nodes, self.named, XML_FILE)
            self._show_props(self.selected)
            messagebox.showinfo("Saved", f"Updated script blocks in:\n{XML_FILE}")
        except Exception as e:
            messagebox.showerror("Script save error", str(e))

    def _persist_tabs(self):
        if not self.tabs:
            raise ValueError("At least one tab is required.")
        sync_tabs_in_xml(XML_FILE, self.tabs)
        save_tabs_to_lua(LUA_FILE, self.tabs)
        self._load()

    def _add_tab(self):
        new_tab = simpledialog.askstring("Add Tab", "New tab name (letters/numbers/underscore):", parent=self.root)
        if not new_tab:
            return
        new_tab = new_tab.strip()
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", new_tab):
            messagebox.showerror("Invalid tab name", "Use letters, numbers, and underscores only, starting with a letter.")
            return
        if new_tab in self.tabs:
            messagebox.showwarning("Exists", f"Tab '{new_tab}' already exists.")
            return
        self.tabs.append(new_tab)
        self.active_tab = new_tab
        self._persist_tabs()

    def _delete_tab(self):
        if len(self.tabs) <= 1:
            messagebox.showwarning("Cannot delete", "The addon needs at least one tab.")
            return
        tab = self.active_tab
        if not messagebox.askyesno("Delete Tab", f"Delete tab '{tab}' and its XML page?"):
            return
        self.tabs = [t for t in self.tabs if t != tab]
        self.active_tab = self.tabs[0]
        self._persist_tabs()

    def _build_canvas(self, parent):
        self.canvas = tk.Canvas(parent, bg="#111827", highlightthickness=0,
                                cursor="crosshair")
        vsb = ttk.Scrollbar(parent, orient="vertical",   command=self.canvas.yview)
        hsb = ttk.Scrollbar(parent, orient="horizontal", command=self.canvas.xview)
        self.canvas.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)
        vsb.pack(side="right", fill="y")
        hsb.pack(side="bottom", fill="x")
        self.canvas.pack(fill="both", expand=True)
        self.canvas.configure(takefocus=1)
        self.canvas.bind("<ButtonPress-1>", self._on_press)
        self.canvas.bind("<B1-Motion>",  self._on_drag)
        self.canvas.bind("<ButtonRelease-1>", self._on_release)
        self.canvas.bind("<ButtonPress-2>", self._on_pan_start)
        self.canvas.bind("<B2-Motion>", self._on_pan_drag)
        self.canvas.bind("<ButtonRelease-2>", self._on_pan_end)
        self.canvas.bind("<Left>", lambda e: self._nudge_selected(-1, 0))
        self.canvas.bind("<Right>", lambda e: self._nudge_selected(1, 0))
        self.canvas.bind("<Up>", lambda e: self._nudge_selected(0, 1))
        self.canvas.bind("<Down>", lambda e: self._nudge_selected(0, -1))
        self.canvas.bind("<Delete>", self._on_delete_selected)
        self.canvas.bind("<BackSpace>", self._on_delete_selected)
        self.canvas.bind("<Motion>",     self._on_motion)
        self.canvas.bind("<MouseWheel>", self._on_scroll)

    def _schedule_splitter_init(self):
        self.root.after(0, self._init_splitter)
        self.root.after(50, self._init_splitter)
        self.root.after(150, self._init_splitter)

    def _init_splitter(self):
        try:
            self.root.update_idletasks()
            total_w = self.body.winfo_width()
            if total_w <= 3:
                total_w = self.root.winfo_width()
            total_w = max(total_w, CODE_W + CANVAS_W + PANEL_W)
            third = total_w // 3
            self.body.sashpos(0, third)
            self.body.sashpos(1, third * 2)
            self._splitter_initialized = True
        except tk.TclError:
            pass

    def _build_props(self, parent):
        split = ttk.Panedwindow(parent, orient="vertical")
        split.pack(fill="both", expand=True)
        self._prop_split = split

        toolbox = tk.Frame(split, bg="#17212f", height=280)
        props = tk.Frame(split, bg="#1f2937")
        toolbox.pack_propagate(False)
        props.pack_propagate(False)
        split.add(toolbox, weight=1)
        split.add(props, weight=1)
        self.root.after_idle(self._schedule_prop_splitter_init)

        self._build_toolbox(toolbox)

        tk.Label(props, text="Properties", bg="#1f2937", fg="#f59e0b",
                 font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=12, pady=(12, 4))
        ttk.Separator(props, orient="horizontal").pack(fill="x", padx=8)

        scf = tk.Frame(props, bg="#1f2937")
        scf.pack(fill="both", expand=True, padx=10, pady=8)

        self._pv: dict[str, tk.StringVar] = {}
        self._field_widgets: dict[str, tk.Widget] = {}
        FIELDS = [
            ("name",    "Name",        "label"),
            ("ftype",   "Type",        "label"),
            ("parent",  "Parent",      "label"),
            ("on_layer","On Layer",    "combo_layer"),
            ("w",       "Width",       "entry"),
            ("h",       "Height",      "entry"),
            ("text",    "Text",        "entry"),
            ("hidden",  "Hidden",      "combo_readonly"),
            ("a_pt",    "Anchor",      "combo_readonly"),
            ("a_relto", "Relative To", "combo"),
            ("a_relpt", "Rel Point",   "combo_readonly"),
            ("a_ox",    "Offset X",    "entry"),
            ("a_oy",    "Offset Y",    "entry"),
        ]
        for key, label, field_kind in FIELDS:
            row = tk.Frame(scf, bg="#1f2937")
            row.pack(fill="x", pady=2)
            tk.Label(row, text=label, bg="#1f2937", fg="#9ca3af",
                     font=("Segoe UI", 8), width=11, anchor="w").pack(side="left")
            var = tk.StringVar()
            self._pv[key] = var
            if field_kind == "entry":
                widget = tk.Entry(row, textvariable=var, bg="#374151", fg="white",
                                  relief="flat", insertbackground="white",
                                  font=("Segoe UI", 8))
                widget.pack(side="left", fill="x", expand=True, ipady=2)
            elif field_kind == "combo_readonly":
                widget = ttk.Combobox(row, textvariable=var, state="readonly", font=("Segoe UI", 8))
                widget.pack(side="left", fill="x", expand=True)
                if key == "hidden":
                    widget.configure(values=("false", "true"))
                else:
                    widget.configure(values=ANCHOR_POINTS)
            elif field_kind == "combo":
                widget = ttk.Combobox(row, textvariable=var, state="normal", font=("Segoe UI", 8))
                widget.pack(side="left", fill="x", expand=True)
            elif field_kind == "combo_layer":
                widget = ttk.Combobox(row, textvariable=var, state="readonly", font=("Segoe UI", 8))
                widget.pack(side="left", fill="x", expand=True)
                widget.configure(values=("",) + LAYER_LEVELS)
            else:
                widget = tk.Label(row, textvariable=var, bg="#1f2937", fg="#d1d5db",
                                  font=("Segoe UI", 8), anchor="w")
                widget.pack(side="left", fill="x", expand=True)
            self._field_widgets[key] = widget

        ttk.Separator(scf, orient="horizontal").pack(fill="x", pady=8)

        tk.Button(scf, text="Apply Changes", command=self._apply,
                  bg="#d97706", fg="white", relief="flat", padx=10, pady=6,
                  cursor="hand2", font=("Segoe UI", 9, "bold"),
                  activebackground="#b45309").pack(fill="x")

        # Info / canvas coords
        self._info_var = tk.StringVar(value="Click a frame to select it.")
        tk.Label(scf, textvariable=self._info_var, bg="#1f2937", fg="#6b7280",
                 font=("Segoe UI", 7), wraplength=PANEL_W - 24,
                 justify="left", anchor="nw").pack(fill="x", pady=(8, 0))

    def _schedule_prop_splitter_init(self):
        self.root.after(0, self._init_prop_splitter)
        self.root.after(50, self._init_prop_splitter)
        self.root.after(150, self._init_prop_splitter)

    def _init_prop_splitter(self):
        try:
            self.root.update_idletasks()
            total_h = self._prop_split.winfo_height()
            if total_h <= 3:
                total_h = self.root.winfo_height()
            total_h = max(total_h, 420)
            self._prop_split.sashpos(0, total_h // 2)
        except tk.TclError:
            pass

    def _build_toolbox(self, parent):
        tk.Label(parent, text="Toolbox", bg="#17212f", fg="#f59e0b",
                 font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=12, pady=(12, 4))
        ttk.Separator(parent, orient="horizontal").pack(fill="x", padx=8)

        inner = tk.Frame(parent, bg="#17212f")
        inner.pack(fill="both", expand=True, padx=10, pady=8)

        self._tool_name_var = tk.StringVar()
        self._tool_type_var = tk.StringVar(value="Button")
        self._tool_text_var = tk.StringVar(value="New Button")
        self._tool_parent_var = tk.StringVar(value="Active Tab Page")
        self._tool_info_var = tk.StringVar(value="Creates elements under the selected frame or active tab page.")

        for label, var in [("Name", self._tool_name_var), ("Text", self._tool_text_var)]:
            row = tk.Frame(inner, bg="#17212f")
            row.pack(fill="x", pady=2)
            tk.Label(row, text=label, bg="#17212f", fg="#9ca3af",
                     font=("Segoe UI", 8), width=11, anchor="w").pack(side="left")
            tk.Entry(row, textvariable=var, bg="#374151", fg="white",
                     relief="flat", insertbackground="white",
                     font=("Segoe UI", 8)).pack(side="left", fill="x", expand=True, ipady=2)

        type_row = tk.Frame(inner, bg="#17212f")
        type_row.pack(fill="x", pady=2)
        tk.Label(type_row, text="Type", bg="#17212f", fg="#9ca3af",
                 font=("Segoe UI", 8), width=11, anchor="w").pack(side="left")
        self._tool_type_combo = ttk.Combobox(type_row, textvariable=self._tool_type_var,
                                             state="readonly", font=("Segoe UI", 8),
                                             values=tuple(ELEMENT_DEFAULTS.keys()))
        self._tool_type_combo.pack(side="left", fill="x", expand=True)
        self._tool_type_combo.bind("<<ComboboxSelected>>", self._on_tool_type_changed)

        parent_row = tk.Frame(inner, bg="#17212f")
        parent_row.pack(fill="x", pady=2)
        tk.Label(parent_row, text="Parent", bg="#17212f", fg="#9ca3af",
                 font=("Segoe UI", 8), width=11, anchor="w").pack(side="left")
        self._tool_parent_combo = ttk.Combobox(parent_row, textvariable=self._tool_parent_var,
                                               state="readonly", font=("Segoe UI", 8))
        self._tool_parent_combo.pack(side="left", fill="x", expand=True)

        quick = tk.Frame(inner, bg="#17212f")
        quick.pack(fill="x", pady=(10, 6))
        for elem_type in ("Button", "Frame", "FontString", "EditBox", "CheckButton", "Slider"):
            tk.Button(quick, text=elem_type, command=lambda t=elem_type: self._quick_add_element(t),
                      bg="#374151", fg="white", relief="flat", padx=8, pady=4,
                      cursor="hand2", font=("Segoe UI", 8),
                      activebackground="#4b5563", activeforeground="white").pack(side="left", padx=2)

        tk.Button(inner, text="Add New Element", command=self._add_new_element,
                  bg="#d97706", fg="white", relief="flat", padx=10, pady=6,
                  cursor="hand2", font=("Segoe UI", 9, "bold"),
                  activebackground="#b45309").pack(fill="x", pady=(4, 0))

        tk.Label(inner, textvariable=self._tool_info_var, bg="#17212f", fg="#6b7280",
                 font=("Segoe UI", 7), wraplength=PANEL_W - 24,
                 justify="left", anchor="nw").pack(fill="x", pady=(8, 0))

    # ── Load & layout ──────────────────────────────────────────────────────────
    def _load(self):
        global SCALE
        if not os.path.exists(XML_FILE):
            messagebox.showerror("Error", f"XML not found:\n{XML_FILE}")
            return
        try:
            FNode._ctr = 0
            self.nodes = parse_xml(XML_FILE)
        except Exception as e:
            messagebox.showerror("Parse error", str(e))
            return

        self.tabs = load_tabs_from_lua(LUA_FILE)
        if self.active_tab not in self.tabs:
            self.active_tab = self.tabs[0]
        self._refresh_toolbar_tabs()
        self._load_lua_text()
        self._undo_stack.clear()
        self._deleted_nodes.clear()
        self._refresh_undo_button()
        self.selected = None
        self._clear_props()

        self.named = {}
        for root_n in self.nodes:
            for n in root_n.all_descendants():
                if n.name:
                    self.named[n.name] = n

        self._deleted_nodes.extend(prune_legacy_gear_page_nodes(self.named))

        gear_layouts = load_gear_slot_layouts(LUA_FILE)
        bags_tab_layouts = load_bags_tab_layouts(LUA_FILE)
        inject_gear_slots(self.named, gear_layouts)
        inject_bags_tab_controls(self.named, bags_tab_layouts)
        self._refresh_tool_parent_options()
        self._on_tool_type_changed()
        self._relayout()
        self._render()
        count = sum(1 for n in self.named if not self.named[n]._synthetic)
        synth = sum(1 for n in self.named if self.named[n]._synthetic)
        self._info_var.set(f"Loaded {count} XML frames + {synth} dynamic gear slots.")

    def _relayout(self):
        global SCALE
        SCALE = self._scale_var.get()
        resolve_all(self.nodes, self.named)

    # ── Render ─────────────────────────────────────────────────────────────────
    def _render(self):
        self.canvas.delete("all")
        self._cid_node.clear()
        preview = self._preview_mode.get()

        cw = max(CANVAS_W, int(CANVAS_W * (SCALE / 1.5)))
        ch = max(CANVAS_H, int(CANVAS_H * (SCALE / 1.5)))
        self.canvas.configure(scrollregion=(0, 0, cw + 20, ch + 20))

        # Background
        self.canvas.create_rectangle(0, 0, cw + 20, ch + 20,
                                     fill=("#0b0c10" if preview else "#111827"), outline="")

        if not preview:
            # Reference grid lines (light)
            for x in range(0, cw, 50):
                self.canvas.create_line(x, 0, x, ch, fill="#1e2736", width=1)
            for y in range(0, ch, 50):
                self.canvas.create_line(0, y, cw, y, fill="#1e2736", width=1)

            # Minimap placeholder
            mx, my, mw, mh = _ref_rect("Minimap", self.named, UIPARENT)
            self.canvas.create_oval(mx, my, mx + mw * SCALE, my + mh * SCALE,
                                    fill="#1a2a1a", outline="#2a4a2a")
            self.canvas.create_text(mx + mw * SCALE / 2, my + mh * SCALE / 2,
                                    text="Mini\nmap", fill="#2a4a2a",
                                    font=("Segoe UI", 7))

        # Hidden tab pages for current tab selection
        hidden_pages = {f"LWCPPage{t}" for t in self.tabs if t != self.active_tab}

        for n in self.nodes:
            self._render_node(n, hidden_pages)

        # Restore selection highlight
        if self.selected:
            self._draw_selection(self.selected)

    def _render_node(self, node: FNode, hidden_pages: set):
        # Skip minimap button container (positioned on actual minimap)
        if node.name == "LWCPButtonFrame":
            return
        active_page_name = f"LWCPPage{self.active_tab}"
        # Tab visibility
        if node.name in hidden_pages:
            return
        # Hidden frames (except the main panel which is "hidden" at startup)
        if node.hidden and node.name not in {"LWCPFrame", active_page_name}:
            # Still render retrieve prompt dimmed so it's discoverable
            if node.name == "LWCPRetrievePrompt":
                self._draw_frame(node, alpha_hint=True)
                for ch in node.children:
                    self._render_node(ch, hidden_pages)
            return

        self._draw_frame(node)
        for ch in self._iter_render_children(node):
            self._render_node(ch, hidden_pages)

    def _iter_render_children(self, node: FNode):
        def sort_key(item):
            layer_rank = LAYER_RANK.get((item.on_layer or "").upper(), -1 if item.ftype == "Frame" else 2)
            return (layer_rank, node.children.index(item))
        return sorted(node.children, key=sort_key)

    def _draw_frame(self, node: FNode, alpha_hint=False):
        l, t, r, b = node.cx_l, node.cx_t, node.cx_r, node.cx_b
        preview = self._preview_mode.get()
        fill   = (_PREVIEW_FILL if preview else _FILL).get(node.ftype, "#1f2937")
        border = (_PREVIEW_BORDER if preview else _BORDER).get(node.ftype, "#4b5563")
        lw = 1

        if alpha_hint:
            fill = "#0d0d0d"
            border = "#2a2a2a"

        if preview and node.name == "LWCPFrame":
            fill = "#121013"
            border = "#9f8452"
            lw = 2
        elif preview and node.ftype == "Button":
            lw = 2
        elif preview and node.ftype == "EditBox":
            lw = 2

        cid = self.canvas.create_rectangle(l, t, r, b, fill=fill,
                                           outline=border, width=lw,
                                           tags=("node", f"n{node.uid}"))
        self._cid_node[cid] = node

        fw, fh = r - l, b - t
        label = (node.text or node.name or node.ftype)
        if len(label) > 20:
            label = label[:18] + ".."
        if fh >= 9 and fw >= 16:
            fs = max(6, min((10 if preview else 9), int(fh * 0.5)))
            text_fill = (_PREVIEW_TEXT if preview else {}).get(node.ftype, "#e5e7eb")
            if alpha_hint:
                text_fill = "#444444"
            tid = self.canvas.create_text(
                l + fw / 2, t + fh / 2, text=label,
                fill=text_fill,
                font=("Segoe UI", fs, "bold" if preview and node.ftype == "FontString" else "normal"),
                width=fw - 4,
                tags=("node", f"n{node.uid}"))
            self._cid_node[tid] = node

    def _draw_selection(self, node: FNode):
        l, t, r, b = node.cx_l, node.cx_t, node.cx_r, node.cx_b
        self._resize_handles.clear()
        self.canvas.create_rectangle(l, t, r, b, fill="", outline=_HLSEL,
                                     width=2, tags="selection")
        handles = {
            "nw": (l, t),
            "n": ((l + r) / 2, t),
            "ne": (r, t),
            "e": (r, (t + b) / 2),
            "se": (r, b),
            "s": ((l + r) / 2, b),
            "sw": (l, b),
            "w": (l, (t + b) / 2),
        }
        hs = HANDLE_SIZE / 2
        for name, (hx, hy) in handles.items():
            cid = self.canvas.create_rectangle(
                hx - hs, hy - hs, hx + hs, hy + hs,
                fill=_HANDLE_FILL, outline="#111827", width=1,
                tags=("selection", "resize_handle", name)
            )
            self._resize_handles[cid] = name

    # ── Interaction ────────────────────────────────────────────────────────────
    def _pick_node_at(self, cx, cy):
        items = self.canvas.find_overlapping(cx - 1, cy - 1, cx + 1, cy + 1)
        best, best_area = None, float("inf")
        for item in reversed(items):
            node = self._cid_node.get(item)
            if node:
                area = (node.cx_r - node.cx_l) * (node.cx_b - node.cx_t)
                if area < best_area:
                    best, best_area = node, area
        return best

    def _pick_resize_handle_at(self, cx, cy):
        items = self.canvas.find_overlapping(cx - 1, cy - 1, cx + 1, cy + 1)
        for item in reversed(items):
            handle = self._resize_handles.get(item)
            if handle:
                return handle
        return None

    def _handle_cursor(self, handle: str | None) -> str:
        return {
            "n": "sb_v_double_arrow",
            "s": "sb_v_double_arrow",
            "e": "sb_h_double_arrow",
            "w": "sb_h_double_arrow",
            "nw": "size_nw_se",
            "se": "size_nw_se",
            "ne": "size_ne_sw",
            "sw": "size_ne_sw",
        }.get(handle or "", "crosshair")

    def _current_ref_rect(self, node: FNode):
        parent = _find_parent_node(self.nodes, node)
        par_rect = UIPARENT
        if parent is not None:
            par_rect = (parent.cx_l, parent.cx_t, parent.w * SCALE, parent.h * SCALE)
        return _ref_rect(node.a_relto, self.named, par_rect)

    def _normalize_node_to_topleft(self, node: FNode):
        rl, rt, _rw, _rh = self._current_ref_rect(node)
        left = (node.cx_l - rl) / SCALE
        top_down = (node.cx_t - rt) / SCALE
        node.a_pt = "TOPLEFT"
        node.a_relpt = "TOPLEFT"
        node.a_ox = left
        node.a_oy = -top_down
        return left, top_down

    def _on_press(self, event):
        self.canvas.focus_set()
        cx, cy = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
        handle = self._pick_resize_handle_at(cx, cy)
        if handle and self.selected:
            self._push_undo_state()
            left, top_down = self._normalize_node_to_topleft(self.selected)
            self._resize_active = True
            self._resize_handle = handle
            self._resize_orig = {
                "left": left,
                "top": top_down,
                "w": self.selected.w,
                "h": self.selected.h,
                "node": self.selected,
            }
            self._press_cx = cx
            self._press_cy = cy
            self.canvas.configure(cursor=self._handle_cursor(handle))
            return
        best = self._pick_node_at(cx, cy)

        self.canvas.delete("selection")
        if best:
            self.selected = best
            self._draw_selection(best)
            self._show_props(best)
            self._press_cx = cx
            self._press_cy = cy
            self._press_node = best
            self._drag_active = False
            self._drag_lock_axis = None
            self._drag_orig_ox = best.a_ox
            self._drag_orig_oy = best.a_oy
        else:
            self.selected = None
            self._press_node = None
            self._clear_props()

    def _on_drag(self, event):
        if not self._press_node:
            if not self._resize_active:
                return

        cx, cy = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
        dx = cx - self._press_cx
        dy = cy - self._press_cy
        if self._resize_active and self.selected:
            orig = self._resize_orig
            left = orig["left"]
            top_down = orig["top"]
            width = orig["w"]
            height = orig["h"]
            right = orig["left"] + orig["w"]
            bottom = orig["top"] + orig["h"]
            dw = dx / SCALE
            dh = dy / SCALE
            handle = self._resize_handle or ""
            min_size = 4.0
            shift_held = bool(event.state & 0x0001)
            if shift_held:
                prop_w = orig["w"]
                prop_h = orig["h"]
                if "w" in handle:
                    prop_w = orig["w"] - dw
                if "e" in handle:
                    prop_w = orig["w"] + dw
                if "n" in handle:
                    prop_h = orig["h"] - dh
                if "s" in handle:
                    prop_h = orig["h"] + dh

                scale_x = prop_w / orig["w"] if orig["w"] else 1.0
                scale_y = prop_h / orig["h"] if orig["h"] else 1.0
                if handle in {"n", "s"}:
                    scale = scale_y
                elif handle in {"e", "w"}:
                    scale = scale_x
                else:
                    scale = scale_x if abs(scale_x - 1.0) >= abs(scale_y - 1.0) else scale_y
                scale = max(scale, min_size / max(orig["w"], orig["h"], min_size))
                width = max(min_size, orig["w"] * scale)
                height = max(min_size, orig["h"] * scale)

                if handle == "nw":
                    left = right - width
                    top_down = bottom - height
                elif handle == "ne":
                    left = orig["left"]
                    top_down = bottom - height
                elif handle == "sw":
                    left = right - width
                    top_down = orig["top"]
                elif handle == "se":
                    left = orig["left"]
                    top_down = orig["top"]
                elif handle == "n":
                    left = orig["left"] - (width - orig["w"]) / 2.0
                    top_down = bottom - height
                elif handle == "s":
                    left = orig["left"] - (width - orig["w"]) / 2.0
                    top_down = orig["top"]
                elif handle == "w":
                    left = right - width
                    top_down = orig["top"] - (height - orig["h"]) / 2.0
                elif handle == "e":
                    left = orig["left"]
                    top_down = orig["top"] - (height - orig["h"]) / 2.0
            else:
                if "w" in handle:
                    left = orig["left"] + dw
                    width = orig["w"] - dw
                if "e" in handle:
                    width = orig["w"] + dw
                if "n" in handle:
                    top_down = orig["top"] + dh
                    height = orig["h"] - dh
                if "s" in handle:
                    height = orig["h"] + dh

                if "w" in handle and width < min_size:
                    left = orig["left"] + (orig["w"] - min_size)
                width = max(min_size, width)
                if "n" in handle and height < min_size:
                    top_down = orig["top"] + (orig["h"] - min_size)
                height = max(min_size, height)
            self.selected.w = self._snap_coord(width)
            self.selected.h = self._snap_coord(height)
            self.selected.a_ox = self._snap_coord(left)
            self.selected.a_oy = -self._snap_coord(top_down)
            self._update_selected_node()
            return
        if not self._drag_active and abs(dx) < 2 and abs(dy) < 2:
            return

        if not self._drag_active:
            self._push_undo_state()
        self._drag_active = True
        ctrl_held = bool(event.state & 0x0004)
        if ctrl_held:
            if self._drag_lock_axis is None:
                self._drag_lock_axis = "x" if abs(dx) >= abs(dy) else "y"
            if self._drag_lock_axis == "x":
                dy = 0
            else:
                dx = 0
        else:
            self._drag_lock_axis = None
        node = self._press_node
        node.a_ox = self._snap_coord(self._drag_orig_ox + (dx / SCALE))
        node.a_oy = self._snap_coord(self._drag_orig_oy - (dy / SCALE))
        self.selected = node
        self._update_selected_node()

    def _on_release(self, _event):
        self._press_node = None
        self._drag_active = False
        self._drag_lock_axis = None
        self._resize_active = False
        self._resize_handle = None
        self._resize_orig = {}
        self.canvas.configure(cursor="crosshair")

    def _on_pan_start(self, event):
        self._pan_active = True
        self.canvas.configure(cursor="fleur")
        self.canvas.scan_mark(event.x, event.y)

    def _on_pan_drag(self, event):
        if not self._pan_active:
            return
        self.canvas.scan_dragto(event.x, event.y, gain=1)

    def _on_pan_end(self, _event):
        self._pan_active = False
        self.canvas.configure(cursor="crosshair")

    def _on_motion(self, event):
        if self._pan_active:
            return
        cx, cy = self.canvas.canvasx(event.x), self.canvas.canvasy(event.y)
        handle = self._pick_resize_handle_at(cx, cy)
        if handle and not self._drag_active and not self._resize_active:
            self.canvas.configure(cursor=self._handle_cursor(handle))
        elif not self._pan_active and not self._resize_active:
            self.canvas.configure(cursor="crosshair")
        self.canvas.delete("hover")
        if self._resize_active or (self._press_node and self._drag_active):
            return
        node = self._pick_node_at(cx, cy)
        if node and node is not self.selected:
            l, t, r, b = node.cx_l, node.cx_t, node.cx_r, node.cx_b
            self.canvas.create_rectangle(l, t, r, b, fill="", outline=_HLHOV,
                                         width=1, tags="hover")

    def _on_scroll(self, event):
        delta = -1 if event.delta > 0 else 1
        self.canvas.yview_scroll(delta, "units")

    def _on_zoom(self, _=None):
        self._relayout()
        self._render()

    # ── Properties panel ───────────────────────────────────────────────────────
    def _show_props(self, node: FNode):
        parent = _find_parent_node(self.nodes, node)
        self._pv["name"].set(node.name or "(unnamed)")
        self._pv["ftype"].set(node.ftype)
        self._pv["parent"].set(parent.name if parent and parent.name else ("(root)" if parent else "UI root"))
        self._pv["on_layer"].set((node.on_layer or "").upper())
        self._pv["w"].set(str(node.w))
        self._pv["h"].set(str(node.h))
        self._pv["text"].set(node.text)
        self._refresh_relto_options()
        self._pv["hidden"].set("true" if node.hidden else "false")
        self._pv["a_pt"].set(node.a_pt)
        self._pv["a_relto"].set(node.a_relto)
        self._pv["a_relpt"].set(node.a_relpt or node.a_pt)
        self._pv["a_ox"].set(str(node.a_ox))
        self._pv["a_oy"].set(str(node.a_oy))

        self._script_text.delete("1.0", "end")
        self._script_text.insert("1.0", self._format_scripts(node))
        self._code_info_var.set(f"{node.name or '(unnamed)'}  [{node.ftype}]")
        self._refresh_tool_parent_options()
        self._tool_parent_var.set("Selected Element")

        self._info_var.set(
            f"Canvas: ({node.cx_l:.0f}, {node.cx_t:.0f})  "
            f"{node.cx_r - node.cx_l:.0f}×{node.cx_b - node.cx_t:.0f}px\n"
            f"WoW: ({node.a_ox:.0f}, {node.a_oy:.0f})  "
            f"{node.w:.0f}×{node.h:.0f} wow-px\n"
            f"Children: {len(node.children)}"
            + ("  [synthetic]" if node._synthetic else "")
        )

    def _clear_props(self):
        for v in self._pv.values():
            v.set("")
        self._script_text.delete("1.0", "end")
        self._code_info_var.set("Select a frame to inspect its scripts.")
        self._info_var.set("Click a frame to select it.")
        self._refresh_tool_parent_options()

    def _get_snap_increment(self) -> float:
        try:
            value = float(self._snap_value.get() or "1")
            return value if value > 0 else 1.0
        except ValueError:
            return 1.0

    def _snap_coord(self, value: float) -> float:
        if not self._snap_enabled.get():
            return value
        inc = self._get_snap_increment()
        return round(value / inc) * inc

    def _update_selected_node(self):
        if not self.selected:
            return
        self._relayout()
        self._render()
        self._draw_selection(self.selected)
        self._show_props(self.selected)

    def _nudge_selected(self, dx: int, dy: int):
        if not self.selected:
            return "break"
        self._push_undo_state()
        step = self._get_snap_increment() if self._snap_enabled.get() else 1.0
        self.selected.a_ox += dx * step
        self.selected.a_oy += dy * step
        self.selected.a_ox = self._snap_coord(self.selected.a_ox)
        self.selected.a_oy = self._snap_coord(self.selected.a_oy)
        self._update_selected_node()
        return "break"

    def _remove_named_descendants(self, node: FNode):
        for child in node.all_descendants():
            if child.name and self.named.get(child.name) is child:
                del self.named[child.name]

    def _on_delete_selected(self, _event=None):
        if not self.selected:
            return "break"
        node = self.selected
        if node._synthetic:
            messagebox.showinfo("Lua-managed", "Synthetic Lua-created nodes cannot be deleted from XML.")
            return "break"
        parent = _find_parent_node(self.nodes, node)
        if parent is None:
            messagebox.showwarning("Cannot delete", "Root elements cannot be deleted from the editor.")
            return "break"

        self._push_undo_state()
        self._deleted_nodes.append(_node_identity(node))
        if node in parent.children:
            parent.children.remove(node)
        self._remove_named_descendants(node)
        self.selected = None
        self._refresh_relto_options()
        self._refresh_tool_parent_options()
        self._render()
        self._clear_props()
        self._info_var.set("Element deleted. Save XML to persist removal.")
        return "break"

    def _center_selected_to_parent(self, axis: str):
        if not self.selected:
            return
        parent = _find_parent_node(self.nodes, self.selected)
        if parent is None or not parent.name:
            messagebox.showwarning("No parent", "Select an element with a named parent frame first.")
            return

        self._push_undo_state()
        current_x = (self.selected.cx_l - parent.cx_l) / SCALE
        current_y_down = (self.selected.cx_t - parent.cx_t) / SCALE
        target_x = (parent.w - self.selected.w) / 2.0 if axis == "x" else current_x
        target_y_down = (parent.h - self.selected.h) / 2.0 if axis == "y" else current_y_down

        self.selected.a_pt = "TOPLEFT"
        self.selected.a_relto = parent.name
        self.selected.a_relpt = "TOPLEFT"
        self.selected.a_ox = self._snap_coord(target_x)
        self.selected.a_oy = self._snap_coord(-target_y_down)
        self._update_selected_node()

    def _apply(self):
        if not self.selected:
            return
        n = self.selected
        try:
            new_w = float(self._pv["w"].get())
            new_h = float(self._pv["h"].get())
            new_text = self._pv["text"].get()
            new_hidden = self._pv["hidden"].get().strip().lower() == "true"
            new_on_layer = self._pv["on_layer"].get().strip().upper()
            new_a_pt = self._pv["a_pt"].get().strip().upper()
            new_a_relto = self._pv["a_relto"].get().strip()
            new_a_relpt = self._pv["a_relpt"].get().strip().upper()
            new_a_ox = float(self._pv["a_ox"].get() or 0)
            new_a_oy = float(self._pv["a_oy"].get() or 0)
            if new_a_pt not in ANCHOR_POINTS or (new_a_relpt and new_a_relpt not in ANCHOR_POINTS):
                raise ValueError("Anchor values must be chosen from the valid point list.")
            if new_on_layer and new_on_layer not in LAYER_LEVELS:
                raise ValueError("On Layer must be one of the supported layer levels.")
        except ValueError as e:
            messagebox.showerror("Invalid value", str(e))
            return

        self._push_undo_state()
        try:
            n.w = new_w
            n.h = new_h
            n.text = new_text
            n.hidden = new_hidden
            n.on_layer = new_on_layer
            n.a_pt = new_a_pt
            n.a_relto = new_a_relto
            n.a_relpt = new_a_relpt
            n.a_ox = new_a_ox
            n.a_oy = new_a_oy
        except ValueError as e:
            messagebox.showerror("Invalid value", str(e))
            return

        self._relayout()
        self._render()
        if self.selected:
            self._draw_selection(self.selected)
            self._show_props(self.selected)

    # ── Tab switching ──────────────────────────────────────────────────────────
    def _switch_tab(self, tab: str):
        self.active_tab = tab
        self._update_tab_btns()
        self._render()

    def _update_tab_btns(self):
        for tab, btn in self._tab_btns.items():
            if tab == self.active_tab:
                btn.configure(bg="#d97706")
            else:
                btn.configure(bg="#374151")

    # ── Save ───────────────────────────────────────────────────────────────────
    def _save(self):
        if not self.named:
            messagebox.showwarning("Nothing loaded", "Load XML first.")
            return
        try:
            save_xml(self.nodes, self.named, XML_FILE, self._deleted_nodes)
            self._deleted_nodes.clear()
            save_tabs_to_lua(LUA_FILE, self.tabs)
            save_gear_slot_layouts(LUA_FILE, self.named)
            save_bags_tab_layouts(LUA_FILE, self.named)
            messagebox.showinfo("Saved", f"Written to:\n{XML_FILE}\n{LUA_FILE}")
        except Exception as e:
            messagebox.showerror("Save error", str(e))


# ── Entry point ────────────────────────────────────────────────────────────────
def main():
    root = tk.Tk()
    root.geometry(f"{CODE_W + CANVAS_W + PANEL_W + 60}x{CANVAS_H + 50}")
    LWEditor(root)
    root.mainloop()

if __name__ == "__main__":
    main()
