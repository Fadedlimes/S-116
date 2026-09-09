#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
#endif

#include <vector>
#include <memory>
#include <utility>
#include <algorithm>

// =============================================================================
// S-116 Hardware-Style Voice Expansion Card System (.s116card)
// Inspired by vintage Roland SR-JV80 / JD-990 & Korg PCM ROM expansion cards.
// Packages complete soundbanks (Part A, Part B, P-Locks, Mod Matrix) in one file.
// Supports real-time cartridge burning and live soundbank authoring.
// =============================================================================

struct CardPreset
{
    juce::String name;
    juce::String xmlData;
};

struct ExpansionCard
{
    juce::String cardId       = "EXP-00";
    juce::String cardName     = "UNTITLED EXPANSION";
    juce::String author       = "UNKNOWN";
    juce::String version      = "1.0";
    juce::String lcdBanner1   = "EXP CARD MOUNTED";
    juce::String lcdBanner2   = "VOICES READY";
    juce::File   sourceFile;

    std::vector<CardPreset> presets;

    bool isValid() const noexcept
    {
        return cardId.isNotEmpty() && sourceFile.existsAsFile();
    }

    int getNumPresets() const noexcept
    {
        return static_cast<int>(presets.size());
    }
};

class ExpansionCardManager
{
public:
    static constexpr const char* CARD_FILE_EXTENSION = ".s116card";

    ExpansionCardManager() = default;
    ~ExpansionCardManager() = default;

    // =========================================================================
    // Directory Management (Auto-Discovery in Documents/Fadedlimes/S-116/Expansions)
    // =========================================================================
    static juce::File getExpansionsDirectory()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Fadedlimes")
            .getChildFile("S-116")
            .getChildFile("Expansions");

        if (!dir.exists())
            dir.createDirectory();

        return dir;
    }

    void scanExpansionsFolder()
    {
        availableCards.clear();

        const auto dir = getExpansionsDirectory();
        if (!dir.isDirectory())
            return;

        auto files = dir.findChildFiles(juce::File::findFiles, false, "*" + juce::String(CARD_FILE_EXTENSION));
        for (const auto& file : files)
        {
            ExpansionCard card;
            if (parseCardXml(file, card))
                availableCards.push_back(std::move(card));
        }

        // Keep active mounted card synchronized if still present
        if (mountedCardIndex >= 0)
        {
            if (mountedCardIndex >= static_cast<int>(availableCards.size()))
                mountedCardIndex = -1;
        }
    }

    const std::vector<ExpansionCard>& getAvailableCards() const noexcept
    {
        return availableCards;
    }

    // =========================================================================
    // Cartridge Slot Mounting & Ejection
    // =========================================================================
    bool mountCard(int cardIndex)
    {
        if (cardIndex < 0 || cardIndex >= static_cast<int>(availableCards.size()))
            return false;

        mountedCardIndex = cardIndex;
        return true;
    }

    bool mountCardFromFile(const juce::File& file)
    {
        ExpansionCard card;
        if (!parseCardXml(file, card))
            return false;

        for (size_t i = 0; i < availableCards.size(); ++i)
        {
            if (availableCards[i].sourceFile == file || availableCards[i].cardId == card.cardId)
            {
                availableCards[i] = std::move(card);
                mountedCardIndex = static_cast<int>(i);
                return true;
            }
        }

        availableCards.push_back(std::move(card));
        mountedCardIndex = static_cast<int>(availableCards.size() - 1);
        return true;
    }

    void ejectCard() noexcept
    {
        mountedCardIndex = -1;
    }

    bool isCardMounted() const noexcept
    {
        return mountedCardIndex >= 0 && mountedCardIndex < static_cast<int>(availableCards.size());
    }

    int getMountedCardIndex() const noexcept
    {
        return mountedCardIndex;
    }

    const ExpansionCard* getActiveCard() const noexcept
    {
        if (isCardMounted())
            return &availableCards[static_cast<size_t>(mountedCardIndex)];
        return nullptr;
    }

    ExpansionCard* getActiveCardNonConst() noexcept
    {
        if (isCardMounted())
            return &availableCards[static_cast<size_t>(mountedCardIndex)];
        return nullptr;
    }

    // =========================================================================
    // Preset Queries from Mounted Card
    // =========================================================================
    juce::StringArray getActiveCardPresetNames() const
    {
        juce::StringArray names;
        if (auto* card = getActiveCard())
        {
            names.ensureStorageAllocated(static_cast<int>(card->presets.size()));
            for (const auto& p : card->presets)
                names.add(p.name);
        }
        return names;
    }

    juce::String getActiveCardPresetXml(int presetIndex) const
    {
        if (auto* card = getActiveCard())
        {
            if (presetIndex >= 0 && presetIndex < static_cast<int>(card->presets.size()))
                return card->presets[static_cast<size_t>(presetIndex)].xmlData;
        }
        return {};
    }

    // =========================================================================
    // Live Virtual Cartridge Burning (Workflow A)
    // =========================================================================

    /**
     * Formats and creates a new blank .s116card cartridge file on disk and mounts it immediately.
     */
    bool createNewBlankCard(const juce::String& cardId,
                            const juce::String& cardName,
                            const juce::String& author,
                            const juce::String& initialPatchXml = {},
                            const juce::String& version = "1.0")
    {
        const juce::String cleanId     = cardId.trim().isEmpty() ? "EXP-01" : cardId.trim().toUpperCase();
        const juce::String cleanName   = cardName.trim().isEmpty() ? "CUSTOM EXPANSION" : cardName.trim();
        const juce::String cleanAuthor = author.trim().isEmpty() ? "UNKNOWN" : author.trim();

        const juce::String fileName = cleanId + " - " + cleanName + CARD_FILE_EXTENSION;
        const juce::File destFile   = getExpansionsDirectory().getChildFile(fileName);

        ExpansionCard card;
        card.cardId     = cleanId;
        card.cardName   = cleanName;
        card.author     = cleanAuthor;
        card.version    = version.trim().isEmpty() ? "1.0" : version.trim();
        card.lcdBanner1 = cleanId + ": " + cleanName.toUpperCase();
        card.lcdBanner2 = "FORMATTED & READY";
        card.sourceFile = destFile;

        // Populate with initial baseline patch
        CardPreset initPreset;
        initPreset.name = "01 INIT " + cleanId;
        initPreset.xmlData = initialPatchXml.isNotEmpty()
                           ? initialPatchXml
                           : "<S-116Patch patchName=\"01 INIT " + cleanId + "\"/>";
        card.presets.push_back(std::move(initPreset));

        if (!writeCardToDisk(card))
            return false;

        scanExpansionsFolder();

        // Automatically mount newly created card
        for (size_t i = 0; i < availableCards.size(); ++i)
        {
            if (availableCards[i].sourceFile == destFile || availableCards[i].cardId == cleanId)
            {
                mountedCardIndex = static_cast<int>(i);
                return true;
            }
        }

        return false;
    }

    /**
     * Adds or updates the current patch directly inside the mounted cartridge file on disk.
     */
    bool addOrUpdatePresetInActiveCard(const juce::String& patchName, const juce::String& patchXml)
    {
        if (!isCardMounted())
            return false;

        auto& card = availableCards[static_cast<size_t>(mountedCardIndex)];
        const juce::String cleanName = patchName.trim().isEmpty() ? "UNTITLED PATCH" : patchName.trim();

        bool updatedExisting = false;
        for (auto& p : card.presets)
        {
            if (p.name.equalsIgnoreCase(cleanName))
            {
                p.xmlData = patchXml;
                updatedExisting = true;
                break;
            }
        }

        if (!updatedExisting)
        {
            CardPreset cp;
            cp.name    = cleanName;
            cp.xmlData = patchXml;
            card.presets.push_back(std::move(cp));
        }

        return writeCardToDisk(card);
    }

    /**
     * Deletes a preset from the mounted card and writes changes to disk.
     */
    bool deletePresetFromActiveCard(int presetIndex)
    {
        if (!isCardMounted())
            return false;

        auto& card = availableCards[static_cast<size_t>(mountedCardIndex)];
        if (presetIndex < 0 || presetIndex >= static_cast<int>(card.presets.size()))
            return false;

        if (card.presets.size() <= 1)
        {
            // Reset to clean INIT rather than leaving card completely empty
            card.presets[0].name = "01 INIT PATCH";
            card.presets[0].xmlData = "<S-116Patch patchName=\"01 INIT PATCH\"/>";
        }
        else
        {
            card.presets.erase(card.presets.begin() + presetIndex);
        }

        return writeCardToDisk(card);
    }

    // =========================================================================
    // Card Packaging & Serialization
    // =========================================================================
    static bool writeCardToDisk(const ExpansionCard& card)
    {
        if (card.sourceFile == juce::File())
            return false;

        auto root = std::make_unique<juce::XmlElement>("S116ExpansionCard");
        root->setAttribute("cardId", card.cardId.trim().toUpperCase());
        root->setAttribute("cardName", card.cardName.trim());
        root->setAttribute("author", card.author.trim());
        root->setAttribute("version", card.version.trim());
        root->setAttribute("presetCount", static_cast<int>(card.presets.size()));

        auto* bannerXml = root->createNewChildElement("LcdBanner");
        bannerXml->setAttribute("line1", card.lcdBanner1.trim().toUpperCase());
        bannerXml->setAttribute("line2", card.lcdBanner2.trim().toUpperCase());

        for (size_t i = 0; i < card.presets.size(); ++i)
        {
            auto* patchXml = root->createNewChildElement("Patch");
            patchXml->setAttribute("index", static_cast<int>(i));
            patchXml->setAttribute("name", card.presets[i].name);

            if (auto innerDoc = juce::XmlDocument::parse(card.presets[i].xmlData))
                patchXml->addChildElement(innerDoc.release());
            else
                patchXml->addTextElement(card.presets[i].xmlData);
        }

        return root->writeTo(card.sourceFile, juce::XmlElement::TextFormat());
    }

    static bool exportCardToFile(const juce::File& destinationFile,
                                 const juce::String& cardId,
                                 const juce::String& cardName,
                                 const juce::String& author,
                                 const juce::String& version,
                                 const juce::String& banner1,
                                 const juce::String& banner2,
                                 const std::vector<std::pair<juce::String, juce::String>>& patches)
    {
        if (patches.empty())
            return false;

        ExpansionCard card;
        card.cardId     = cardId;
        card.cardName   = cardName;
        card.author     = author;
        card.version    = version;
        card.lcdBanner1 = banner1;
        card.lcdBanner2 = banner2;
        card.sourceFile = destinationFile;

        card.presets.reserve(patches.size());
        for (const auto& p : patches)
            card.presets.push_back({ p.first, p.second });

        return writeCardToDisk(card);
    }

private:
    static bool parseCardXml(const juce::File& file, ExpansionCard& outCard)
    {
        if (!file.existsAsFile())
            return false;

        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr || !xml->hasTagName("S116ExpansionCard"))
            return false;

        outCard.cardId     = xml->getStringAttribute("cardId", "EXP-??").toUpperCase();
        outCard.cardName   = xml->getStringAttribute("cardName", file.getFileNameWithoutExtension());
        outCard.author     = xml->getStringAttribute("author", "UNKNOWN");
        outCard.version    = xml->getStringAttribute("version", "1.0");
        outCard.sourceFile = file;
        outCard.presets.clear();

        if (auto* banner = xml->getChildByName("LcdBanner"))
        {
            outCard.lcdBanner1 = banner->getStringAttribute("line1", "EXP CARD INSERTED").toUpperCase();
            outCard.lcdBanner2 = banner->getStringAttribute("line2", outCard.cardName).toUpperCase();
        }
        else
        {
            outCard.lcdBanner1 = "CARD: " + outCard.cardId;
            outCard.lcdBanner2 = outCard.cardName.toUpperCase();
        }

        for (auto* patchElement : xml->getChildIterator())
        {
            if (patchElement->hasTagName("Patch"))
            {
                CardPreset cp;
                cp.name = patchElement->getStringAttribute("name", "Untitled Patch");

                if (auto* innerPatch = patchElement->getFirstChildElement())
                    cp.xmlData = innerPatch->toString();
                else
                    cp.xmlData = patchElement->getAllSubText().trim();

                if (cp.xmlData.isNotEmpty())
                    outCard.presets.push_back(std::move(cp));
            }
        }

        return outCard.isValid();
    }

    std::vector<ExpansionCard> availableCards;
    int mountedCardIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExpansionCardManager)
};