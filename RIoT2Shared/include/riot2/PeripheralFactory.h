#pragma once

#include <riot2/Factory.h>
#include <riot2/IPeripheral.h>

// Registry mapping a peripheral configuration's classFullName to a factory
// function that creates the matching IPeripheral implementation - see
// riot2::Factory for the shared implementation. Unlike ViewFactory (which
// stays project-local since IView differs per project), IPeripheral is the
// same type everywhere, so this alias can live in the shared library.
using PeripheralFactory = riot2::Factory<IPeripheral>;
