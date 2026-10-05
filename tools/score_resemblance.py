#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (C) 2026, Charles Chiou
#

"""
score_resemblance.py - Evaluates visual resemblance between two board images.

Input #1: Ground truth reference photograph (invariant from plan Figure 2).
Input #2: Cropped board image from web page snapshot (or full snapshot with --auto-crop).
"""

import sys
import os
import argparse
import json
import numpy as np
from PIL import Image

DEFAULT_REF_IMAGE = "doc/assets/abc80_board.png"
DEFAULT_WEB_SNAPSHOT = "doc/assets/web_snapshot.png"


def detect_and_crop_board(img, verbose=False):
    """
    Detects and crops the trainer chassis / board from a full web page snapshot.
    If the image is already cropped to the board, returns the image unchanged.
    """
    arr = np.array(img.convert("RGB"))
    h, w, _ = arr.shape

    # If image already has a portrait board-like aspect ratio (w/h < 0.9), check if it's already cropped
    aspect = w / float(h)
    if aspect < 0.9 and w < 850:
        if verbose:
            print(f"[detect] Image appears already cropped ({w}x{h}, aspect={aspect:.3f}).")
        return img, (0, 0, w, h)

    # In web app screenshots, the body background is dark (e.g. #0a0e16).
    # Chassis is beige (#ded7c8), with high R, G, B values.
    # We locate the chassis region in X in [100..800] and Y > 50 (below header).
    y_start = min(60, h // 4)
    x_limit = min(1050, int(w * 0.95))

    sub = arr[y_start:, :x_limit]
    # Beige chassis: R > 170, G > 165, B > 150
    mask_chassis = (sub[:, :, 0] > 170) & (sub[:, :, 1] > 165) & (sub[:, :, 2] > 150)
    y_idx, x_idx = np.where(mask_chassis)

    if len(x_idx) > 500:
        x0 = int(x_idx.min())
        x1 = int(x_idx.max())
        y0 = int(y_idx.min() + y_start)
        y1 = int(y_idx.max() + y_start)

        # Add modest padding around chassis
        pad = 6
        x0 = max(0, x0 - pad)
        y0 = max(0, y0 - pad)
        x1 = min(w - 1, x1 + pad)
        y1 = min(h - 1, y1 + pad)

        if verbose:
            print(f"[detect] Auto-detected board chassis: [{x0}, {y0}, {x1}, {y1}] (size {x1-x0}x{y1-y0})")
        cropped = img.crop((x0, y0, x1, y1))
        return cropped, (x0, y0, x1, y1)

    # Fallback to green PCB detection
    mask_green = (sub[:, :, 1] > 40) & (sub[:, :, 1] > sub[:, :, 0] * 1.1) & (sub[:, :, 1] > sub[:, :, 2] * 1.1)
    y_idx, x_idx = np.where(mask_green)
    if len(x_idx) > 500:
        x0 = max(0, int(x_idx.min()) - 25)
        x1 = min(w - 1, int(x_idx.max()) + 25)
        y0 = max(0, int(y_idx.min() + y_start) - 30)
        y1 = min(h - 1, int(y_idx.max() + y_start) + 30)
        if verbose:
            print(f"[detect] Auto-detected PCB cluster: [{x0}, {y0}, {x1}, {y1}]")
        cropped = img.crop((x0, y0, x1, y1))
        return cropped, (x0, y0, x1, y1)

    if verbose:
        print("[detect] Fallback: using original uncropped image.")
    return img, (0, 0, w, h)


def calculate_ssim(arr1, arr2):
    """Computes global luminance structural similarity index (SSIM)."""
    # Identical check
    if np.array_equal(arr1, arr2):
        return 1.0

    lum1 = 0.299 * arr1[:, :, 0] + 0.587 * arr1[:, :, 1] + 0.114 * arr1[:, :, 2]
    lum2 = 0.299 * arr2[:, :, 0] + 0.587 * arr2[:, :, 1] + 0.114 * arr2[:, :, 2]

    c1 = (0.01 * 255) ** 2
    c2 = (0.03 * 255) ** 2

    mu1 = np.mean(lum1)
    mu2 = np.mean(lum2)
    var1 = np.var(lum1)
    var2 = np.var(lum2)
    cov = np.cov(lum1.flatten(), lum2.flatten())[0, 1]

    denom = (mu1 ** 2 + mu2 ** 2 + c1) * (var1 + var2 + c2)
    if denom == 0:
        return 1.0 if (mu1 == mu2 and var1 == var2) else 0.0

    ssim = ((2 * mu1 * mu2 + c1) * (2 * cov + c2)) / denom
    return float(np.clip(ssim, 0.0, 1.0))


def calculate_pearson(arr1, arr2):
    """Computes RGB Pearson correlation coefficient."""
    if np.array_equal(arr1, arr2):
        return 1.0

    f1 = arr1.astype(float).flatten()
    f2 = arr2.astype(float).flatten()
    std1 = np.std(f1)
    std2 = np.std(f2)
    if std1 == 0 or std2 == 0:
        return 0.0
    corr = np.corrcoef(f1, f2)[0, 1]
    if np.isnan(corr):
        return 0.0
    return float(np.clip(corr, 0.0, 1.0))


def calculate_color_similarity(arr1, arr2):
    """Computes color histogram intersection and cosine similarity."""
    if np.array_equal(arr1, arr2):
        return 1.0

    h1, _ = np.histogramdd(arr1.reshape(-1, 3), bins=(16, 16, 16), range=[(0, 256), (0, 256), (0, 256)])
    h2, _ = np.histogramdd(arr2.reshape(-1, 3), bins=(16, 16, 16), range=[(0, 256), (0, 256), (0, 256)])

    sum1 = np.sum(h1)
    sum2 = np.sum(h2)
    if sum1 == 0 or sum2 == 0:
        return 0.0

    # Histogram Intersection
    intersect = np.sum(np.minimum(h1 / sum1, h2 / sum2))

    # Cosine Similarity
    v1 = h1.flatten() / (np.linalg.norm(h1.flatten()) + 1e-9)
    v2 = h2.flatten() / (np.linalg.norm(h2.flatten()) + 1e-9)
    cosine = np.dot(v1, v2)

    # Weighted blend of intersection and cosine
    sim = 0.5 * intersect + 0.5 * cosine
    return float(np.clip(sim, 0.0, 1.0))


def evaluate_subsystems(arr1_norm, arr2_norm):
    """
    Evaluates spatial and visual correlation across the functional zones
    strictly above the keypad (Y <= 67%):
    1. Power Header & Regulator (Top ~14% of board -> 0.00 to 0.209 of upper)
    2. DIP IC Subsystem (14% - 53% of board -> 0.209 to 0.791 of upper)
    3. 6-Digit LED Display Module (53% - 67% of board -> 0.791 to 1.000 of upper)
    """
    if np.array_equal(arr1_norm, arr2_norm):
        return {
            "Power & Reset Header": 100.0,
            "DIP IC Subsystems (Z80/PIO/ROM/RAM)": 100.0,
            "6-Digit 7-Segment Display Module": 100.0,
            "mean_spatial": 100.0,
        }

    h, w, _ = arr1_norm.shape
    zones = [
        ("Power & Reset Header", 0.00, 112.0 / 485.0),
        ("DIP IC Subsystems (Z80/PIO/ROM/RAM)", 112.0 / 485.0, 400.0 / 485.0),
        ("6-Digit 7-Segment Display Module", 400.0 / 485.0, 1.00),
    ]

    scores = {}
    for name, y0_pct, y1_pct in zones:
        y0 = int(y0_pct * h)
        y1 = int(y1_pct * h)
        z1 = arr1_norm[y0:y1, :]
        z2 = arr2_norm[y0:y1, :]

        # Pearson correlation in this zone
        corr = calculate_pearson(z1, z2)
        # SSIM in this zone
        ssim = calculate_ssim(z1, z2)

        # Zone score: correlation + structural (color excluded per user directive)
        zone_score = (0.50 * corr + 0.50 * ssim) * 100.0
        # If identical
        if np.array_equal(z1, z2):
            zone_score = 100.0
        scores[name] = float(np.clip(zone_score, 0.0, 100.0))

    scores["mean_spatial"] = float(np.mean(list(scores.values())))
    return scores


def evaluate_resemblance(img_ref, img_eval, auto_crop=True, verbose=False):
    """
    Main evaluation routine comparing Reference (Image #1) and Evaluation (Image #2).
    """
    # If auto-crop requested, detect and crop the board chassis in img_eval
    if auto_crop:
        img_eval_board, crop_box = detect_and_crop_board(img_eval, verbose=verbose)
    else:
        img_eval_board = img_eval
        crop_box = (0, 0, img_eval.width, img_eval.height)

    w_ref, h_ref = img_ref.size
    w_eval, h_eval = img_eval_board.size

    # Check identical image shortcut
    is_identical = False
    if img_ref.size == img_eval_board.size:
        diff_test = np.sum(np.abs(np.array(img_ref.convert("RGB")) - np.array(img_eval_board.convert("RGB"))))
        if diff_test == 0:
            is_identical = True

    # 1. Geometric & Aspect Ratio Score (S_geom)
    ratio_ref = float(w_ref) / float(h_ref)
    ratio_eval = float(w_eval) / float(h_eval)
    max_ratio = max(ratio_ref, ratio_eval)
    if is_identical:
        s_geom = 100.0
    else:
        ratio_diff = abs(ratio_ref - ratio_eval)
        s_geom = max(0.0, 1.0 - (ratio_diff / max_ratio)) * 100.0

    # Normalize both images to a shared standardized canvas (600 x 800) for uniform analysis
    norm_w, norm_h = 600, 800
    if is_identical:
        norm_ref = np.array(img_ref.convert("RGB").resize((norm_w, norm_h), Image.Resampling.LANCZOS))
        norm_eval = norm_ref
    else:
        norm_ref = np.array(img_ref.convert("RGB").resize((norm_w, norm_h), Image.Resampling.LANCZOS))
        norm_eval = np.array(img_eval_board.convert("RGB").resize((norm_w, norm_h), Image.Resampling.LANCZOS))

    # Restrict evaluation canvas strictly to the region above the keypad (Y <= 485px / 60.625%)
    KEYPAD_CUTOFF_Y_PCT = 485.0 / 800.0
    h_above = int(KEYPAD_CUTOFF_Y_PCT * norm_h)
    norm_ref_above = norm_ref[:h_above, :]
    norm_eval_above = norm_eval[:h_above, :]

    # 1. Geometric & Aspect Ratio Score (S_geom) for the evaluated above-keypad region
    ratio_ref = float(w_ref) / (float(h_ref) * KEYPAD_CUTOFF_Y_PCT)
    ratio_eval = float(w_eval) / (float(h_eval) * KEYPAD_CUTOFF_Y_PCT)
    max_ratio = max(ratio_ref, ratio_eval)
    if is_identical:
        s_geom = 100.0
    else:
        ratio_diff = abs(ratio_ref - ratio_eval)
        s_geom = max(0.0, 1.0 - (ratio_diff / max_ratio)) * 100.0

    # 2. Subsystem Spatial & Zone Alignment (S_spatial)
    subsystems = evaluate_subsystems(norm_ref_above, norm_eval_above)
    s_spatial = subsystems["mean_spatial"]

    # 3. Global Color Palette Fidelity (S_color) - 0% weight per user instruction
    if is_identical:
        s_color = 100.0
    else:
        s_color = calculate_color_similarity(norm_ref_above, norm_eval_above) * 100.0

    # 4. Global Structural & Texture Correlation (S_texture) across above-keypad pixels
    if is_identical:
        s_texture = 100.0
        r_pearson = 1.0
        ssim_val = 1.0
    else:
        r_pearson = calculate_pearson(norm_ref_above, norm_eval_above)
        ssim_val = calculate_ssim(norm_ref_above, norm_eval_above)
        s_texture = (0.5 * r_pearson + 0.5 * ssim_val) * 100.0

    # 5. Composite Resemblance Score (Above-Keypad Spatial/Geometry Focus)
    # Weights:
    #   Geometry / Aspect Ratio: 15%
    #   Spatial Subsystem Topology: 55%
    #   Color Palette Fidelity: 0%
    #   Texture & Surface Correlation: 30%
    if is_identical:
        composite = 100.0
    else:
        composite = (
            0.15 * s_geom +
            0.55 * s_spatial +
            0.00 * s_color +
            0.30 * s_texture
        )

    # Compile result dictionary
    result = {
        "identical": is_identical,
        "evaluation_scope": "above_keypad_only (Y <= 67%)",
        "input1_size": [w_ref, h_ref],
        "input1_aspect_ratio": round(ratio_ref, 4),
        "input2_raw_size": [img_eval.width, img_eval.height],
        "input2_board_size": [w_eval, h_eval],
        "input2_crop_box": list(crop_box),
        "input2_aspect_ratio": round(ratio_eval, 4),
        "scores": {
            "geometric_aspect_ratio": round(s_geom, 2),
            "spatial_subsystems": round(s_spatial, 2),
            "color_palette_fidelity": round(s_color, 2),
            "texture_and_ssim": round(s_texture, 2),
            "pearson_correlation": round(r_pearson * 100.0, 2),
            "luminance_ssim": round(ssim_val * 100.0, 2),
            "composite_resemblance": round(composite, 2),
        },
        "subsystems": {k: round(v, 2) for k, v in subsystems.items() if k != "mean_spatial"}
    }

    return result, img_eval_board, norm_ref_above, norm_eval_above


def generate_diff_map(norm_ref, norm_eval, output_path):
    """Generates an absolute difference heatmap between the two normalized images."""
    diff = np.abs(norm_ref.astype(int) - norm_eval.astype(int))
    diff_mag = np.mean(diff, axis=2).astype(np.uint8)

    # Colorize difference: 0 is dark blue/black, 255 is bright red/white
    heatmap = np.zeros_like(norm_ref)
    heatmap[:, :, 0] = np.clip(diff_mag * 2, 0, 255)
    heatmap[:, :, 1] = np.clip(diff_mag, 0, 180)
    heatmap[:, :, 2] = np.clip(255 - diff_mag * 2, 0, 255)

    img_diff = Image.fromarray(heatmap)
    img_diff.save(output_path)


def print_report(res, img1_path, img2_path):
    """Outputs a clean, formatted ASCII evaluation table."""
    print("=" * 78)
    print("      ABC-80 VISUAL RESEMBLANCE REPORT (ABOVE-KEYPAD REGION ONLY)")
    print("=" * 78)
    print(f" Input #1 (Ground Truth Ref) : {img1_path}")
    print(f"   Dimensions                : {res['input1_size'][0]} x {res['input1_size'][1]} px (Aspect Ratio: {res['input1_aspect_ratio']})")
    print(f" Input #2 (Evaluation Target): {img2_path}")
    print(f"   Raw Dimensions            : {res['input2_raw_size'][0]} x {res['input2_raw_size'][1]} px")
    print(f"   Board Crop Box            : {res['input2_crop_box']}")
    print(f"   Board Dimensions          : {res['input2_board_size'][0]} x {res['input2_board_size'][1]} px (Aspect Ratio: {res['input2_aspect_ratio']})")
    print("-" * 78)
    print(" METRIC CATEGORY                          WEIGHT    SCORE")
    print("-" * 78)
    sc = res["scores"]
    print(f" 1. Geometric Proportions & Aspect Ratio   15%      {sc['geometric_aspect_ratio']:6.2f}%")
    print(f" 2. Spatial Subsystems Topology            55%      {sc['spatial_subsystems']:6.2f}%")
    for name, score in res["subsystems"].items():
        print(f"      - {name:<35} : {score:5.2f}%")
    print(f" 3. Color Palette & Chromatic Fidelity      0%      {sc['color_palette_fidelity']:6.2f}% (Excluded)")
    print(f" 4. Structural & Texture Correlation       30%      {sc['texture_and_ssim']:6.2f}%")
    print(f"      - Pearson Correlation (RGB)                   : {sc['pearson_correlation']:5.2f}%")
    print(f"      - Luminance SSIM                              : {sc['luminance_ssim']:5.2f}%")
    print("=" * 78)
    comp = sc["composite_resemblance"]
    print(f" FINAL COMPOSITE RESEMBLANCE SCORE:                 \033[1;32m{comp:6.2f}%\033[0m" if comp >= 90 else f" FINAL COMPOSITE RESEMBLANCE SCORE:                 {comp:6.2f}%")
    print("=" * 78)


def main():
    parser = argparse.ArgumentParser(
        description="Evaluates visual resemblance between two ABC-80 hardware images."
    )
    parser.add_argument(
        "image1",
        nargs="?",
        default=DEFAULT_REF_IMAGE,
        help=f"Input image #1: Ground truth reference (default: {DEFAULT_REF_IMAGE})",
    )
    parser.add_argument(
        "image2",
        nargs="?",
        default=DEFAULT_WEB_SNAPSHOT,
        help=f"Input image #2: Evaluation target board or web snapshot (default: {DEFAULT_WEB_SNAPSHOT})",
    )
    parser.add_argument(
        "--no-auto-crop",
        action="store_true",
        help="Disable automatic detection and cropping of the board chassis in Image #2",
    )
    parser.add_argument(
        "--crop-out",
        type=str,
        default=None,
        help="Save the cropped evaluation board image to this path",
    )
    parser.add_argument(
        "--diff-out",
        type=str,
        default=None,
        help="Generate and save an absolute difference heatmap to this path",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output raw machine-readable JSON instead of tabular report",
    )
    parser.add_argument(
        "--above-keypad",
        action="store_true",
        default=True,
        help="Evaluate strictly the area above the keypad (default: True)",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable diagnostic verbose logging",
    )

    args = parser.parse_args()

    # Validate file existence
    if not os.path.exists(args.image1):
        sys.stderr.write(f"Error: Input #1 reference image '{args.image1}' not found.\n")
        sys.exit(1)
    if not os.path.exists(args.image2):
        sys.stderr.write(f"Error: Input #2 evaluation image '{args.image2}' not found.\n")
        sys.exit(1)

    try:
        img_ref = Image.open(args.image1)
    except Exception as e:
        sys.stderr.write(f"Error opening Image #1 '{args.image1}': {e}\n")
        sys.exit(1)

    try:
        img_eval = Image.open(args.image2)
    except Exception as e:
        sys.stderr.write(f"Error opening Image #2 '{args.image2}': {e}\n")
        sys.exit(1)

    auto_crop = not args.no_auto_crop
    res, img_eval_board, norm_ref, norm_eval = evaluate_resemblance(
        img_ref, img_eval, auto_crop=auto_crop, verbose=args.verbose
    )

    if args.crop_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.crop_out)), exist_ok=True)
        img_eval_board.save(args.crop_out)
        if args.verbose:
            print(f"[crop-out] Saved cropped board image to '{args.crop_out}'")

    if args.diff_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.diff_out)), exist_ok=True)
        generate_diff_map(norm_ref, norm_eval, args.diff_out)
        if args.verbose:
            print(f"[diff-out] Saved difference heatmap to '{args.diff_out}'")

    if args.json:
        print(json.dumps(res, indent=2))
    else:
        print_report(res, args.image1, args.image2)


if __name__ == "__main__":
    main()
