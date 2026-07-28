#pragma once

#include <Arduino.h>

// Mirrors RIoT2.Core.Models.Report: an outbound riot2/{id}/report message for
// one of a view's reportTemplates[].id. `value` must already be a valid JSON
// literal (e.g. "true", "42", "\"Warm\"") so it can be embedded directly into
// the report envelope's "value" field without re-quoting.
struct Report {
    String id;
    String value;
};
