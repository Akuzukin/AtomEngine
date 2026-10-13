#include "EditorLocalization.h"
#include "External/tinyxml2/tinyxml2.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include <map>
#include <regex>
#include <optional>
#include "imgui.h"

namespace AtomEngine::EditorLocalization
{
namespace
{
struct Catalog
{
    std::string name;
    std::map<std::string, std::string> text;
    std::map<std::string, std::string> labels;
};
std::map<std::string, Catalog> catalogs;
std::string current = "en";
std::optional<std::vector<std::string>> Formats(const std::string &text)
{
    static const std::regex format(
        "%(%|[-+ #0]*([0-9]+|\\*)?(\\.([0-9]+|\\*))?(hh|ll|h|l|j|z|t|L)?[diuoxXfFeEgGaAcsp])");
    std::vector<std::string> result;
    size_t cursor = 0;
    while ((cursor = text.find('%', cursor)) != std::string::npos)
    {
        std::match_results<std::string::const_iterator> match;
        if (!std::regex_search(text.begin() + cursor, text.end(), match, format,
                               std::regex_constants::match_continuous))
            return std::nullopt;
        result.push_back(match.str());
        cursor += match.length();
    }
    return result;
}
} // namespace
void Initialize(const std::filesystem::path &directory)
{
    catalogs.clear();
    catalogs["en"].name = "English";
    current = "en";
    std::error_code error;
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
        if (it->is_regular_file(error) && it->path().extension() == ".xml")
            RegisterCatalog(it->path());
    SetLanguage("zh-CN");
}
bool RegisterCatalog(const std::filesystem::path &path)
{
    tinyxml2::XMLDocument document;
    if (document.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS)
        return false;
    auto root = document.FirstChildElement("Language");
    const char *id = root ? root->Attribute("id") : nullptr;
    const char *name = root ? root->Attribute("name") : nullptr;
    if (!id || !*id || !name || !*name)
        return false;
    Catalog catalog;
    catalog.name = name;
    for (auto entry = root->FirstChildElement("Text"); entry; entry = entry->NextSiblingElement("Text"))
    {
        auto key = entry->Attribute("key"), value = entry->GetText();
        if (!key || !*key || !value || !*value)
            continue;
        // Translated printf text must preserve argument types/order and ImGui's ID delimiters are reserved.
        auto sourceFormats = Formats(key), translatedFormats = Formats(value);
        if (!sourceFormats || !translatedFormats || sourceFormats != translatedFormats ||
            std::string(value).find("##") != std::string::npos)
        {
            WriteLog(LogLevel::Warning, "Localization", std::string("Invalid translation: ") + key);
            continue;
        }
        catalog.text.emplace(key, value);
    }
    // Registration is intended for initialization; cached label pointers remain valid during UI submission.
    catalogs.insert_or_assign(id, std::move(catalog));
    return true;
}
bool SetLanguage(const std::string &id)
{
    if (!catalogs.contains(id))
        return false;
    current = id;
    return true;
}
const std::string &CurrentLanguage()
{
    return current;
}
std::vector<Language> Languages()
{
    std::vector<Language> result;
    for (const auto &[id, catalog] : catalogs)
        result.push_back({id, catalog.name});
    return result;
}
const char *Text(const char *key)
{
    if (!key)
        return "";
    auto catalog = catalogs.find(current);
    if (catalog != catalogs.end())
        if (auto entry = catalog->second.text.find(key); entry != catalog->second.text.end())
            return entry->second.c_str();
    return key;
}
const char *Label(const char *key)
{
    if (!key || !*key || std::string_view(key).starts_with("##"))
        return key ? key : "";
    auto &labels = catalogs[current].labels;
    auto [entry, inserted] = labels.try_emplace(key);
    if (inserted)
    {
        std::string visible = key;
        if (auto delimiter = visible.find("##"); delimiter != std::string::npos)
            visible.resize(delimiter);
        auto translation = Text(key);
        entry->second = std::string(translation == key ? Text(visible.c_str()) : translation) + "###" + key;
    }
    return entry->second.c_str();
}
bool Combo(const char *key, int *selected, const char *englishItems)
{
    std::vector<const char *> items;
    for (auto item = englishItems; *item; item += std::char_traits<char>::length(item) + 1)
        items.push_back(Text(item));
    return ImGui::Combo(Label(key), selected, items.data(), int(items.size()));
}
} // namespace AtomEngine::EditorLocalization
