# GPS + SD Investigation — YamuraLog Hub

**Date:** 2026-09-20
**Scope:** Adding a GPS module to the hub, and whether the Teensy can service GPS + CAN FD + SD without dropping CAN frames.
**Status:** Investigation only — no code changes made.

---

## 1. Goal

Add GPS to the YamuraLog Hub and confirm the hub can, during logging, simultaneously:
1. receive CAN FD messages from all sensor nodes,
2. acquire/parse GPS, and
3. write everything to the SD card,

**without dropping CAN messages.**

---

## 2. Current architecture (as reviewed)

- **MCU:** Teensy **4.0** (`platformio.ini` → `board = teensy40`). 600 MHz Cortex-M7.
- **CAN:** `FlexCAN_T4FD<CAN3, RX_SIZE_256, TX_SIZE_16>` — a **256-frame software RX queue**. Nine RX mailboxes (MB0–MB8) with enhanced range filters, one `onReceive` callback each.
- **Callback dispatch:** callbacks run from `canFD.events()` **in the main-loop context** (the `StartLogging()` `while(true){ canFD.events(); … }` loop), **not** the ISR. The ISR moves frames hardware-MB → 256 queue; `events()` drains the queue and runs the `CAN_*Message` callbacks. Good: an SD stall in a callback cannot block CAN reception at the hardware level — the ISR keeps filling the queue.
- **SD write pattern:** each callback does `memcpy` then **two tiny writes** — `targetFile.write((uint8_t)msg.id)` + `targetFile.write(buffer, SIZE)`. Relies on the SD library's internal 512-byte block buffer. **No batching, no preallocation.**
- **Bus sharing:** Teensy 4.0 has **no SD socket**, so SD is on **SPI0** (`SD_CS=3`), shared with the **AirLift WiFi** (`SPIWIFI_SS=5`) and **TFT** (`TFT_CS=15`) on MOSI 11 / MISO 12 / SCK 13. During logging, WiFi is idle and the TFT isn't drawn, so SD effectively owns SPI0 while logging.
- **Timestamps:** each payload is **timestamped at the source node**, so SD latency does not distort logged timing. (This makes buffering/batching safe to add.)
- **Record framing:** `.yl5` is binary, self-describing by a leading **id byte** + fixed payload (`GPS_SIZE=28`, `IR_SIZE=64`, `AD_SIZE=21`, `IMU_SIZE=16`, `SPEED_SIZE=8`).

---

## 3. Two GPS design options considered

### Option A — GPS as a CAN node (original design)
GPS is a separate node (ID `0x50`, MB3, `CAN_GPSMessage`), already fully wired. "Adding GPS" = more CAN traffic into MB3. Load is **negligible**: 28+1 = 29 B/msg, even at 25 Hz ≈ 725 B/s (vs the 64-byte IR and fast A2D/shock nodes that dominate). **The GPS itself is not what breaks anything.**

### Option B — GPS on the hub board via I2C or SPI (chosen direction)
GPS moves onto the hub board, read directly by the Teensy; no CAN between Teensy and GPS. Effects:
- **CAN bus gets lighter** (one fewer talker; MB3 / 0x50–0x5F frees up).
- **Hub loop gains a job:** read + parse GPS and write its record, woven into the same logging loop.
- Risk shifts from *CAN bandwidth* to **"does GPS servicing steal time from `canFD.events()` / SD writes?"** Data volume is still tiny; the real danger is a **blocking read** or **bus contention**.

**I2C vs SPI on this board:**
- **I2C preferred** — separate bus (Wire), so GPS reads never contend with SPI0/SD/TFT/WiFi.
- **Pin conflict to watch:** buttons are on `16/17/18`. Teensy 4.0 I2C pinouts: **Wire** SDA18/SCL19 (conflicts BUTTON_2=18), **Wire1** SDA17/SCL16 (conflicts BUTTON_0/1), **Wire2** SDA25/SCL24 (**free**). → Use **Wire2 (24/25)** or relocate buttons.
- **SPI is strictly worse here:** a 5th CS on SPI0 that must interleave with SD writes *during logging*, plus `beginTransaction` mode/speed juggling. The only upside (faster transfer) is irrelevant for ~29 bytes.

**Non-blocking read is the make-or-break (either bus):**
- u-blox: **auto-PVT push + `checkUblox()`** (consume only already-arrived bytes) — not a polling `getPVT()` that can stall up to the nav period.
- NMEA: parse **incrementally**, a few bytes per loop pass; never wait for a full sentence.
- Set an **I2C/Wire timeout** so a hung/clock-stretching module can't freeze the loop.
- Budget: ~100-byte I2C burst @ 400 kHz ≈ 2 ms, at ≤25 Hz → <5% of loop time if non-blocking.

**Keep compatible:** hub writes the **same `.yl5` framing** the old GPS node produced (id `0x50` + payload) so the YamuraView parser is unchanged; **timestamp locally** (millis/micros) at read time since the source-node timestamp is gone.

---

## 4. Bottlenecks, ranked (independent of GPS)

1. **Teensy 4.0 → SD on SPI (shared bus).** The structural ceiling. SPI-SD is slow and has worse worst-case latency than SDIO.
2. **Per-message tiny writes, no batching.** Two `write()` calls per frame; high per-call/metadata overhead.
3. **No file preallocation.** `FILE_WRITE` grows the file cluster-by-cluster; each new cluster touches the FAT → a **latency spike mid-log**.
4. **256-frame queue is the *entire* stall cushion — and it's marginal.** Drop occurs when `aggregate_msg_rate × worst_SD_stall > 256`. e.g. 2000 msg/s × 100 ms = 200 (just under); × 250 ms = 500 → **overflow/dropped CAN**. SPI-SD makes stalls longer.
5. **No drop detection.** Nothing watches RX queue high-water or FlexCAN overrun/error flags — can't prove headroom.
6. **Debug builds are timing-poison.** `DEBUG_EXTRA_VERBOSE` does `sprintf`+`Serial.println` inside the receive path; must be off in production.

---

## 5. Teensy 4.0 vs 4.1 — SD write speed

Same CPU; the difference is **how the SD card is wired**.

| | Teensy 4.0 (SPI SD) | Teensy 4.1 (SDIO SD) |
|---|---|---|
| Interface | SPI, 1-bit, shared with TFT/WiFi | SDIO, 4-bit, dedicated (`BUILTIN_SDCARD`) |
| Typical sustained write | ~1–4 MB/s | ~15–22 MB/s |
| Relative | 1× | **~6–8× (up to ~10×)** |

Notes:
- For dropped frames the metric is **worst-case write latency**, not throughput. SDIO shrinks routine block-flush/FAT stalls ~6–8×, but **rare card-internal GC stalls (100+ ms) are a card property**, not the bus — so a **high-endurance/industrial card + preallocation** still matter on either board.
- SDIO's full speed only shows with **batched writes**; the current per-message pattern won't hit peak (latency per flush still drops immediately).
- 4.1 also takes **SD off SPI0 entirely** (helps the GPS/TFT/WiFi contention) and has **PSRAM footprints** (solder 8/16 MB) for large log buffers.

**Recommendation:** if either board is acceptable, **use the Teensy 4.1.** It removes the #1 bottleneck (SPI-SD), frees the SPI bus, and adds PSRAM headroom.

---

## 6. Recommended changes (when work resumes) — cheap → involved

1. Turn off all verbose debug in the logging path.
2. **Instrument**: RX queue high-water + FlexCAN overrun counters; show at StopLogging. (Proves whether a problem exists.)
3. **Preallocate** the log file + **batch** writes through a 4–8 KB buffer (SdFat). Biggest gain without a hardware change.
4. Enlarge `RX_SIZE` to 512/1024 (trivial RAM).
5. GPS on **I2C Wire2 (24/25)**, read **non-blocking**, with a Wire timeout; write `0x50`+payload records, timestamped locally.
6. If overruns persist under full load: **Teensy 4.1 + SDIO**.
7. Use a **high-endurance/industrial SD card** regardless.

---

## 7. PSRAM buffering analysis (Teensy 4.1)

Question considered: is it safer/more efficient to add PSRAM (8/16 MB QSPI, `EXTMEM`) and either (A) hold the whole log file in PSRAM and copy to SD at stop, or (B) copy from a PSRAM buffer to SD during free cycles while logging?

**Verdict: don't hold the whole file in PSRAM; stream to SD. PSRAM does not attack the thing that drops CAN frames.**

**Safety (decisive):**
- **Option A (whole file in PSRAM → dump at stop):** PSRAM is **volatile**. A power interruption mid-session (crash, battery/master-switch disconnect) loses the **entire run** — strictly *less* safe than streaming. Also capacity-capped: 16 MB at even ~100 KB/s aggregate ≈ ~2.5 min, so it can't hold a full session anyway.
- **Streaming to SD (periodic `flush()`):** worst case loses only the last unflushed buffer (≤1–2 s with a 1–2 s flush interval).

**The trap in "copy during free cycles" (Option B):**
- CAN callbacks run from `canFD.events()` in the **main loop**. During a long SD write (the 100+ ms card-GC stall that actually drops frames), the main loop is blocked, so the callbacks aren't running — meaning **a PSRAM SD-side buffer isn't being filled during the stall either.**
- The only thing capturing CAN during that stall is the **FlexCAN ISR filling its `RX_SIZE` queue.** So the effective cushion is whatever fills **independently of the blocked SD write** — the ISR queue, *not* a PSRAM byte buffer on the SD side.
- **Therefore the highest-leverage change is enlarging `RX_SIZE_256` → `RX_SIZE_1024`** (ISR-filled, internal RAM, ~80 KB, 4× the stall tolerance). PSRAM adds nothing this doesn't do better/simpler — unless frame capture is moved into the ISR path.

**Not throughput-bound:** SDIO ≈ 15–22 MB/s vs a tens-to-low-hundreds KB/s data rate → 100×+ average headroom. The problem is latency *spikes*, not average speed. PSRAM doesn't make SD faster or its stalls shorter, so it doesn't address the actual failure. Spikes are fixed by **preallocation + a good card + a deep ISR queue.**

**Where PSRAM genuinely helps (likely overkill here):**
1. Restructure so the **ISR/receive path** copies frames into a large PSRAM ring (tens of MB = tens of seconds of cushion) — far more than this workload needs, and PSRAM's higher latency is a mild hot-path cost.
2. A very large sequential **write-staging** buffer for max throughput — but a 4–32 KB **internal-RAM** buffer already reaches SDIO's full speed.

For log buffering, internal RAM (DTCM/OCRAM) is faster/lower-latency than PSRAM; reserve PSRAM for when multi-MB depth is truly required.

**Recommended (4.1):** stream, don't hoard — (1) `RX_SIZE_1024`, (2) preallocate + batch writes through a ~4–32 KB internal-RAM buffer in bounded chunks, (3) periodic `flush()` (~1–2 s), (4) high-endurance card, (5) instrument RX high-water + overrun flags. PSRAM optional, only if measured worst case still overruns, and then as an ISR-filled ring — never an end-of-session dump.

---

## 8. Pre-allocating the log file on SD

**What it is.** A FAT/exFAT file is a chain of **clusters**. Normally a file grows on demand: each time writes pass the end of the allocated space, the filesystem must **find a free cluster, update the FAT table on the card, and update the directory entry** — extra SD transactions that happen **mid-log at unpredictable moments** (a primary source of latency spikes), and that **fragment** the file. **Preallocation reserves a large, contiguous block of clusters once, before logging**, so during the run writes just fill already-owned, contiguous space — no per-cluster FAT lookup, no allocation stalls, no fragmentation, no directory-size churn until close.

**Why it helps here.**
- Removes mid-log allocation spikes (FAT work done once, up front, not sprinkled through the run).
- Contiguous layout enables SdFat's fastest multi-block write path (no per-block metadata update).
- Fewer/shorter SD stalls → the FlexCAN queue absorbs less → fewer dropped frames.

**How (SdFat).**
1. **Estimate max session size:** `bytes/sec × max_minutes × 60`, rounded up generously (cards are huge; the unused tail is reclaimed at close).
2. **Create the file and `preAllocate(size)` before the logging loop** — SdFat grabs a single contiguous run.
3. **Write into it** during logging (batched, 512-byte-aligned chunks).
4. **At stop: `truncate()` to actual bytes written, then `close()`** — file ends up exactly the real length, not padded with the reservation.

**Details.**
- **Format exFAT** for big logs: >4 GB files (FAT32 caps at 4 GB) and a "contiguous" flag that makes preAllocate + append clean and fast. SdFat's `ExFatLogger` example is essentially this pattern.
- The **preAllocate cost is one-time, up front** (short delay before logging), not during the run — exactly where you want it.
- **Still flush periodically (~1–2 s):** preallocation removes *allocation* metadata writes, but a periodic `flush()` keeps the directory size/FAT current so a power loss costs ≤ the flush interval. If power is cut before `close()`, the data is on the card but the directory length may lag the last flush. Preallocation removes the stall source; it does **not** replace flushing.
- **Pairs with batching:** contiguous space + batched aligned writes = full SDIO throughput.

Net: preallocation converts "SD stalls at random cluster boundaries during the run" into "one predictable cost before the run" — the trade a real-time logger wants.

---

## 9. Relationship to `CODE_REVIEW.md`

The repo's `CODE_REVIEW.md` is **unrelated to this investigation** — different subsystem, no overlap or conflict.

- **It covers the FTP client**: converting the FTP upload code from `.ino` to a C++ class (`FTPClient.h/.cpp`) with 21 fixes (architecture, error handling, C++ idioms, logic, quality, memory, API). That's the **WiFi file-transfer path** ("Send File" menu), not the logging hot path.
- **Different phase:** FTP transfer runs in `SendFileMenu()` (heartbeat stopped, separate device state) — **never at the same time as logging**. So the AirLift's use of the shared SPI0 bus does **not** contend with SD *during logging*, which reinforces this investigation's assumption that SD effectively owns SPI0 while logging.
- **Shared principle, different code:** CODE_REVIEW's future-work list wants async/non-blocking FTP (so uploads don't freeze the UI); this investigation wants non-blocking GPS reads (so they don't stall the CAN drain). Same idea, unrelated paths.
- **One open gap worth tracking:** CODE_REVIEW Issue 4.4 notes "upload file **from SD**" is a placeholder/unfinished — the other half of the data pipeline (getting completed logs *off* the device). Orthogonal to logging performance, but still open.

Nothing in `CODE_REVIEW.md` changes any conclusion here.

---

## 10. Bottom line

Adding GPS is safe on its own — on-board GPS actually **reduces CAN load**, and the data volume is trivial. Two conditions make it safe: put it on **I2C (Wire2)** and read it **non-blocking**. The dominant limiter remains the **SD path** (SPI-SD on the 4.0, unbatched per-message writes, no preallocation, 256-frame cushion). Moving to a **Teensy 4.1 (SDIO)** plus batching + preallocation + a good card removes that ceiling; add overrun/high-water instrumentation to prove the margin rather than guess. **PSRAM is not the fix** — streaming to SD with a deeper ISR queue is safer and more effective than buffering the file in volatile PSRAM.
