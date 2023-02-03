// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#    include <spdlog/details/log_msg_buffer.h>
#endif

namespace spdlog {
namespace details {

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(const log_msg &orig_msg)
    : log_msg{orig_msg}
{
    store_payload();
    update_views();
}

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(const log_msg_buffer &other)
    : log_msg{other}
{
    store_payload();
    update_views();
}

SPDLOG_INLINE log_msg_buffer::log_msg_buffer(log_msg_buffer &&other) SPDLOG_NOEXCEPT : log_msg{other}, buffer{std::move(other.buffer)}
{
    update_views();
}

SPDLOG_INLINE log_msg_buffer &log_msg_buffer::operator=(const log_msg_buffer &other)
{
    log_msg::operator=(other);
    buffer.clear();
    buffer.append(other.buffer.data(), other.buffer.data() + other.buffer.size());
    attributes_buffer = other.attributes_buffer;
    update_views();
    return *this;
}

SPDLOG_INLINE log_msg_buffer &log_msg_buffer::operator=(log_msg_buffer &&other) SPDLOG_NOEXCEPT
{
    log_msg::operator=(other);
    buffer = std::move(other.buffer);
    attributes_buffer = std::move(other.attributes_buffer);
    update_views();
    return *this;
}

SPDLOG_INLINE void log_msg_buffer::store_payload()
{
    buffer.append(logger_name.begin(), logger_name.end());
    buffer.append(payload.begin(), payload.end());
    attributes_buffer.assign(attributes.begin(), attributes.end());
    for (const auto& attr : attributes)
    {
        buffer.append(attr.name.begin(), attr.name.end());
    }
}

SPDLOG_INLINE void log_msg_buffer::update_views()
{
    logger_name = string_view_t{buffer.data(), logger_name.size()};
    payload = string_view_t{buffer.data() + logger_name.size(), payload.size()};

    auto* attr_buf_ptr = buffer.data() + logger_name.size() + payload.size();
    for (auto& attr : attributes_buffer) {
        attr.name = string_view_t{attr_buf_ptr, attr.name.size()};
        attr_buf_ptr += attr.name.size();
    }
    attributes = attributes_buffer;
}

} // namespace details
} // namespace spdlog
