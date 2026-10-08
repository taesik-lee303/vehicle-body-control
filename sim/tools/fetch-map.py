#!/usr/bin/env python3
"""세종대학교 주변 실제 도로(OpenStreetMap)를 시뮬레이터 지도 sim/assets/map/sejong.json 으로 만든다.

한 번만 실행하면 된다 (인터넷 필요). 결과 파일은 저장소에 커밋되어 있어 보통은 다시 받을 필요가 없다.
지도 데이터: (c) OpenStreetMap contributors, ODbL 1.0 (https://www.openstreetmap.org/copyright)

  python3 sim/tools/fetch-map.py               # 받아서 변환
  python3 sim/tools/fetch-map.py --cache DIR   # 받은 원본을 DIR에 저장하고, 있으면 다시 쓴다

좌표: 원점은 세종대학교, 단위 m. 화면 좌표와 같게 x = 동쪽, z = 남쪽(북쪽이 -z).
주행 경로: 능동로 → 광나루로 → 군자로를 도는 순환로를 일방통행을 지키며 자동으로 찾는다.
"""
import argparse
import heapq
import json
import math
import os
import sys
import time
import urllib.parse
import urllib.request

ORIGIN = (37.5503, 127.0731)                 # 세종대학교 (위도, 경도)
BBOX = (37.540, 127.060, 37.560, 127.086)    # 남, 서, 북, 동
LOOP_NAMES = ('능동로', '광나루로', '군자로')   # 학교를 감싸는 도로 (이 도로를 우선해서 달린다)
# 순환로가 꼭 지나야 할 점 (동, 북 m): 각 도로의 중간. 교차로에서 도로를 스치기만 하는 짧은 고리를 막는다
VIA = [(240.0, 0.0), (-80.0, -285.0), (-166.0, 100.0), (430.0, 450.0)]   # 능동로, 광나루로, 군자로, 능동로 북쪽(군자역 쪽)
VIA_R = 35.0
CAMPUS = (80.0, 100.0)                       # 학교 가운데 (동, 북 m). 순환로가 이 점을 감싸야 한다
START_NEAR = (150.0, -60.0)                  # 출발점: 능동로의 세종대 정문 부근
KEEP_M = 650                                 # 경로에서 이 거리 안의 도로·건물·녹지만 남긴다
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'map', 'sejong.json')
SERVERS = ['https://overpass-api.de/api/interpreter', 'https://overpass.kumi.systems/api/interpreter']

ROAD_CLASSES = ['trunk', 'primary', 'secondary', 'tertiary', 'unclassified', 'residential',
                'trunk_link', 'primary_link', 'secondary_link', 'tertiary_link']
# 차로 수 기본값 (lanes 태그가 없을 때). 일방통행이면 그 방향 차로 수, 아니면 양방향 합
DEFAULT_LANES = {'trunk': (3, 4), 'primary': (3, 6), 'secondary': (2, 4), 'tertiary': (1, 2),
                 'unclassified': (1, 2), 'residential': (1, 2)}
LANE_W = {'trunk': 3.4, 'primary': 3.3, 'secondary': 3.2, 'tertiary': 3.0, 'unclassified': 2.9, 'residential': 2.8}

Q_ROADS = ('[out:json][timeout:90];('
           'way["highway"~"^(' + '|'.join(ROAD_CLASSES) + ')$"]({b});'
           'way["building"]({b}););out body;>;out skel qt;')
Q_GREEN = ('[out:json][timeout:90];('
           'way["leisure"~"^(park|garden|pitch|playground)$"]({b});'
           'way["landuse"~"^(grass|forest|recreation_ground|village_green)$"]({b});'
           'way["natural"~"^(wood|scrub|water)$"]({b}););out geom;')
Q_CAMPUS = '[out:json][timeout:60];way["amenity"="university"]({b});out geom;'

# 지형: AWS 공개 Terrain Tiles (Terrarium PNG, 키 불필요). 한국은 SRTM 30m 기반이라 건물·나무 높이가 섞여 있어 평활화한다
TERRAIN_URL = 'https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png'
TERRAIN_Z = 15
TERRAIN_STEP = 10.0          # 저장하는 높이 격자 간격 (m)
TERRAIN_SIGMA = 25.0         # 평활화 (m). 건물·나무가 만든 울퉁불퉁함을 지운다
OPEN_M = 50.0                # 이보다 작은 솟은 것(건물·숲)은 깎는다 (m)
OVERRIDES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'map-overrides.json')


def overpass(query, cache_file):
    if cache_file and os.path.exists(cache_file):
        with open(cache_file, encoding='utf-8') as f:
            return json.load(f)
    body = urllib.parse.urlencode({'data': query.format(b=','.join(map(str, BBOX)))}).encode()
    last = None
    for attempt in range(6):
        url = SERVERS[attempt % len(SERVERS)]
        try:
            req = urllib.request.Request(url, data=body, headers={'User-Agent': 'vbc-sim (education project)'})
            with urllib.request.urlopen(req, timeout=120) as r:
                text = r.read().decode('utf-8')
            data = json.loads(text)
            if cache_file:
                with open(cache_file, 'w', encoding='utf-8') as f:
                    f.write(text)
            return data
        except Exception as e:  # 서버가 바쁘면 504·500이 자주 난다. 잠시 뒤 다른 서버로
            last = e
            print(f'  {url}: {e} → 다시 시도', file=sys.stderr)
            time.sleep(5 + attempt * 5)
    raise SystemExit(f'Overpass에서 받지 못했습니다: {last}')


def png_rgb(data):
    """8비트 RGB/RGBA PNG → (폭, 높이, 픽셀 함수). 표준 라이브러리만 쓴다 (zlib)."""
    import struct
    import zlib
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'PNG가 아닙니다'
    pos, idat, w = 8, b'', 0
    while pos < len(data):
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
            assert depth == 8 and ctype in (2, 6), f'지원하지 않는 PNG 형식 {depth}/{ctype}'
            bpp = 3 if ctype == 2 else 4
        elif kind == b'IDAT':
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    out = bytearray(h * stride)
    prev = bytearray(stride)
    for r in range(h):
        f = raw[r * (stride + 1)]
        line = bytearray(raw[r * (stride + 1) + 1:(r + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out[r * stride:(r + 1) * stride] = line
        prev = line
    return w, h, lambda x, y: out[y * stride + x * bpp:y * stride + x * bpp + 3]


def terrain_grid(bounds, cache):
    """bounds(동·북 m: x0, y0, x1, y1) 안의 지면 높이 격자. 값은 m (해발)."""
    z = TERRAIN_Z
    n = 2 ** z
    tiles = {}

    def tile(tx, ty):
        if (tx, ty) not in tiles:
            path = cache and os.path.join(cache, f'terrain_{z}_{tx}_{ty}.png')
            if path and os.path.exists(path):
                data = open(path, 'rb').read()
            else:
                url = TERRAIN_URL.format(z=z, x=tx, y=ty)
                req = urllib.request.Request(url, headers={'User-Agent': 'vbc-sim (education project)'})
                with urllib.request.urlopen(req, timeout=60) as r:
                    data = r.read()
                if path:
                    open(path, 'wb').write(data)
            tiles[(tx, ty)] = png_rgb(data)
        return tiles[(tx, ty)]

    def height(lat, lon):
        fx = (lon + 180) / 360 * n * 256
        fy = (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2 * n * 256
        px, py = int(fx), int(fy)
        _, _, pix = tile(px // 256, py // 256)
        r, g, b = pix(px % 256, py % 256)
        return r * 256 + g + b / 256 - 32768

    x0, y0, x1, y1 = bounds
    nx = int((x1 - x0) / TERRAIN_STEP) + 1
    ny = int((y1 - y0) / TERRAIN_STEP) + 1
    k = math.cos(math.radians(ORIGIN[0])) * 111320.0
    grid = [[height(ORIGIN[0] + (y0 + j * TERRAIN_STEP) / 110540.0, ORIGIN[1] + (x0 + i * TERRAIN_STEP) / k)
             for i in range(nx)] for j in range(ny)]
    # 열림 연산(최소 → 최대 필터): 반경 OPEN_M보다 작은 솟은 것(건물·숲)을 깎아 지표면에 가깝게 한다
    W = int(OPEN_M / TERRAIN_STEP)

    def morph(rows, op):
        return [[op(row[max(0, i - W):i + W + 1]) for i in range(len(row))] for row in rows]

    def cols(g):
        return [list(c) for c in zip(*g)]
    grid = cols(morph(cols(morph(grid, min)), min))
    grid = cols(morph(cols(morph(grid, max)), max))
    # 가우시안 평활화 (가로, 세로 따로)
    R = int(TERRAIN_SIGMA / TERRAIN_STEP * 3)
    wts = [math.exp(-((d * TERRAIN_STEP) ** 2) / (2 * TERRAIN_SIGMA ** 2)) for d in range(-R, R + 1)]

    def blur(rows):
        out = []
        for row in rows:
            m = len(row)
            out.append([sum(row[min(m - 1, max(0, i + d))] * w for d, w in zip(range(-R, R + 1), wts)) / sum(wts) for i in range(m)])
        return out
    grid = blur(grid)
    grid = [list(c) for c in zip(*blur([list(c) for c in zip(*grid)]))]
    return nx, ny, grid


def load_overrides():
    if not os.path.exists(OVERRIDES):
        return {}
    with open(OVERRIDES, encoding='utf-8') as f:
        return {k: v for k, v in json.load(f).get('buildings', {}).items() if isinstance(v, dict)}


def project(lat, lon):
    """위도·경도 → (동, 북) m. 2km 안쪽이라 평면 근사로 충분하다."""
    return ((lon - ORIGIN[1]) * 111320.0 * math.cos(math.radians(ORIGIN[0])), (lat - ORIGIN[0]) * 110540.0)


def lanes_of(tags):
    hw = tags['highway'].replace('_link', '')
    oneway = tags.get('oneway') in ('yes', 'true', '1', '-1') or tags['highway'].endswith('_link') and tags.get('oneway') != 'no'
    try:
        n = int(str(tags.get('lanes', '')).split(';')[0])
    except ValueError:
        n = 0
    if n <= 0:
        n = 1 if tags['highway'].endswith('_link') else DEFAULT_LANES[hw][0 if oneway else 1]
    n = min(n, 6 if oneway else 8)     # 잘못 적힌 큰 값(예: 10) 막기
    return n, oneway, LANE_W.get(hw, 3.0)


def car_offset(n, oneway, lw):
    """차가 달릴 차로 가운데가 도로 중심선에서 오른쪽으로 몇 m인지. 맨 오른쪽(버스·우회전) 바로 안쪽 차로."""
    if oneway:
        i = n - 1 if n <= 2 else n - 2
        return (i + 0.5) * lw - n * lw / 2
    half = max(1, n // 2)
    k = 0 if half <= 1 else half - 2
    return (k + 0.5) * lw


def point_in_poly(p, poly):
    x, y = p
    inside = False
    for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]):
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            inside = not inside
    return inside


def find_loop(nodes, roads):
    """VIA를 모두 지나 출발점으로 돌아오는 가장 짧은 순환로 (일방통행 준수)."""
    graph = {}
    for w in roads:
        t = w['tags']
        name = t.get('name', '')
        if t['highway'] == 'residential':        # 골목은 쓰지 않는다
            continue
        is_link = t['highway'].endswith('_link')
        # 세 도로 사이를 잇는 다른 도로(천호대로 등)는 쓸 수 있지만 비싸게: 되도록 세 도로 위를 달린다
        factor = 1.0 if name in LOOP_NAMES else 1.3 if is_link else 2.0
        n, oneway, lw = lanes_of(t)
        fwd = t.get('oneway') != '-1'
        back = not oneway or t.get('oneway') == '-1'
        ids = [i for i in w['nodes'] if i in nodes]
        for a, b in zip(ids, ids[1:]):
            cost = math.dist(nodes[a], nodes[b]) * factor
            info = (w['id'], car_offset(n, oneway, lw), name)
            if fwd:
                graph.setdefault(a, []).append((b, cost, info))
            if back:
                graph.setdefault(b, []).append((a, cost, info))
    bits = {i: sum(1 << k for k, p in enumerate(VIA) if math.dist(nodes[i], p) < VIA_R) for i in graph}
    full = (1 << len(VIA)) - 1
    starts = [i for i in graph if bits[i] & 1]
    best = None
    for s in starts:
        m0 = bits[s]
        dist = {(s, m0): 0.0}
        prev = {}
        pq = [(0.0, s, m0)]
        found = None
        while pq:
            d, u, m = heapq.heappop(pq)
            if d > dist.get((u, m), 1e18):
                continue
            if u == s and m == full and d > 0:
                found = d
                break
            if best and d >= best[0]:
                break
            for v, c, info in graph.get(u, []):
                nm = m | bits.get(v, 0)
                if v == s and nm != full:
                    continue
                key, nd = (v, nm), d + c
                if nd < dist.get(key, 1e18):
                    dist[key] = nd
                    prev[key] = ((u, m), info)
                    heapq.heappush(pq, (nd, v, nm))
        if found is None:
            continue
        path, key = [], (s, full)
        while key != (s, m0):
            pk, info = prev[key]
            path.append((key[0], info))
            key = pk
        path.reverse()
        poly = [nodes[s]] + [nodes[v] for v, _ in path]
        if not point_in_poly(CAMPUS, poly[:-1]):
            continue
        if best is None or found < best[0]:
            best = (found, s, path)
    if best is None:
        raise SystemExit('학교를 감싸는 순환로를 찾지 못했습니다. VIA/CAMPUS를 확인하세요.')
    return best


def resample(pts, step):
    out, carry = [pts[0]], 0.0
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        seg = math.hypot(x2 - x1, y2 - y1)
        t = step - carry
        while t <= seg:
            out.append((x1 + (x2 - x1) * t / seg, y1 + (y2 - y1) * t / seg))
            t += step
        carry = seg - (t - step)
    return out


def build_route(nodes, loop):
    _, s, path = loop
    # 각 점에 그 구간의 차로 오프셋·도로 이름을 붙여 1m 간격으로 나눈다
    samples = []   # (x, y, offset, name)
    cur = nodes[s]
    for v, (_, off, name) in path:
        nxt = nodes[v]
        seg = math.dist(cur, nxt)
        k = max(1, int(seg))
        for j in range(k):
            t = j / k
            samples.append((cur[0] + (nxt[0] - cur[0]) * t, cur[1] + (nxt[1] - cur[1]) * t, off, name))
        cur = nxt
    N = len(samples)
    # 진행 방향 기준 오른쪽으로 오프셋 (교차로·차로 수가 바뀌는 곳의 계단은 아래 평활화가 펴 준다)
    shifted = []
    for i in range(N):
        x0, y0 = samples[i - 1][:2]
        x1, y1 = samples[(i + 1) % N][:2]
        dx, dy = x1 - x0, y1 - y0
        L = math.hypot(dx, dy) or 1.0
        off = sum(samples[(i + k) % N][2] for k in range(-6, 7)) / 13
        shifted.append((samples[i][0] + dy / L * off, samples[i][1] - dx / L * off))
    # 순환 가우시안 평활화: 교차로의 꺾임을 반지름 십여 m의 곡선으로
    sigma, R = 7.0, 21
    wts = [math.exp(-(k * k) / (2 * sigma * sigma)) for k in range(-R, R + 1)]
    wsum = sum(wts)
    smooth = []
    for i in range(N):
        sx = sy = 0.0
        for k, w in zip(range(-R, R + 1), wts):
            px, py = shifted[(i + k) % N]
            sx += px * w
            sy += py * w
        smooth.append((sx / wsum, sy / wsum))
    ring = smooth + [smooth[0]]
    pts = resample(ring, 2.0)
    if math.dist(pts[-1], pts[0]) < 1.0:
        pts.pop()
    # 이름: 가장 가까운 원래 표본의 이름
    names = []
    j = 0
    for p in pts:
        best, bj = 1e18, j
        for k in range(j - 5, j + 40):
            d = math.dist(p, samples[k % N][:2])
            if d < best:
                best, bj = d, k % N
        j = bj
        names.append(samples[bj][3])
    start = min(range(len(pts)), key=lambda i: math.dist(pts[i], START_NEAR) + (0 if names[i] == LOOP_NAMES[0] else 1e6))
    pts = pts[start:] + pts[:start]
    names = names[start:] + names[:start]
    return pts, names


def near_route(route, keep):
    pts = route[::5]

    def ok(p):
        return any(abs(p[0] - q[0]) < keep and abs(p[1] - q[1]) < keep and math.dist(p, q) < keep for q in pts)
    return ok


TALL_FLOORS = ('university', 'school', 'college', 'commercial', 'office', 'hospital', 'public', 'civic')


def floor_h(kind):
    return 3.9 if kind in TALL_FLOORS else 3.2      # 학교·업무 건물은 층고가 높다


def height_of(w, overrides):
    """건물 높이 (m). 우선순위: 보정 파일(map-overrides.json) > OSM height > OSM building:levels > 종류별 기본값."""
    t = w['tags']
    kind = t.get('building', 'yes')
    o = overrides.get(t.get('name', '')) or overrides.get(str(w['id']))
    if o:
        if o.get('height'):
            return round(float(o['height']), 1)
        if o.get('levels'):
            return round(float(o['levels']) * floor_h(kind) + 1.0, 1)
    try:
        if 'height' in t:
            return round(float(str(t['height']).replace('m', '').strip()), 1)
        if 'building:levels' in t:
            return round(float(str(t['building:levels']).split(';')[0]) * floor_h(kind) + 0.8, 1)
    except ValueError:
        pass
    h = (w['id'] * 2654435761) % 1000 / 1000.0          # 태그가 없으면 id로 정해지는 값 (매번 같은 도시)
    if kind in ('apartments',):
        return round(28 + h * 30, 1)
    if kind in TALL_FLOORS:
        return round(12 + h * 14, 1)
    if kind in ('house', 'detached', 'garage', 'shed', 'hut', 'roof'):
        return round(4 + h * 4, 1)
    return round(7 + h * 10, 1)


def simplify(ring, tol=0.6):
    out = [ring[0]]
    for p in ring[1:]:
        if math.dist(p, out[-1]) >= tol:
            out.append(p)
    if len(out) > 2 and math.dist(out[0], out[-1]) < tol:
        out.pop()
    return out


def flat(pts):
    # 화면 좌표 (x = 동, z = -북), 0.1m 단위
    r = []
    for x, y in pts:
        r += [round(x, 1), round(-y, 1)]
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cache', help='Overpass·지형 원본을 저장·재사용할 폴더')
    ap.add_argument('--campus', action='store_true', help='세종대 캠퍼스 안 건물과 층수 상태만 출력 (map-overrides.json 채울 때)')
    args = ap.parse_args()
    if args.cache:
        os.makedirs(args.cache, exist_ok=True)
    raw = overpass(Q_ROADS, args.cache and os.path.join(args.cache, 'roads.json'))
    green = overpass(Q_GREEN, args.cache and os.path.join(args.cache, 'green.json'))
    campus_raw = overpass(Q_CAMPUS, args.cache and os.path.join(args.cache, 'campus.json'))
    overrides = load_overrides()

    nodes = {e['id']: project(e['lat'], e['lon']) for e in raw['elements'] if e['type'] == 'node'}
    ways = [e for e in raw['elements'] if e['type'] == 'way']
    # 지하차도·터널은 땅 위에 그리면 다른 도로와 겹치므로 뺀다 (지도는 평면)
    roads = [w for w in ways if w.get('tags', {}).get('highway') in ROAD_CLASSES
             and w['tags'].get('tunnel', 'no') == 'no' and not str(w['tags'].get('layer', '0')).startswith('-')]
    buildings = [w for w in ways if 'building' in w.get('tags', {})]

    loop = find_loop(nodes, roads)
    route, route_names = build_route(nodes, loop)
    length = sum(math.dist(route[i], route[(i + 1) % len(route)]) for i in range(len(route)))
    keep = near_route(route, KEEP_M)

    # 교차로: 두 도로 이상이 만나는 점 (같은 이름 도로가 끝과 끝으로 이어지는 곳은 제외)
    use = {}
    for w in roads:
        ids = w['nodes']
        for k, i in enumerate(ids):
            use.setdefault(i, []).append((w, k in (0, len(ids) - 1)))
    junction = {}
    for i, lst in use.items():
        if len(lst) < 2 or i not in nodes:
            continue
        if len(lst) == 2 and all(end for _, end in lst) and lst[0][0]['tags'].get('name') == lst[1][0]['tags'].get('name'):
            continue
        r = max(lanes_of(w['tags'])[0] * lanes_of(w['tags'])[2] / (1 if lanes_of(w['tags'])[1] else 2) for w, _ in lst)
        junction[i] = r

    names = sorted({w['tags'].get('name', '') for w in roads} | set(LOOP_NAMES))
    name_idx = {n: k for k, n in enumerate(names)}
    out_roads = []
    for w in roads:
        pts = [nodes[i] for i in w['nodes'] if i in nodes]
        inside = [keep(p) for p in pts]
        n, oneway, lw = lanes_of(w['tags'])
        # 경로 근처 구간만 잘라 남긴다 (동부간선로처럼 몇 km 이어지는 도로)
        k = 0
        while k < len(pts):
            if not inside[k]:
                k += 1
                continue
            j = k
            while j + 1 < len(pts) and inside[j + 1]:
                j += 1
            piece = pts[max(0, k - 1):min(len(pts), j + 2)]
            if len(piece) >= 2:
                out_roads.append({'n': name_idx[w['tags'].get('name', '')], 'c': ROAD_CLASSES.index(w['tags']['highway']),
                                  'l': n, 'w': round(n * lw, 1), 'o': 1 if oneway else 0, 'p': flat(piece)})
            k = j + 1
    out_junctions = []
    for i, r in junction.items():
        if keep(nodes[i]):
            out_junctions += [round(nodes[i][0], 1), round(-nodes[i][1], 1), round(r + 1.5, 1)]

    out_buildings = []
    for w in buildings:
        ring = [nodes[i] for i in w['nodes'] if i in nodes]
        if len(ring) < 4:
            continue
        ring = simplify(ring)
        if len(ring) < 3 or not keep(ring[0]):
            continue
        out_buildings.append({'h': height_of(w, overrides), 'p': flat(ring)})

    out_green = []
    for e in green['elements']:
        if e['type'] != 'way' or 'geometry' not in e:
            continue
        t = e.get('tags', {})
        kind = ('water' if t.get('natural') == 'water' else
                'wood' if t.get('natural') in ('wood', 'scrub') or t.get('landuse') == 'forest' else
                'park' if t.get('leisure') == 'park' else 'grass')
        ring = simplify([project(g['lat'], g['lon']) for g in e['geometry']], 1.0)
        if len(ring) < 3 or not any(keep(p) for p in ring):
            continue
        out_green.append({'k': kind, 'p': flat(ring)})

    # 세종대 캠퍼스 안 건물: 층수가 어디서 왔는지 (보정 파일을 채우는 데 쓴다)
    campus = [[project(g['lat'], g['lon']) for g in e['geometry']] for e in campus_raw['elements']
              if e['type'] == 'way' and '세종대' in e.get('tags', {}).get('name', '')]
    in_campus = []
    for w in buildings:
        ring = [nodes[i] for i in w['nodes'] if i in nodes]
        if not ring:
            continue
        c = (sum(p[0] for p in ring) / len(ring), sum(p[1] for p in ring) / len(ring))
        if any(point_in_poly(c, poly) for poly in campus):
            t = w['tags']
            ov = overrides.get(t.get('name', '')) or overrides.get(str(w['id'])) or {}
            src_ = ('보정' if (ov.get('levels') or ov.get('height')) else
                    'OSM' if ('building:levels' in t or 'height' in t) else '없음(기본값)')
            in_campus.append((t.get('name', f'(이름 없음 #{w["id"]})'), t.get('building:levels', ''), src_, height_of(w, overrides)))
    if args.campus:
        for name, lv, s, h in sorted(in_campus):
            print(f'{name:24s} OSM층수={lv or "-":3s} 출처={s:8s} 높이={h} m')
        return
    missing = [n for n, _, s, _ in in_campus if s.startswith('없음') and not n.startswith('(이름 없음')]
    if missing:
        print(f'층수 정보가 없는 세종대 건물 {len(missing)}개 (sim/tools/map-overrides.json에 적으면 반영): {", ".join(sorted(missing))}')

    # 지형: 모든 지도 요소를 덮는 범위 + 여유. 높이는 출발점 기준(출발점 = 0 m)
    xs, ys = [], []
    for item in out_roads + out_buildings + out_green:
        xs += item['p'][0::2]
        ys += [-v for v in item['p'][1::2]]
    bounds = (min(xs) - 150, min(ys) - 150, max(xs) + 150, max(ys) + 150)
    nx, ny, grid = terrain_grid(bounds, args.cache)
    sx, sy = route[0]
    i0 = min(nx - 1, max(0, int((sx - bounds[0]) / TERRAIN_STEP)))
    j0 = min(ny - 1, max(0, int((sy - bounds[1]) / TERRAIN_STEP)))
    base = grid[j0][i0]
    import array
    import base64
    cm = array.array('h', [max(-32768, min(32767, int(round((v - base) * 100)))) for row in grid for v in row])
    if sys.byteorder != 'little':
        cm.byteswap()
    flat_h = [v for row in grid for v in row]
    terrain = {'x0': round(bounds[0], 1), 'n0': round(bounds[1], 1), 'step': TERRAIN_STEP, 'nx': nx, 'ny': ny,
               'base': round(base, 1), 'cm': base64.b64encode(cm.tobytes()).decode('ascii'),
               'source': 'AWS Terrain Tiles (Terrarium, SRTM 기반), 열림 %.0f m + 평활화 %.0f m' % (OPEN_M, TERRAIN_SIGMA)}

    data = {
        'name': '세종대학교 주변 (서울 광진구)',
        'attribution': '© OpenStreetMap contributors (ODbL)',
        'origin': ORIGIN,
        'made': time.strftime('%Y-%m-%d'),
        'names': names,
        'classes': ROAD_CLASSES,
        'route': {'length': round(length, 1), 'p': flat(route), 'n': [name_idx[n] for n in route_names]},
        'roads': out_roads,
        'junctions': out_junctions,
        'buildings': out_buildings,
        'green': out_green,
        'terrain': terrain,
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, separators=(',', ':'))
    order = []
    for n in route_names:
        if not order or order[-1] != n:
            order.append(n)
    print(f'경로 {length:.0f} m ({" → ".join(order)}), 도로 {len(out_roads)}, 교차로 {len(out_junctions) // 3}, '
          f'건물 {len(out_buildings)}, 녹지 {len(out_green)}, 지형 {nx}x{ny} (출발점 해발 {base:.0f} m, '
          f'{min(flat_h) - base:+.1f}~{max(flat_h) - base:+.1f} m) → {os.path.relpath(OUT)} ({os.path.getsize(OUT) // 1024} KB)')


if __name__ == '__main__':
    main()
