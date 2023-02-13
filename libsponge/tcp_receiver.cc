#include "tcp_receiver.hh"

#include "iostream"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);
    switch (_state) {
        case LISTEN: {
            if (!seg.header().syn) {
                return;
            }
            _state = SYN_RECV;
            _isn = seg.header().seqno;
            _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
            if (stream_out().input_ended()) {
                _state = FIN_RECV;
            }
        } break;
        case SYN_RECV: {
            // a byte with invalid stream index should be ignored
            if (seg.header().seqno <= _isn) {
                return;
            }
            auto index = unwrap(seg.header().seqno - 1, _isn, _reassembler.first_unassembled());
            _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
            if (stream_out().input_ended()) {
                _state = FIN_RECV;
            }
        } break;
        case FIN_RECV: {
        } break;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    switch (_state) {
        case LISTEN:
            return std::nullopt;
        case SYN_RECV:
            return wrap(_reassembler.first_unassembled(), _isn) + 1;
        case FIN_RECV:
            return wrap(_reassembler.first_unassembled(), _isn) + 2;
        default:
            return std::nullopt;
    }
}

size_t TCPReceiver::window_size() const { return _reassembler.first_unacceptable() - _reassembler.first_unassembled(); }
