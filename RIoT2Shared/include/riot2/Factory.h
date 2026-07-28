#pragma once

#include <Arduino.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

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

    static Factory<T>& instance() {
        static Factory<T> factory;
        return factory;
    }

    void registerCreator(const String& classFullName, Creator creator) {
        _creators.push_back({classFullName, std::move(creator)});
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

private:
    std::vector<std::pair<String, Creator>> _creators;
};

}  // namespace riot2
