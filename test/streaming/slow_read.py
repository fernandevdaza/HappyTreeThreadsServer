import asyncio
import time
from dataclasses import dataclass


@dataclass
class OneResult:
    ok: bool
    bytes_read: int
    seconds: float
    err: str


async def slow_read_one(
    host: str, port: int, path: str, read_bytes: int, sleep_s: float, run_s: float
) -> OneResult:
    t0 = time.time()
    total = 0
    try:
        reader, writer = await asyncio.open_connection(host, port)
        req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}\r\n"
            f"Connection: keep-alive\r\n"
            f"\r\n"
        ).encode()
        writer.write(req)
        await writer.drain()

        head = b""
        while b"\r\n\r\n" not in head:
            b = await reader.read(1)
            if not b:
                raise RuntimeError("server closed before headers")
            head += b
            if len(head) > 65536:
                raise RuntimeError("headers too large")

        head_txt = head.decode("latin1", "ignore")
        if " 200 " not in head_txt and " 206 " not in head_txt:
            status = head_txt.splitlines()[0] if head_txt.splitlines() else "bad status"
            raise RuntimeError(status)

        end_t = t0 + run_s
        while time.time() < end_t:
            b = await reader.read(read_bytes)
            if not b:
                break
            total += len(b)
            await asyncio.sleep(sleep_s)

        writer.close()
        await writer.wait_closed()
        return OneResult(True, total, time.time() - t0, "")
    except Exception as e:
        return OneResult(False, total, time.time() - t0, str(e))


async def main():
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--path", required=True)
    ap.add_argument("--clients", type=int, default=200)
    ap.add_argument("--concurrency", type=int, default=50)
    ap.add_argument("--read-bytes", type=int, default=1)
    ap.add_argument("--sleep", type=float, default=0.05)
    ap.add_argument("--seconds", type=float, default=30.0)
    ap.add_argument("--conn-timeout", type=float, default=4.0)
    args = ap.parse_args()

    sem = asyncio.Semaphore(args.concurrency)
    active = 0
    started = 0
    done = 0
    lock = asyncio.Lock()
    results = []

    async def runner(i: int):
        nonlocal active, started, done
        async with sem:
            async with lock:
                active += 1
                started += 1
            try:
                r = await asyncio.wait_for(
                    slow_read_one(
                        args.host,
                        args.port,
                        args.path,
                        args.read_bytes,
                        args.sleep,
                        args.seconds,
                    ),
                    timeout=args.seconds + args.conn_timeout + 10.0,
                )
                results.append(r)
            except Exception as e:
                results.append(OneResult(False, 0, 0.0, f"timeout_or_error:{e}"))
            finally:
                async with lock:
                    active -= 1
                    done += 1

    async def heartbeat():
        t0 = time.time()
        while True:
            await asyncio.sleep(1.0)
            async with lock:
                a, s, d = active, started, done
            print(
                f"[{time.time()-t0:6.1f}s] active={a} started={s}/{args.clients} done={d}/{args.clients}",
                flush=True,
            )
            if d >= args.clients:
                return

    t0 = time.time()
    tasks = [asyncio.create_task(runner(i)) for i in range(args.clients)]
    hb = asyncio.create_task(heartbeat())
    await asyncio.gather(*tasks)
    await hb
    dt = time.time() - t0

    ok = sum(1 for r in results if r.ok)
    fail = len(results) - ok
    total_bytes = sum(r.bytes_read for r in results)
    print("clients:", args.clients, "concurrency:", args.concurrency)
    print("ok:", ok, "fail:", fail)
    if fail:
        sample = [r.err for r in results if not r.ok][:5]
        print("sample_errors:", sample)
    mib = total_bytes / (1024 * 1024)
    print(f"bytes_read_total: {total_bytes} ({mib:.2f} MiB)")
    print(f"elapsed: {dt:.2f}s")


if __name__ == "__main__":
    asyncio.run(main())
