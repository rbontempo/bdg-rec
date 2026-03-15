# Recording Safety & Backup — Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make BDG rec safe for 2-hour recording sessions by adding chunk-based recording, disk space monitoring, and crash recovery.

**Architecture:** Replace single-file WAV recording with 5-minute chunk rotation. Each chunk is a valid WAV. On stop, chunks are concatenated. Disk space is monitored with auto-stop. On startup, orphaned chunks are detected and offered for recovery.

**Tech Stack:** JUCE 8 (`AudioFormatWriter`, `AudioFormatReader`, `FileOutputStream`, `Timer`)

**Spec:** `docs/superpowers/specs/2026-03-14-recording-safety-backup.md`

---

## Chunk 1: Chunk-based recording

### Task 1: Refactor AudioEngine for chunk recording

**Files:**
- Modify: `src/AudioEngine.h`
- Modify: `src/AudioEngine.cpp`

- [ ] **Step 1: Add chunk members to AudioEngine**

Add to private section of `AudioEngine.h`:
```cpp
// Chunk recording
juce::File chunkFolder;              // timestamped subfolder
int chunkIndex{0};                   // current chunk number (1-based)
juce::int64 samplesInChunk{0};       // samples written to current chunk
juce::int64 samplesPerChunk{0};      // 5 min * sampleRate
std::unique_ptr<juce::FileOutputStream> chunkStream; // kept for flush()
```

Remove `currentRecordingFile` — replace with `chunkFolder`.

- [ ] **Step 2: Rewrite startRecording()**

New logic:
1. Create timestamped subfolder: `destFolder.getChildFile("BDG_rec_" + timestamp)`
2. `chunkFolder.createDirectory()`
3. Set `samplesPerChunk = (juce::int64)(5.0 * 60.0 * nativeSampleRate)`
4. Set `chunkIndex = 0`, call `openNextChunk()` (new private method)
5. Set `recording = true`

- [ ] **Step 3: Add openNextChunk() private method**

```cpp
bool openNextChunk()
{
    // Finalize previous chunk if any
    threadedWriter.reset();
    chunkStream.reset();

    chunkIndex++;
    auto chunkFile = chunkFolder.getChildFile(
        juce::String::formatted("chunk_%03d.wav", chunkIndex));

    chunkStream.reset(chunkFile.createOutputStream());
    if (!chunkStream) return false;

    auto* writer = wavFormat.createWriterFor(
        chunkStream.get(), nativeSampleRate, 1, 24, {}, 0);
    if (!writer) { chunkStream.reset(); return false; }

    chunkStream.release(); // writer owns it now — BUT we need flush access
    // Actually: keep a raw pointer to the stream for flush, but writer owns it
    // Better: create stream, save raw ptr, release unique_ptr to writer

    writerThread = std::make_unique<juce::TimeSliceThread>("BDG Writer");
    writerThread->startThread();
    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, *writerThread, 65536);

    samplesInChunk = 0;
    return true;
}
```

Note: for flush, we need the FileOutputStream pointer. Store a raw pointer before releasing ownership:
```cpp
juce::FileOutputStream* rawChunkStream{nullptr}; // non-owning, for flush
```

- [ ] **Step 4: Update audio callback for chunk rotation**

In `audioDeviceIOCallbackWithContext`, after writing to threadedWriter:
```cpp
samplesInChunk += numSamples;
if (samplesInChunk >= samplesPerChunk)
{
    // Signal rotation needed — do it on message thread to avoid
    // blocking audio thread with file I/O
    juce::MessageManager::callAsync([this]() {
        if (recording.load())
            openNextChunk();
    });
}
```

- [ ] **Step 5: Add flush after each write**

After `threadedWriter->write(...)` in the audio callback, flush is not possible from the audio thread (would block). Instead, add a periodic flush in a timer or after chunk rotation. The ThreadedWriter handles buffering internally — the key protection is that each chunk is a complete WAV when finalized.

- [ ] **Step 6: Rewrite stopRecording()**

New logic:
1. Set `recording = false`
2. Finalize current chunk: `threadedWriter.reset()`, `writerThread->stopThread()`, `writerThread.reset()`
3. Call `concatenateChunks()` (new method) — returns final `juce::File`
4. Delete `chunkFolder`
5. Return final file

- [ ] **Step 7: Add concatenateChunks() method**

```cpp
juce::File concatenateChunks()
{
    auto finalFile = chunkFolder.getParentDirectory().getChildFile(
        chunkFolder.getFileName() + ".wav");

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> outStream(finalFile.createOutputStream());
    if (!outStream) return {};

    auto* writer = wav.createWriterFor(outStream.get(), nativeSampleRate, 1, 24, {}, 0);
    if (!writer) return {};
    outStream.release();

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    // Read and write each chunk
    for (int i = 1; i <= chunkIndex; ++i)
    {
        auto chunkFile = chunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));

        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(chunkFile));
        if (!reader) continue; // skip corrupted chunk

        const int blockSize = 65536;
        juce::AudioBuffer<float> buffer(1, blockSize);
        juce::int64 pos = 0;
        while (pos < reader->lengthInSamples)
        {
            int toRead = (int)std::min((juce::int64)blockSize, reader->lengthInSamples - pos);
            reader->read(&buffer, 0, toRead, pos, true, false);
            writer->writeFromAudioSampleBuffer(buffer, 0, toRead);
            pos += toRead;
        }
    }

    delete writer;
    return finalFile;
}
```

- [ ] **Step 8: Build and verify**

```bash
cd "/Users/rbontempo/Documents/AI - antigravity/bdg-rec-juce"
cmake --build build --target BDG_REC
```

Test: record for 10 seconds, stop. Check that chunk folder is created during recording and cleaned up after stop. Final WAV should exist.

- [ ] **Step 9: Commit**

```bash
git add -A && git commit -m "feat: chunk-based recording (5-min chunks, crash-safe)"
```

---

## Chunk 2: Disk space monitoring

### Task 2: Disk space warning system

**Files:**
- Modify: `src/AudioEngine.h`
- Modify: `src/AudioEngine.cpp`
- Modify: `src/RecordingPanel.h`
- Modify: `src/RecordingPanel.cpp`
- Modify: `src/MainComponent.cpp`

- [ ] **Step 1: Add listener callbacks to AudioEngine**

In `AudioEngine::Listener`:
```cpp
virtual void diskSpaceWarning(int remainingMinutes) {}
virtual void recordingAutoStopped() {}
```

- [ ] **Step 2: Add disk space check timer to AudioEngine**

Make AudioEngine inherit `juce::Timer`. In `startRecording()`, call `startTimer(10000)` (10s). In `stopRecording()`, call `stopTimer()`.

`timerCallback()`:
```cpp
void timerCallback() override
{
    if (!recording.load()) return;

    auto freeBytes = chunkFolder.getBytesFreeOnVolume();
    int bytesPerSec = (int)(nativeSampleRate * 3.0); // 24-bit mono
    int remainingSec = (int)(freeBytes / bytesPerSec);
    int remainingMin = remainingSec / 60;

    listeners.call(&Listener::diskSpaceWarning, remainingMin);

    if (remainingMin < 2)
    {
        // Auto-stop
        juce::MessageManager::callAsync([this]() {
            stopRecording();
            listeners.call(&Listener::recordingAutoStopped);
        });
    }
}
```

- [ ] **Step 3: Update RecordingPanel disk space bar colors**

In `RecordingPanel`, handle `diskSpaceWarning(int min)`:
- `min > 10`: green (current)
- `2 <= min <= 10`: yellow (`#eab308`)
- `min < 2`: red (`#ef4444`)

Store `diskWarningLevel` enum and use it in `paintDiskSpaceBar()`.

- [ ] **Step 4: Handle auto-stop in MainComponent**

Implement `recordingAutoStopped()`:
```cpp
void recordingAutoStopped()
{
    isRecording = false;
    recordingPanel.stopRecording();
    toastComponent.showError("Gravacao parada: disco quase cheio.");
}
```

- [ ] **Step 5: Show toast on warning**

In `diskSpaceWarning(int min)`, if `min <= 10` and not already warned:
```cpp
toastComponent.showError("Espaco em disco baixo. Restam ~" + juce::String(min) + "min.");
```
Only show once (use a flag, reset on new recording).

- [ ] **Step 6: Build and verify**

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat: disk space monitoring with auto-stop"
```

---

## Chunk 3: Post-crash recovery

### Task 3: Detect and recover orphaned chunks

**Files:**
- Modify: `src/AudioEngine.h`
- Modify: `src/AudioEngine.cpp`
- Modify: `src/MainComponent.h`
- Modify: `src/MainComponent.cpp`
- Modify: `src/ToastComponent.h`
- Modify: `src/ToastComponent.cpp`

- [ ] **Step 1: Add recovery methods to AudioEngine**

```cpp
// Returns list of orphaned chunk folders in destFolder
juce::Array<juce::File> findOrphanedRecordings(const juce::File& destFolder);

// Concatenate chunks in a folder into a single WAV, delete folder
juce::File recoverRecording(const juce::File& chunkFolder);

// Delete an orphaned chunk folder
void discardRecording(const juce::File& chunkFolder);
```

`findOrphanedRecordings`: scan for subdirectories matching `BDG_rec_*` that contain `chunk_*.wav` files.

`recoverRecording`: reuse `concatenateChunks()` logic but parameterized on a folder. Read chunks in order, skip invalid ones, write final WAV.

`discardRecording`: `chunkFolder.deleteRecursively()`.

- [ ] **Step 2: Add action toast to ToastComponent**

Extend ToastComponent to support two action buttons:
```cpp
void showWithActions(const juce::String& msg,
                     const juce::String& action1, std::function<void()> onAction1,
                     const juce::String& action2, std::function<void()> onAction2);
```

Draw two small text buttons at the bottom of the toast. No auto-dismiss when actions are present.

- [ ] **Step 3: Check for orphaned recordings on startup**

In `MainComponent` constructor, after `loadSettings()`:
```cpp
auto orphans = audioEngine.findOrphanedRecordings(outputPanel.getDestFolder());
if (!orphans.isEmpty())
{
    auto folder = orphans.getFirst();
    toastComponent.showWithActions(
        "Gravacao anterior encontrada. Deseja recuperar?",
        "Recuperar", [this, folder]() {
            auto recovered = audioEngine.recoverRecording(folder);
            if (recovered.existsAsFile())
                toastComponent.showSuccess("Recuperado!", recovered.getFileName());
            else
                toastComponent.showError("Falha na recuperacao.");
        },
        "Descartar", [this, folder]() {
            audioEngine.discardRecording(folder);
            toastComponent.showSuccess("Gravacao descartada.", "");
        }
    );
}
```

- [ ] **Step 4: Build and verify**

Test: start recording, force-quit the app (`Cmd+Q` or `kill -9`), reopen. Should see recovery toast.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "feat: post-crash recovery for orphaned recordings"
```
