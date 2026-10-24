#include "SessionState.h"

namespace
{
juce::MemoryBlock decode(const juce::String& base64)
{
    juce::MemoryOutputStream out;
    if (base64.isNotEmpty())
        juce::Base64::convertFromBase64(out, base64);
    return out.getMemoryBlock();
}

juce::String encode(const juce::MemoryBlock& block)
{
    return juce::Base64::toBase64(block.getData(), block.getSize());
}
}

juce::MemoryBlock SessionState::toBlock() const
{
    juce::XmlElement root("NMESession");
    root.setAttribute("version", kFormatVersion);
    root.setAttribute("activeSlot", activeSlot);

    for (const auto& s : slots)
    {
        auto* e = root.createNewChildElement("Slot");
        e->setAttribute("index", s.slot);
        e->setAttribute("name", s.name);
        e->createNewChildElement("Pch")->addTextElement(encode(s.pch));
        if (s.var.getSize() > 0)
            e->createNewChildElement("Var")->addTextElement(encode(s.var));
    }

    juce::MemoryBlock block;
    juce::MemoryOutputStream out(block, false);
    out << root.toString(juce::XmlElement::TextFormat().singleLine());
    return block;
}

bool SessionState::fromBlock(const void* data, size_t size, SessionState& out)
{
    if (data == nullptr || size == 0)
        return false;

    const auto root = juce::XmlDocument::parse(
        juce::String::fromUTF8(static_cast<const char*>(data), static_cast<int>(size)));
    if (root == nullptr || !root->hasTagName("NMESession")
        || root->getIntAttribute("version", 0) < 1
        || root->getIntAttribute("version", 0) > kFormatVersion)
        return false;

    SessionState state;
    state.activeSlot = root->getIntAttribute("activeSlot", 0);
    for (const auto* e : root->getChildWithTagNameIterator("Slot"))
    {
        SessionSlot s;
        s.slot = e->getIntAttribute("index", -1);
        s.name = e->getStringAttribute("name");
        if (const auto* pch = e->getChildByName("Pch"))
            s.pch = decode(pch->getAllSubText());
        if (const auto* var = e->getChildByName("Var"))
            s.var = decode(var->getAllSubText());
        if (s.slot >= 0 && s.pch.getSize() > 0)
            state.slots.push_back(std::move(s));
    }

    out = std::move(state);
    return true;
}
