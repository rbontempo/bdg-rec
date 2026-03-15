# Update Checker Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add weekly update checking that queries GitHub releases and shows a modal dialog when a new version is available.

**Architecture:** New `UpdateChecker` class (Thread + URL) queries GitHub API in background. `MainComponent` owns it, receives callback, shows `AlertWindow`. Timestamp persisted via existing `ApplicationProperties`.

**Tech Stack:** JUCE 8 (`juce::Thread`, `juce::URL`, `juce::JSON`, `juce::AlertWindow`, `juce::ApplicationProperties`)

---

## Chunk 1: Implementation

### Task 1: Add bilingual strings

**Files:**
- Modify: `src/Strings.h`

- [ ] **Step 1: Add update strings to StringTable struct**

In `src/Strings.h`, add four new fields at the end of `struct StringTable`:

```cpp
juce::String updateAvailableTitle, updateAvailableBody, updateDownload, updateIgnore;
```

- [ ] **Step 2: Add Portuguese values**

In the `pt` table, after the last entry (`"Erro desconhecido no processamento."`), add:

```cpp
// Update checker
juce::CharPointer_UTF8 ("Atualiza\xc3\xa7\xc3\xa3" "o dispon\xc3\xad" "vel"),
juce::CharPointer_UTF8 ("Nova vers\xc3\xa3" "o %s dispon\xc3\xad" "vel.\nVers\xc3\xa3" "o atual: %s."),
"Baixar",
"Ignorar",
```

- [ ] **Step 3: Add English values**

In the `en` table, after `"Unknown processing error."`, add:

```cpp
// Update checker
"Update available",
"New version %s available.\nCurrent version: %s.",
"Download",
"Ignore",
```

- [ ] **Step 4: Build to verify compilation**

Run: `cd build && cmake --build . --target BDG_REC 2>&1 | tail -5`
Expected: build succeeds

- [ ] **Step 5: Commit**

```bash
git add src/Strings.h
git commit -m "feat: add bilingual strings for update checker"
```

### Task 2: Create UpdateChecker class

**Files:**
- Create: `src/UpdateChecker.h`
- Create: `src/UpdateChecker.cpp`

- [ ] **Step 1: Write UpdateChecker.h**

```cpp
#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <functional>

class UpdateChecker : private juce::Thread
{
public:
    UpdateChecker();
    ~UpdateChecker() override;

    /// Call from MainComponent constructor.
    /// If 7+ days since last check, queries GitHub in background.
    /// onUpdateAvailable is called on the message thread with the new version string.
    void checkIfDue(juce::ApplicationProperties& props,
                    std::function<void(juce::String newVersion)> onUpdateAvailable);

private:
    void run() override;

    std::function<void(juce::String)> callback;
    juce::ApplicationProperties* appProps = nullptr;

    static constexpr int CHECK_INTERVAL_SECONDS = 7 * 24 * 60 * 60; // 7 days

    static bool isNewerVersion(const juce::String& remote, const juce::String& local);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};
```

- [ ] **Step 2: Write UpdateChecker.cpp**

```cpp
#include "UpdateChecker.h"
#include <juce_gui_basics/juce_gui_basics.h>

UpdateChecker::UpdateChecker() : Thread("UpdateChecker") {}

UpdateChecker::~UpdateChecker()
{
    stopThread(3000);
}

void UpdateChecker::checkIfDue(juce::ApplicationProperties& props,
                                std::function<void(juce::String)> onUpdateAvailable)
{
    appProps = &props;
    callback = std::move(onUpdateAvailable);

    if (auto* pf = props.getUserSettings())
    {
        auto last = pf->getIntValue("lastUpdateCheck", 0);
        auto now  = (int)juce::Time::currentTimeMillis() / 1000;

        if (now - last < CHECK_INTERVAL_SECONDS)
            return; // too soon
    }

    startThread(juce::Thread::Priority::low);
}

void UpdateChecker::run()
{
    auto url = juce::URL("https://api.github.com/repos/rbontempo/bdg-rec-juce/releases/latest");
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(5000)
                       .withExtraHeaders("Accept: application/vnd.github.v3+json");

    auto stream = url.createInputStream(options);
    if (stream == nullptr)
        return; // no internet or error — fail silently

    auto response = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse(response);

    auto tagName = json.getProperty("tag_name", "").toString();
    if (tagName.isEmpty())
        return;

    // Strip leading 'v' if present
    if (tagName.startsWithChar('v') || tagName.startsWithChar('V'))
        tagName = tagName.substring(1);

    auto currentVersion = juce::String(JUCE_APPLICATION_VERSION_STRING);

    // Save timestamp regardless of result
    if (auto* pf = appProps->getUserSettings())
    {
        pf->setValue("lastUpdateCheck", (int)(juce::Time::currentTimeMillis() / 1000));
        pf->saveIfNeeded();
    }

    if (isNewerVersion(tagName, currentVersion) && callback)
    {
        auto ver = tagName;
        juce::MessageManager::callAsync([this, ver]() {
            if (callback)
                callback(ver);
        });
    }
}

bool UpdateChecker::isNewerVersion(const juce::String& remote, const juce::String& local)
{
    auto remParts = juce::StringArray::fromTokens(remote, ".", "");
    auto locParts = juce::StringArray::fromTokens(local,  ".", "");

    for (int i = 0; i < juce::jmax(remParts.size(), locParts.size()); ++i)
    {
        int r = (i < remParts.size()) ? remParts[i].getIntValue() : 0;
        int l = (i < locParts.size()) ? locParts[i].getIntValue() : 0;
        if (r > l) return true;
        if (r < l) return false;
    }
    return false; // equal
}
```

- [ ] **Step 3: Build to verify compilation**

Run: First add to CMakeLists.txt (see Task 3), then build.

### Task 3: Register in CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add source files**

In `CMakeLists.txt`, inside the `target_sources(BDG_REC PRIVATE ...)` block, add after `src/DspOverlay.cpp`:

```cmake
    src/UpdateChecker.cpp
```

- [ ] **Step 2: Build**

Run:
```bash
cd "/Users/rbontempo/Documents/AI - antigravity/bdg-rec/build"
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target BDG_REC 2>&1 | tail -5
```
Expected: build succeeds

- [ ] **Step 3: Commit**

```bash
git add src/UpdateChecker.h src/UpdateChecker.cpp CMakeLists.txt
git commit -m "feat: add UpdateChecker class — queries GitHub for new releases"
```

### Task 4: Integrate into MainComponent

**Files:**
- Modify: `src/MainComponent.h`
- Modify: `src/MainComponent.cpp`

- [ ] **Step 1: Add include and member to MainComponent.h**

Add include at the top:
```cpp
#include "UpdateChecker.h"
```

Add member in the private section, after `DspOverlay dspOverlay;`:
```cpp
UpdateChecker updateChecker;
```

Add method declaration in the private section:
```cpp
void showUpdateDialog(const juce::String& newVersion);
```

- [ ] **Step 2: Add checkIfDue call to MainComponent constructor**

In `MainComponent.cpp`, at the end of the constructor (after `loadSettings();`), add:

```cpp
    // Update checker — weekly GitHub release check
    updateChecker.checkIfDue(appProperties, [this](juce::String newVersion) {
        showUpdateDialog(newVersion);
    });
```

- [ ] **Step 3: Implement showUpdateDialog**

Add at the end of `MainComponent.cpp`:

```cpp
void MainComponent::showUpdateDialog(const juce::String& newVersion)
{
    auto& s = Strings::get();
    auto currentVersion = juce::String(JUCE_APPLICATION_VERSION_STRING);
    auto body = s.updateAvailableBody
                    .replace("%s", newVersion, false)  // first %s = new version
                    .replace("%s", currentVersion, false); // second %s = current

    auto options = juce::MessageBoxOptions()
                       .withTitle(s.updateAvailableTitle)
                       .withMessage(body)
                       .withButton(s.updateDownload)
                       .withButton(s.updateIgnore)
                       .withIconType(juce::MessageBoxIconType::InfoIcon);

    juce::AlertWindow::showAsync(options, [](int result) {
        if (result == 1) // "Download" button
            juce::URL("https://rec.bdg.fm").launchInDefaultBrowser();
    });
}
```

- [ ] **Step 4: Build and test**

Run:
```bash
cd "/Users/rbontempo/Documents/AI - antigravity/bdg-rec/build"
cmake --build . --target BDG_REC 2>&1 | tail -5
```
Expected: build succeeds

- [ ] **Step 5: Commit**

```bash
git add src/MainComponent.h src/MainComponent.cpp
git commit -m "feat: integrate update checker — weekly dialog on new release"
```

### Task 5: Test manually and push

- [ ] **Step 1: Run the app**

```bash
open "/Users/rbontempo/Documents/AI - antigravity/bdg-rec/build/BDG_REC_artefacts/Release/BDG rec.app"
```

Verify: app opens normally. If a newer release exists on GitHub (v > 1.0.0), dialog should appear. If not, no dialog (expected — version is current).

- [ ] **Step 2: Force-test the dialog (optional)**

Temporarily change `getApplicationVersion()` in `Main.cpp` to return `"0.0.1"` and rebuild. The dialog should appear showing the newer version. Revert after testing.

- [ ] **Step 3: Push**

```bash
git push
```
