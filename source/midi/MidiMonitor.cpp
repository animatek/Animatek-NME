#include "MidiMonitor.h"

MidiMonitor& MidiMonitor::instance()
{
    static MidiMonitor m;
    return m;
}

void MidiMonitor::record(Direction dir, const std::uint8_t* data, std::size_t len)
{
    if (!enabled_.load(std::memory_order_relaxed) || data == nullptr || len == 0)
        return;

    Entry e;
    e.timeMs = juce::Time::getMillisecondCounterHiRes();
    e.dir    = dir;
    e.bytes.assign(data, data + len);

    std::lock_guard<std::mutex> lock(mutex_);
    e.seq = nextSeq_++;
    entries_.push_back(std::move(e));
    while (entries_.size() > kMaxEntries)
        entries_.pop_front();
}

void MidiMonitor::fetchSince(std::uint64_t& cursor, std::vector<Entry>& out) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& e : entries_)
        if (e.seq >= cursor)
            out.push_back(e);
    cursor = nextSeq_;
}

void MidiMonitor::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    // Keep nextSeq_ monotonic so any open viewer's cursor stays valid.
}
