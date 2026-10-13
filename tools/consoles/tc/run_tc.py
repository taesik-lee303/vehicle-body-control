#!/usr/bin/env python3
"""모듈별 테스트 케이스(tools/consoles/tc/*.txt)를 실행하고 결과 문서를 만든다.

  python3 tools/consoles/tc/run_tc.py          # make tc 와 같음

- 각 TC 파일을 해당 콘솔(build/con_<모듈>)에 넣어 실행하고 로그를 docs/test/logs/<모듈>.log 에 저장한다.
- TC 파일의 `tc ID | 요구사항 | 기법 | 입력 | 기대` 줄과 로그의 PASS/FAIL/OBS 줄을 묶어
  docs/test/tc-results.md 를 만든다. 문서는 손으로 고치지 않는다 (다시 실행하면 덮어씀).
- expect(e)가 하나라도 FAIL이면 종료 코드 1.
"""

import datetime
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TC_DIR = ROOT / "tools" / "consoles" / "tc"
OUT_DIR = ROOT / "docs" / "test"
LOG_DIR = OUT_DIR / "logs"

MODULES = [
    ("ecu", "BCU 전체 (ECU)"),
    ("doors", "도어·잠금 (D1~D3)"),
    ("dashboard", "계기판 경고 (D4·D5)"),
    ("lighting", "조명 (L1~L6)"),
    ("wipers", "와이퍼 (W1~W5)"),
]

RE_PASS = re.compile(r"✔ PASS \[(.+?)\] (\S+) = (\S+)  \(([\d.]+)s\)")
RE_FAIL = re.compile(r"✘ FAIL \[(.+?)\] (\S+): 기대 (\S+), 실제 (\S+)  \(([\d.]+)s\)")
RE_OBS = re.compile(r"◎ OBS  \[(.+?)\] (\S+) = (\S+)  \(([\d.]+)s\)")
RE_REQ = re.compile(r"\b(C[1-5]|L[1-6]|W[1-5]|D[1-5]|Q\d+|SWR-[\w-]+)\b")


def git(*args):
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def parse_script(path):
    tcs = []
    for line in path.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if s.lower().startswith("tc "):
            parts = [p.strip() for p in s[3:].split("|")]
            parts += [""] * (5 - len(parts))
            tcs.append({"id": parts[0], "req": parts[1], "tech": parts[2], "input": parts[3], "expect": parts[4],
                        "pass": [], "fail": [], "obs": []})
    return tcs


def run_module(name):
    exe = ROOT / "build" / f"con_{name}"
    script = TC_DIR / f"{name}.txt"
    with script.open(encoding="utf-8") as f:
        res = subprocess.run([str(exe)], stdin=f, capture_output=True, text=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    (LOG_DIR / f"{name}.log").write_text(res.stdout, encoding="utf-8")
    tcs = parse_script(script)
    by_id = {t["id"]: t for t in tcs}
    for line in res.stdout.splitlines():
        for rx, key in ((RE_PASS, "pass"), (RE_FAIL, "fail"), (RE_OBS, "obs")):
            m = rx.search(line)
            if m and m.group(1) in by_id:
                by_id[m.group(1)][key].append(m.groups()[1:])
    return tcs


def verdict(t):
    if t["fail"]:
        return "FAIL"
    if t["pass"] and t["obs"]:
        return "PASS + 관찰"
    if t["pass"]:
        return "PASS"
    if t["obs"]:
        return "관찰"
    return "확인 없음"


def actual(t):
    out = []
    for item, want, got, ts in t["fail"]:
        out.append(f"**{item}: 기대 {want}, 실제 {got}** ({ts}s)")
    if t["pass"]:
        out.append(f"기대와 같음 (확인 {len(t['pass'])}건: " + ", ".join(f"{i}={v}" for i, v, _ in t["pass"]) + ")")
    for item, v, ts in t["obs"]:
        out.append(f"관찰 {item}={v} ({ts}s)")
    return "<br>".join(out) or "-"


def esc(s):
    return s.replace("|", "\\|")


def main():
    results = {name: run_module(name) for name, _ in MODULES}
    commit = git("rev-parse", "--short", "HEAD")
    dirty = " (커밋 안 된 변경 포함)" if git("status", "--porcelain", "--", "src", "tools") else ""
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")

    L = []
    L.append("# 모듈별 테스트 케이스 실행 결과")
    L.append("")
    L.append(f"> 자동 생성 문서 (`make tc`, {now}, 커밋 `{commit}`{dirty}). 손으로 고치지 마세요.")
    L.append("> TC 정의: `tools/consoles/tc/<모듈>.txt`, 실행 로그(증거): `docs/test/logs/<모듈>.log`. 사용법은 `docs/test/README.md`.")
    L.append("")
    L.append("판정: **PASS** 사양서에서 계산한 기대와 같음 / **FAIL** 다름 / **관찰** 사양서에 정해지지 않은 동작이라 기대 없이 기록만 함.")
    L.append("")
    L.append("## 요약")
    L.append("")
    L.append("| 모듈 | TC | PASS | FAIL | 관찰 포함 | 확인(e) 건수 | 관찰(obs) 건수 |")
    L.append("|---|---|---|---|---|---|---|")
    total_fail = 0
    for name, title in MODULES:
        tcs = results[name]
        n_fail = sum(1 for t in tcs if t["fail"])
        total_fail += sum(len(t["fail"]) for t in tcs)
        L.append(f"| {title} | {len(tcs)} | {sum(1 for t in tcs if t['pass'] and not t['fail'])} | {n_fail} | "
                 f"{sum(1 for t in tcs if t['obs'])} | {sum(len(t['pass']) + len(t['fail']) for t in tcs)} | "
                 f"{sum(len(t['obs']) for t in tcs)} |")
    L.append("")

    for name, title in MODULES:
        L.append(f"## {title}")
        L.append("")
        L.append(f"실행: `./build/con_{name} < tools/consoles/tc/{name}.txt` · 로그: `docs/test/logs/{name}.log`")
        L.append("")
        L.append("| TC ID | 요구사항 | 기법 | 입력 | 기대 (사양서) | 실제 | 판정 |")
        L.append("|---|---|---|---|---|---|---|")
        for t in results[name]:
            L.append(f"| {t['id']} | {esc(t['req'])} | {esc(t['tech'])} | {esc(t['input'])} | {esc(t['expect'])} | "
                     f"{esc(actual(t))} | {verdict(t)} |")
        L.append("")

    L.append("## 요구사항별 TC")
    L.append("")
    L.append("요구사항 칸에 적힌 ID 기준입니다. 표에 없거나 TC가 적은 요구사항은 아직 덜 시험된 곳입니다.")
    L.append("")
    cover = {}
    for name, _ in MODULES:
        for t in results[name]:
            for r in sorted(set(RE_REQ.findall(t["req"]))):
                cover.setdefault(r, []).append(t["id"])

    def key(r):
        m = re.match(r"([A-Z]+)(\d+)", r)
        return (m.group(1), int(m.group(2))) if m else (r, 0)

    L.append("| 요구사항 | TC 수 | TC |")
    L.append("|---|---|---|")
    for r in sorted(cover, key=key):
        L.append(f"| {r} | {len(cover[r])} | {', '.join(cover[r])} |")
    L.append("")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "tc-results.md").write_text("\n".join(L) + "\n", encoding="utf-8")
    for name, title in MODULES:
        tcs = results[name]
        print(f"{title}: TC {len(tcs)}, FAIL TC {sum(1 for t in tcs if t['fail'])}, 관찰 TC {sum(1 for t in tcs if t['obs'])}")
    print(f"-> {OUT_DIR / 'tc-results.md'}")
    return 1 if total_fail else 0


if __name__ == "__main__":
    sys.exit(main())
