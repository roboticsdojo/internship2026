"""Fake "cheap Pi camera photographing a cheap screen 30 cm away".

Not a physics model. It stacks the things that go wrong in that setup, each with random strength,
so different pipeline choices can be compared on the same kind of damage:
  screen:  washed-out contrast, raised black level, colour cast, uneven backlight, visible pixel grid
  optics:  screen not square to the camera (perspective), bezel/desk around it, defocus blur, glare
  sensor:  aliasing when the high-res scene lands on few pixels (moire), noise, refresh banding, JPEG
"""
import cv2
import numpy as np


def degrade(img, rng, cam=(640, 480), strength=1.0):
    k = strength
    # --- screen panel: the picture shown fit-to-screen on a 1024x600 panel, black bars around it
    pw, ph = 1024, 600
    h, w = img.shape[:2]
    r = min(pw / w, ph / h)
    nw, nh = int(w * r), int(h * r)
    panel = np.zeros((ph, pw, 3), np.float32)
    x0, y0 = (pw - nw) // 2, (ph - nh) // 2
    panel[y0:y0 + nh, x0:x0 + nw] = cv2.resize(img, (nw, nh), interpolation=cv2.INTER_AREA) / 255.0
    lo = rng.uniform(0.05, 0.20) * k                     # black level: blacks look grey
    hi = 1.0 - rng.uniform(0.0, 0.25) * k                # dim whites
    panel = lo + (hi - lo) * panel ** rng.uniform(0.8, 1.3)
    cast = 1 + rng.uniform(-0.15, 0.15, 3) * k           # cheap panels are blue- or yellow-ish
    panel *= cast
    yy, xx = np.mgrid[0:ph, 0:pw]
    cx, cy = rng.uniform(0.3, 0.7) * pw, rng.uniform(0.3, 0.7) * ph
    vign = 1 - 0.35 * k * (((xx - cx) / pw) ** 2 + ((yy - cy) / ph) ** 2)
    panel *= vign[..., None]
    # pixel grid: every panel pixel becomes 3x3 sub-pixels, R G B stripes with dark gaps
    sub = np.repeat(np.repeat(panel, 3, 0), 3, 1)
    mask = np.zeros((3, 3, 3), np.float32)
    mask[:, 0, 2] = mask[:, 1, 1] = mask[:, 2, 0] = 1.0  # BGR order: col0 R, col1 G, col2 B
    mask[2, :, :] *= 0.55                                # dark row between pixel rows
    g = rng.uniform(0.15, 0.6) * k                       # how visible the grid is (focus, diffuser)
    grid = np.tile(mask, (ph, pw, 1)) * 2.2
    sub = np.clip(sub * ((1 - g) + g * grid), 0, 1)

    # --- camera: render the screen into a 2x-camera-res scene with a random perspective
    W, H = cam[0] * 2, cam[1] * 2
    fill = rng.uniform(0.75, 0.98)                       # how much of the frame the screen covers
    sw, sh = W * fill, W * fill * ph / pw
    if sh > H * 0.98:
        sh = H * 0.98
        sw = sh * pw / ph
    ox, oy = (W - sw) / 2 + rng.uniform(-0.05, 0.05) * W, (H - sh) / 2 + rng.uniform(-0.05, 0.05) * H
    j = 0.04 * k * min(W, H)
    dst = np.float32([[ox, oy], [ox + sw, oy], [ox + sw, oy + sh], [ox, oy + sh]])
    dst += rng.uniform(-j, j, dst.shape).astype(np.float32)
    src = np.float32([[0, 0], [pw * 3, 0], [pw * 3, ph * 3], [0, ph * 3]])
    M = cv2.getPerspectiveTransform(src, dst)
    desk = np.full((H, W, 3), rng.uniform(0.05, 0.35), np.float32)
    scene = cv2.warpPerspective(sub, M, (W, H), flags=cv2.INTER_LINEAR,
                                borderMode=cv2.BORDER_TRANSPARENT, dst=desk)
    # glare: a soft bright blob (room light reflected in the glossy screen)
    if rng.random() < 0.6 * k:
        gx, gy = rng.uniform(0, W), rng.uniform(0, H)
        gr = rng.uniform(0.08, 0.3) * W
        yy, xx = np.mgrid[0:H, 0:W]
        g = np.exp(-(((xx - gx) ** 2 + (yy - gy) ** 2) / (2 * gr ** 2)))
        scene = scene + g[..., None] * rng.uniform(0.15, 0.5) * k
    # defocus (fixed-focus lens at 30 cm), then sample to sensor res WITHOUT averaging -> moire
    sigma = rng.uniform(0.6, 2.5) * k
    scene = cv2.GaussianBlur(scene, (0, 0), sigma)
    # cheap sensors + ISP sometimes alias hard (nearest), sometimes average a bit (area)
    interp = cv2.INTER_NEAREST if rng.random() < 0.5 else cv2.INTER_AREA
    frame = cv2.resize(scene, cam, interpolation=interp)
    # refresh banding: the rolling shutter catches the backlight PWM as horizontal stripes
    if rng.random() < 0.5 * k:
        period = rng.uniform(20, 80)
        band = 1 - rng.uniform(0.05, 0.2) * k * (0.5 + 0.5 * np.sin(2 * np.pi * np.arange(cam[1]) / period
                                                                 + rng.uniform(0, 6.3)))
        frame *= band[:, None, None]
    frame *= rng.uniform(0.7, 1.3)                       # auto-exposure guess
    frame += rng.normal(0, rng.uniform(0.01, 0.05) * k, frame.shape)
    frame = np.clip(frame * 255, 0, 255).astype(np.uint8)
    q = int(rng.uniform(35, 75))
    ok, enc = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, q])
    return cv2.imdecode(enc, cv2.IMREAD_COLOR)