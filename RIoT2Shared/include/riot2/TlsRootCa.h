#pragma once

// Root CA (PEM, X.509) used to validate the orchestrator's HTTPS certificate
// and, when NodeConfig::mqttUseTls is set, the MQTT broker's TLS
// certificate. Overridable at build time via
// -D'RIOT2_ROOT_CA_PEM="-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n"'
// so each deployment can pin its own CA without editing source. Returns an
// empty string when no CA has been configured - callers should fall back to
// an insecure (WiFiClientSecure::setInsecure()) connection and log a clear
// warning rather than silently trusting nothing, since no real CA exists
// for this deployment yet.
namespace riot2 {

const char* rootCaPem();

}  // namespace riot2
