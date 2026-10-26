"""Runs every analysis for which inputs are given and writes report.md,
results.json, figures/ and gis/ into an output folder."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from ..utm_check import compare_records
from . import gis, plots, tests
from .data import Survey, load_points, read_csv_dicts


@dataclass
class Inputs:
    survey: list[Path]
    period_ms: float = 1000.0
    out: Path = Path("data/processed/analysis")
    telemetry: Path | None = None
    static: list[Path] = field(default_factory=list)
    reference: Path | None = None
    marks_reference: Path | None = None
    mark_window_s: float = 5.0
    route: Path | None = None
    truth: Path | None = None


def _clean(obj):
    """Drop private '_series' entries and make numpy types JSON-friendly."""
    if isinstance(obj, dict):
        return {k: _clean(v) for k, v in obj.items() if not k.startswith("_")}
    if isinstance(obj, list):
        return [_clean(v) for v in obj]
    if isinstance(obj, (np.floating, np.integer)):
        return obj.item()
    return obj


def f(x, nd=3):
    return "—" if x is None else f"{x:.{nd}f}"


def run(inp: Inputs) -> dict:
    out = Path(inp.out)
    figs = out / "figures"
    figs.mkdir(parents=True, exist_ok=True)
    survey = Survey.load(*inp.survey)
    zone = survey.zone()
    res: dict = {"inputs": {k: str(v) for k, v in vars(inp).items()}, "zone": zone}
    md: list[str] = ["# ENG401 survey validation report", "",
                     f"Survey files: {', '.join(p.name for p in inp.survey)}  ",
                     f"Records: {len(survey.rows)}, fixes: {len(survey.fixes())}, sessions: {len(survey.sessions())}, "
                     f"UTM zone: {zone[0]}{zone[1] if zone else ''}", ""]

    # 1. GNSS accuracy -------------------------------------------------------
    md += ["## 1. GNSS accuracy", ""]
    gnss_results = []
    if inp.static and inp.reference:
        st = Survey.load(*inp.static)
        z = st.zone() or zone
        ref = load_points(inp.reference, *z)[0]
        gnss_results.append(("Static on " + ref["id"], tests.gnss_static(st, ref["easting"], ref["northing"])))
    if inp.marks_reference and zone:
        gnss_results.append(("Observation points (marks)",
                             tests.gnss_marks(survey, load_points(inp.marks_reference, *zone), inp.mark_window_s)))
    if inp.truth:
        gnss_results.append(("Against ground truth (simulation)", tests.gnss_truth(survey, inp.truth)))
    if not gnss_results:
        md += ["_No reference data given (--static/--reference, --marks-reference or --truth)._", ""]
    for i, (title, g) in enumerate(gnss_results):
        e, q = g["errors"], g["quality"]
        md += [f"### {title}", "", "| n | mean error | RMSE | std | max | CEP50 | CEP95 | bias E | bias N |",
               "|---|---|---|---|---|---|---|---|---|",
               f"| {e.get('n', 0)} | {f(e.get('mean_error_m'))} m | {f(e.get('rmse_m'))} m | {f(e.get('std_m'))} m | "
               f"{f(e.get('max_error_m'))} m | {f(e.get('cep50_m'))} m | {f(e.get('cep95_m'))} m | "
               f"{f(e.get('bias_e_m'))} m | {f(e.get('bias_n_m'))} m |", "",
               f"Satellites mean {f(q['satellites'].get('mean'), 1)} (min {f(q['satellites'].get('min'), 0)}, "
               f"max {f(q['satellites'].get('max'), 0)}); HDOP mean {f(q['hdop'].get('mean'), 2)} "
               f"(min {f(q['hdop'].get('min'), 2)}, max {f(q['hdop'].get('max'), 2)})", ""]
        if g["mode"] == "marks":
            md += ["| point | mark | status | n | ΔE | ΔN | d |", "|---|---|---|---|---|---|---|"]
            md += [f"| {p['id']} | {p['mark']} | {p['status']} | {p.get('n_fixes', '')} | {f(p.get('de_m'))} | "
                   f"{f(p.get('dn_m'))} | {f(p.get('d_m'))} |" for p in g["points"]] + [""]
        if "_series" in g and e.get("n"):
            name = f"gnss_errors_{i + 1}.png"
            plots.gnss_errors(g, figs / name)
            md += [f"![{title}](figures/{name})", ""]
    res["gnss_accuracy"] = [{"title": t, **g} for t, g in gnss_results]
    md += ["Definitions: d = √(ΔE²+ΔN²); mean error = mean(d); RMSE = √mean(d²); std = sample std of d; "
           "CEP50/95 = 50th/95th percentile of d.", ""]

    # 2. Coordinate transformation -------------------------------------------
    u = compare_records(survey.rows)
    res["utm_verification"] = {"n": u.n, "mean_mm": u.mean_mm, "rmse_mm": u.rmse_mm, "max_mm": u.max_mm}
    md += ["## 2. Coordinate transformation (device UTM vs pyproj)", "",
           f"{u.n} points: mean {f(u.mean_mm)} mm, RMSE {f(u.rmse_mm)} mm, max {f(u.max_mm)} mm. "
           "Differences up to ~1.4 mm are CSV rounding (lat/lon 8 decimals, E/N 3 decimals); the algorithm "
           "itself agrees to < 0.001 mm (firmware test_utm).", ""]

    # 3. Sensor repeatability ------------------------------------------------
    rep_src = Survey.load(*inp.static) if inp.static else survey
    rep = tests.sensor_repeatability(rep_src)
    res["sensor_repeatability"] = rep
    md += ["## 3. Sensor repeatability" + (" (static dataset)" if inp.static else " (survey dataset — not stationary!)"),
           "", "| channel | unit | n | mean | std | min | max | range |", "|---|---|---|---|---|---|---|---|"]
    md += [f"| {k} | {v['unit']} | {v.get('n', 0)} | {f(v.get('mean'))} | {f(v.get('std'), 4)} | {f(v.get('min'))} | "
           f"{f(v.get('max'))} | {f(v.get('range'))} |" for k, v in rep.items()] + [""]

    # 4. Sampling performance ------------------------------------------------
    sp = tests.sampling_performance(survey, inp.period_ms)
    res["sampling"] = sp
    a = sp["actual_ms"]
    plots.histogram(sp["_series"]["dt_ms"], "Sampling interval", "interval (ms)", figs / "sampling_interval.png",
                    inp.period_ms)
    md += ["## 4. Sampling performance", "",
           f"Requested {inp.period_ms:g} ms; actual mean {f(a.get('mean'))} ms, std {f(a.get('std'))} ms, "
           f"min {f(a.get('min'))} ms, max {f(a.get('max'))} ms (n = {a.get('n', 0)}). "
           f"Mean |jitter| {f(sp['mean_abs_jitter_ms'])} ms, max {f(sp['max_abs_jitter_ms'])} ms. "
           f"Missed samples: {sp['missed_samples']} (firmware counter: {sp['firmware_missed_counter']}).", "",
           "![interval](figures/sampling_interval.png)", ""]

    # 5. Latency -------------------------------------------------------------
    md += ["## 5. Communication latency (sensor → PC)", ""]
    if inp.telemetry:
        lt = tests.latency(inp.telemetry)
        res["latency"] = lt
        d = lt["latency_ms"]
        plots.histogram(lt["_series"]["latency_ms"], "Sensor-to-PC latency", "latency (ms)", figs / "latency.png")
        md += [f"n = {d.get('n', 0)}: min {f(d.get('min'), 1)} ms, mean {f(d.get('mean'), 1)} ms, "
               f"max {f(d.get('max'), 1)} ms, std {f(d.get('std'), 1)} ms, p95 {f(d.get('p95'), 1)} ms. "
               f"{lt['without_clock_sync']} records arrived before clock sync.", "",
               "![latency](figures/latency.png)", ""]
    else:
        md += ["_No telemetry log given (--telemetry)._", ""]

    # 6. Completeness ---------------------------------------------------------
    c = tests.completeness(survey, inp.period_ms)
    res["completeness"] = c
    md += ["## 6. Data completeness (valid / expected × 100)", "", f"Expected observations: {c['expected']}", "",
           "| level | count | % |", "|---|---|---|"]
    md += [f"| {k} | {v['count']} | {v['pct']:.2f} |" for k, v in c["levels"].items()] + [""]
    if inp.telemetry:
        tc = tests.telemetry_completeness(inp.telemetry)
        res["telemetry_completeness"] = tc
        md += [f"Telemetry: {tc['received']} of {tc['expected']} packets received ({tc['pct']:.2f} %), "
               f"{tc['lost']} lost (SD log is complete independently).", ""]

    # 7. Route comparison ------------------------------------------------------
    md += ["## 7. Trajectory vs planned route", ""]
    planned_latlon = None
    if inp.route and zone:
        planned = load_points(inp.route, *zone)
        rc = tests.route_comparison(survey, planned)
        res["route"] = rc
        ct = rc["cross_track_m"]
        plots.track_vs_route(rc, figs / "track_vs_route.png")
        md += [f"Cross-track distance over {ct.get('n', 0)} fixes: mean {f(ct.get('mean'))} m, RMSE {f(ct.get('rmse'))} m, "
               f"p95 {f(ct.get('p95'))} m, max {f(ct.get('max'))} m. Within 2 m: {f(rc['within_m']['2'], 1)} %, "
               f"within 5 m: {f(rc['within_m']['5'], 1)} %.", "",
               f"Planned length {f(rc['planned_length_m'], 1)} m, measured {f(rc['measured_length_m'], 1)} m "
               "(measured length is inflated by position noise between epochs).", "",
               "![route](figures/track_vs_route.png)", ""]
        if Path(inp.route).suffix.lower() == ".csv":
            rows = read_csv_dicts(inp.route)
            if rows and "lat" in rows[0]:
                planned_latlon = [(float(r["lat"]), float(r["lon"])) for r in rows]
    else:
        md += ["_No planned route given (--route)._", ""]

    # GIS --------------------------------------------------------------------
    files = gis.export(survey, out / "gis", planned_latlon)
    plots.thematic_preview(survey, "temp", "°C", figs / "thematic_temp.png")
    res["gis_files"] = [str(p) for p in files.values()]
    md += ["## GIS products", "", "QGIS layers in `gis/`: " + ", ".join(f"`{n}`" for n in files), "",
           "![thematic](figures/thematic_temp.png)", ""]

    (out / "report.md").write_text("\n".join(md))
    (out / "results.json").write_text(json.dumps(_clean(res), indent=2, default=str))
    return res
