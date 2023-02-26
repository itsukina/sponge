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

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t count{};
    for (const auto &segment_kv : _segments_out_cache) {
        count += segment_kv.second.length_in_sequence_space();
    }
    return count;
}

void TCPSender::fill_window() {
    switch (_state) {
        case CLOSED: {
            if (next_seqno_absolute() == 0) {
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
            size_t len = min(_stream.buffer_size(), static_cast<size_t>(_window_size));
            if (len != 0) {
                send_segment(next_seqno(), false, false, _stream.read(len));
            }
        } break;
        case FIN_SENT:
            break;
        case FIN_ACKED:
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    DUMMY_CODE(ackno, window_size);
    _window_size = min(TCPConfig::MAX_PAYLOAD_SIZE, max(static_cast<size_t>(window_size), static_cast<size_t>(1)));
    _retransmission_timer.stop(ackno);
    _segments_out_cache.erase(ackno.raw_value());
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);
    _elapsed_time += ms_since_last_tick;
    _retransmission_timer.expire(ms_since_last_tick);
}

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_segment(WrappingInt32 seqno, bool syn, bool fin, std::string &&payload) {
    TCPSegment segment;
    segment.header().seqno = seqno;
    segment.header().syn = syn;
    segment.header().fin = fin;
    if (!payload.empty()) {
        segment.payload() = std::move(payload);
    }
    _segments_out.push(segment);
    std::cout << "===================>" << segment.payload().copy() << std::endl;
    _segments_out_cache[seqno.raw_value() + segment.length_in_sequence_space()] = segment;
    _next_seqno += segment.length_in_sequence_space();
}

void TCPSender::send_empty_segment() {}

void TCPSender::RetransmissionTimer::start(const WrappingInt32 seqno, const uint16_t retx_timeout) {
    _timer_map[seqno.raw_value()] = {retx_timeout, 0};
}

void TCPSender::RetransmissionTimer::stop(const WrappingInt32 seqno) { _timer_map.erase(seqno.raw_value()); }

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
std::list<WrappingInt32> TCPSender::RetransmissionTimer::expire(const size_t ms_since_last_tick) {
    std::list<WrappingInt32> _expire_ackno_list;
    for (auto timer_kv : _timer_map) {
        timer_kv.second._elapsed_time += ms_since_last_tick;
        if (timer_kv.second._elapsed_time >= timer_kv.second._initial_retransmission_timeout) {
            _expire_ackno_list.emplace_back(timer_kv.first);
        }
    }
    //    if (!_expire_ackno_list.empty()) {
    //        sort(_expire_ackno_list.begin(), _expire_ackno_list.end(), [](WrappingInt32 a, WrappingInt32 b) {
    //            return a.raw_value() <= b.raw_value();
    //        });
    //    }
    return _expire_ackno_list;
}
