#include "PatchExtras.h"

// ─── Fingerprint ─────────────────────────────────────────────────────────────

juce::String patchFingerprint(const Patch& patch)
{
    // Canonical text first, hashed after, so the same patch always produces the
    // same string whatever order the containers happen to hold things in.
    juce::StringArray lines;
    lines.add("n:" + patch.getName().trim());

    for (int section = 0; section <= 1; ++section)
    {
        const auto& container = patch.getContainer(section);

        juce::StringArray modules;
        for (const auto& modulePtr : container.getModules())
        {
            const auto* m = modulePtr.get();
            if (m == nullptr)
                continue;
            const auto pos = m->getPosition();
            const int type = m->getDescriptor() != nullptr ? m->getDescriptor()->index : -1;
            modules.add(juce::String(section) + ":" + juce::String(type) + ":"
                        + juce::String(pos.x) + ":" + juce::String(pos.y));
        }
        modules.sort(false);
        lines.addArray(modules);

        // Cables are named by the modules they join and the connectors they use,
        // never by pointer or by index into a vector that a delete would shift.
        juce::StringArray cables;
        for (const auto& connection : container.getConnections())
        {
            auto describe = [&container](const Connector* conn) -> juce::String
            {
                if (conn == nullptr)
                    return "?";
                for (const auto& modulePtr : container.getModules())
                {
                    const auto* m = modulePtr.get();
                    if (m == nullptr)
                        continue;
                    for (const auto& c : m->getConnectors())
                        if (&c == conn)
                        {
                            const auto pos = m->getPosition();
                            const int type = m->getDescriptor() != nullptr
                                                 ? m->getDescriptor()->index : -1;
                            const int idx = c.getDescriptor() != nullptr
                                                ? c.getDescriptor()->index : -1;
                            return juce::String(type) + "@" + juce::String(pos.x) + ","
                                 + juce::String(pos.y) + "." + juce::String(idx);
                        }
                }
                return "?";
            };

            cables.add("c" + juce::String(section) + ":" + describe(connection.output)
                       + ">" + describe(connection.input));
        }
        cables.sort(false);
        lines.addArray(cables);
    }

    // A 64-bit hash of a canonical string. Not a cryptographic digest, and it
    // does not need to be: it answers "is this the same patch", and a collision
    // would have to also match the patch name to do any harm.
    const auto hash = lines.joinIntoString("\n").hashCode64();
    return juce::String::toHexString(hash);
}

// ─── One entry ───────────────────────────────────────────────────────────────

void PatchExtras::rememberFingerprint(const juce::String& fingerprint)
{
    if (fingerprint.isEmpty())
        return;

    fingerprints.removeString(fingerprint);
    fingerprints.insert(0, fingerprint);
    while (fingerprints.size() > maxFingerprints)
        fingerprints.remove(fingerprints.size() - 1);
}

// ─── Text form ───────────────────────────────────────────────────────────────
//
// Deliberately the same shape as the .pch and the .var: plain text, sections in
// square brackets, readable and repairable by hand.

static juce::String escapeLine(const juce::String& text)
{
    return text.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "");
}

static juce::String unescapeLine(const juce::String& text)
{
    juce::String out;
    for (int i = 0; i < text.length(); ++i)
    {
        if (text[i] == '\\' && i + 1 < text.length())
        {
            const auto next = text[i + 1];
            if (next == 'n')  { out += '\n'; ++i; continue; }
            if (next == '\\') { out += '\\'; ++i; continue; }
        }
        out += text[i];
    }
    return out;
}

juce::String extrasToText(const PatchExtras& extras)
{
    juce::String out;
    out << "[NMEExtras]\n";
    out << "Version=1\n";
    out << "Id=" << extras.id << "\n";
    out << "Name=" << escapeLine(extras.name) << "\n";
    out << "LastUsed=" << juce::String(extras.lastUsed) << "\n";

    out << "[Fingerprints]\n";
    for (const auto& fingerprint : extras.fingerprints)
        out << fingerprint << "\n";
    out << "[/Fingerprints]\n";

    out << "[Comments]\n";
    for (const auto& c : extras.comments)
    {
        juce::String size(c.gridHeight());
        if (c.gridWidth() > 1)
            size += "x" + juce::String(c.gridWidth());
        out << c.section << " " << c.x << " " << c.y << " " << size << " "
            << escapeLine(c.text) << "\n";
    }
    out << "[/Comments]\n";

    out << "[Notes]\n";
    out << extras.notes << "\n";
    out << "[/Notes]\n";

    out << "[Variations]\n";
    out << varToText(extras.variations);
    out << "[/Variations]\n";

    return out;
}

bool extrasFromText(PatchExtras& extras, const juce::String& text)
{
    juce::StringArray lines;
    lines.addLines(text);
    if (lines.isEmpty() || lines[0].trim() != "[NMEExtras]")
        return false;

    extras = PatchExtras{};

    enum class Mode { None, Fingerprints, Comments, Notes, Variations };
    Mode mode = Mode::None;
    juce::StringArray notesLines, variationLines;

    for (int i = 1; i < lines.size(); ++i)
    {
        const auto raw = lines[i];
        const auto line = raw.trim();

        if (line == "[Fingerprints]") { mode = Mode::Fingerprints; continue; }
        if (line == "[Comments]")     { mode = Mode::Comments;     continue; }
        if (line == "[Notes]")        { mode = Mode::Notes;        continue; }
        if (line == "[Variations]")   { mode = Mode::Variations;   continue; }
        if (line.startsWith("[/"))    { mode = Mode::None;         continue; }

        if (mode == Mode::None)
        {
            if (line.startsWith("Id="))
                extras.id = line.fromFirstOccurrenceOf("=", false, false).trim();
            else if (line.startsWith("Name="))
                extras.name = unescapeLine(line.fromFirstOccurrenceOf("=", false, false));
            else if (line.startsWith("LastUsed="))
                extras.lastUsed = line.fromFirstOccurrenceOf("=", false, false)
                                      .trim().getLargeIntValue();
            continue;
        }

        if (mode == Mode::Fingerprints)
        {
            if (line.isNotEmpty())
                extras.fingerprints.add(line);
            continue;
        }

        if (mode == Mode::Comments)
        {
            if (line.isEmpty())
                continue;

            // Four fields, then the rest of the line is the text, which may
            // hold spaces of its own. Same layout the .pch uses.
            juce::StringArray fields;
            auto rest = line;
            for (int f = 0; f < 4; ++f)
            {
                const auto space = rest.indexOfChar(' ');
                if (space < 0) { fields.clear(); break; }
                fields.add(rest.substring(0, space));
                rest = rest.substring(space + 1);
            }
            if (fields.size() != 4)
                continue;

            PatchComment c;
            c.section = juce::jlimit(0, 1, fields[0].getIntValue());
            c.x       = juce::jmax(0, fields[1].getIntValue());
            c.y       = juce::jmax(0, fields[2].getIntValue());
            c.height  = juce::jlimit(1, 128, fields[3].getIntValue());
            c.width   = fields[3].contains("x")
                            ? juce::jlimit(1, 40, fields[3].fromFirstOccurrenceOf("x", false, false)
                                                           .getIntValue())
                            : 1;
            c.text = unescapeLine(rest);
            extras.comments.push_back(c);
            continue;
        }

        if (mode == Mode::Notes)
            notesLines.add(raw);
        else if (mode == Mode::Variations)
            variationLines.add(raw);
    }

    extras.notes = notesLines.joinIntoString("\n").trimEnd();
    if (!variationLines.isEmpty())
        varFromText(extras.variations, variationLines);

    return extras.id.isNotEmpty();
}

// ─── The store ───────────────────────────────────────────────────────────────

juce::String PatchExtrasStore::newId()
{
    return juce::Uuid().toDashedString();
}

juce::File PatchExtrasStore::fileFor(const juce::String& id) const
{
    return folder.getChildFile(id + ".nmx");
}

void PatchExtrasStore::setFolder(const juce::File& newFolder)
{
    folder = newFolder;
    entries.clear();

    if (!folder.isDirectory())
        return;

    for (const auto& file : folder.findChildFiles(juce::File::findFiles, false, "*.nmx"))
    {
        PatchExtras extras;
        if (extrasFromText(extras, file.loadFileAsString()))
            entries.push_back(std::move(extras));
    }
}

PatchExtras* PatchExtrasStore::findById(const juce::String& id)
{
    if (id.isEmpty())
        return nullptr;

    for (auto& e : entries)
        if (e.id == id)
            return &e;
    return nullptr;
}

PatchExtras* PatchExtrasStore::findByFingerprint(const juce::String& fingerprint)
{
    if (fingerprint.isEmpty())
        return nullptr;

    // Several entries can share a fingerprint: two patches built the same way
    // and named the same are indistinguishable from here, so the one used most
    // recently wins, which is the one the user is most likely to mean.
    PatchExtras* best = nullptr;
    for (auto& e : entries)
        if (e.fingerprints.contains(fingerprint))
            if (best == nullptr || e.lastUsed > best->lastUsed)
                best = &e;

    return best;
}

PatchExtras& PatchExtrasStore::obtain(const juce::String& id)
{
    if (auto* existing = findById(id))
        return *existing;

    PatchExtras fresh;
    fresh.id = id.isNotEmpty() ? id : newId();
    entries.push_back(std::move(fresh));
    return entries.back();
}

void PatchExtrasStore::write(const PatchExtras& extras)
{
    if (extras.id.isEmpty() || folder == juce::File())
        return;

    // Keep the in-memory copy in step, so a lookup right after a write finds
    // what was written rather than what was there before.
    if (auto* mine = findById(extras.id))
    {
        if (mine != &extras)
            *mine = extras;
    }
    else
    {
        entries.push_back(extras);
    }

    const auto file = fileFor(extras.id);

    if (extras.isEmpty())
    {
        // Nothing left worth keeping. Leaving the file behind would leave its
        // fingerprints matching future patches for no reason.
        file.deleteFile();
        return;
    }

    folder.createDirectory();
    file.replaceWithText(extrasToText(extras));
}
