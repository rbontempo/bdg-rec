#include "AudioEngine.h"
#include "Dsp.h"

// Length of each on-disk chunk. Overridable at build time so the rotation and
// concatenation paths can be exercised by a test without recording for
// 5 minutes per boundary; production builds always use the default.
#ifndef BDG_CHUNK_SECONDS
 #define BDG_CHUNK_SECONDS (5.0 * 60.0)
#endif

//==============================================================================
AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    // Mark as dead so pending callAsync lambdas bail out
    alive->store(false);

    // Stop DSP thread if active
    if (dspThread != nullptr)
    {
        dspThread->stopThread(10000);
        dspThread.reset();
    }

    // Stop recording if active
    if (recording.load())
        stopRecording();

    // Unregister callbacks before destruction
    deviceManager.removeChangeListener(this);
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

//==============================================================================
void AudioEngine::initialise()
{
    // Request 2 input channels, 0 output channels
    auto error = deviceManager.initialiseWithDefaultDevices(2, 0);
    if (error.isNotEmpty())
        DBG("AudioEngine::initialise error: " + error);

    deviceManager.addAudioCallback(this);
    deviceManager.addChangeListener(this);

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        DBG("AudioEngine::initialise OK: " + dev->getName());
    else
        DBG("AudioEngine::initialise: no device opened by default");
}

//==============================================================================
juce::StringArray AudioEngine::getInputDevices()
{
    juce::StringArray names;

    // Try current device type first
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        names = type->getDeviceNames(true);
    }

    // Fallback: iterate all available device types
    if (names.isEmpty())
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            type->scanForDevices();
            auto devNames = type->getDeviceNames(true);
            names.addArray(devNames);
        }
    }

    return names;
}

juce::String AudioEngine::getCurrentInputDeviceName() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getName();
    return {};
}

juce::String AudioEngine::getCurrentInputDeviceCategory() const
{
    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev == nullptr)
        return "none";

    // Only ever returns one of these five fixed values — the device name is
    // matched here and then discarded, so nothing user-identifying escapes.
    const auto name = dev->getName().toLowerCase();

    if (name.contains("bluetooth") || name.contains("airpod") || name.contains("wireless"))
        return "bluetooth";

    if (name.contains("usb"))
        return "usb";

    if (name.contains("built-in") || name.contains("builtin")
        || name.contains("internal") || name.contains("macbook")
        || name.contains("interno") || name.contains("integrado"))
        return "builtin";

    return "other";
}

void AudioEngine::setInputDevice(const juce::String& name)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName  = name;
    setup.useDefaultInputChannels = true;
    setup.inputChannels.setRange(0, 2, true); // ensure at least 2 input channels requested
    setup.sampleRate = 0; // use device default
    setup.bufferSize = 0; // use device default
    auto error = deviceManager.setAudioDeviceSetup(setup, true);

    if (error.isNotEmpty())
        DBG("setInputDevice error: " + error);
    else
        DBG("setInputDevice OK: " + name);

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        DBG("  Active device: " + dev->getName()
            + " SR=" + juce::String(dev->getCurrentSampleRate())
            + " inputs=" + juce::String(dev->getActiveInputChannels().countNumberOfSetBits()));
    else
        DBG("  No active device after setInputDevice!");
}

//==============================================================================
void AudioEngine::setGain(float g)
{
    gain.store(juce::jlimit(0.0f, 2.0f, g));
}

float AudioEngine::getGain() const
{
    return gain.load();
}

void AudioEngine::setMonitorEnabled(bool enabled)
{
    monitorEnabled.store(enabled);
    auto setup = deviceManager.getAudioDeviceSetup();
    if (enabled)
    {
        setup.useDefaultOutputChannels = true;
    }
    else
    {
        setup.outputChannels.clear();
        setup.useDefaultOutputChannels = false;
    }
    deviceManager.setAudioDeviceSetup(setup, true);
}

float AudioEngine::getRmsL() const
{
    return rmsL.load();
}

float AudioEngine::getRmsR() const
{
    return rmsR.load();
}

//==============================================================================
void AudioEngine::addListener(Listener* l)
{
    listeners.add(l);
}

void AudioEngine::removeListener(Listener* l)
{
    listeners.remove(l);
}

//==============================================================================
// AudioIODeviceCallback
//==============================================================================
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        const double newRate = device->getCurrentSampleRate();

        // The device can reopen at a different rate mid-take without the user
        // touching anything (unplugging an interface, toggling monitoring).
        // Carrying on would feed new-rate samples into a writer built for the
        // old rate, and the concatenation copies raw samples without
        // resampling — every earlier chunk would come out at the wrong pitch
        // and duration. Salvage what we have instead of corrupting it.
        if (recording.load()
            && newRate != nativeSampleRate.load()
            && ! deviceMismatch.exchange(true))
        {
            auto aliveFlag = alive;
            juce::MessageManager::callAsync([this, aliveFlag]()
            {
                if (! aliveFlag->load() || ! recording.load())
                    return;

                auto saved = stopRecording();
                if (saved != juce::File())
                    listeners.call(&Listener::recordingStoppedDeviceChanged, saved);
            });

            // Leave nativeSampleRate alone: concatenateChunks() writes the
            // final header with it, and it must stay at the rate the chunks
            // on disk were actually recorded at.
            return;
        }

        nativeSampleRate.store(newRate);

        // Size the downmix scratch buffer here, off the audio thread. Generous
        // headroom over the declared block size so the callback never has to
        // clamp; `false, false, true` avoids keeping old contents and avoids
        // reallocating when the size does not actually grow.
        const int declared = juce::jmax(device->getCurrentBufferSizeSamples(), 512);
        monoBuffer.setSize(1, declared * 4, false, false, true);
        int ch = device->getActiveInputChannels().countNumberOfSetBits();
        if (ch == 0) ch = 1;
        nativeChannels.store(ch);
        DBG("audioDeviceAboutToStart: " + device->getName()
            + " SR=" + juce::String(nativeSampleRate.load())
            + " ch=" + juce::String(nativeChannels.load()));
    }

    lastUpdateMs.store(0);
}

void AudioEngine::audioDeviceStopped()
{
    rmsL.store(0.0f);
    rmsR.store(0.0f);
}

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    if (numSamples == 0 || numInputChannels == 0)
        return;

    const float g = gain.load();

    // ---- Compute RMS for L and R (pre-gain, then scale by gain, clamped to 1) ----
    auto computeRms = [&](int channelIndex) -> float
    {
        if (channelIndex >= numInputChannels || inputChannelData[channelIndex] == nullptr)
            return 0.0f;

        const float* data = inputChannelData[channelIndex];
        double sumSq = 0.0;
        for (int i = 0; i < numSamples; ++i)
            sumSq += static_cast<double>(data[i]) * static_cast<double>(data[i]);

        float rms = static_cast<float>(std::sqrt(sumSq / numSamples));
        return juce::jmin(rms * g, 1.0f);
    };

    rmsL.store(computeRms(0));
    rmsR.store(computeRms(numInputChannels > 1 ? 1 : 0));

    // ---- Monitor: route input to output ----
    if (monitorEnabled.load() && numOutputChannels > 0)
    {
        for (int outCh = 0; outCh < numOutputChannels; ++outCh)
        {
            if (outputChannelData[outCh] == nullptr)
                continue;

            int inCh = outCh < numInputChannels ? outCh : 0;
            if (inputChannelData[inCh] != nullptr)
            {
                for (int i = 0; i < numSamples; ++i)
                    outputChannelData[outCh][i] = inputChannelData[inCh][i] * g;
            }
            else
            {
                juce::FloatVectorOperations::clear(outputChannelData[outCh], numSamples);
            }
        }
    }
    else if (numOutputChannels > 0)
    {
        for (int outCh = 0; outCh < numOutputChannels; ++outCh)
            if (outputChannelData[outCh] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[outCh], numSamples);
    }

    // ---- Throttled UI notification (50 ms) ----
    const juce::int64 now = juce::Time::currentTimeMillis();
    if (now - lastUpdateMs.load() >= 50)
    {
        lastUpdateMs.store(now);
        triggerAsyncUpdate();
    }

    // ---- Recording pipeline ----
    // deviceMismatch means the rate changed under us: stop feeding the writer
    // at once, but leave `recording` set so stopRecording() still saves.
    if (recording.load() && ! deviceMismatch.load())
    {
        // The scratch buffer is sized with plenty of headroom in
        // audioDeviceAboutToStart; clamping here keeps a device that suddenly
        // delivers a larger block from writing past the end. Anything that
        // does not fit is counted rather than lost silently.
        const int n = juce::jmin(numSamples, monoBuffer.getNumSamples());
        if (n < numSamples)
            droppedSamples.fetch_add(numSamples - n);

        if (n <= 0)
            return;

        // Downmix all input channels to mono, applying gain
        monoBuffer.clear(0, 0, n);

        float* mono = monoBuffer.getWritePointer(0);
        const float channelScale = 1.0f / static_cast<float>(numInputChannels);

        for (int ch = 0; ch < numInputChannels; ++ch)
        {
            if (inputChannelData[ch] != nullptr)
            {
                for (int i = 0; i < n; ++i)
                    mono[i] += inputChannelData[ch][i] * channelScale * g;
            }
        }

        // Try to acquire the writer lock; skip write if can't (chunk rotating)
        const juce::SpinLock::ScopedTryLockType lock(writerLock);
        if (lock.isLocked() && threadedWriter != nullptr)
        {
            // write() returns false when its FIFO is full, which happens when
            // the disk stalls long enough to back up ~1.4 s of audio. Those
            // samples are gone; at least make the loss countable.
            if (! threadedWriter->write(monoBuffer.getArrayOfReadPointers(), n))
                droppedSamples.fetch_add(n);

            auto newCount = samplesInChunk.fetch_add(n) + n;

            // Just raise a flag — the rotation itself is done by the timer on
            // the message thread. callAsync() from here allocates the message
            // and can take a lock inside the OS queue, neither of which
            // belongs on the audio thread.
            if (newCount >= samplesPerChunk)
                chunkRotationPending.store(true);
        }
        else
        {
            droppedSamples.fetch_add(n);
        }
    }
}

//==============================================================================
// AsyncUpdater
//==============================================================================
void AudioEngine::handleAsyncUpdate()
{
    float l = rmsL.load();
    float r = rmsR.load();
    listeners.call(&Listener::audioLevelsChanged, l, r);
}

//==============================================================================
// ChangeListener – device hot-plug (Task 19)
//==============================================================================
void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    // This is called on the message thread by AudioDeviceManager when
    // the device configuration changes (including hot-plug events).

    // If we are recording and the current device is gone, stop recording.
    // The take is handed to the listener so it can be revealed and processed
    // like any other; if the save itself failed, stopRecording() has already
    // reported that through recordingSaveFailed and we stay quiet here rather
    // than stacking a second dialog on top.
    if (recording.load() && deviceManager.getCurrentAudioDevice() == nullptr)
    {
        auto saved = stopRecording();
        if (saved != juce::File())
            listeners.call(&Listener::recordingStoppedDeviceChanged, saved);
    }

    // Notify all listeners so UI can refresh device list
    listeners.call(&Listener::devicesChanged);
}

//==============================================================================
// Recording pipeline
//==============================================================================
bool AudioEngine::startRecording(const juce::File& destFolder)
{
    DBG("AudioEngine::startRecording destFolder=" + destFolder.getFullPathName());
    if (recording.load())
    {
        DBG("  Already recording, returning false");
        return false;
    }

    // Make sure destination folder exists
    if (!destFolder.isDirectory())
        destFolder.createDirectory();

    // Retrieve current device parameters
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        nativeSampleRate.store(device->getCurrentSampleRate());
        int ch = device->getActiveInputChannels().countNumberOfSetBits();
        if (ch == 0) ch = 1;
        nativeChannels.store(ch);

        // Belt and braces: audioDeviceAboutToStart normally sizes this, but
        // the callback must never find it empty.
        const int declared = juce::jmax(device->getCurrentBufferSizeSamples(), 512);
        monoBuffer.setSize(1, declared * 4, false, false, true);
    }
    else
    {
        return false;
    }

    // Create timestamped subfolder for chunks
    const juce::String timestamp =
        juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
    chunkFolder = destFolder.getChildFile("BDG_rec_" + timestamp);
    chunkFolder.createDirectory();

    samplesPerChunk = (juce::int64)(BDG_CHUNK_SECONDS * nativeSampleRate.load());
    chunkIndex = 0;
    deviceMismatch.store(false);

    if (!openNextChunk())
    {
        chunkFolder.deleteRecursively();
        return false;
    }

    droppedSamples.store(0);
    timerTicks = 0;

    recording.store(true);
    startTimer(1000); // chunk rotation each tick; disk space every 10th
    DBG("  Chunk recording started. chunkFolder=" + chunkFolder.getFullPathName()
        + " samplesPerChunk=" + juce::String(samplesPerChunk));
    return true;
}

bool AudioEngine::openNextChunk()
{
    // The previous writer stays live and recording until the new one is fully
    // built. Closing it first (flushing 64k samples, finalising the header,
    // creating a file, creating a writer) would leave the audio callback with
    // nowhere to write for the whole duration — tens to hundreds of ms of
    // samples dropped at every 5-minute boundary, an audible gap.
    const int nextIndex = chunkIndex + 1;

    juce::File chunkFile = chunkFolder.getChildFile(
        juce::String::formatted("chunk_%03d.wav", nextIndex));

    auto fileStream = std::unique_ptr<juce::FileOutputStream>(
        chunkFile.createOutputStream());

    if (fileStream == nullptr)
    {
        DBG("  Failed to create FileOutputStream for chunk: " + chunkFile.getFullPathName());
        chunkRotationPending.store(false);
        return false;
    }

    auto* writer = wavFormat.createWriterFor(
        fileStream.get(),
        nativeSampleRate.load(),
        1,    // mono
        24,   // bit depth
        {},   // metadata
        0);   // quality option index

    if (writer == nullptr)
    {
        DBG("  Failed to create WAV writer for chunk " + juce::String(nextIndex));
        chunkRotationPending.store(false);
        return false;
    }

    // Transfer ownership of the stream to the writer
    fileStream.release();

    // One writer thread serves every chunk for the whole session — tearing it
    // down and restarting it per rotation was pure added latency in the gap.
    if (writerThread == nullptr)
    {
        writerThread = std::make_unique<juce::TimeSliceThread>("BDG Audio Writer");
        writerThread->startThread();
    }

    auto newWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, *writerThread, 65536);

    // Everything above happened while the old writer was still accepting
    // samples. The swap itself is a couple of pointer moves — the audio
    // callback can miss at most one block on the try-lock, not a whole flush.
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> oldWriter;
    {
        const juce::SpinLock::ScopedLockType lock(writerLock);
        oldWriter = std::move(threadedWriter);
        threadedWriter = std::move(newWriter);
        samplesInChunk.store(0);
    }

    chunkIndex = nextIndex;

    // Flush and finalise the previous chunk outside the lock, so the audio
    // thread is never blocked waiting on disk I/O.
    oldWriter.reset();

    chunkRotationPending.store(false);

    DBG("  Opened chunk " + juce::String(chunkIndex) + ": " + chunkFile.getFullPathName());
    return true;
}

AudioEngine::ConcatResult AudioEngine::concatenateChunks()
{
    ConcatResult result;

    juce::File finalFile = chunkFolder.getParentDirectory().getChildFile(
        chunkFolder.getFileName() + ".wav");
    result.file = finalFile;

    auto outStream = std::unique_ptr<juce::FileOutputStream>(
        finalFile.createOutputStream());

    if (outStream == nullptr)
    {
        DBG("  concatenateChunks: failed to create output stream");
        return result;
    }

    auto* finalWriter = wavFormat.createWriterFor(
        outStream.get(),
        nativeSampleRate.load(),
        1,    // mono
        24,   // bit depth
        {},   // metadata
        0);   // quality option index

    if (finalWriter == nullptr)
    {
        DBG("  concatenateChunks: failed to create WAV writer");
        finalFile.deleteFile();
        return result;
    }

    outStream.release(); // writer owns the stream

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::int64 samplesExpected = 0;
    juce::int64 samplesWritten  = 0;
    bool allWritesSucceeded = true;
    int  chunksSkipped = 0;

    for (int i = 1; i <= chunkIndex; ++i)
    {
        juce::File chunkFile = chunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));

        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(chunkFile));

        if (reader == nullptr)
        {
            // Salvage policy: an unreadable chunk costs us that chunk, not the
            // whole take. Failing here would delete a final file that still
            // holds every other chunk. Only a failed *write* (a full disk) is
            // fatal, because then the output itself is untrustworthy.
            DBG("  concatenateChunks: skipping corrupted chunk " + juce::String(i));
            ++chunksSkipped;
            continue;
        }

        const juce::int64 totalSamples = reader->lengthInSamples;
        samplesExpected += totalSamples;

        const int blockSize = 65536;
        juce::AudioBuffer<float> buffer(1, blockSize);

        for (juce::int64 pos = 0; pos < totalSamples; pos += blockSize)
        {
            int samplesToRead = (int)juce::jmin((juce::int64)blockSize, totalSamples - pos);
            reader->read(&buffer, 0, samplesToRead, pos, true, false);

            // A full disk makes this fail silently — the stream just stops
            // accepting bytes. Ignoring the result is how a truncated file
            // used to pass for a finished recording.
            if (! finalWriter->writeFromAudioSampleBuffer(buffer, 0, samplesToRead))
            {
                DBG("  concatenateChunks: write FAILED at chunk " + juce::String(i));
                allWritesSucceeded = false;
                break;
            }

            samplesWritten += samplesToRead;
        }

        if (! allWritesSucceeded)
            break;
    }

    // Destroying the writer flushes any buffered data and rewrites the WAV
    // header with the real length — must happen before we verify the file.
    delete finalWriter;

    // Final proof: read the finished file back and check it actually holds
    // every sample we set out to write. Cheap, and it catches truncation the
    // write path missed. A take with no samples at all (started and stopped
    // inside one buffer, or no microphone permission) is an honest empty
    // result, not a save failure — reporting "disk full" for it would be a lie.
    bool verified = false;
    if (allWritesSucceeded && samplesWritten == samplesExpected)
    {
        if (samplesWritten == 0)
        {
            verified = true;
        }
        else
        {
            std::unique_ptr<juce::AudioFormatReader> check(
                formatManager.createReaderFor(finalFile));

            if (check != nullptr && check->lengthInSamples == samplesExpected)
                verified = true;
            else
                DBG("  concatenateChunks: verification FAILED on final file");
        }
    }

    result.ok = verified;

    DBG("  concatenateChunks: final file = " + finalFile.getFullPathName()
        + " ok=" + juce::String(verified ? "yes" : "no")
        + " samples=" + juce::String(samplesWritten) + "/" + juce::String(samplesExpected)
        + " skippedChunks=" + juce::String(chunksSkipped));

    return result;
}

juce::File AudioEngine::stopRecording()
{
    stopTimer();

    // Reentrancy guard. The auto-stop path leaves a window in which a queued
    // button click can call this a second time; the second pass would run the
    // concatenation against an already-deleted chunk folder and write an empty
    // 44-byte WAV straight over the good recording.
    if (! recording.exchange(false))
        return {};

    // Flush and close current chunk under writer lock
    {
        const juce::SpinLock::ScopedLockType lock(writerLock);
        threadedWriter.reset();
    }

    if (writerThread != nullptr)
    {
        writerThread->stopThread(2000);
        writerThread.reset();
    }

    if (droppedSamples.load() > 0)
        DBG("  stopRecording: WARNING " + juce::String(droppedSamples.load())
            + " samples were dropped by the write path");

    // Concatenate all chunks into final file
    auto result = concatenateChunks();

    if (result.ok)
    {
        chunkFolder.deleteRecursively();
        return result.file;
    }

    // Concatenation failed — most likely the volume filled up. The chunks are
    // the only intact copy of the audio, so they stay put; findOrphanedRecordings
    // picks them up next launch. Drop the truncated output so it can't be
    // mistaken for a finished recording.
    DBG("  stopRecording: concat failed, PRESERVING chunks at " + chunkFolder.getFullPathName());
    result.file.deleteFile();
    listeners.call(&Listener::recordingSaveFailed, chunkFolder);

    return {};
}

bool AudioEngine::isRecording() const
{
    return recording.load();
}

//==============================================================================
// Task 2: Disk space polling timer
//==============================================================================
void AudioEngine::timerCallback()
{
    if (!recording.load()) return;

    // ---- Chunk rotation (flag raised by the audio thread) ----
    if (chunkRotationPending.load())
    {
        // A failed rotation leaves the previous writer in place, so no audio
        // is lost — but the sample counter stays over the threshold and every
        // following tick would retry. Stop cleanly instead of hammering a disk
        // that just refused us.
        if (! openNextChunk())
        {
            DBG("  chunk rotation failed — stopping recording");
            if (stopRecording() != juce::File())
                listeners.call(&Listener::recordingAutoStopped);
            return;
        }
    }

    // ---- Disk space (every 10th tick, i.e. the original 10 s cadence) ----
    if (++timerTicks < kDiskCheckEveryTicks)
        return;

    timerTicks = 0;

    auto freeBytes = chunkFolder.getBytesFreeOnVolume();
    int bytesPerSec = (int)(nativeSampleRate.load() * 3.0); // 24-bit mono = 3 bytes/sample
    int remainingSec = (bytesPerSec > 0) ? (int)(freeBytes / bytesPerSec) : 9999;
    int remainingMin = remainingSec / 60;

    listeners.call(&Listener::diskSpaceWarning, remainingMin);

    if (remainingMin < 2)
    {
        stopTimer();
        // Auto-stop on message thread
        auto aliveFlag = alive;
        juce::MessageManager::callAsync([this, aliveFlag]() {
            if (!aliveFlag->load()) return;
            if (recording.load())
            {
                // If the save itself failed, stopRecording() already told the
                // listeners via recordingSaveFailed — don't stack a second
                // dialog on top of it.
                if (stopRecording() != juce::File())
                    listeners.call(&Listener::recordingAutoStopped);
            }
        });
    }
}

//==============================================================================
// Task 3: Post-crash recovery
//==============================================================================
juce::Array<juce::File> AudioEngine::findOrphanedRecordings(const juce::File& destFolder)
{
    juce::Array<juce::File> orphans;

    if (!destFolder.isDirectory())
        return orphans;

    for (const auto& entry : juce::RangedDirectoryIterator(destFolder, false, "*", juce::File::findDirectories))
    {
        juce::File dir = entry.getFile();
        if (!dir.getFileName().startsWith("BDG_rec_"))
            continue;

        // Check for at least one chunk file (chunk_001.wav is always the first)
        if (dir.getChildFile("chunk_001.wav").existsAsFile())
            orphans.add(dir);
    }

    return orphans;
}

juce::File AudioEngine::recoverRecording(const juce::File& orphanedChunkFolder)
{
    // Detect sample rate from first valid chunk
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    double detectedSampleRate = 44100.0;
    int detectedChunkCount = 0;

    // Count chunks and detect sample rate
    for (int i = 1; i <= 9999; ++i)
    {
        juce::File chunkFile = orphanedChunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));
        if (!chunkFile.existsAsFile())
            break;

        if (detectedChunkCount == 0)
        {
            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(chunkFile));
            if (reader != nullptr)
                detectedSampleRate = reader->sampleRate;
        }
        detectedChunkCount++;
    }

    if (detectedChunkCount == 0)
    {
        DBG("recoverRecording: no chunks found in " + orphanedChunkFolder.getFullPathName());
        return {};
    }

    // Create output file next to the orphaned folder
    juce::File finalFile = orphanedChunkFolder.getParentDirectory().getChildFile(
        orphanedChunkFolder.getFileName() + ".wav");

    auto outStream = std::unique_ptr<juce::FileOutputStream>(
        finalFile.createOutputStream());

    if (outStream == nullptr)
    {
        DBG("recoverRecording: failed to create output stream");
        return {};
    }

    auto* finalWriter = wavFormat.createWriterFor(
        outStream.get(),
        detectedSampleRate,
        1,    // mono
        24,   // bit depth
        {},
        0);

    if (finalWriter == nullptr)
    {
        DBG("recoverRecording: failed to create WAV writer");
        return {};
    }

    outStream.release(); // writer owns the stream

    for (int i = 1; i <= detectedChunkCount; ++i)
    {
        juce::File chunkFile = orphanedChunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));

        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(chunkFile));

        if (reader == nullptr)
        {
            DBG("recoverRecording: skipping corrupted chunk " + juce::String(i));
            continue;
        }

        const juce::int64 totalSamples = reader->lengthInSamples;
        const int blockSize = 65536;
        juce::AudioBuffer<float> buffer(1, blockSize);

        for (juce::int64 pos = 0; pos < totalSamples; pos += blockSize)
        {
            int samplesToRead = (int)juce::jmin((juce::int64)blockSize, totalSamples - pos);
            reader->read(&buffer, 0, samplesToRead, pos, true, false);
            finalWriter->writeFromAudioSampleBuffer(buffer, 0, samplesToRead);
        }
    }

    delete finalWriter;

    // Remove orphaned folder
    orphanedChunkFolder.deleteRecursively();

    DBG("recoverRecording: recovered to " + finalFile.getFullPathName());
    return finalFile;
}

void AudioEngine::discardRecording(const juce::File& orphanedChunkFolder)
{
    orphanedChunkFolder.deleteRecursively();
    DBG("discardRecording: deleted " + orphanedChunkFolder.getFullPathName());
}

//==============================================================================
// DSP processing on background thread
//==============================================================================
void AudioEngine::processRecording(const juce::File& file,
                                   bool doNormalize,
                                   bool doNoiseReduction,
                                   bool doCompressor,
                                   bool doDeEsser)
{
    // If a previous DSP thread is still running, wait for it
    if (dspThread != nullptr)
    {
        dspThread->stopThread(10000);
        dspThread.reset();
    }

    // Capture what we need for the lambda
    auto* engine = this;

    class DspThread : public juce::Thread
    {
    public:
        DspThread(AudioEngine* e, juce::File f, bool norm, bool nr, bool comp, bool de,
                  std::shared_ptr<std::atomic<bool>> aliveFlag)
            : juce::Thread("BDG DSP"),
              engine(e), file(std::move(f)),
              doNormalize(norm), doNoiseReduction(nr), doCompressor(comp), doDeEsser(de),
              alive(std::move(aliveFlag)) {}

        void run() override
        {
            auto emitStep = [this](const juce::String& step)
            {
                auto* eng = engine;
                auto flag = alive;
                juce::MessageManager::callAsync([eng, step, flag]()
                {
                    if (!flag->load()) return;
                    eng->listeners.call(&AudioEngine::Listener::dspStepChanged, step);
                });
            };

            auto emitError = [this](const juce::String& msg)
            {
                auto* eng = engine;
                auto flag = alive;
                juce::MessageManager::callAsync([eng, msg, flag]()
                {
                    if (!flag->load()) return;
                    eng->listeners.call(&AudioEngine::Listener::dspError, msg);
                });
            };

            auto emitFinish = [this](const juce::File& f)
            {
                auto* eng = engine;
                juce::File resultFile = f;
                auto flag = alive;
                juce::MessageManager::callAsync([eng, resultFile, flag]()
                {
                    if (!flag->load()) return;
                    eng->listeners.call(&AudioEngine::Listener::dspFinished, resultFile);
                });
            };

            // 1) Read the WAV file
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();

            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(file));

            if (reader == nullptr)
            {
                emitError("Failed to read file: " + file.getFullPathName());
                return;
            }

            const juce::int64 totalSamples = reader->lengthInSamples;
            const double sampleRate = reader->sampleRate;
            const int bitsPerSample = reader->bitsPerSample;

            // Limit to ~6 hours at 48kHz (~1 billion samples) to avoid excessive memory use
            constexpr juce::int64 maxSamples = 1'036'800'000LL;
            if (totalSamples > maxSamples)
            {
                emitError("File too large for DSP processing (>" + juce::String(maxSamples / (juce::int64)sampleRate / 3600) + "h)");
                return;
            }

            const int numSamples = static_cast<int>(totalSamples);
            juce::AudioBuffer<float> buffer(1, numSamples);
            reader->read(&buffer, 0, numSamples, 0, true, false);
            reader.reset(); // close the file

            // 2) Notify DSP started
            {
                auto* eng = engine;
                auto flag = alive;
                juce::MessageManager::callAsync([eng, flag]()
                {
                    if (!flag->load()) return;
                    eng->listeners.call(&AudioEngine::Listener::dspStarted);
                });
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 3) Normalize
            if (doNormalize)
            {
                DBG("DSP: running normalize on " + juce::String(numSamples) + " samples");
                emitStep("normalize");
                Dsp::normalize(buffer, sampleRate);
                DBG("DSP: normalize done");
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 4) Noise reduction
            if (doNoiseReduction)
            {
                DBG("DSP: running noiseReduce on " + juce::String(numSamples) + " samples, SR=" + juce::String(sampleRate)
                    + " (" + juce::String(numSamples / sampleRate, 1) + "s)");
                emitStep("noise_reduction");
                Dsp::noiseReduce(buffer, sampleRate);
                DBG("DSP: noiseReduce done");
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 5) Compressor
            if (doCompressor)
            {
                emitStep("compressor");
                Dsp::compress(buffer, sampleRate);
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 6) De-Esser
            if (doDeEsser)
            {
                emitStep("de_esser");
                Dsp::deEss(buffer, sampleRate);
            }

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 7) Save back to file
            DBG("DSP: saving to " + file.getFullPathName());
            emitStep("saving");

            juce::File tempFile = file.getSiblingFile(file.getFileNameWithoutExtension() + "_tmp.wav");

            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::FileOutputStream> outStream(
                    tempFile.createOutputStream());

                if (outStream == nullptr)
                {
                    emitError("Failed to create output file");
                    return;
                }

                auto* writer = wav.createWriterFor(
                    outStream.get(), sampleRate, 1, bitsPerSample, {}, 0);

                if (writer == nullptr)
                {
                    emitError("Failed to create WAV writer");
                    return;
                }

                outStream.release(); // writer owns the stream now
                writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
                delete writer;
            }

            // Replace original with processed file
            bool moved = tempFile.moveFileTo(file);
            DBG("DSP: moveFileTo " + juce::String(moved ? "OK" : "FAILED"));

            // 8) Notify finished
            emitFinish(file);
        }

    private:
        AudioEngine* engine;
        juce::File file;
        bool doNormalize, doNoiseReduction, doCompressor, doDeEsser;
        std::shared_ptr<std::atomic<bool>> alive;
    };

    dspThread = std::make_unique<DspThread>(engine, file,
                                            doNormalize, doNoiseReduction, doCompressor, doDeEsser,
                                            alive);
    dspThread->startThread();
}
