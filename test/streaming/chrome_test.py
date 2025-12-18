import asyncio
import time
from dataclasses import dataclass
from typing import List

from playwright.async_api import async_playwright


@dataclass
class Result:
    client_id: int
    stalls: int
    stall_ms: float
    ended: bool
    current_time: float
    buffered_end: float
    dropped_frames: int
    total_frames: int


JS_METRICS = r"""
(() => {
  const v = document.querySelector('#v');
  if (!v) {
    window.__metrics = { error: "No se encontró video #v" };
    return;
  }

  const m = {
    stalls: 0,
    stallMs: 0,
    stallStart: null,
    ended: false,
    lastTime: 0,
    lastBufferedEnd: 0,
    totalFrames: 0,
    droppedFrames: 0,
  };

  function bufferedEnd(video) {
    try {
      if (video.buffered && video.buffered.length > 0) {
        return video.buffered.end(video.buffered.length - 1);
      }
    } catch {}
    return 0;
  }

  function updateQuality(video) {
    try {
      if (typeof video.getVideoPlaybackQuality === 'function') {
        const q = video.getVideoPlaybackQuality();
        m.totalFrames = q.totalVideoFrames || 0;
        m.droppedFrames = q.droppedVideoFrames || 0;
        return;
      }
    } catch {}
    m.totalFrames = video.webkitDecodedFrameCount || 0;
    m.droppedFrames = video.webkitDroppedFrameCount || 0;
  }

  v.addEventListener('playing', () => {
    if (m.stallStart !== null) {
      m.stallMs += (performance.now() - m.stallStart);
      m.stallStart = null;
    }
  });

  v.addEventListener('waiting', () => {
    m.stalls += 1;
    if (m.stallStart === null) m.stallStart = performance.now();
  });

  v.addEventListener('stalled', () => {
    m.stalls += 1;
    if (m.stallStart === null) m.stallStart = performance.now();
  });

  v.addEventListener('ended', () => { m.ended = true; });

  setInterval(() => {
    m.lastTime = v.currentTime || 0;
    m.lastBufferedEnd = bufferedEnd(v);
    updateQuality(v);
  }, 250);

  window.__metrics = m;
})();
"""


async def click_play_for_item(page, item_id: str) -> None:
    card_sel = f'.card[data-id="{item_id}"]'
    btn_sel = f'{card_sel} button[data-action="play"]'

    await page.wait_for_selector(card_sel, timeout=20_000)

    await page.evaluate(
        """(sel) => {
            const el = document.querySelector(sel);
            if (el) el.scrollIntoView({behavior: 'instant', block: 'center', inline: 'center'});
        }""",
        card_sel,
    )

    await page.evaluate(
        """(sel) => {
            const el = document.querySelector(sel);
            if (!el) return;
            const scroller = el.closest('.scroller');
            if (!scroller) return;
            const r = el.getBoundingClientRect();
            const sr = scroller.getBoundingClientRect();
            if (r.left < sr.left || r.right > sr.right) {
              el.scrollIntoView({behavior: 'instant', block: 'nearest', inline: 'center'});
            }
        }""",
        card_sel,
    )

    await page.wait_for_selector(btn_sel, timeout=20_000)
    await page.hover(card_sel)
    await page.click(btn_sel, force=True)


async def run_one(
    client_id: int, page_url: str, item_id: str, duration_s: int, headless: bool
) -> Result:
    async with async_playwright() as p:
        browser = await p.chromium.launch(
            channel="chrome",
            headless=headless,
            args=["--autoplay-policy=no-user-gesture-required"],
        )
        ctx = await browser.new_context()
        page = await ctx.new_page()

        await page.goto(page_url, wait_until="domcontentloaded")

        await click_play_for_item(page, item_id)

        await page.wait_for_selector("#modal.is-open", timeout=15_000)
        await page.wait_for_selector("#v", timeout=15_000)

        await page.eval_on_selector("#v", "v => { v.muted = true; v.volume = 0; }")

        await page.evaluate(JS_METRICS)

        await page.wait_for_function(
            "() => window.hls && window.hls.levels && window.hls.levels.length > 0",
            timeout=20_000,
        )

        await page.eval_on_selector("#v", "v => v.play().catch(()=>{})")

        t_start = time.time()
        started = False
        while time.time() - t_start < 25:
            paused = await page.eval_on_selector("#v", "v => v.paused")
            ct = await page.eval_on_selector("#v", "v => v.currentTime")
            if (not paused) and ct and ct > 0.2:
                started = True
                break
            await asyncio.sleep(0.25)

        if not started:
            await page.eval_on_selector("#v", "v => { v.muted = true; v.volume = 0; }")
            await page.click("#v", force=True)
            await page.eval_on_selector("#v", "v => v.play().catch(()=>{})")

            t_retry = time.time()
            while time.time() - t_retry < 10:
                paused = await page.eval_on_selector("#v", "v => v.paused")
                ct = await page.eval_on_selector("#v", "v => v.currentTime")
                if (not paused) and ct and ct > 0.2:
                    started = True
                    break
                await asyncio.sleep(0.25)

        if not started:
            await browser.close()
            raise RuntimeError(
                "No arrancó playback. Probable 404 de m3u8/segments o rutas relativas en el playlist."
            )

        t0 = time.time()
        ended = False
        while time.time() - t0 < duration_s:
            m = await page.evaluate("() => window.__metrics || {}")
            if m.get("error"):
                raise RuntimeError(f"[cliente {client_id}] {m['error']}")
            ended = bool(m.get("ended", False))
            if ended:
                break
            await asyncio.sleep(0.5)

        m = await page.evaluate("() => window.__metrics || {}")

        await browser.close()

        return Result(
            client_id=client_id,
            stalls=int(m.get("stalls", 0)),
            stall_ms=float(m.get("stallMs", 0.0)),
            ended=bool(m.get("ended", False)),
            current_time=float(m.get("lastTime", 0.0)),
            buffered_end=float(m.get("lastBufferedEnd", 0.0)),
            dropped_frames=int(m.get("droppedFrames", 0)),
            total_frames=int(m.get("totalFrames", 0)),
        )


async def main():
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--url", required=True)
    ap.add_argument("--item", required=True)
    ap.add_argument("--clients", type=int, default=4)
    ap.add_argument("--duration", type=int, default=45)
    ap.add_argument("--headless", action="store_true")
    args = ap.parse_args()

    tasks = [
        run_one(i + 1, args.url, args.item, args.duration, args.headless)
        for i in range(args.clients)
    ]
    results: List[Result] = await asyncio.gather(*tasks)

    print("\n=== RESULTADOS ===")
    total_stalls = 0
    total_stall_ms = 0.0

    for r in results:
        total_stalls += r.stalls
        total_stall_ms += r.stall_ms
        drop_pct = (
            (100.0 * r.dropped_frames / r.total_frames) if r.total_frames > 0 else 0.0
        )
        print(
            f"Cliente {r.client_id:02d} | stalls={r.stalls:3d} | stall_ms={r.stall_ms:8.1f} "
            f"| t={r.current_time:6.1f}s | buf_end={r.buffered_end:6.1f}s "
            f"| drop={r.dropped_frames}/{r.total_frames} ({drop_pct:.2f}%) | ended={r.ended}"
        )

    print("\n=== RESUMEN ===")
    print(f"Clientes: {len(results)}")
    print(f"Stalls totales: {total_stalls}")
    print(f"Tiempo total en stall: {total_stall_ms/1000.0:.2f} s")


if __name__ == "__main__":
    asyncio.run(main())
