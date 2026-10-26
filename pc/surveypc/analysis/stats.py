"""Descriptive statistics with explicit definitions (used in the report)."""

from __future__ import annotations

import numpy as np


def describe(x) -> dict:
    """n, mean, std (sample, ddof=1), min, max, range of the finite values."""
    a = np.asarray(x, float)
    a = a[np.isfinite(a)]
    if a.size == 0:
        return {"n": 0}
    return {
        "n": int(a.size),
        "mean": float(a.mean()),
        "std": float(a.std(ddof=1)) if a.size > 1 else 0.0,
        "min": float(a.min()),
        "max": float(a.max()),
        "range": float(a.max() - a.min()),
    }


def error_stats(d_e, d_n) -> dict:
    """Horizontal positioning error statistics.

    d = sqrt(dE^2 + dN^2) per epoch;  mean error = mean(d);  RMSE = sqrt(mean(d^2));
    std = sample std of d;  max = max(d);  CEP50/CEP95 = 50th/95th percentile of d;
    bias = mean dE, mean dN (systematic offset).
    """
    de = np.asarray(d_e, float)
    dn = np.asarray(d_n, float)
    ok = np.isfinite(de) & np.isfinite(dn)
    de, dn = de[ok], dn[ok]
    if de.size == 0:
        return {"n": 0}
    d = np.hypot(de, dn)
    return {
        "n": int(d.size),
        "mean_error_m": float(d.mean()),
        "rmse_m": float(np.sqrt(np.mean(d ** 2))),
        "std_m": float(d.std(ddof=1)) if d.size > 1 else 0.0,
        "max_error_m": float(d.max()),
        "cep50_m": float(np.percentile(d, 50)),
        "cep95_m": float(np.percentile(d, 95)),
        "bias_e_m": float(de.mean()),
        "bias_n_m": float(dn.mean()),
        "std_e_m": float(de.std(ddof=1)) if de.size > 1 else 0.0,
        "std_n_m": float(dn.std(ddof=1)) if dn.size > 1 else 0.0,
    }
