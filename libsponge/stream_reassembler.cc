#include "stream_reassembler.hh"

#include "iostream"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    DUMMY_CODE(data, index, eof);

    if (eof) {
        _eof_index = index + data.length();
        _has_ended = true;
    }

    auto first_unread = _output.bytes_read();
    auto first_unacceptable = first_unread + _capacity;
    // bytes that exceed the capacity are discarded
    size_t discard_len;
    if (index + data.length() > first_unacceptable) {
        discard_len = index + data.length() - first_unacceptable;
    } else {
        discard_len = 0;
    }

    auto first_unassembled = _output.bytes_written();
    // bytes that overlap are discarded
    size_t discard_prefix;
    if (first_unassembled > index) {
        discard_prefix = first_unassembled - index;
    } else {
        discard_prefix = 0;
    }

    for (size_t i = discard_prefix; i < data.length() - discard_len; i++) {
        // bytes that overlap are discarded
        if (!_buffer.count(index + i)) {
            _buffer[index + i] = data[i];
        }
    }
    string assemble_str{};
    for (size_t i = first_unassembled; i < first_unacceptable; i++) {
        if (_buffer.count(i)) {
            assemble_str += _buffer[i];
        } else {
            break;
        }
    }

    if (assemble_str.length() != 0) {
        auto write_len = _output.write(assemble_str);
        for (size_t i = first_unassembled; i < first_unassembled + write_len; i++) {
            _buffer.erase(i);
        }
    }

    if (_has_ended && _output.bytes_written() == _eof_index) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _buffer.size(); }

bool StreamReassembler::empty() const { return _buffer.empty(); }
