#include "includes.h"
#include "test_sink.h"

namespace spdlog {
namespace sinks {

template<typename Mutex>
class buffered_msg_sink : public base_sink<Mutex>
{
public:
    std::vector<details::log_msg> messages()
    {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        return messages_;
    }

private:
    static constexpr size_t kMaxMessages = 1000;

protected:
    void sink_it_(const details::log_msg &msg) override
    {
        if (messages_.size() < kMaxMessages) {
            messages_.push_back(details::log_msg_buffer{msg});
        }
    }

    void flush_() override {}

    std::vector<details::log_msg> messages_;
};

using buffered_msg_sink_mt = buffered_msg_sink<std::mutex>;
using buffered_msg_sink_st = buffered_msg_sink<details::null_mutex>;

} // namespace sinks
} // namespace spdlog

enum class Foo {
    Bar,
    Baz
};

template<>
struct fmt::formatter<Foo> : fmt::formatter<std::string>
{
    template<typename FormatContext>
    auto format(Foo foo, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "Foo::{}", foo == Foo::Bar ? "Bar" : "Baz");
    }
};

std::string log(spdlog::string_view_t msg, spdlog::attribute_list attrs, spdlog::level::level_enum logger_level = spdlog::level::info)
{

    std::ostringstream oss;
    auto oss_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);

    spdlog::logger oss_logger("oss", oss_sink);
    oss_logger.set_level(logger_level);
    oss_logger.set_pattern("%v");
    oss_logger.info(msg, attrs);

    return oss.str().substr(0, oss.str().length() - strlen(spdlog::details::os::default_eol));
}

TEST_CASE("basic logging with attributes", "[attributes]")
{
    REQUIRE(log("message", {}) == "message");
    REQUIRE(log("message", {{"str_attr", "value"}, {"int_attr", 42}}) == "message $ str_attr=value int_attr=42");
    REQUIRE(log("message", {{"custom_attr", Foo::Baz}}) == "message $ custom_attr=Foo::Baz");

    std::string msg("message");
    std::string name("name");
    std::string value("value");
    REQUIRE(log(msg, {{name, value}}) == "message $ name=value");
}

TEST_CASE("message attributes via sink", "[attributes]")
{
    using spdlog::sinks::buffered_msg_sink_mt;
    auto sink = std::make_shared<buffered_msg_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("attributes", sink);

    logger->info("message", {{"str_field", "value"}, {"int_field", 42}});
    auto messages = sink->messages();
    REQUIRE(messages.size() == 1);
    const auto& attributes = messages[0].attributes;
    REQUIRE(attributes.size() == 2);
    REQUIRE(attributes[0].name == "str_field");
    REQUIRE(attributes[0].value == "value");
    REQUIRE(attributes[1].name == "int_field");
    REQUIRE(attributes[1].value == "42");
}

TEST_CASE("attribute value formatting", "[attributes]")
{
    REQUIRE(spdlog::attribute("name", 0).value == "0");
    REQUIRE(spdlog::attribute("name", 0.0).value == "0");
    REQUIRE(spdlog::attribute("name", true).value == "true");
    REQUIRE(spdlog::attribute("name", "").value == "");
    REQUIRE(spdlog::attribute("name", "value").value == "value");
    REQUIRE(spdlog::attribute("name", Foo::Bar).value == "Foo::Bar");
}

TEST_CASE("log msg buffering", "[attributes]")
{
    std::unique_ptr<spdlog::details::log_msg_buffer> msg_buffer;
    {
        spdlog::details::log_msg msg(spdlog::source_loc{}, "name", spdlog::level::info, "msg", {{"str_field", "value"}, {"int_field", 42}});
        msg_buffer = std::make_unique<spdlog::details::log_msg_buffer>(msg);
    }
    REQUIRE(msg_buffer->attributes.size() == 2);
    REQUIRE(msg_buffer->attributes[0].name == "str_field");
    REQUIRE(msg_buffer->attributes[0].value == "value");
    REQUIRE(msg_buffer->attributes[1].name == "int_field");
    REQUIRE(msg_buffer->attributes[1].value == "42");
}

TEST_CASE("async logging with attributes", "[attributes]")
{
    auto test_sink = std::make_shared<spdlog::sinks::test_sink_mt>();
    size_t overrun_counter = 0;
    size_t queue_size = 128;
    size_t messages = 256;
    {
        auto tp = std::make_shared<spdlog::details::thread_pool>(queue_size, 1);
        auto logger = std::make_shared<spdlog::async_logger>("as", test_sink, tp, spdlog::async_overflow_policy::block);
        for (size_t i = 0; i < messages; i++)
        {
            std::string msg("message");
            std::string name("name");
            std::string value("value");
            logger->info(msg, {{name, value}});
        }
        logger->flush();
        overrun_counter = tp->overrun_counter();
    }
    REQUIRE(test_sink->msg_counter() == messages);
    REQUIRE(test_sink->flush_counter() == 1);
    REQUIRE(overrun_counter == 0);
}
