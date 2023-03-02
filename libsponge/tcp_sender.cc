#include "tcp_sender.hh"

#include "iostream"
#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _retransmission_timer.cache_size(); }

void TCPSender::fill_window() {
    // state: "CLOSED"
    if (next_seqno_absolute() == 0) {
        // stream to begin
        send_segment(_isn, true);
        return;
    }

    // state: "SYN_ACKED" (also)
    if (stream_in().eof()) {
        // stream has reached EOF, but FIN flag hasn't been sent yet,
        // so we should send EOF
        if (next_seqno_absolute() < stream_in().bytes_written() + 2 && bytes_in_flight() < _window_size) {
            // take window_size into account
            send_segment(next_seqno(), false, true);
        }
        return;
    }

    // state: "SYN_ACKED"
    size_t window_size = _window_size >= bytes_in_flight() ? _window_size - bytes_in_flight() : 0;
    size_t payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, min(stream_in().buffer_size(), window_size));
    if (payload_size == 0) {
        return;
    }

    // buffer that is not empty should be sent and stream has EOF
    if (stream_in().input_ended() && window_size >= payload_size + 1) {
        send_segment(next_seqno(), false, true, stream_in().read(payload_size));
        return;
    }

    send_segment(next_seqno(), false, false, stream_in().read(payload_size));

    // window_size still has space
    fill_window();
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    DUMMY_CODE(ackno, window_size);
    _nonzero = window_size != 0;
    _window_size = window_size != 0 ? window_size : 1;
    _retransmission_timer.stop(ackno.raw_value());
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);
    _elapsed_time += ms_since_last_tick;
    _retransmission_timer.tick(ms_since_last_tick, segments_out(), _nonzero);
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _retransmission_timer.consecutive_retransmissions();
}

void TCPSender::send_segment(WrappingInt32 seqno, bool syn, bool fin, std::string &&payload) {
    TCPSegment segment;
    segment.header().seqno = seqno;
    segment.header().syn = syn;
    segment.header().fin = fin;
    if (!payload.empty()) {
        segment.payload() = std::move(payload);
    }
    segments_out().push(segment);
    auto ackno = seqno.raw_value() + segment.length_in_sequence_space();
    _retransmission_timer.start(ackno, segment);
    _next_seqno += segment.length_in_sequence_space();
}

void TCPSender::send_empty_segment() {}

TCPSender::RetransmissionTimer::RetransmissionTimer(const unsigned int retx_timeout)
    : _initial_retransmission_timeout(retx_timeout), _retransmission_timeout(retx_timeout) {}

void TCPSender::RetransmissionTimer::start(const uint32_t ackno, const TCPSegment &segment) {
    if (_segments_out_cache.empty()) {
        _elapsed_time = 0;
    }
    _segments_out_cache[ackno] = segment;
}

void TCPSender::RetransmissionTimer::stop(const uint32_t ackno) {
    // impossible ackno (beyond next seqno) is ignored
    if (!_segments_out_cache.count(ackno)) {
        return;
    }
    std::list<uint32_t> stop_ackno_list{};
    // think that segment is received whose ackno less than ackno received
    for (const auto &_segment_pair : _segments_out_cache) {
        if (_segment_pair.first <= ackno) {
            stop_ackno_list.emplace_back(_segment_pair.first);
        }
    }
    if (stop_ackno_list.empty()) {
        return;
    }
    for (auto stop_ackno : stop_ackno_list) {
        _segments_out_cache.erase(stop_ackno);
    }
    _elapsed_time = 0;
    _retransmission_count = 0;
    _retransmission_timeout = _initial_retransmission_timeout;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::RetransmissionTimer::tick(const size_t ms_since_last_tick,
                                          std::queue<TCPSegment> &segments_out,
                                          const bool nonzero) {
    if (_segments_out_cache.empty()) {
        return;
    }
    _elapsed_time += ms_since_last_tick;
    if (_elapsed_time >= _retransmission_timeout) {
        segments_out.push(_segments_out_cache.begin()->second);
        _elapsed_time = 0;

        //! Unlike a zero-size window, a full window of nonzero size should be respected
        //! When filling window, treat a '0' window size as equal to '1' but don't back off RTO
        _retransmission_timeout = nonzero ? 2 * _retransmission_timeout : _retransmission_timeout;
        _retransmission_count++;
    }
}

uint16_t TCPSender::RetransmissionTimer::cache_size() const {
    uint64_t count{};
    for (const auto &segment_pair : _segments_out_cache) {
        count += segment_pair.second.length_in_sequence_space();
    }
    return count;
}
unsigned int TCPSender::RetransmissionTimer::consecutive_retransmissions() const { return _retransmission_count; }
