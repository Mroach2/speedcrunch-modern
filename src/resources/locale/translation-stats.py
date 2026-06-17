#!/usr/bin/env python3
"""
Translation statistics for Qt Linguist TS/QM files.

This repository ships Qt translation source files (*.ts) and compiled catalogs
(*.qm). A TS file is an XML document with <message> entries containing a
<translation> element. Untranslated entries are typically marked as:

  <translation type="unfinished">...</translation>

or have an empty translation text.
"""

from __future__ import annotations

import argparse
import os
import sys
import xml.etree.ElementTree as ET


MessageKey = tuple[str, str]


def _pct(n: int, d: int) -> float:
    if d <= 0:
        return 100.0
    return (100.0 * n) / d


def _is_untranslated(tr_el: ET.Element | None) -> bool:
    if tr_el is None:
        return True
    # Qt uses type="unfinished" for missing translations.
    if (tr_el.get("type") or "").strip().lower() == "unfinished":
        return True
    # Some catalogs still carry empty strings without the attribute.
    txt = (tr_el.text or "").strip()
    return txt == ""


def _is_inactive_message(msg_el: ET.Element) -> bool:
    # Message-level inactive marker (less common).
    if (msg_el.get("type") or "").strip().lower() in {"vanished", "obsolete"}:
        return True
    # Qt often stores inactive state on the <translation> node instead.
    tr_el = msg_el.find("translation")
    if tr_el is not None and (tr_el.get("type") or "").strip().lower() in {"vanished", "obsolete"}:
        return True
    return False


def _message_key(msg_el: ET.Element, ctx_name: str) -> MessageKey:
    src = (msg_el.findtext("source") or "").strip()
    return (ctx_name, src)


def _ts_messages(ts_path: str) -> dict[MessageKey, bool]:
    tree = ET.parse(ts_path)
    root = tree.getroot()
    messages: dict[MessageKey, bool] = {}

    for ctx in root.findall("context"):
        ctx_name = (ctx.findtext("name") or "").strip()
        for msg in ctx.findall("message"):
            # Ignore inactive messages; they don't ship in compiled catalogs.
            if _is_inactive_message(msg):
                continue

            tr_el = msg.find("translation")
            messages[_message_key(msg, ctx_name)] = not _is_untranslated(tr_el)

    return messages


def _ts_message_stats(ts_path: str) -> tuple[int, int, dict[MessageKey, bool]]:
    messages = _ts_messages(ts_path)
    translated = sum(1 for is_translated in messages.values() if is_translated)
    return len(messages), translated, messages


def _qm_info(qm_path: str) -> dict:
    try:
        st = os.stat(qm_path)
        return {"present": True, "size": st.st_size, "mtime": int(st.st_mtime)}
    except FileNotFoundError:
        return {"present": False, "size": 0, "mtime": 0}


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(
        description="Print translation statistics for src/resources/locale (*.ts/*.qm)."
    )
    script_dir = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument(
        "--root",
        default=script_dir,
        help="Locale directory (default: directory containing this script)",
    )
    ap.add_argument(
        "--samples",
        type=int,
        default=0,
        help="Show up to N sample untranslated source strings per language.",
    )
    args = ap.parse_args(argv)

    root_dir = os.path.abspath(args.root)
    if not os.path.isdir(root_dir):
        print(f"error: not a directory: {root_dir}", file=sys.stderr)
        return 2

    ts_files = sorted(
        f for f in os.listdir(root_dir) if f.endswith(".ts") and os.path.isfile(os.path.join(root_dir, f))
    )
    if not ts_files:
        print(f"No .ts files found under {root_dir}")
        return 0

    rows = []
    g_total = g_translated = g_unfinished = 0

    template_path = os.path.join(root_dir, "en_US.ts")
    if not os.path.isfile(template_path):
        print(f"error: missing canonical template: {template_path}", file=sys.stderr)
        return 2
    template_messages = _ts_messages(template_path)
    template_keys = set(template_messages.keys())

    for ts_name in ts_files:
        ts_path = os.path.join(root_dir, ts_name)
        lang = ts_name[:-3]
        if lang == "en_US":
            continue
        qm_name = lang + ".qm"

        total, translated, lang_messages = _ts_message_stats(ts_path)

        # Keep some samples for optional debug output.
        samples: list[tuple[str, str]] = []
        for key in sorted(template_keys - set(lang_messages.keys())):
            if key[1]:
                samples.append(key)

        unfinished = total - translated

        g_total += total
        g_translated += translated
        g_unfinished += unfinished

        rows.append(
            {
                "lang": lang,
                "total": total,
                "translated": translated,
                "unfinished": unfinished,
                "completion": _pct(translated, total),
                "samples": samples,
            }
        )

    print("Qt TS/QM Translation Statistics")
    print(f"Locale root: {root_dir}")
    print(f"Languages: {len(rows)}\n")

    header = f"{'language':<12} {'total':>7} {'translated':>11} {'unfinished':>11} {'completion':>11}"
    print(header)
    for r in rows:
        print(
            f"{r['lang']:<12} {r['total']:>7} {r['translated']:>11} {r['unfinished']:>11} {r['completion']:>10.1f}%"
        )

        if args.samples > 0:
            shown = 0
            for ctx_name, src in r["samples"]:
                if shown >= args.samples:
                    break
                # Keep this single-line for greppable output.
                print(f"  - [{ctx_name}] {src}")
                shown += 1

    print(
        f"\nGlobal: languages={len(rows)} total={g_total} translated={g_translated} unfinished={g_unfinished} completion={_pct(g_translated, g_total):.1f}%"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
