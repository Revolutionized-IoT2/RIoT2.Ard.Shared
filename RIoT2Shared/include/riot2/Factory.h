#pragma once

#include <Arduino.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <riot2/DeviceConfiguration.h>

namespace riot2 {

// Generic registry mapping a DeviceConfiguration's classFullName to a
// factory function that creates a T. Shared by every classFullName-keyed
// registry in the codebase (concrete Views, concrete IPeripheral drivers) -
// see PeripheralFactory.h (shared) and each project's own ViewFactory.h
// (`using ViewFactory = riot2::Factory<IView>;`), since IView itself differs
// per project (M5Dial's has render(M5Canvas&); Core2's doesn't).
template <typename T>
class Factory {
public:
    using Creator = std::function<std::unique_ptr<T>()>;
    // Returns an example DeviceConfiguration illustrating this
    // classFullName's expected commandTemplates/reportTemplates/
    // deviceParameters shape, for the /api/device/configuration/templates
    // endpoint (see ConfigTemplateServer.h) - the on-device equivalent of
    // RIoT2.Core.Interfaces.IDeviceWithConfiguration.GetConfigurationTemplate().
    using TemplateProvider = std::function<DeviceConfiguration()>;

    static Factory<T>& instance() {
        static Factory<T> factory;
        return factory;
    }

    // templateProvider is optional: entries with none simply contribute no
    // template to configurationTemplates() (e.g. a classFullName not meant
    // to be user-configurable via that endpoint).
    void registerCreator(const String& classFullName, Creator creator, TemplateProvider templateProvider = nullptr) {
        _creators.push_back({classFullName, std::move(creator)});
        if (templateProvider) {
            _templateProviders.push_back(std::move(templateProvider));
        }
    }

    // True if a T is registered for classFullName - lets other systems check
    // ownership without constructing an instance just to test for one.
    bool isRegistered(const String& classFullName) const {
        for (const auto& entry : _creators) {
            if (entry.first == classFullName) {
                return true;
            }
        }
        return false;
    }

    // Returns nullptr if no T is registered for classFullName.
    std::unique_ptr<T> create(const String& classFullName) const {
        for (const auto& entry : _creators) {
            if (entry.first == classFullName) {
                return entry.second();
            }
        }
        return nullptr;
    }

    // One DeviceConfiguration template per registered classFullName that
    // supplied a templateProvider, in registration order.
    std::vector<DeviceConfiguration> configurationTemplates() const {
        std::vector<DeviceConfiguration> templates;
        templates.reserve(_templateProviders.size());
        for (const auto& provider : _templateProviders) {
            templates.push_back(provider());
        }
        return templates;
    }

private:
    std::vector<std::pair<String, Creator>> _creators;
    std::vector<TemplateProvider> _templateProviders;
};

}  // namespace riot2
