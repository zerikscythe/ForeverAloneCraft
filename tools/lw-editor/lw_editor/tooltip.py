"""
tooltip.py -- Reusable hover Tooltip widget.
"""
import tkinter as tk

# ═══════════════════════════════════════════════════════════════════════════

class Tooltip:
    """
    A lightweight hover tooltip for any Tkinter widget.

    Usage — static text:
        Tooltip(widget, text="Hello")

    Usage — dynamic text (evaluated at show-time):
        Tooltip(widget, text_fn=lambda: compute_something())
    """

    _PAD   = 6          # inner padding in pixels
    _DELAY = 500        # ms before tooltip appears

    def __init__(self, widget, *, text: str = "", text_fn=None):
        self._widget  = widget
        self._text    = text
        self._text_fn = text_fn
        self._win     = None
        self._after   = None
        widget.bind("<Enter>",   self._on_enter,  add="+")
        widget.bind("<Leave>",   self._on_leave,  add="+")
        widget.bind("<Destroy>", self._on_destroy, add="+")

    # ── public ──────────────────────────────────────────────────────────────

    def update_text(self, text: str):
        self._text = text
        if self._win:
            self._label.configure(text=text)

    # ── private ─────────────────────────────────────────────────────────────

    def _get_text(self) -> str:
        if self._text_fn:
            try:
                return self._text_fn() or ""
            except Exception:
                return ""
        return self._text

    def _on_enter(self, event):
        self._cancel()
        self._after = self._widget.after(self._DELAY, self._show)

    def _on_leave(self, event):
        self._cancel()
        self._hide()

    def _on_destroy(self, event):
        self._cancel()
        self._hide()

    def _cancel(self):
        if self._after is not None:
            try:
                self._widget.after_cancel(self._after)
            except Exception:
                pass
            self._after = None

    def _show(self):
        text = self._get_text()
        if not text or not self._widget.winfo_exists():
            return

        self._hide()

        x = self._widget.winfo_rootx() + self._widget.winfo_width() // 2
        y = self._widget.winfo_rooty() + self._widget.winfo_height() + 4

        self._win = tw = tk.Toplevel(self._widget)
        tw.wm_overrideredirect(True)
        tw.wm_geometry(f"+{x}+{y}")
        tw.attributes("-topmost", True)

        bg = "#fffde7"
        self._label = tk.Label(
            tw, text=text, justify="left",
            background=bg, relief="solid", borderwidth=1,
            font=("TkDefaultFont", 9),
            wraplength=340,
            padx=self._PAD, pady=self._PAD,
        )
        self._label.pack()

    def _hide(self):
        if self._win:
            try:
                self._win.destroy()
            except Exception:
                pass
            self._win = None


