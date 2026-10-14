#include "view_test.h"

using namespace riot2::views;

int main() {
    DeviceConfiguration config;
    config.reportTemplates = {makeReport("A", "a", 0), makeReport("B", "b", 0), makeReport("C", "", 0)};
    config.commandTemplates = {makeCommand("B", "b", 0), makeCommand("A", "a", 0), makeCommand("C", "", 0)};

    auto slots = pairByAddress(config, 3);
    CHECK(slots.size() == 3);
    CHECK(slots[0].hasCommand && slots[0].command.id == config.commandTemplates[1].id);
    CHECK(slots[1].hasCommand && slots[1].command.id == config.commandTemplates[0].id);
    CHECK(!slots[2].hasCommand);  // no address, no pairing
    CHECK(pairByAddress(config, 2).size() == 2);
    CHECK(findSlotByCommand(slots, config.commandTemplates[0].id) == 1);
    CHECK(findSlotByCommand(slots, "missing") == -1);

    TestCommand yes("c", "true"), no("c", "false"), one("c", "1"), zero("c", "0"), number("c", "21.5"),
        text("c", "\"warm\""), object("c", "{\"on\":true}");
    CHECK(commandBool(yes.command) && !commandBool(no.command));
    CHECK(commandBool(one.command) && !commandBool(zero.command));
    CHECK(commandInt(number.command) == 21);
    CHECK(commandText(text.command) == "warm");
    CHECK(commandText(number.command) == "21.5");
    CHECK(commandText(object.command) == "{\"on\":true}");
    CHECK(commandText(yes.command) == "true");

    CHECK(boolLiteral(true) == "true" && boolLiteral(false) == "false");
    CHECK(intLiteral(-42) == "-42");
    CHECK(stringLiteral("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"");

    config.deviceParameters = {{"n", "7"}, {"empty", ""}};
    CHECK(intParameter(config, "n", 1) == 7);
    CHECK(intParameter(config, "missing", 3) == 3);
    CHECK(intParameter(config, "empty", 3) == 0);  // present: parsed, as String::toInt does

    auto made = makeTemplate("Board.View", "View");
    CHECK(made.classFullName == "Board.View" && made.name == "View" && made.id.length() > 0);
    std::printf("ViewSupport ok\n");
}
