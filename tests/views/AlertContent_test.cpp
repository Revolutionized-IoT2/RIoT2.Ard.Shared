#include "view_test.h"

using namespace riot2::views;

int main() {
    TestCommand full("a", R"({"title":"Doorbell","message":"Someone is at the door","subHeader":"tap","soundEnabled":true})");
    AlertContent content = AlertContent::parse(full.command, "Alert", "default");
    CHECK(content.title == "Doorbell" && content.message == "Someone is at the door");
    CHECK(content.subHeader == "tap" && content.soundEnabled);

    // A plain string is the message.
    TestCommand text("a", R"("Hello")");
    content = AlertContent::parse(text.command, "Alert", "default");
    CHECK(content.title == "Alert" && content.message == "Hello" && content.subHeader == "default" &&
          !content.soundEnabled);

    // "text" when "message" is missing; non-string fields fall back.
    TestCommand fallback("a", R"({"text":"From text","title":5,"subHeader":null})");
    content = AlertContent::parse(fallback.command, "Notification", "");
    CHECK(content.title == "Notification" && content.message == "From text" && content.subHeader.length() == 0);
    TestCommand both("a", R"({"message":"From message","text":"From text"})");
    CHECK(AlertContent::parse(both.command, "", "").message == "From message");

    // soundEnabled as a string, any case; anything else is false.
    TestCommand soundText("a", R"({"soundEnabled":"TRUE"})"), soundFalse("a", R"({"soundEnabled":"false"})"),
        soundNumber("a", R"({"soundEnabled":1})");
    CHECK(AlertContent::parse(soundText.command, "", "").soundEnabled);
    CHECK(!AlertContent::parse(soundFalse.command, "", "").soundEnabled);
    CHECK(!AlertContent::parse(soundNumber.command, "", "").soundEnabled);

    TestCommand number("a", "42"), null("a", "null"), array("a", R"(["x"])");
    for (TestCommand* command : {&number, &null, &array}) {
        content = AlertContent::parse(command->command, "Alert", "sub");
        CHECK(content.title == "Alert" && content.message.length() == 0 && content.subHeader == "sub" &&
              !content.soundEnabled);
    }
    std::printf("AlertContent ok\n");
}
