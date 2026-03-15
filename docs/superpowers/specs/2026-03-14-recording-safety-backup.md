# Recording safety and backup system

## Overview

Add crash-safe recording, disk space monitoring, and post-crash recovery to BDG rec. Designed for long recording sessions (up to 2 hours, ~1GB at mono 24-bit 48kHz).

## 1. Chunk-based recording (crash-safe)

Instead of writing a single WAV that only finalizes its header on stop, record into **5-minute chunks**. Each chunk is a complete, valid WAV file.

### File structure during recording
```
<destFolder>/BDG_rec_2026-03-14_14-00-00/
  chunk_001.wav   ← complete, valid
  chunk_002.wav   ← complete, valid
  chunk_003.wav   ← currently recording
```

### Chunk lifecycle
1. When recording starts, create a timestamped subfolder in `destFolder`
2. Open `chunk_001.wav` as the first chunk (mono, native sample rate, 24-bit)
3. Every 5 minutes (based on sample count: `5 * 60 * sampleRate`), finalize the current chunk and open the next one
4. Each chunk transition: finalize writer (writes correct WAV header), create new writer for next chunk. No audio gap — the audio callback writes to whichever chunk is active.
5. On stop: finalize current chunk, concatenate all chunks into a single `BDG_rec_<timestamp>.wav` in `destFolder`, delete the chunk subfolder.

### Thread safety
The chunk rotation happens in the writer thread (not the audio callback). The audio callback pushes samples to the ThreadedWriter. When it's time to rotate, the writer thread:
1. Resets the current ThreadedWriter (flushes + closes)
2. Creates a new WAV file + ThreadedWriter for the next chunk
3. The audio callback's `write()` calls are briefly blocked during rotation (~1ms, acceptable)

## 2. Disk space monitoring

### During recording
- Check `destFolder.getBytesFreeOnVolume()` every **10 seconds** via the existing timer
- Calculate remaining recording time: `freeBytes / bytesPerSecond` where `bytesPerSecond = sampleRate * 3` (24-bit = 3 bytes per sample, mono)

### Thresholds
| Condition | Remaining time | Action |
|-----------|---------------|--------|
| Normal | > 10 min | Green disk bar (current behavior) |
| Warning | 2-10 min | Yellow disk bar + toast "Espaco em disco baixo. Restam ~Xmin" |
| Critical | < 2 min | Red disk bar + auto-stop recording + toast "Gravacao parada: disco quase cheio" |

### Implementation
- Add `AudioEngine::Listener::diskSpaceWarning(int remainingMinutes)` callback
- AudioEngine checks disk space in a timer (not audio thread)
- On critical: call `stopRecording()` automatically, then notify UI

## 3. Post-crash recovery

### On app startup
1. Scan `destFolder` for subdirectories matching `BDG_rec_*` pattern
2. If found, these are orphaned chunk folders from a crashed recording
3. Check if chunks inside are valid WAV files (readable by `AudioFormatReader`)

### UI flow
- Show toast: "Gravacao anterior encontrada. Deseja recuperar?"
- Two actions:
  - **Recuperar**: concatenate valid chunks into a single WAV in `destFolder`, delete chunk folder, show success toast with filename
  - **Descartar**: delete chunk folder

### Concatenation
- Read each chunk sequentially with `AudioFormatReader`
- Write to a single output WAV with `AudioFormatWriter`
- Use the timestamp from the folder name for the output filename
- Skip any chunk that fails to open (corrupted last chunk)

## 4. Flush strategy

- The `AudioFormatWriter::ThreadedWriter` already buffers writes
- After each buffer batch is written to disk, call `flush()` on the underlying `FileOutputStream` to ensure data reaches disk
- This protects against OS-level buffer loss on power failure
- Trade-off: slightly more I/O, but for mono 24-bit at 48kHz the data rate is only ~144 KB/s — negligible

## Changes to existing code

### AudioEngine
- `startRecording()`: create chunk subfolder, open first chunk
- Audio callback: track sample count, signal chunk rotation at 5-min intervals
- `stopRecording()`: finalize last chunk, concatenate all, clean up
- Add disk space timer (10s interval)
- Add `diskSpaceWarning()` and `recordingAutoStopped()` to Listener

### RecordingPanel
- DiskSpaceBar: change color based on warning level (green/yellow/red)
- Handle auto-stop event

### MainComponent
- On startup: check for orphaned chunks, show recovery toast
- Handle recovery/discard actions

### OutputPanel / DSP
- No changes — DSP runs on the final concatenated file as before
