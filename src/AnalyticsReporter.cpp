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

AnalyticsReporter::AnalyticsReporter() = default;

AnalyticsReporter::~AnalyticsReporter()
{
    stopTimer();
    flush();
}

void AnalyticsReporter::initialise(juce::ApplicationProperties& props, const juce::String& analyticsUrl)
{
    appProps = &props;
    apiUrl = analyticsUrl;
    apiKey = "brec_REPLACE_WITH_PRODUCTION_KEY";

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
    startTimer(BATCH_INTERVAL_MS);
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
    if (!extra.isVoid())
        evt->setProperty("extra", extra);

    juce::ScopedLock lock(queueLock);
    eventQueue.add(juce::var(evt));
}

void AnalyticsReporter::timerCallback()
{
    if (!isSending.exchange(true))
    {
        juce::Thread::launch([this]() {
            sendBatch();
            isSending = false;
        });
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
        return;
    }

    auto response = stream->readEntireStreamAsString();
    if (!response.contains("\"ok\""))
    {
        juce::ScopedLock lock(queueLock);
        for (auto& evt : batch)
            eventQueue.add(evt);
    }
}

void AnalyticsReporter::flush()
{
    savePendingEvents();
}

void AnalyticsReporter::loadPendingEvents()
{
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
