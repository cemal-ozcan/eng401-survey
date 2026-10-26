"""Figures for the validation report (matplotlib, PNG)."""

from __future__ import annotations

from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

from .data import Survey, column  # noqa: E402


def _save(fig, path: Path) -> Path:
    fig.tight_layout()
    fig.savefig(path, dpi=130)
    plt.close(fig)
    return path


def track_vs_route(route_res: dict, path: Path) -> Path:
    s = route_res["_series"]
    e0, n0 = np.floor(s["ve"].min() / 100) * 100, np.floor(s["vn"].min() / 100) * 100
    fig, ax = plt.subplots(figsize=(7, 6))
    ax.plot(s["ve"] - e0, s["vn"] - n0, "k--", lw=2, label="planned route")
    sc = ax.scatter(s["e"] - e0, s["n"] - n0, c=s["d"], s=8, cmap="viridis", label="measured fixes")
    fig.colorbar(sc, ax=ax, label="cross-track distance (m)")
    ax.set_aspect("equal")
    ax.set_xlabel(f"Easting − {e0:.0f} (m)")
    ax.set_ylabel(f"Northing − {n0:.0f} (m)")
    ax.set_title("Measured trajectory vs planned route")
    ax.legend(loc="best")
    ax.grid(alpha=0.3)
    return _save(fig, path)


def gnss_errors(gnss_res: dict, path: Path) -> Path:
    s = gnss_res["_series"]
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 4.5))
    a1.scatter(s["de"], s["dn"], s=6, alpha=0.6)
    lim = np.nanmax(np.abs(np.r_[s["de"], s["dn"]])) * 1.1
    a1.set_xlim(-lim, lim)
    a1.set_ylim(-lim, lim)
    a1.set_aspect("equal")
    a1.axhline(0, color="k", lw=0.5)
    a1.axvline(0, color="k", lw=0.5)
    r = gnss_res["errors"]["cep95_m"]
    a1.add_patch(plt.Circle((0, 0), r, fill=False, ls="--", color="r", label=f"CEP95 = {r:.2f} m"))
    a1.set_xlabel("ΔE (m)")
    a1.set_ylabel("ΔN (m)")
    a1.set_title("Horizontal error scatter")
    a1.legend()
    a2.plot(s["seq"], np.hypot(s["de"], s["dn"]), lw=0.8)
    a2.set_xlabel("record (seq)")
    a2.set_ylabel("horizontal error (m)")
    a2.set_title("Error over time")
    for a in (a1, a2):
        a.grid(alpha=0.3)
    return _save(fig, path)


def histogram(values: np.ndarray, title: str, xlabel: str, path: Path, ref: float | None = None) -> Path:
    fig, ax = plt.subplots(figsize=(6.5, 4))
    ax.hist(values[np.isfinite(values)], bins=40, color="#1f77b4", alpha=0.8)
    if ref is not None:
        ax.axvline(ref, color="r", ls="--", label=f"requested {ref:g}")
        ax.legend()
    ax.set_title(title)
    ax.set_xlabel(xlabel)
    ax.set_ylabel("count")
    ax.grid(alpha=0.3)
    return _save(fig, path)


def thematic_preview(survey: Survey, attr: str, unit: str, path: Path) -> Path:
    """Quick look of a thematic map (the real map is made in QGIS)."""
    fx = survey.fixes()
    e, n, v = column(fx, "easting"), column(fx, "northing"), column(fx, attr)
    e0, n0 = np.floor(np.nanmin(e) / 100) * 100, np.floor(np.nanmin(n) / 100) * 100
    fig, ax = plt.subplots(figsize=(7, 6))
    sc = ax.scatter(e - e0, n - n0, c=v, s=12, cmap="coolwarm")
    fig.colorbar(sc, ax=ax, label=f"{attr} ({unit})")
    ax.set_aspect("equal")
    ax.set_xlabel(f"Easting − {e0:.0f} (m)")
    ax.set_ylabel(f"Northing − {n0:.0f} (m)")
    ax.set_title(f"Thematic preview: {attr}")
    ax.grid(alpha=0.3)
    return _save(fig, path)
