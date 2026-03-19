#include "AnalyticsReporter.h"
#include <juce_gui_basics/juce_gui_basics.h>

static const char* funnyWords[] = {
    "abacaxi", "capivara", "jacare", "pinguim", "tucano",
    "girafa", "golfinho", "papagaio", "tartaruga", "borboleta",
    "canguru", "flamingo", "hipopotamo", "lagarto", "macaco",
    "ornitorrinco", "pantera", "quati", "rinoceronte", "sardinha",
    "tamandua", "urubu", "veado", "zebra", "arara",
    "besouro", "cachorro", "dragao", "elefante", "formiga",
    "gato", "hamster", "iguana", "jabuti", "koala",
    "leao", "morcego", "narvalo", "ovelha", "piranha",
    "raposa", "sapo", "tigre", "unicornio", "vaca",
    "xexeu", "yak", "zagaia", "aranha", "baleia"
};

static constexpr int NUM_FUNNY_WORDS = sizeof(funnyWords) / sizeof(funnyWords[0]);

AnalyticsReporter::AnalyticsReporter() : Thread("AnalyticsReporter") {}

AnalyticsReporter::~AnalyticsReporter()
{
    stopThread(5000);
    flush();
}

void AnalyticsReporter::initialise(juce::ApplicationProperties& props, const juce::String& analyticsUrl)
{
    appProps = &props;
    apiUrl = analyticsUrl;
    // Obfuscated API key (XOR 0x5A) — not encryption, just prevents plain-text grep
    static const unsigned char k[] = {
        0x38,0x28,0x3f,0x39,0x05,0x68,0x6b,0x6e,0x3e,0x3b,
        0x3c,0x3e,0x62,0x6e,0x38,0x6f,0x6e,0x6e,0x38,0x6a,
        0x3e,0x62,0x62,0x63,0x6b,0x6d,0x6f,0x6f,0x6e,0x6a,
        0x3f,0x6c,0x39,0x63,0x68,0x6b,0x6a
    };
    for (auto c : k) apiKey += juce::String::charToString(static_cast<juce::juce_wchar>(c ^ 0x5A));

    if (auto* pf = props.getUserSettings())
    {
        machineId = pf->getValue("machineId", "");
        if (machineId.isEmpty())
        {
            machineId = generateMachineId();
            pf->setValue("machineId", machineId);
            pf->saveIfNeeded();
        }
    }

    loadPendingEvents();
    startThread(juce::Thread::Priority::low);
}

juce::String AnalyticsReporter::generateMachineId()
{
    juce::Random rng;
    int wordIndex = rng.nextInt(NUM_FUNNY_WORDS);
    auto uuid = juce::Uuid().toString().removeCharacters("-").substring(0, 8);
    return juce::String(funnyWords[wordIndex]) + "-" + uuid;
}

void AnalyticsReporter::setContext(const juce::String& os,
                                    const juce::String& version,
                                    const juce::String& hardware,
                                    const juce::String& locale)
{
    osName = os;
    appVersion = version;
    hardwareName = hardware;
    localeName = locale;
}

void AnalyticsReporter::trackEvent(const juce::String& eventType, const juce::var& extra)
{
    auto evt = new juce::DynamicObject();
    evt->setProperty("event", eventType);
    evt->setProperty("machine_id", machineId);
    evt->setProperty("os", osName);
    evt->setProperty("app_version", appVersion);
    evt->setProperty("hardware", hardwareName);
    evt->setProperty("locale", localeName);
    evt->setProperty("timestamp", juce::Time::currentTimeMillis());
    if (!extra.isVoid())
        evt->setProperty("extra", extra);

    juce::ScopedLock lock(queueLock);
    eventQueue.add(juce::var(evt));

    // Cap queue size to prevent unbounded growth when offline
    while (eventQueue.size() > MAX_QUEUE_SIZE)
        eventQueue.remove(0);
}

void AnalyticsReporter::run()
{
    while (!threadShouldExit())
    {
        wait(currentIntervalMs);
        if (threadShouldExit()) break;
        sendBatch();
    }
}

void AnalyticsReporter::sendBatch()
{
    juce::Array<juce::var> batch;
    {
        juce::ScopedLock lock(queueLock);
        if (eventQueue.isEmpty()) return;
        batch = eventQueue;
        eventQueue.clear();
    }

    auto batchObj = new juce::DynamicObject();
    juce::Array<juce::var> batchArray;
    for (auto& evt : batch)
        batchArray.add(evt);
    batchObj->setProperty("batch", batchArray);

    auto jsonBody = juce::JSON::toString(juce::var(batchObj));

    auto url = juce::URL(apiUrl).withPOSTData(jsonBody);

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withConnectionTimeoutMs(10000)
                       .withExtraHeaders("Content-Type: application/json\r\nX-API-Key: " + apiKey);

    auto stream = url.createInputStream(options);

    if (stream == nullptr)
    {
        juce::ScopedLock lock(queueLock);
        for (auto& evt : batch)
            eventQueue.add(evt);
        consecutiveFailures++;
        currentIntervalMs = juce::jmin(BATCH_INTERVAL_MS * (1 << consecutiveFailures), MAX_BATCH_INTERVAL_MS);
        return;
    }

    auto response = stream->readEntireStreamAsString();
    auto responseJson = juce::JSON::parse(response);
    if (!responseJson.getProperty("ok", false))
    {
        juce::ScopedLock lock(queueLock);
        for (auto& evt : batch)
            eventQueue.add(evt);
        consecutiveFailures++;
        currentIntervalMs = juce::jmin(BATCH_INTERVAL_MS * (1 << consecutiveFailures), MAX_BATCH_INTERVAL_MS);
    }
    else
    {
        consecutiveFailures = 0;
        currentIntervalMs = BATCH_INTERVAL_MS;
    }
}

void AnalyticsReporter::flush()
{
    savePendingEvents();
}

void AnalyticsReporter::loadPendingEvents()
{
    if (appProps == nullptr) return;
    if (auto* pf = appProps->getUserSettings())
    {
        auto pending = pf->getValue("pendingAnalytics", "");
        if (pending.isNotEmpty())
        {
            auto parsed = juce::JSON::parse(pending);
            if (parsed.isArray())
            {
                juce::ScopedLock lock(queueLock);
                for (int i = 0; i < parsed.size(); ++i)
                    eventQueue.add(parsed[i]);
            }
            pf->setValue("pendingAnalytics", "");
            pf->saveIfNeeded();
        }
    }
}

void AnalyticsReporter::savePendingEvents()
{
    if (appProps == nullptr) return;
    juce::ScopedLock lock(queueLock);
    if (eventQueue.isEmpty()) return;

    if (auto* pf = appProps->getUserSettings())
    {
        juce::Array<juce::var> arr;
        for (auto& evt : eventQueue)
            arr.add(evt);
        pf->setValue("pendingAnalytics", juce::JSON::toString(arr));
        pf->saveIfNeeded();
    }
}
