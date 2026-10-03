#include "OutputPaths.h"

#include <system_error>

namespace outputpaths {

std::string FolderRelativePath(const std::string& type) {
    if (type == "Holder") return "output/holder";
    if (type == "Tool")   return "output/tools";
    return {};
}

std::string STLRelativePath(const std::string& type, const std::string& id) {
    std::string folder = FolderRelativePath(type);
    if (folder.empty()) return {};
    return folder + "/" + id + ".stl";
}

std::string PNGRelativePath(const std::string& type, const std::string& id) {
    std::string folder = FolderRelativePath(type);
    if (folder.empty()) return {};
    return folder + "/" + id + ".png";
}

std::filesystem::path STLAbsolutePath(const std::filesystem::path& root,
                                      const std::string& type,
                                      const std::string& id)
{
    std::string rel = STLRelativePath(type, id);
    if (rel.empty()) return {};
    return root / std::filesystem::path(rel);
}

std::filesystem::path PNGAbsolutePath(const std::filesystem::path& root,
                                      const std::string& type,
                                      const std::string& id)
{
    std::string rel = PNGRelativePath(type, id);
    if (rel.empty()) return {};
    return root / std::filesystem::path(rel);
}

bool IsValidType(const std::string& type) {
    return type == "Holder" || type == "Tool";
}

bool IDExistsOnDisk(const std::filesystem::path& root,
                    const std::string& type,
                    const std::string& id)
{
    std::string folder = FolderRelativePath(type);
    if (folder.empty() || id.empty()) return false;

    std::error_code ec;
    std::filesystem::path dir = root / std::filesystem::path(folder);

    bool stlExists = std::filesystem::exists(dir / (id + ".stl"), ec);
    bool pngExists = std::filesystem::exists(dir / (id + ".png"), ec);
    return stlExists || pngExists;
}

} // namespace outputpaths
