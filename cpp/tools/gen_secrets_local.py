# -*- coding: utf-8 -*-
"""
gen_secrets_local.py — 生成本地真实值头文件 src/secrets_local.h（git 忽略）。

用法：python tools/gen_secrets_local.py --azure 1167b841-... --owner xiaole1173 --repo shadow-launcher
说明：生成的 secrets_local.h 被 .gitignore 忽略，仅存在于开发者机器；
      仓库内 src/secrets.h 为占位符版本，两者通过 __has_include 联动。
"""
import argparse
import io
import os

OUT = os.path.join(os.path.dirname(__file__), '..', 'src', 'secrets_local.h')

TEMPLATE = '''// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// secrets_local.h — 本地真实值（.gitignore 忽略，严禁提交）。
// 由 tools/gen_secrets_local.py 生成；真实值覆盖 src/secrets.h 的占位符。

#pragma once

// ── 微软登录 Azure App ID（真实值，仅本地）──
#define SHADOW_AZURE_CLIENT_ID "{azure}"

// ── 更新服务器仓库（真实值，仅本地）──
#define SHADOW_GITEE_OWNER "{owner}"
#define SHADOW_GITEE_REPO "{repo}"
'''

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--azure', required=True, help='Azure App (client) ID')
    ap.add_argument('--owner', required=True, help='Gitee owner')
    ap.add_argument('--repo', required=True, help='Gitee repo')
    args = ap.parse_args()

    content = TEMPLATE.format(azure=args.azure, owner=args.owner, repo=args.repo)
    out_path = os.path.normpath(OUT)
    with io.open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)
    print('generated:', out_path)

if __name__ == '__main__':
    main()
