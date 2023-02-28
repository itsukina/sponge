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
    switch (_state) {
        case CLOSED: {
            if (next_seqno_absolute() == 0) {
                // stream to begin
                send_segment(_isn, true);
            }
            if (next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight()) {
                _state = SYN_SENT;
            }
        } break;
        case SYN_SENT: {
            if (next_seqno_absolute() > bytes_in_flight() && !stream_in().eof()) {
                _state = SYN_ACKED;
            }
        } break;
        case SYN_ACKED: {
            if (stream_in().eof()) {
                if (next_seqno_absolute() < stream_in().bytes_written() + 2) {
                    // stream has reached EOF, but FIN flag hasn't been sent yet,
                    // so we should send EOF
                    send_segment(next_seqno(), false, true);
                } else if (next_seqno_absolute() == stream_in().bytes_written() + 2 && bytes_in_flight() > 0) {
                    // FIN sent but not fully acknowledged
                    _state = FIN_SENT;
                }
            } else {
                // stream has not reached EOF
                size_t len = min(stream_in().buffer_size(), static_cast<size_t>(_window_size));
                if (len != 0) {
                    // buffer that is not empty should be sent
                    send_segment(next_seqno(), false, false, stream_in().read(len));
                }
            }
        } break;
        case FIN_SENT: {
            if (stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2 &&
                bytes_in_flight() == 0) {
                // bytes_in_flight() ==0 , state change to FIN_ACKED
                _state = FIN_ACKED;
            }
        } break;
        case FIN_ACKED:
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    DUMMY_CODE(ackno, window_size);
    _window_size = min(TCPConfig::MAX_PAYLOAD_SIZE, max(static_cast<size_t>(window_size), static_cast<size_t>(1)));
    _retransmission_timer.stop(ackno.raw_value());
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);
    _elapsed_time += ms_since_last_tick;
    _retransmission_timer.expire(ms_since_last_tick, segments_out());
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
    _retransmission_timer.start(ackno, _initial_retransmission_timeout, segment);
    _next_seqno += segment.length_in_sequence_space();
}

void TCPSender::send_empty_segment() {}

void TCPSender::RetransmissionTimer::start(const uint32_t ackno,
                                           const uint16_t retx_timeout,
                                           const TCPSegment &segment) {
    _timer_map[ackno] = {retx_timeout, 0};
    _segments_out_cache[ackno] = segment;
}

void TCPSender::RetransmissionTimer::stop(const uint32_t ackno) {
    _timer_map.erase(ackno);
    _segments_out_cache.erase(ackno);
    _retransmission_count = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::RetransmissionTimer::expire(const size_t ms_since_last_tick, std::queue<TCPSegment> &segments_out) {
    std::list<uint32_t> _ackno_expire_list;
    for (auto &timer_pair : _timer_map) {
        timer_pair.second._elapsed_time += ms_since_last_tick;
        if (timer_pair.second._elapsed_time >= timer_pair.second._retransmission_timeout) {
            _ackno_expire_list.emplace_back(timer_pair.first);
        }
    }

    if (!_ackno_expire_list.empty()) {
        _ackno_expire_list.sort();
        for (auto ackno : _ackno_expire_list) {
            segments_out.push(_segments_out_cache[ackno]);
            // _timer_map[ackno]._resend_count++;
            _retransmission_count++;
            _timer_map[ackno] = {_timer_map[ackno]._retransmission_timeout * 2, 0};
        }
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
