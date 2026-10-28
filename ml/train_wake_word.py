#!/usr/bin/env python3
"""Train a logistic-regression hey-STM vs other wake-word classifier.

MCU-friendly: exports
  wake_word_model.h    - dot-product weights + bias (scaler folded in)
  wake_tables.h        - window / mel / DCT / delta tables for the C feature code
  wake_test_vector.h   - one real clip + its Python features, to verify the C port

Usage (from repo root):
  source .venv/bin/activate
  python ml/train_wake_word.py
"""

from __future__ import annotations

from pathlib import Path

import joblib
import librosa
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import scipy.fft
import scipy.signal
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import (
    ConfusionMatrixDisplay,
    classification_report,
    confusion_matrix,
)
from sklearn.model_selection import StratifiedKFold, cross_val_score, train_test_split
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler

# ---------------------------------------------------------------------------
# Paths / constants
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
OUT = ROOT / "ml" / "artifacts"
OUT.mkdir(parents=True, exist_ok=True)

N_SAMPLES = 256
# Firmware reads the mic once every SAMPLE_PERIOD_MS (SysTick = 1 ms ticks).
# CHANGED: tie the rate to the firmware pacing instead of a magic number.
SAMPLE_PERIOD_MS = 9
SAMPLE_RATE = round(1000 / SAMPLE_PERIOD_MS)  # 111 Hz
N_MFCC = 13
N_FFT = 64
HOP = 32
N_MELS = 20
# CHANGED: width=9 on exactly 9 frames made delta constant (std ~ 1e-16) and
# blew the exported weights up to ~1e15. Use a narrower window.
DELTA_WIDTH = 5
N_FRAMES = 1 + N_SAMPLES // HOP  # librosa center=True
N_BINS = N_FFT // 2 + 1

# Light augmentation multiplicity (train waves only)
AUG_PER_SAMPLE = 3
RNG = np.random.default_rng(42)


# ---------------------------------------------------------------------------
# Data loading / features
# ---------------------------------------------------------------------------

def load_dump(path: Path) -> np.ndarray:
    df = pd.read_csv(path)
    y = df["y"].to_numpy(dtype=np.float64)
    if len(y) != N_SAMPLES:
        raise ValueError(f"{path.name}: expected {N_SAMPLES} samples, got {len(y)}")
    return y


def load_class(folder: Path, label: int):
    Xs, ys, names = [], [], []
    for p in sorted(folder.glob("buffer_dump_*.csv")):
        Xs.append(load_dump(p))
        ys.append(label)
        names.append(p.name)
    return Xs, ys, names


def augment_wave(x: np.ndarray) -> np.ndarray:
    """Small time-shift + gain + noise — keeps label, adds variety."""
    out = x.copy()
    shift = int(RNG.integers(-20, 21))
    out = np.roll(out, shift)
    out *= float(RNG.uniform(0.7, 1.3))
    noise = RNG.normal(0.0, 0.02 * (np.std(out) + 1e-9), size=out.shape)
    return out + noise


def waveform_to_features(x: np.ndarray) -> np.ndarray:
    peak = np.max(np.abs(x)) + 1e-9
    x = x / peak

    mfcc = librosa.feature.mfcc(
        y=x,
        sr=SAMPLE_RATE,
        n_mfcc=N_MFCC,
        n_fft=N_FFT,
        hop_length=HOP,
        n_mels=N_MELS,
        fmax=SAMPLE_RATE / 2.0,
    )
    delta = librosa.feature.delta(mfcc, width=DELTA_WIDTH)
    delta2 = librosa.feature.delta(mfcc, order=2, width=DELTA_WIDTH)

    rms = librosa.feature.rms(y=x, frame_length=N_FFT, hop_length=HOP)
    zcr = librosa.feature.zero_crossing_rate(x, frame_length=N_FFT, hop_length=HOP)

    parts = [
        mfcc.mean(axis=1),
        mfcc.std(axis=1),
        delta.mean(axis=1),
        delta.std(axis=1),
        delta2.mean(axis=1),
        delta2.std(axis=1),
        np.array([rms.mean(), rms.std(), zcr.mean(), zcr.std()]),
    ]
    return np.concatenate(parts)


def build_feature_matrix(waves: np.ndarray, labels: np.ndarray, augment: bool):
    Xs, ys = [], []
    for w, lab in zip(waves, labels):
        Xs.append(waveform_to_features(w))
        ys.append(lab)
        if augment:
            for _ in range(AUG_PER_SAMPLE):
                Xs.append(waveform_to_features(augment_wave(w)))
                ys.append(lab)
    return np.stack(Xs), np.array(ys, dtype=np.int64)


# ---------------------------------------------------------------------------
# Models
# ---------------------------------------------------------------------------

def make_logreg() -> Pipeline:
    """MCU-friendly linear model: one dot product + bias on device."""
    return Pipeline(
        [
            ("scaler", StandardScaler()),
            (
                "model",
                LogisticRegression(
                    max_iter=10000,
                    C=3.0,
                    class_weight="balanced",
                    solver="lbfgs",
                ),
            ),
        ]
    )


# ---------------------------------------------------------------------------
# Export helpers
# ---------------------------------------------------------------------------

def _c_floats(arr: np.ndarray, per_line: int = 4) -> str:
    flat = np.asarray(arr, dtype=np.float32).ravel()
    lines = []
    for i in range(0, len(flat), per_line):
        chunk = ", ".join(f"{v:.9e}f" for v in flat[i : i + per_line])
        lines.append("    " + chunk + ",")
    return "\n".join(lines)


def _c_matrix(arr: np.ndarray) -> str:
    """Nested braces for 2-D tables (keeps -Wmissing-braces quiet)."""
    rows = []
    for r in np.asarray(arr, dtype=np.float32):
        rows.append("    {\n" + _c_floats(r).replace("    ", "        ", 1) + "\n    },")
    return "\n".join(rows)


def export_logreg_header(clf: Pipeline, path: Path) -> None:
    scaler: StandardScaler = clf.named_steps["scaler"]
    lr: LogisticRegression = clf.named_steps["model"]
    w = lr.coef_.ravel()
    b = float(lr.intercept_[0])
    mean = scaler.mean_
    scale = scaler.scale_

    # CHANGED: never divide by a ~0 std. A dead (constant) feature carries no
    # information, so give it weight 0 instead of w/1e-16.
    dead = scale < 1e-6
    if dead.any():
        print(f"WARNING: {int(dead.sum())} near-constant features zeroed: {np.where(dead)[0].tolist()}")
    safe_scale = np.where(dead, 1.0, scale)
    w_eff = np.where(dead, 0.0, w)
    w_x = w_eff / safe_scale
    b_x = b - float(np.dot(w_eff, mean / safe_scale))
    print(f"max |exported weight| = {np.abs(w_x).max():.3f}  bias = {b_x:.3f}")

    with path.open("w", encoding="utf-8") as f:
        f.write("/* Auto-generated by ml/train_wake_word.py — do not edit by hand */\n")
        f.write("#pragma once\n\n")
        f.write(f"#define WAKE_N_FEATURES {len(w_x)}\n")
        f.write(f"#define WAKE_SAMPLE_PERIOD_MS {SAMPLE_PERIOD_MS}\n")
        f.write(f"#define WAKE_SAMPLE_RATE {SAMPLE_RATE}\n")
        f.write(f"#define WAKE_N_MFCC {N_MFCC}\n")
        f.write(f"#define WAKE_N_FFT {N_FFT}\n")
        f.write(f"#define WAKE_HOP {HOP}\n")
        f.write(f"#define WAKE_N_MELS {N_MELS}\n")
        f.write(f"#define WAKE_N_SAMPLES {N_SAMPLES}\n\n")
        f.write(f"static const float WAKE_BIAS = {b_x:.8e}f;\n")
        f.write("static const float WAKE_WEIGHTS[WAKE_N_FEATURES] = {\n")
        f.write(_c_floats(w_x))
        f.write("\n};\n")


def export_feature_tables(path: Path) -> None:
    """Tables so the C code reproduces librosa without re-implementing it."""
    window = scipy.signal.get_window("hann", N_FFT, fftbins=True)
    mel = librosa.filters.mel(
        sr=SAMPLE_RATE, n_fft=N_FFT, n_mels=N_MELS, fmax=SAMPLE_RATE / 2.0
    )
    dct = scipy.fft.dct(np.eye(N_MELS), axis=0, type=2, norm="ortho")[:N_MFCC]
    d1 = scipy.signal.savgol_filter(
        np.eye(N_FRAMES), DELTA_WIDTH, polyorder=1, deriv=1, axis=0, mode="interp"
    )
    d2 = scipy.signal.savgol_filter(
        np.eye(N_FRAMES), DELTA_WIDTH, polyorder=2, deriv=2, axis=0, mode="interp"
    )
    k = np.arange(N_FFT)
    tw_cos = np.cos(2 * np.pi * k / N_FFT)
    tw_sin = np.sin(2 * np.pi * k / N_FFT)

    empty = int((mel.sum(axis=1) == 0).sum())
    if empty:
        print(f"WARNING: {empty} of {N_MELS} mel filters are empty at sr={SAMPLE_RATE}")

    with path.open("w", encoding="utf-8") as f:
        f.write("/* Auto-generated by ml/train_wake_word.py — do not edit by hand */\n")
        f.write("#pragma once\n\n")
        f.write(f"#define WAKE_N_FRAMES {N_FRAMES}\n")
        f.write(f"#define WAKE_N_BINS {N_BINS}\n\n")
        for name, arr, dims in [
            ("WAKE_HANN", window, "[WAKE_N_FFT]"),
            ("WAKE_TW_COS", tw_cos, "[WAKE_N_FFT]"),
            ("WAKE_TW_SIN", tw_sin, "[WAKE_N_FFT]"),
            ("WAKE_MEL", mel, "[WAKE_N_MELS][WAKE_N_BINS]"),
            ("WAKE_DCT", dct, "[WAKE_N_MFCC][WAKE_N_MELS]"),
            ("WAKE_D1", d1, "[WAKE_N_FRAMES][WAKE_N_FRAMES]"),
            ("WAKE_D2", d2, "[WAKE_N_FRAMES][WAKE_N_FRAMES]"),
        ]:
            body = _c_matrix(arr) if np.ndim(arr) == 2 else _c_floats(arr)
            f.write(f"static const float {name}{dims} = {{\n{body}\n}};\n\n")


def export_test_vector(clf: Pipeline, wave: np.ndarray, path: Path) -> None:
    feats = waveform_to_features(wave)
    score = float(clf.decision_function(feats.reshape(1, -1))[0])
    with path.open("w", encoding="utf-8") as f:
        f.write("/* Auto-generated by ml/train_wake_word.py — do not edit by hand */\n")
        f.write("#pragma once\n\n")
        f.write("static const float WAKE_TEST_WAVE[WAKE_N_SAMPLES] = {\n")
        f.write(_c_floats(wave))
        f.write("\n};\n\n")
        f.write("static const float WAKE_TEST_FEATURES[WAKE_N_FEATURES] = {\n")
        f.write(_c_floats(feats))
        f.write("\n};\n\n")
        f.write(f"static const float WAKE_TEST_SCORE = {score:.9e}f;\n")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    print("ROOT =", ROOT)
    print(f"Model: logistic regression, sample rate {SAMPLE_RATE} Hz ({SAMPLE_PERIOD_MS} ms period)\n")

    hey_dir = DATA / "hey_stm"
    other_dir = DATA / "other"
    print("hey_stm dumps:", len(list(hey_dir.glob("buffer_dump_*.csv"))))
    print("other dumps:  ", len(list(other_dir.glob("buffer_dump_*.csv"))))

    X_list, y_list, names = [], [], []
    for xs, ys, ns in [
        load_class(hey_dir, 1),
        load_class(other_dir, 0),
    ]:
        X_list.extend(xs)
        y_list.extend(ys)
        names.extend(ns)

    if not X_list:
        raise SystemExit("No dumps found under data/hey_stm or data/other")

    X_wave = np.stack(X_list)
    y = np.array(y_list, dtype=np.int64)
    print("X_wave", X_wave.shape, "y", y.shape)
    print("class balance: hey_stm", int((y == 1).sum()), "other", int((y == 0).sum()))

    fig, axes = plt.subplots(2, 3, figsize=(12, 5), sharey=True)
    for ax, idx in zip(axes[0], np.where(y == 1)[0][:3]):
        ax.plot(X_wave[idx])
        ax.set_title(f"hey_stm\n{names[idx]}")
    for ax, idx in zip(axes[1], np.where(y == 0)[0][:3]):
        ax.plot(X_wave[idx], color="C1")
        ax.set_title(f"other\n{names[idx]}")
    fig.suptitle("Example dumps")
    fig.tight_layout()
    examples_path = OUT / "examples.png"
    fig.savefig(examples_path, dpi=120)
    plt.close(fig)
    print("wrote", examples_path)

    # Hold out raw waves first, then augment only the train split
    idx = np.arange(len(y))
    i_train, i_test = train_test_split(
        idx, test_size=0.25, random_state=42, stratify=y
    )
    waves_train, y_train = X_wave[i_train], y[i_train]
    waves_test, y_test = X_wave[i_test], y[i_test]

    print("Computing MFCC+delta features (with train augmentation)...")
    X_train, y_train = build_feature_matrix(waves_train, y_train, augment=True)
    X_test, y_test = build_feature_matrix(waves_test, y_test, augment=False)
    print("train features", X_train.shape, "test features", X_test.shape)

    print("\n=== 5-fold CV on original clips (no aug) ===")
    X_all = np.stack([waveform_to_features(w) for w in X_wave])
    cv = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
    cv_scores = cross_val_score(
        make_logreg(), X_all, y, cv=cv, scoring="accuracy", n_jobs=-1
    )
    cv_mean = float(cv_scores.mean())
    print(f"  logreg  CV acc = {cv_scores.mean():.3f} ± {cv_scores.std():.3f}")

    print("\n=== Hold-out test (augmented train) ===")
    clf = make_logreg()
    clf.fit(X_train, y_train)
    train_acc = float(clf.score(X_train, y_train))
    test_acc = float(clf.score(X_test, y_test))
    print(f"  logreg  train={train_acc:.3f}  test={test_acc:.3f}")

    y_pred = clf.predict(X_test)
    print(classification_report(y_test, y_pred, target_names=["other", "hey_stm"]))

    cm = confusion_matrix(y_test, y_pred)
    ConfusionMatrixDisplay(cm, display_labels=["other", "hey_stm"]).plot(cmap="Blues")
    plt.title("Test confusion matrix (logreg)")
    cm_path = OUT / "confusion_matrix.png"
    plt.savefig(cm_path, dpi=120, bbox_inches="tight")
    plt.close()
    print("wrote", cm_path)

    print("train acc", round(train_acc, 4))
    print("test  acc", round(test_acc, 4))
    print("CV    acc", round(cv_mean, 4))

    model_path = OUT / "wake_word_model.joblib"
    joblib.dump(
        {
            "pipeline": clf,
            "model_name": "logreg",
            "sample_rate": SAMPLE_RATE,
            "n_mfcc": N_MFCC,
            "n_fft": N_FFT,
            "hop": HOP,
            "n_mels": N_MELS,
            "n_samples": N_SAMPLES,
            "delta_width": DELTA_WIDTH,
            "feature_version": "mfcc+delta+delta2+rms+zcr",
            "label_names": ["other", "hey_stm"],
            "cv_accuracy": cv_mean,
            "test_accuracy": test_acc,
        },
        model_path,
    )
    print("saved", model_path)

    export_logreg_header(clf, OUT / "wake_word_model.h")
    export_feature_tables(OUT / "wake_tables.h")
    first_hey = sorted(hey_dir.glob("buffer_dump_*.csv"))[0]
    export_test_vector(clf, load_dump(first_hey), OUT / "wake_test_vector.h")
    print("wrote wake_word_model.h, wake_tables.h, wake_test_vector.h to", OUT)
    print("MCU: score = dot(WAKE_WEIGHTS, features) + WAKE_BIAS; hey_stm if score > 0")

    def predict_dump(path: Path):
        feats = waveform_to_features(load_dump(path)).reshape(1, -1)
        proba = float(clf.predict_proba(feats)[0, 1])
        return int(clf.predict(feats)[0]), proba

    print("\nhey_stm examples:")
    for p in sorted(hey_dir.glob("buffer_dump_*.csv"))[:5]:
        pred, proba = predict_dump(p)
        print(f"  {p.name}: pred={'hey_stm' if pred else 'other'}  P(hey)={proba:.3f}")

    print("other examples:")
    for p in sorted(other_dir.glob("buffer_dump_*.csv"))[:5]:
        pred, proba = predict_dump(p)
        print(f"  {p.name}: pred={'hey_stm' if pred else 'other'}  P(hey)={proba:.3f}")


if __name__ == "__main__":
    main()