#!/usr/bin/env python3
"""模拟 OptiFine 安装全流程，验证 GLX.isUsingFBOs 是否被正确补丁。"""

import zipfile, os, shutil, subprocess, tempfile, json, sys, urllib.request, hashlib

MC_VER = "1.16.5"
GAME_DIR = os.path.abspath("D:/latest-code/cpp/build/Release/.minecraft")

def log(msg=""):
    print(msg, flush=True)

def check_jar_glx(path, label=""):
    if not os.path.exists(path):
        log(f"  [{label}] JAR 不存在")
        return
    size = os.path.getsize(path)
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        has_lw = "net/minecraft/launchwrapper/Launch.class" in names
        glx = [n for n in names if n.endswith("GLX.class")]
        deg = [n for n in names if n == "deg.class"]
        patched = {g: b"isUsingFBOs" in z.read(g) for g in glx} if glx else {}
        deg_fbo = b"isUsingFBOs" in z.read(deg[0]) if deg else None
        glx_sizes = {g: len(z.read(g)) for g in glx}
        log(f"  [{label}] size={size:,}B  entries={len(names)}  "
            f"hasLaunch={has_lw}  "
            f"GLX={glx_sizes}  FBO={patched}  "
            f"deg.fbo={deg_fbo}")


log("=" * 60)
log("OptiFine 安装全流程模拟")
log("=" * 60)
log()

# ──────────────────────────────────────────────
# 1. 下载 MC 1.16.5 原版文件
# ──────────────────────────────────────────────
log("=== 1. 下载 MC 1.16.5 原版文件 ===")
MC_DIR = os.path.join(tempfile.gettempdir(), "of_sim_mc", MC_VER)
os.makedirs(MC_DIR, exist_ok=True)
mc_jar = os.path.join(MC_DIR, MC_VER + ".jar")
mc_json = os.path.join(MC_DIR, MC_VER + ".json")

if not os.path.exists(mc_json):
    log("下载 version manifest...")
    urllib.request.urlretrieve(
        "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json",
        os.path.join(MC_DIR, "manifest.json"))
    with open(os.path.join(MC_DIR, "manifest.json")) as f:
        manifest = json.load(f)
    for v in manifest["versions"]:
        if v["id"] == MC_VER:
            log(f"下载 version JSON: {v['url']}")
            urllib.request.urlretrieve(v["url"], mc_json)
            break

# 从 version JSON 拿 client.jar URL
with open(mc_json) as f:
    vinfo = json.load(f)
client_url = vinfo["downloads"]["client"]["url"]
client_sha1 = vinfo["downloads"]["client"]["sha1"]

if not os.path.exists(mc_jar) or hashlib.sha1(open(mc_jar, "rb").read()).hexdigest() != client_sha1:
    log(f"下载 client.jar ({client_url})...")
    urllib.request.urlretrieve(client_url, mc_jar)
    actual = hashlib.sha1(open(mc_jar, "rb").read()).hexdigest()
    log(f"SHA1 匹配: {actual == client_sha1}")

log(f"原版 jar: {mc_jar}  size={os.path.getsize(mc_jar):,}B")
log(f"原版 json: {mc_json}")
check_jar_glx(mc_jar, "原版MC")
log()

# ──────────────────────────────────────────────
# 2. 准备 OptiFine JAR
# ──────────────────────────────────────────────
log("=== 2. OptiFine JAR ===")
of_candidates = [
    os.path.join(GAME_DIR, "libraries/optifine/OptiFine", f"{MC_VER}_HD_U_G7", f"OptiFine-{MC_VER}_HD_U_G7.jar"),
]
of_jar = None
for c in of_candidates:
    if os.path.exists(c):
        of_jar = c
        break

if not of_jar:
    log("下载 OptiFine JAR...")
    of_jar = os.path.join(tempfile.gettempdir(), f"OptiFine_{MC_VER}_HD_U_G7.jar")
    url = f"https://bmclapi2.bangbang93.com/optifine/{MC_VER}/HD_U/G7"
    urllib.request.urlretrieve(url, of_jar)

log(f"OptiFine JAR: {of_jar}  size={os.path.getsize(of_jar):,}B")
check_jar_glx(of_jar, "OptiFine原始")

# 检查是否含 Installer.class
has_installer = False
with zipfile.ZipFile(of_jar) as z:
    has_installer = "optifine/Installer.class" in z.namelist()
    has_profile = "install_profile.json" in z.namelist()
log(f"有 optifine/Installer.class: {has_installer}")
log(f"有 install_profile.json: {has_profile}")
log()

# ──────────────────────────────────────────────
# 3. setupTempMc 模拟
# ──────────────────────────────────────────────
TEMP_ROOT = tempfile.mkdtemp(prefix="of_sim_")
TEMP_MC = os.path.join(TEMP_ROOT, ".minecraft")
TEMP_VERS = os.path.join(TEMP_MC, "versions")
TEMP_LIBS = os.path.join(TEMP_MC, "libraries")
TEMP_VER_DIR = os.path.join(TEMP_VERS, MC_VER)
os.makedirs(TEMP_VER_DIR, exist_ok=True)
os.makedirs(TEMP_LIBS, exist_ok=True)

log("=== 3. setupTempMc 模拟 ===")
log(f"临时目录: {TEMP_ROOT}")
shutil.copy2(mc_json, os.path.join(TEMP_VER_DIR, MC_VER + ".json"))
shutil.copy2(mc_jar, os.path.join(TEMP_VER_DIR, MC_VER + ".jar"))
check_jar_glx(os.path.join(TEMP_VER_DIR, MC_VER + ".jar"), "temp中的原版jar(安装前)")
log()

# ──────────────────────────────────────────────
# 4. 运行 OptiFine 安装器
# ──────────────────────────────────────────────
log("=== 4. 运行 OptiFine 安装器 ===")
if not has_installer:
    log("ERROR: 无 optifine/Installer.class")
    sys.exit(1)

# 找 Java
java_path = None
for name in ["javaw", "java"]:
    try:
        r = subprocess.run([name, "-version"], capture_output=True, text=True, timeout=5)  # noqa
        if r.returncode == 0:
            java_path = name
            break
    except:
        pass

if not java_path:
    log("ERROR: 未找到 Java")
    sys.exit(1)

temp_mc_parent = os.path.dirname(TEMP_MC)
jargs = [f"-Duser.home={temp_mc_parent}", "-cp", of_jar, "optifine.Installer"]
env = os.environ.copy()
env["APPDATA"] = temp_mc_parent

log(f"工作目录: {temp_mc_parent}")
log(f"命令行: {java_path} {' '.join(jargs)}")

r = subprocess.run(
    [java_path] + jargs,
    capture_output=True, text=True,
    cwd=temp_mc_parent, env=env, timeout=30)

log(f"退出码: {r.returncode}")
if r.stdout:
    lines = r.stdout.strip().split("\n")
    for line in lines[:15]:
        log(f"  out | {line}")
    if len(lines) > 15:
        log(f"  out | ... (共 {len(lines)} 行)")
install_ok = (r.returncode == 0 and len(r.stdout) >= 1000
              and not (lines[-1] if lines else "").startswith("at "))
log(f"安装检查: exit={r.returncode}  out_len={len(r.stdout)}  ok={install_ok}")
log()

# ──────────────────────────────────────────────
# 5. 检查 temp 目录
# ──────────────────────────────────────────────
log("=" * 60)
log("5. 安装后临时目录分析")
log("=" * 60)

# 5a. versions
log()
log("--- 5a. versions/ ---")
versions_dir = os.path.join(TEMP_MC, "versions")
if os.path.exists(versions_dir):
    for vd in sorted(os.listdir(versions_dir)):
        vdp = os.path.join(versions_dir, vd)
        if not os.path.isdir(vdp):
            continue
        log(f"\n[版本] {vd}/")
        for f in sorted(os.listdir(vdp)):
            fp = os.path.join(vdp, f)
            log(f"  {f}: {os.path.getsize(fp):,}B")
            if f.endswith(".json"):
                with open(fp) as jf:
                    j = json.load(jf)
                    log(f"    inheritsFrom: {j.get('inheritsFrom', 'N/A')}")
                    log(f"    jar: {j.get('jar', 'N/A')}")
                    log(f"    mainClass: {j.get('mainClass', 'N/A')}")
                    for l in j.get("libraries", []):
                        log(f"    lib: {l.get('name', '?')}")
            if f.endswith(".jar"):
                check_jar_glx(fp, f"{vd}")

# 5b. libraries
log()
log("--- 5b. libraries/ ---")
if os.path.exists(TEMP_LIBS):
    for root, dirs, files in os.walk(TEMP_LIBS):
        for f in files:
            if f.endswith(".jar"):
                fp = os.path.join(root, f)
                rel = os.path.relpath(fp, TEMP_LIBS)
                check_jar_glx(fp, f"lib:{rel}")

# ──────────────────────────────────────────────
# 6. 模拟 copyOptifineTempMc + flatten
# ──────────────────────────────────────────────
log()
log("=" * 60)
log("6. 模拟 copyOptifineTempMc + flatten")
log("=" * 60)

SIM_GAME = os.path.join(TEMP_ROOT, "sim_game")
SIM_VERSIONS = os.path.join(SIM_GAME, "versions")
SIM_LIBS = os.path.join(SIM_GAME, "libraries")
os.makedirs(SIM_VERSIONS, exist_ok=True)
os.makedirs(SIM_LIBS, exist_ok=True)

# 6a. copyOptifineTempMc: 复制版本（先删 MC_VER 目录）
of_ver_id = None
for vd in sorted(os.listdir(versions_dir)):
    if vd != MC_VER:
        of_ver_id = vd

log(f"OptiFine 版本 ID: {of_ver_id}")

# 删除游戏目录中原版 MC 版本目录
mc_sim = os.path.join(SIM_VERSIONS, MC_VER)
if os.path.exists(mc_sim):
    shutil.rmtree(mc_sim)
    log(f"已删除原版 MC 版本目录")

# 复制所有版本
for vd in sorted(os.listdir(versions_dir)):
    src = os.path.join(versions_dir, vd)
    dst = os.path.join(SIM_VERSIONS, vd)
    if os.path.isdir(src):
        shutil.copytree(src, dst)
        for f in sorted(os.listdir(dst)):
            fp = os.path.join(dst, f)
            if f.endswith(".jar"):
                check_jar_glx(fp, f"复制后-{vd}")

log()
log("--- 复制后版本目录 ---")
for vd in sorted(os.listdir(SIM_VERSIONS)):
    vdp = os.path.join(SIM_VERSIONS, vd)
    if os.path.isdir(vdp):
        log(f"[版本] {vd}/")
        for f in sorted(os.listdir(vdp)):
            log(f"  {f}: {os.path.getsize(os.path.join(vdp, f)):,}B")

# 6b. flattenOptifineVersion
if of_ver_id:
    of_dir = os.path.join(SIM_VERSIONS, of_ver_id)
    of_json_path = os.path.join(of_dir, of_ver_id + ".json")
    log()
    log("--- flattenOptifineVersion ---")
    
    with open(of_json_path) as jf:
        jdata = json.load(jf)
    inherits = jdata.get("inheritsFrom", "")
    log(f"inheritsFrom: {inherits}")
    
    if inherits:
        inherited_jar = os.path.join(SIM_VERSIONS, inherits, inherits + ".jar")
        target_jar = os.path.join(of_dir, of_ver_id + ".jar")
        log(f"继承 jar: {inherited_jar}")
        log(f"目标 jar: {target_jar}")
        
        if os.path.exists(inherited_jar) and not os.path.exists(target_jar):
            shutil.copy2(inherited_jar, target_jar)
            log(f"patched jar 已复制 ({os.path.getsize(target_jar):,}B)")
        
        # 去掉 inheritsFrom
        jdata.pop("inheritsFrom", None)
        with open(of_json_path, "w") as jf:
            json.dump(jdata, jf, indent=2)
        log(f"JSON 已拍平")
        
        # 删除原版 MC 目录
        inherited_dir = os.path.join(SIM_VERSIONS, inherits)
        if os.path.exists(inherited_dir):
            shutil.rmtree(inherited_dir)
            log(f"已删除原版 MC 目录: {inherited_dir}")

log()
log("--- 拍平后版本 ---")
for vd in sorted(os.listdir(SIM_VERSIONS)):
    vdp = os.path.join(SIM_VERSIONS, vd)
    if os.path.isdir(vdp):
        log(f"[版本] {vd}/")
        for f in sorted(os.listdir(vdp)):
            fp = os.path.join(vdp, f)
            log(f"  {f}: {os.path.getsize(fp):,}B")
            if f.endswith(".jar"):
                check_jar_glx(fp, f"最终-{vd}")

# ──────────────────────────────────────────────
# 7. 类路径模拟 + 结论
# ──────────────────────────────────────────────
log()
log("=" * 60)
log("7. 启动类路径分析")
log("=" * 60)

if of_ver_id:
    final_jar = os.path.join(SIM_VERSIONS, of_ver_id, of_ver_id + ".jar")
    log(f"最终 jar: {final_jar}  exist={os.path.exists(final_jar)}")
    if os.path.exists(final_jar):
        check_jar_glx(final_jar, "最终jar")
else:
    log("未找到 OptiFine 版本")

log()
log("=" * 60)
log("8. 关键发现")
log("=" * 60)
log()
log(f"OptiFine 原始 JAR (安装器) 中的 GLX 有 isUsingFBOs: ", end="")
with zipfile.ZipFile(of_jar) as z:
    has_fbo = b"isUsingFBOs" in z.read("com/mojang/blaze3d/platform/GLX.class")
    log(f"{has_fbo}")
log()

# 检查安装后 temp 中的 jar
for vd in sorted(os.listdir(versions_dir)):
    vdp = os.path.join(versions_dir, vd)
    for f in sorted(os.listdir(vdp)):
        fp = os.path.join(vdp, f)
        if f.endswith(".jar"):
            with zipfile.ZipFile(fp) as z:
                glx_entries = [n for n in z.namelist() if n.endswith("GLX.class")]
                for g in glx_entries:
                    fbo = b"isUsingFBOs" in z.read(g)
                    log(f"安装后 temp/{vd}/{f}:{g} FBO={fbo}")

log()
log(f"保留临时目录: {TEMP_ROOT}")
