#include <riot2/DeviceConfigurationJson.h>

#include <ArduinoJson.h>

String riot2::serializeDeviceConfigurations(const std::vector<DeviceConfiguration>& configs) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& config : configs) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = config.id;
        obj["name"] = config.name;
        obj["classFullName"] = config.classFullName;

        JsonArray cmdArr = obj["commandTemplates"].to<JsonArray>();
        for (const auto& cmd : config.commandTemplates) {
            JsonObject cmdObj = cmdArr.add<JsonObject>();
            cmdObj["id"] = cmd.id;
            cmdObj["type"] = cmd.type;
            cmdObj["name"] = cmd.name;
            cmdObj["address"] = cmd.address;
            cmdObj["valueType"] = cmd.valueType;
            cmdObj["model"] = cmd.model;
        }

        JsonArray reportArr = obj["reportTemplates"].to<JsonArray>();
        for (const auto& report : config.reportTemplates) {
            JsonObject reportObj = reportArr.add<JsonObject>();
            reportObj["id"] = report.id;
            reportObj["type"] = report.type;
            reportObj["name"] = report.name;
            reportObj["address"] = report.address;
            JsonObject paramsObj = reportObj["parameters"].to<JsonObject>();
            for (const auto& kv : report.parameters) {
                paramsObj[kv.first] = kv.second;
            }
        }

        JsonObject deviceParamsObj = obj["deviceParameters"].to<JsonObject>();
        for (const auto& kv : config.deviceParameters) {
            deviceParamsObj[kv.first] = kv.second;
        }
    }

    String body;
    serializeJson(doc, body);
    return body;
}
