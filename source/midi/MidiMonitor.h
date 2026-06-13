#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

/**
 * Process-wide ring buffer of MIDI SysEx traffic for the SysEx Monitor floater.
 *
 * It lives outside MidiDeviceManager (which is recreated on every connect) so
 * the log survives reconnects. record() is a single atomic check when disabled,
 * so leaving the hooks in the hot TX/RX paths costs nothing until the user opens
 * the monitor and enables it. Raw bytes are stored; hex formatting happens in the
 * UI thread, never on the MIDI thread.
 */
class MidiMonitor
{
public:
    enum class Direction { Tx, Rx };

    struct Entry
    {
        std::uint64_t      seq = 0;
        double             timeMs = 0.0;   // getMillisecondCounterHiRes()
        Direction          dir = Direction::Tx;
        std::vector<std::uint8_t> bytes;
    };

    static MidiMonitor& instance();

    void setEnabled(bool e) { enabled_.store(e, std::memory_order_relaxed); }
    bool isEnabled() const  { return enabled_.load(std::memory_order_relaxed); }

    // Called from MIDI/message threads. No-op (one atomic load) when disabled.
    void record(Direction dir, const std::uint8_t* data, std::size_t len);

    // UI thread: append entries newer than `cursor` to `out`, advancing cursor.
    void fetchSince(std::uint64_t& cursor, std::vector<Entry>& out) const;

    void clear();

private:
    MidiMonitor() = default;

    static constexpr std::size_t kMaxEntries = 4000;

    std::atomic<bool>     enabled_ { false };
    mutable std::mutex    mutex_;
    std::deque<Entry>     entries_;
    std::uint64_t         nextSeq_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMonitor)
};
