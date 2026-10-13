#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace AtomEngine::EditorLocalization
{
struct Language
{
    std::string id;
    std::string name;
};
// UTF-8 XML catalogs are discovered at initialization. Unknown/missing keys fall back to English.
void Initialize(const std::filesystem::path &directory = "Asset/Localization");
bool RegisterCatalog(const std::filesystem::path &path);
bool SetLanguage(const std::string &id);
const std::string &CurrentLanguage();
std::vector<Language> Languages();
const char *Text(const char *key);
// Visible translation + ###original key keeps widget/docking IDs stable across languages.
const char *Label(const char *key);
bool Combo(const char *key, int *selected, const char *englishItems);
} // namespace AtomEngine::EditorLocalization
