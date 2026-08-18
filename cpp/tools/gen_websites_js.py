# -*- coding: utf-8 -*-
"""Generate qml/websites_data.js (pragma library) from qml/websites_data.json.

Run: python tools/gen_websites_js.py
The QML page imports websites_data.js directly (no XHR) because sync XHR to
qrc throws "Invalid state" inside Loader pages on this Qt build.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # cpp/
SRC = os.path.join(ROOT, "qml", "websites_data.json")
DST = os.path.join(ROOT, "qml", "websites_data.js")

with open(SRC, "r", encoding="utf-8") as f:
    data = json.load(f)

lines = []
lines.append("// SPDX-License-Identifier: AGPL-3.0-or-later")
lines.append("// Copyright (C) 2025-2026 影 / Shadow / xiaole1173")
lines.append("// 实用网站数据（由 qml/websites_data.json 生成，勿手改 —— 改 JSON 后运行 tools/gen_websites_js.py）")
lines.append(".pragma library")
lines.append("")
lines.append("var data = ")
lines.append(json.dumps(data, ensure_ascii=False, indent=2))
lines.append("")

with open(DST, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
print("written", DST)
