#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <streambuf>

// The editor reports what it does with std::cout, which on Windows has nowhere to go (a GUI
// program has no console) and on macOS only to a console nobody opens. This puts a copy of
// everything written to cout and cerr in a file, a time stamp at the start of each line, so
// "check the console for details" has something to point at (issue #115).
//
// One file per run, named after the day, the time and the process, so two editors never write to the
// same one; the ten newest are kept. The console keeps getting its copy as before.
namespace AppLog
{
    inline juce::File folder()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
#if JUCE_MAC
            .getChildFile("Application Support")
#endif
            .getChildFile("AnimatekNME").getChildFile("Logs");
    }

    class Tee : public std::streambuf
    {
    public:
        Tee(std::streambuf* original, juce::FileOutputStream& file, std::mutex& lock)
            : original_(original), file_(file), lock_(lock) {}

        std::streambuf* original() const { return original_; }

    protected:
        int_type overflow(int_type ch) override
        {
            if (traits_type::eq_int_type(ch, traits_type::eof()))
                return traits_type::not_eof(ch);
            std::lock_guard<std::mutex> guard(lock_);
            if (original_ != nullptr)
                original_->sputc(traits_type::to_char_type(ch));
            write(traits_type::to_char_type(ch));
            return ch;
        }

        int sync() override
        {
            std::lock_guard<std::mutex> guard(lock_);
            if (original_ != nullptr)
                original_->pubsync();
            file_.flush();
            return 0;
        }

    private:
        // Past this the file stops growing: a runaway loop must not fill the disk
        static constexpr juce::int64 maxBytes = 20 * 1024 * 1024;

        void write(char c)
        {
            if (file_.getPosition() >= maxBytes)
            {
                if (!full_)
                {
                    full_ = true;
                    file_.writeText("\n[log full: nothing more is written to this file]\n", false, false, nullptr);
                }
                return;
            }
            if (atLineStart_)
            {
                const auto now = juce::Time::getCurrentTime();
                file_.writeText(now.formatted("%H:%M:%S") + juce::String::formatted(".%03d ", now.getMilliseconds()),
                                false, false, nullptr);
                atLineStart_ = false;
            }
            file_.writeByte(c);
            if (c == '\n')
                atLineStart_ = true;
        }

        std::streambuf* original_;
        juce::FileOutputStream& file_;
        std::mutex& lock_;
        bool atLineStart_ = true;
        bool full_ = false;
    };

    struct State
    {
        std::unique_ptr<juce::FileOutputStream> file;
        std::mutex lock;
        std::unique_ptr<Tee> out, err;
        juce::File path;
    };
    inline State& state() { static State s; return s; }

    inline juce::File currentFile() { return state().path; }

    inline void start(const juce::String& header)
    {
        auto& s = state();
        if (s.file != nullptr)
            return;

        const auto dir = folder();
        if (!dir.createDirectory().wasOk())
            return;

        // Keep the ten newest, counting the one about to be made
        auto old = dir.findChildFiles(juce::File::findFiles, false, "nme-*.log");
        std::sort(old.begin(), old.end(), [](const juce::File& a, const juce::File& b)
                  { return a.getFileName() > b.getFileName(); });
        for (int i = 9; i < old.size(); ++i)
            old[i].deleteFile();

        const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
        // A random tail tells apart two editors started in the same second
        s.path = dir.getChildFile("nme-" + stamp + "-"
                                  + juce::String::toHexString(juce::Random::getSystemRandom().nextInt(0xffff))
                                  + ".log");
        auto stream = std::make_unique<juce::FileOutputStream>(s.path);
        if (!stream->openedOk())
        {
            s.path = juce::File();
            return;
        }
        s.file = std::move(stream);
        s.out = std::make_unique<Tee>(std::cout.rdbuf(), *s.file, s.lock);
        s.err = std::make_unique<Tee>(std::cerr.rdbuf(), *s.file, s.lock);
        std::cout.rdbuf(s.out.get());
        std::cerr.rdbuf(s.err.get());

        std::cout << header << "\n"
                  << "System: " << juce::SystemStats::getOperatingSystemName() << ", "
                  << juce::SystemStats::getNumPhysicalCpus() << " cores\n"
                  << "Log: " << s.path.getFullPathName() << std::endl;
    }

    inline void stop()
    {
        auto& s = state();
        if (s.file == nullptr)
            return;
        std::cout.flush();
        std::cout.rdbuf(s.out->original());
        std::cerr.rdbuf(s.err->original());
        s.out.reset();
        s.err.reset();
        s.file.reset();
    }
}
