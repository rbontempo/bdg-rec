#include "AudioEngine.h"
#include "Dsp.h"

// Length of each on-disk chunk. Overridable at build time so the rotation and
// concatenation paths can be exercised by a test without recording for
// 5 minutes per boundary; production builds always use the default.
#ifndef BDG_CHUNK_SECONDS
 #define BDG_CHUNK_SECONDS (5.0 * 60.0)
#endif

namespace
{
    struct RawWavInfo
    {
        juce::int64 dataOffset = 0;
        double      sampleRate = 0.0;
        bool        ok = false;
    };

    // Walks the RIFF chunks by hand. Needed because a chunk left behind by a
    // crash has a header the normal reader will not touch, yet the `fmt ` and
    // `data` chunks themselves are perfectly intact.
    RawWavInfo readWavLayout(juce::InputStream& stream, juce::int64 fileSize)
    {
        RawWavInfo info;

        // RF64 is what JUCE switches to past 4 GB (around 8 h at 48 kHz); its
        // chunk layout up to `data` is the same as RIFF's.
        const int magic = stream.readInt();
        if (magic != (int) juce::ByteOrder::littleEndianInt("RIFF")
            && magic != (int) juce::ByteOrder::littleEndianInt("RF64"))
            return info;
        stream.readInt(); // riff size (unreliable on a truncated file)
        if (stream.readInt() != (int) juce::ByteOrder::littleEndianInt("WAVE"))
            return info;

        while (! stream.isExhausted() && stream.getPosition() + 8 <= fileSize)
        {
            const int id   = stream.readInt();
            const int size = stream.readInt();
            const juce::int64 payload = stream.getPosition();

            if (id == (int) juce::ByteOrder::littleEndianInt("fmt ") && size >= 16)
            {
                // audioFormat(2) numChannels(2) sampleRate(4) ...
                // The size check matters: on a malformed file those bytes
                // would be the next chunk's header read as a sample rate.
                stream.setPosition(payload + 4);
                info.sampleRate = (double) (juce::uint32) stream.readInt();
            }
            else if (id == (int) juce::ByteOrder::littleEndianInt("data"))
            {
                info.dataOffset = payload;
                info.ok = info.sampleRate > 0.0;
                return info;
            }

            if (size < 0)
                return info;

            stream.setPosition(payload + size + (size & 1)); // chunks are word-aligned
        }

        return info;
    }

    // How many 24-bit mono samples the file really holds, from its size on
    // disk rather than from the length its header claims. -1 if the layout
    // cannot be read. The two disagree exactly in the cases that matter: a
    // chunk that was open when the app died (header says 0, or the length at
    // the last flush) and a file whose final bytes never reached the disk
    // (header says complete, the data is short).
    juce::int64 samplesOnDisk(const juce::File& file)
    {
        auto stream = std::unique_ptr<juce::FileInputStream>(file.createInputStream());
        if (stream == nullptr || ! stream->openedOk())
            return -1;

        const juce::int64 fileSize = stream->getTotalLength();
        const auto info = readWavLayout(*stream, fileSize);
        if (! info.ok || info.dataOffset <= 0 || info.dataOffset > fileSize)
            return -1;

        return (fileSize - info.dataOffset) / 3;
    }

    // The finished file reads back at the expected length AND that many
    // samples are physically on disk. Checking the header alone was not
    // enough: JUCE rewrites it with the full length even when the last
    // buffered bytes failed to reach a full disk.
    bool verifyWavOnDisk(juce::AudioFormatManager& formats,
                         const juce::File& file, juce::int64 expectedSamples)
    {
        std::unique_ptr<juce::AudioFormatReader> check(formats.createReaderFor(file));
        if (check == nullptr || check->lengthInSamples != expectedSamples)
            return false;

        return samplesOnDisk(file) >= expectedSamples;
    }

    // Reads the sample rate of a chunk whose header the WAV reader rejects.
    double readRawSampleRate(const juce::File& chunkFile)
    {
        auto stream = std::unique_ptr<juce::FileInputStream>(chunkFile.createInputStream());
        if (stream == nullptr || ! stream->openedOk())
            return 0.0;

        return readWavLayout(*stream, stream->getTotalLength()).sampleRate;
    }

    // A WAV writer only backfills the real length into the header when it is
    // closed. After a crash the chunk that was being recorded still holds all
    // its PCM, but the header claims zero samples, so the normal copy path
    // silently drops it. This derives the sample count from the bytes that are
    // actually on disk.
    //
    // `failed` is set when the copy stopped short — a read error, or a write
    // the output refused (full disk). The caller must not treat the result as
    // complete then: it used to, and deleted the chunks.
    juce::int64 salvageTruncatedChunk(const juce::File& chunkFile,
                                      juce::AudioFormatWriter* writer,
                                      bool& failed)
    {
        failed = false;
        auto stream = std::unique_ptr<juce::FileInputStream>(chunkFile.createInputStream());
        if (stream == nullptr || ! stream->openedOk())
        {
            failed = true;
            return 0;
        }

        const juce::int64 fileSize = stream->getTotalLength();
        if (fileSize <= 44)
            return 0;

        const auto info = readWavLayout(*stream, fileSize);
        const juce::int64 dataOffset = info.dataOffset;

        if (dataOffset <= 0 || dataOffset >= fileSize)
        {
            failed = true;   // real bytes, but no readable layout
            return 0;
        }

        // 24-bit mono: 3 bytes per sample.
        const juce::int64 numSamples = (fileSize - dataOffset) / 3;
        if (numSamples <= 0)
            return 0;

        stream->setPosition(dataOffset);

        const int blockSize = 65536;
        juce::AudioBuffer<float> buffer(1, blockSize);
        std::vector<juce::uint8> raw((size_t) blockSize * 3);
        juce::int64 written = 0;

        while (written < numSamples)
        {
            const int thisBlock = (int) juce::jmin((juce::int64) blockSize, numSamples - written);
            const int wanted = thisBlock * 3;

            if (stream->read(raw.data(), wanted) != wanted)
            {
                failed = true;
                break;
            }

            float* dest = buffer.getWritePointer(0);
            for (int i = 0; i < thisBlock; ++i)
            {
                const juce::uint8* p = raw.data() + (size_t) i * 3;
                juce::int32 v = (juce::int32) (p[0] | (p[1] << 8) | (p[2] << 16));
                if (v & 0x800000) v |= ~0xffffff; // sign-extend 24 -> 32 bits
                dest[i] = (float) v / 8388608.0f;
            }

            if (! writer->writeFromAudioSampleBuffer(buffer, 0, thisBlock))
            {
                failed = true;
                break;
            }

            written += thisBlock;
        }

        return written;
    }

    // WavAudioFormatWriter::flush() only rewrites the header and hands the
    // buffered bytes to the OS; it never calls the stream's flush(), which is
    // where FileOutputStream does its fsync. So the data could still be in
    // the page cache when the chunks were deleted, and a power cut right
    // after "saved" lost the take. This pairs the two.
    bool flushToDisk(juce::AudioFormatWriter* writer, juce::FileOutputStream* stream)
    {
        const bool headerOk = writer->flush();
        stream->flush();
        return headerOk && stream->getStatus().wasOk();
    }

    // Wraps the live chunk's WAV writer so the ThreadedWriter's periodic
    // flush() also syncs the file, on the writer thread where it runs.
    class SyncingWriter : public juce::AudioFormatWriter
    {
    public:
        SyncingWriter(juce::AudioFormatWriter* inner, juce::FileOutputStream* s)
            : juce::AudioFormatWriter(nullptr, inner->getFormatName(), inner->getSampleRate(),
                                      (unsigned int) inner->getNumChannels(),
                                      (unsigned int) inner->getBitsPerSample()),
              wav(inner), stream(s)
        {
            usesFloatingPointData = inner->isFloatingPoint();
        }

        bool write(const int** samples, int numSamples) override
        {
            return wav->write(samples, numSamples);
        }

        bool flush() override { return flushToDisk(wav.get(), stream); }

    private:
        std::unique_ptr<juce::AudioFormatWriter> wav; // owns the stream
        juce::FileOutputStream* stream;
    };

    // Copies one chunk into `writer`. Takes the normal reader path when the
    // header and the bytes on disk agree, and reads the raw PCM otherwise.
    // Anything but `ok` means the chunk was not copied in full: `unreadable`
    // when its bytes cannot be made sense of, `writeFailed` when the output
    // refused data (full disk) or a read stopped mid-way. `samples` gets what
    // was written either way.
    enum class ChunkCopy { ok, unreadable, writeFailed };

    ChunkCopy copyChunk(juce::AudioFormatManager& formats, const juce::File& chunkFile,
                        juce::AudioFormatWriter* writer, juce::int64& samples)
    {
        samples = 0;

        // A header-only file is an empty chunk, not a damaged one.
        if (chunkFile.getSize() <= 44)
            return ChunkCopy::ok;

        const juce::int64 onDisk = samplesOnDisk(chunkFile);

        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(chunkFile));
        const juce::int64 inHeader = (reader != nullptr) ? reader->lengthInSamples : -1;

        if (reader == nullptr || inHeader != onDisk)
        {
            reader.reset(); // release the handle before re-reading the file
            bool failed = false;
            samples = salvageTruncatedChunk(chunkFile, writer, failed);
            DBG("  copyChunk: salvaged " + juce::String(samples) + " samples from "
                + chunkFile.getFileName() + " (header " + juce::String(inHeader)
                + ", on disk " + juce::String(onDisk) + ")");
            if (! failed)
                return ChunkCopy::ok;
            return (onDisk < 0 && samples == 0) ? ChunkCopy::unreadable : ChunkCopy::writeFailed;
        }

        const int blockSize = 65536;
        juce::AudioBuffer<float> buffer(1, blockSize);

        for (juce::int64 pos = 0; pos < inHeader; pos += blockSize)
        {
            const int n = (int) juce::jmin((juce::int64) blockSize, inHeader - pos);
            if (! reader->read(&buffer, 0, n, pos, true, false))
                return ChunkCopy::unreadable;

            // A full disk makes this fail silently — the stream just stops
            // accepting bytes. Ignoring the result is how a truncated file
            // used to pass for a finished recording.
            if (! writer->writeFromAudioSampleBuffer(buffer, 0, n))
                return ChunkCopy::writeFailed;

            samples += n;
        }

        return ChunkCopy::ok;
    }
}

//==============================================================================
AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    // First: the timer touches listeners and chunk state, and ~Timer would
    // otherwise only run after those members are gone.
    stopTimer();

    // Mark as dead so pending callAsync lambdas bail out
    alive->store(false);

    // Stop DSP thread if active
    if (dspThread != nullptr)
    {
        dspThread->stopThread(10000);
        dspThread.reset();
    }

    // Stop recording if active. Blocking on purpose: the app is going away,
    // so the merge has to finish here rather than on a background thread.
    if (recording.load())
        stopRecording();

    if (recoveryThread != nullptr)
    {
        // Same reasoning as the save thread: never kill a copy half-way.
        recoveryThread->stopThread(-1);
        recoveryThread.reset();
    }

    if (saveThread != nullptr)
    {
        // -1 waits indefinitely. A forced kill would leave a half-written
        // final file on disk, which the next launch's recovery would then
        // append to.
        saveThread->stopThread(-1);
        saveThread.reset();
    }

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

    // 20 Hz: fast enough for the VU meter, and it doubles as the chunk
    // rotation and disk-space poll while a take is running.
    startTimer(kTimerIntervalMs);

    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        // initialiseWithDefaultDevices already enumerated on its way to
        // opening this device, so the first getInputDevices() must not scan
        // again — that second scan was a second microphone prompt.
        needsDeviceRescan = false;
        DBG("AudioEngine::initialise OK: " + dev->getName());
    }
    else
    {
        DBG("AudioEngine::initialise: no device opened by default");
    }
}

//==============================================================================
juce::StringArray AudioEngine::getInputDevices()
{
    if (! cachedInputDevices.isEmpty())
        return cachedInputDevices;

    juce::StringArray names;

    // Scan only when something actually changed. A device is already open at
    // this point (initialise() enumerated on the way), so rescanning would
    // just be a second permission request for the same list.
    const bool rescan = needsDeviceRescan || deviceManager.getCurrentAudioDevice() == nullptr;

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        if (rescan)
            type->scanForDevices();
        names = type->getDeviceNames(true);
    }

    // Fallback: iterate all available device types
    if (names.isEmpty())
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            if (rescan)
                type->scanForDevices();
            auto devNames = type->getDeviceNames(true);
            names.addArray(devNames);
        }
    }

    cachedInputDevices = names;
    needsDeviceRescan = false;
    return names;
}

juce::String AudioEngine::getCurrentInputDeviceName() const
{
    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev == nullptr)
        return {};

    // The configured input, not the device's own name: with monitoring on,
    // CoreAudio combines input and output into one device named after the
    // OUTPUT, which would not match any entry in the input list.
    const auto input = deviceManager.getAudioDeviceSetup().inputDeviceName;
    return input.isNotEmpty() ? input : dev->getName();
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
        if (recording.load() && newRate != nativeSampleRate.load())
        {
            // Only post the stop once, but return on EVERY reopen. Devices
            // renegotiate in bursts, and the earlier version fell through to
            // the store() below on the second one — writing the new rate into
            // the very field the merge uses for the final header.
            if (! deviceMismatch.exchange(true))
            {
                auto aliveFlag = alive;
                juce::MessageManager::callAsync([this, aliveFlag]()
                {
                    if (! aliveFlag->load() || ! recording.load())
                        return;

                    stopRecordingAsync(StopReason::DeviceRateChanged);
                });
            }

            // Leave nativeSampleRate alone: it must stay at the rate the
            // chunks on disk were actually recorded at.
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

    // Levels are published as plain atomics; the always-on timer picks them
    // up on the message thread. Posting a message from here (triggerAsyncUpdate)
    // goes through the message queue's lock and can allocate when the queue
    // grows — the same thing the comment below rejects for callAsync.

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

        // Downmix all input channels to mono, applying gain.
        // Divide by the channels that actually carry samples, not by the size
        // of the array. The engine asks for 2 inputs; a mono interface hands
        // back one live pointer and one null, and dividing by 2 anyway made
        // every recording 6 dB quieter than it should be.
        int contributing = 0;
        for (int ch = 0; ch < numInputChannels; ++ch)
            if (inputChannelData[ch] != nullptr)
                ++contributing;

        if (contributing == 0)
            return;

        monoBuffer.clear(0, 0, n);

        float* mono = monoBuffer.getWritePointer(0);
        const float channelScale = 1.0f / static_cast<float>(contributing);

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
            // the disk stalls long enough to back up ~5 s of audio. Those
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
        stopRecordingAsync(StopReason::DeviceLost);

    // The input the take started on is gone and JUCE fell back to another
    // one (typically the built-in microphone). The rate often matches, so the
    // rate guard does not fire, and the take used to carry on from the wrong
    // microphone with the old name still on screen. Compare the configured
    // input, not the device name: toggling monitoring can reopen the same
    // input under a combined input+output name.
    else if (recording.load() && recordingDeviceName.isNotEmpty()
             && deviceManager.getAudioDeviceSetup().inputDeviceName != recordingDeviceName)
    {
        DBG("  input changed from " + recordingDeviceName + " to "
            + deviceManager.getAudioDeviceSetup().inputDeviceName + " — stopping");
        stopRecordingAsync(StopReason::DeviceLost);
    }

    // The device set really did change, so this is the one moment a rescan
    // is warranted.
    cachedInputDevices.clear();
    needsDeviceRescan = true;

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

    // Rotation and disk polling ride on the timer started in initialise().
    jassert(isTimerRunning());

    // The merge of the previous take reads chunkFolder/chunkIndex; starting a
    // new one would repoint them under it, and its cleanup would delete the
    // chunks being written right now.
    if (saveThread != nullptr && saveThread->isThreadRunning())
    {
        DBG("  Previous take is still being saved, returning false");
        return false;
    }

    // Make sure destination folder exists
    if (!destFolder.isDirectory())
        destFolder.createDirectory();

    // Retrieve current device parameters
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        recordingDeviceName = deviceManager.getAudioDeviceSetup().inputDeviceName;
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

    takeLock = std::make_unique<juce::InterProcessLock>(lockNameFor(chunkFolder));
    if (! takeLock->enter(0))
        DBG("  could not lock " + chunkFolder.getFullPathName() + " — continuing unlocked");

    samplesPerChunk = (juce::int64)(BDG_CHUNK_SECONDS * nativeSampleRate.load());
    chunkIndex = 0;
    deviceMismatch.store(false);

    if (!openNextChunk())
    {
        chunkFolder.deleteRecursively();
        takeLock.reset();
        return false;
    }

    droppedSamples.store(0);
    timerTicks = 0;

    recording.store(true);
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
    auto* rawChunkStream = fileStream.release();

    // One writer thread serves every chunk for the whole session — tearing it
    // down and restarting it per rotation was pure added latency in the gap.
    if (writerThread == nullptr)
    {
        writerThread = std::make_unique<juce::TimeSliceThread>("BDG Audio Writer");
        writerThread->startThread();
    }

    // FIFO of ~5 s at 48 kHz: the periodic flush below does an fsync, and on a
    // slow USB disk that can stall the writer thread for longer than the old
    // 65536-sample (1.4 s) buffer could absorb.
    auto newWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        new SyncingWriter(writer, rawChunkStream), *writerThread, 262144);

    // Rewrite the header and fsync every ~2 s. Without it the chunk being
    // recorded had a zero-length header and unsynced data until it closed:
    // after a crash it needed raw salvage, and after a power cut up to five
    // minutes could simply be missing from disk.
    newWriter->setFlushInterval((int) juce::jmax(1.0, nativeSampleRate.load() * 2.0));

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

AudioEngine::ConcatResult AudioEngine::concatenateChunks(const SaveJob& job)
{
    ConcatResult result;

    juce::File finalFile = job.chunkFolder.getParentDirectory().getChildFile(
        job.chunkFolder.getFileName() + ".wav");
    result.file = finalFile;

    // FileOutputStream opens an existing file positioned at the END, and the
    // WAV writer stamps its header wherever the stream currently is. A partial
    // file left by a killed merge would therefore get the new take appended
    // after it, behind a stale header.
    finalFile.deleteFile();

    auto outStream = std::unique_ptr<juce::FileOutputStream>(
        finalFile.createOutputStream());

    if (outStream == nullptr)
    {
        DBG("  concatenateChunks: failed to create output stream");
        return result;
    }

    auto* finalWriter = wavFormat.createWriterFor(
        outStream.get(),
        job.sampleRate,
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

    auto* rawOut = outStream.release(); // writer owns the stream

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::int64 samplesWritten = 0;
    bool allWritesSucceeded = true;
    int  chunksSkipped = 0;

    for (int i = 1; i <= job.chunkIndex; ++i)
    {
        juce::File chunkFile = job.chunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));

        juce::int64 copied = 0;
        const auto outcome = copyChunk(formatManager, chunkFile, finalWriter, copied);
        samplesWritten += copied;

        if (outcome == ChunkCopy::unreadable)
        {
            // An unreadable chunk used to be skipped, the take reported as
            // saved and the folder deleted — the only copy of those five
            // minutes went with it. Often the cause is transient (antivirus
            // or a sync client holding the file), so fail the merge and keep
            // the chunks: the recovery flow retries on the next launch.
            DBG("  concatenateChunks: chunk " + juce::String(i) + " is unreadable");
            ++chunksSkipped;
            break;
        }

        if (outcome == ChunkCopy::writeFailed)
        {
            DBG("  concatenateChunks: write FAILED at chunk " + juce::String(i));
            allWritesSucceeded = false;
            break;
        }
    }

    // Push the data to disk (header + fsync) before anything is verified and
    // the chunks are deleted. The writer's destructor below only rewrites the
    // header; it does not sync.
    if (allWritesSucceeded && chunksSkipped == 0 && ! flushToDisk(finalWriter, rawOut))
    {
        DBG("  concatenateChunks: final sync FAILED");
        allWritesSucceeded = false;
    }

    // Destroying the writer flushes any buffered data and rewrites the WAV
    // header with the real length — must happen before we verify the file.
    delete finalWriter;

    // Final proof: read the finished file back and check it holds every
    // sample we wrote — by header AND by the bytes actually on disk. A take
    // with no samples at all (started and stopped inside one buffer, or no
    // microphone permission) is an honest empty result, not a save failure —
    // reporting "disk full" for it would be a lie.
    bool verified = false;
    if (allWritesSucceeded && chunksSkipped == 0)
    {
        if (samplesWritten == 0)
            verified = true;
        else if (verifyWavOnDisk(formatManager, finalFile, samplesWritten))
            verified = true;
        else
            DBG("  concatenateChunks: verification FAILED on final file");
    }

    result.ok = verified;

    DBG("  concatenateChunks: final file = " + finalFile.getFullPathName()
        + " ok=" + juce::String(verified ? "yes" : "no")
        + " samples=" + juce::String(samplesWritten)
        + " unreadableChunks=" + juce::String(chunksSkipped));

    return result;
}

bool AudioEngine::finishWriting()
{
    // The timer keeps running — it also drives the VU meter.

    // Reentrancy guard. The auto-stop path leaves a window in which a queued
    // button click can call this a second time; the second pass would run the
    // concatenation against an already-deleted chunk folder and write an empty
    // 44-byte WAV straight over the good recording.
    if (! recording.exchange(false))
        return false;

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
        DBG("  finishWriting: WARNING " + juce::String(droppedSamples.load())
            + " samples were dropped by the write path");

    // Freeze everything the merge needs. From here on the device may reopen,
    // the user may start another take — none of it can reach the job in
    // flight.
    pendingSave.chunkFolder = chunkFolder;
    pendingSave.chunkIndex  = chunkIndex;
    pendingSave.sampleRate  = nativeSampleRate.load();

    return true;
}

juce::File AudioEngine::finaliseSave()
{
    const SaveJob job = pendingSave;
    auto result = concatenateChunks(job);

    // From here on the folder is either gone or a genuine orphan for the
    // recovery flow, so nobody needs to be kept away from it any more.
    // startRecording() refuses to run while this thread is alive, so the
    // member is not touched concurrently.
    struct ReleaseLock { std::unique_ptr<juce::InterProcessLock>& l; ~ReleaseLock() { l.reset(); } };
    const ReleaseLock release { takeLock };

    if (result.ok)
    {
        job.chunkFolder.deleteRecursively();
        return result.file;
    }

    // Concatenation failed — most likely the volume filled up. The chunks are
    // the only intact copy of the audio, so they stay put; findOrphanedRecordings
    // picks them up next launch. Drop the truncated output so it can't be
    // mistaken for a finished recording.
    DBG("  finaliseSave: concat failed, PRESERVING chunks at " + job.chunkFolder.getFullPathName());
    result.file.deleteFile();

    auto folder = job.chunkFolder;
    auto aliveFlag = alive;
    juce::MessageManager::callAsync([this, aliveFlag, folder]()
    {
        if (aliveFlag->load())
            listeners.call(&Listener::recordingSaveFailed, folder);
    });

    return {};
}

juce::File AudioEngine::stopRecording()
{
    if (! finishWriting())
        return {};

    return finaliseSave();
}

void AudioEngine::stopRecordingAsync(StopReason reason)
{
    if (! finishWriting())
        return;

    // Merging the chunks reads and rewrites the entire take — around 2 GB for
    // a two-hour session. Doing that on the message thread froze the app for
    // as long as the copy took, and a user who force-quits a beachballed app
    // mid-merge is exactly how recordings used to disappear.
    saveThread = std::make_unique<SaveThread>(*this, reason);
    saveThread->startThread();
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
    // ---- UI levels (runs whether or not we are recording) ----
    listeners.call(&Listener::audioLevelsChanged, rmsL.load(), rmsR.load());

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
            stopRecordingAsync(StopReason::DiskFull);
            return;
        }
    }

    // ---- Disk space (kept at the original 10 s cadence) ----
    if (++timerTicks < kDiskCheckEveryTicks)
        return;

    timerTicks = 0;

    // Stopping is not free: the merge writes a second, complete copy of the
    // take before the chunks are deleted. Stopping with "2 minutes left" used
    // to leave room for 2 minutes of audio and no room at all for the merge,
    // so the auto-stop could never actually save. Reserve the take's size,
    // and count every further second twice (chunk now, merged copy later).
    const juce::int64 freeBytes = chunkFolder.getBytesFreeOnVolume();
    const double bytesPerSec = nativeSampleRate.load() * 3.0; // 24-bit mono
    const juce::int64 takeSamples = (juce::int64) juce::jmax(0, chunkIndex - 1) * samplesPerChunk
                                    + samplesInChunk.load();
    const juce::int64 mergeReserve = takeSamples * 3 + 16 * 1024 * 1024; // + 16 MB margin
    const juce::int64 usable = juce::jmax((juce::int64) 0, freeBytes - mergeReserve);
    int remainingSec = (bytesPerSec > 0.0) ? (int) ((double) usable / (2.0 * bytesPerSec)) : 9999;
    int remainingMin = remainingSec / 60;

    listeners.call(&Listener::diskSpaceWarning, remainingMin);

    if (remainingMin < 2)
    {
        // Don't stopTimer() here — it also drives the VU meter now. Clearing
        // `recording` in finishWriting() is what stops the disk polling, and
        // the stop itself is idempotent, so a second tick before it lands is
        // harmless.
        auto aliveFlag = alive;
        juce::MessageManager::callAsync([this, aliveFlag]() {
            if (!aliveFlag->load()) return;
            if (recording.load())
                stopRecordingAsync(StopReason::DiskFull);
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
        if (! dir.getChildFile("chunk_001.wav").existsAsFile())
            continue;

        // Already recovered as far as it goes; kept only for safety.
        if (dir.getChildFile(kRecoveredMarker).exists())
            continue;

        // This process's own take (recording or still merging). The lock
        // below cannot tell: it is per process, so we would get it back.
        if (takeLock != nullptr && dir == chunkFolder)
            continue;

        // Another copy of the app is still writing or merging this take.
        // Treating it as an orphan let "Discard" delete a live recording.
        // Caution: on POSIX this is an fcntl lock, owned per PROCESS, and
        // exit() below releases it for the whole process. Safe because this
        // only runs at launch, before any recovery thread holds a lock; do
        // not call it while recoverRecordingAsync is running.
        juce::InterProcessLock probe(lockNameFor(dir));
        if (! probe.enter(0))
        {
            DBG("findOrphanedRecordings: " + dir.getFileName() + " is in use, skipping");
            continue;
        }
        probe.exit();

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

            if (reader != nullptr && reader->sampleRate > 0.0)
            {
                detectedSampleRate = reader->sampleRate;
            }
            else
            {
                // Single-chunk crash: the reader refuses this file, but the
                // `fmt ` chunk is intact. Falling back to a hardcoded 44100
                // here would relabel a 48 kHz take and shift its pitch — the
                // exact corruption the device-change guard exists to prevent.
                reader.reset();
                const double raw = readRawSampleRate(chunkFile);
                if (raw > 0.0)
                    detectedSampleRate = raw;
                else
                    DBG("recoverRecording: could not determine sample rate, assuming "
                        + juce::String(detectedSampleRate));
            }
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

    // A killed merge can leave a partial file with exactly this name. Opening
    // it would append behind a stale header — and since the chunks get deleted
    // on success, that would destroy the take it is meant to rescue.
    finalFile.deleteFile();

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

    auto* rawOut = outStream.release(); // writer owns the stream

    juce::int64 recoveredSamples = 0;
    bool writeFailed = false;
    int  unreadableChunks = 0;

    for (int i = 1; i <= detectedChunkCount; ++i)
    {
        juce::File chunkFile = orphanedChunkFolder.getChildFile(
            juce::String::formatted("chunk_%03d.wav", i));

        // copyChunk reads the raw PCM whenever the header disagrees with the
        // bytes on disk — the signature of the chunk that was open when the
        // app died, whose header says 0 (or the length at the last flush).
        juce::int64 copied = 0;
        const auto outcome = copyChunk(formatManager, chunkFile, finalWriter, copied);
        recoveredSamples += copied;

        if (outcome == ChunkCopy::writeFailed)
        {
            DBG("recoverRecording: write FAILED on chunk " + juce::String(i));
            writeFailed = true;
            break;
        }

        if (outcome == ChunkCopy::unreadable)
        {
            // The user asked for whatever can be saved, so carry on — but the
            // folder is kept below, because this chunk may still be readable
            // by other means.
            DBG("recoverRecording: chunk " + juce::String(i) + " is unreadable, skipping");
            ++unreadableChunks;
        }
    }

    if (! writeFailed && recoveredSamples > 0 && ! flushToDisk(finalWriter, rawOut))
        writeFailed = true;

    delete finalWriter;

    // Same rule as stopRecording(): the chunks are the only intact copy, so
    // they are not deleted until the recovered file is known to be good.
    // `delete finalWriter` above returns void and can fail silently on a full
    // disk, so a successful-looking write is not enough — read it back.
    bool verified = false;
    if (! writeFailed && recoveredSamples > 0)
    {
        verified = verifyWavOnDisk(formatManager, finalFile, recoveredSamples);

        if (! verified)
            DBG("recoverRecording: verification FAILED on the recovered file");
    }

    if (! verified)
    {
        DBG("recoverRecording: FAILED — keeping chunks at "
            + orphanedChunkFolder.getFullPathName());
        finalFile.deleteFile();
        return {};
    }

    if (unreadableChunks > 0)
    {
        // Keep the chunks, but stop offering this folder at every launch.
        orphanedChunkFolder.getChildFile(kRecoveredMarker).create();
        DBG("recoverRecording: " + juce::String(unreadableChunks)
            + " chunk(s) unreadable — folder kept at " + orphanedChunkFolder.getFullPathName());
    }
    else
    {
        orphanedChunkFolder.deleteRecursively();
    }

    DBG("recoverRecording: recovered " + juce::String(recoveredSamples)
        + " samples to " + finalFile.getFullPathName());
    return finalFile;
}

juce::String AudioEngine::lockNameFor(const juce::File& folder)
{
    // InterProcessLock names become file names / mutex names: keep them short
    // and free of path separators.
    return "BDGrec_take_" + juce::String::toHexString(folder.getFullPathName().hashCode64());
}

void AudioEngine::recoverRecordingAsync(const juce::File& orphanedChunkFolder,
                                        std::function<void(const juce::File&)> onDone)
{
    class RecoveryThread : public juce::Thread
    {
    public:
        RecoveryThread(AudioEngine& e, juce::File f, std::function<void(const juce::File&)> cb)
            : juce::Thread("BDG Recovery"), engine(e), folder(std::move(f)), done(std::move(cb)) {}

        void run() override
        {
            // Lock the folder while copying, so a second copy of the app does
            // not offer it for recovery at the same time.
            juce::InterProcessLock lock(AudioEngine::lockNameFor(folder));
            lock.enter(0);

            const auto result = engine.recoverRecording(folder);
            auto aliveFlag = engine.alive;
            auto cb = done;
            juce::MessageManager::callAsync([aliveFlag, cb, result]()
            {
                if (aliveFlag->load() && cb)
                    cb(result);
            });
        }

    private:
        AudioEngine& engine;
        juce::File folder;
        std::function<void(const juce::File&)> done;
    };

    if (recoveryThread != nullptr)
    {
        // The caller chains recoveries one after another, so the previous one
        // has already reported back; this only joins a finished thread.
        recoveryThread->stopThread(-1);
        recoveryThread.reset();
    }

    recoveryThread = std::make_unique<RecoveryThread>(*this, orphanedChunkFolder, std::move(onDone));
    recoveryThread->startThread();
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

            // Envelope followers and IIR states decay into denormal range on
            // silence, where each operation can cost orders of magnitude more.
            juce::ScopedNoDenormals noDenormals;

            // The whole body runs inside this guard. The DSP path allocates
            // several buffers the size of the entire take (the multiband
            // compressor alone needs four), so a long episode on a machine
            // with modest RAM throws std::bad_alloc right here. Uncaught in a
            // thread, that is std::terminate — the app just vanished. The
            // try/catch at the call site could never see it, because that only
            // wraps the code that *starts* this thread.
            try
            {
                runProcessing(emitStep, emitError, emitFinish);
            }
            catch (const std::bad_alloc&)
            {
                emitError("Sem memoria suficiente para processar este arquivo. "
                          "Tente desativar alguns tratamentos.");
            }
            catch (const std::exception& e)
            {
                emitError(juce::String(e.what()));
            }
            catch (...)
            {
                emitError("Erro desconhecido no processamento.");
            }
        }

    private:
        // std::function rather than templates: DspThread is a local class, and
        // local classes cannot have member templates.
        void runProcessing(const std::function<void(const juce::String&)>& emitStep,
                           const std::function<void(const juce::String&)>& emitError,
                           const std::function<void(const juce::File&)>& emitFinish)
        {
            // 1) Read the WAV file
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();

            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(file));

            if (reader == nullptr)
            {
                // No path here: this string ends up in the error telemetry,
                // and a path carries the user's account name.
                emitError("Failed to read file");
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

            // 6b) The compressor (no make-up gain) and the de-esser both pull
            // the level down after the first normalisation, so the file used
            // to come out well below the -16 dB target. Normalising first is
            // still useful — it puts the material where the compressor
            // thresholds expect it — but the target has to be set last, and
            // its limiter replaces their hard clip as the final peak stage.
            // No emitStep: the overlay lists each treatment once, and
            // re-announcing "normalize" relit the first item after the rest.
            if (doNormalize && (doCompressor || doDeEsser))
                Dsp::normalize(buffer, sampleRate);

            if (threadShouldExit()) { emitError("Cancelado"); return; }

            // 7) Save back to file
            DBG("DSP: saving to " + file.getFullPathName());
            emitStep("saving");

            juce::File tempFile = file.getSiblingFile(file.getFileNameWithoutExtension() + "_tmp.wav");

            // A leftover from an interrupted run would be appended to:
            // FileOutputStream opens an existing file at its end.
            tempFile.deleteFile();

            bool writeOk = false;
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
                    tempFile.deleteFile();
                    emitError("Failed to create WAV writer");
                    return;
                }

                auto* rawOut = outStream.release(); // writer owns the stream now
                writeOk = writer->writeFromAudioSampleBuffer(buffer, 0, numSamples)
                          && flushToDisk(writer, rawOut); // synced before the original is replaced
                delete writer;
            }

            // The processed file replaces the original take, so it must be
            // proven complete first. This used to ignore the write result and
            // swap in whatever reached the disk — a full disk mid-save turned
            // the recording into a truncated file reported as "saved".
            bool good = writeOk;
            if (good && bitsPerSample == 24)
            {
                good = verifyWavOnDisk(formatManager, tempFile, totalSamples);
            }
            else if (good)
            {
                std::unique_ptr<juce::AudioFormatReader> check(formatManager.createReaderFor(tempFile));
                good = check != nullptr && check->lengthInSamples == totalSamples;
            }

            if (! good)
            {
                tempFile.deleteFile();
                emitError("Could not write the processed file (disk full?). "
                          "The original recording was kept unchanged.");
                return;
            }

            // replaceFileIn renames over the original in one step; moveFileTo
            // deleted the original first, leaving a window with neither file.
            if (! tempFile.replaceFileIn(file))
            {
                tempFile.deleteFile();
                emitError("Could not replace the original recording. "
                          "It was kept unchanged.");
                return;
            }

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
