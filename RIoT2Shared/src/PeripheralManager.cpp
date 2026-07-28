#include <riot2/PeripheralManager.h>

#include <riot2/PeripheralFactory.h>

void PeripheralManager::rebuild(const NodeConfiguration& nodeConfiguration,
                                 std::function<bool(const String&)> isKnownElsewhere) {
    _entries.clear();

    // Views and peripherals share the same deviceConfigurations array,
    // distinguished purely by classFullName - silently skip any entry the
    // caller's isKnownElsewhere() claims (that's ViewManager's job) rather
    // than logging it as an unrecognized peripheral.
    for (const auto& config : nodeConfiguration.deviceConfigurations) {
        std::unique_ptr<IPeripheral> peripheral = PeripheralFactory::instance().create(config.classFullName);
        if (!peripheral) {
            if (!isKnownElsewhere || !isKnownElsewhere(config.classFullName)) {
                Serial.printf(
                    "[PeripheralManager] classFullName=%s (id=%s) not registered as a View or a Peripheral, "
                    "skipping\n",
                    config.classFullName.c_str(), config.id.c_str());
            }
            continue;
        }

        peripheral->setReportCallback(_reportCallback);
        peripheral->begin(config);

        Entry entry;
        entry.config = config;
        entry.peripheral = std::move(peripheral);
        _entries.push_back(std::move(entry));
    }

    Serial.printf("[PeripheralManager] Rebuilt %u peripheral(s)\n", static_cast<unsigned>(_entries.size()));
}

void PeripheralManager::loop() {
    for (auto& entry : _entries) {
        entry.peripheral->loop();
    }
}

bool PeripheralManager::onCommand(const String& commandId, const Command& command) {
    for (auto& entry : _entries) {
        for (const auto& cmdTemplate : entry.config.commandTemplates) {
            if (cmdTemplate.id == commandId) {
                entry.peripheral->onCommand(command);
                return true;
            }
        }
    }
    return false;
}
