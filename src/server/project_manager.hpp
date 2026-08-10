
#ifndef PROJECT_MANAGER_HPP
#define PROJECT_MANAGER_HPP

#include <server/types.hpp>

#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>

namespace gdshader_lsp
{
    
class ProjectManager {
private:

    ProjectManager() {}

    // Guards the `units` map, which is accessed from both the message thread
    // and the background compiler thread.
    mutable std::mutex unitsMutex;

    std::string rootPath;
    std::unordered_map<std::string, std::shared_ptr<ShaderUnit>> units; // guarded by unitsMutex

    std::vector<std::string> includeStack; // guarded by unitsMutex

    // Internal helpers. The caller must hold unitsMutex.
    std::string loadSourceLocked(const std::string& path);
    std::shared_ptr<ShaderUnit> getUnitLocked(const std::string& path);
    void updateFileLocked(const std::string& uri, const std::string& code);
    std::shared_ptr<SymbolTable> getExportsLocked(const std::string& path);
    std::vector<std::string> getDependentFilesLocked(const std::string& origin_path);

public:

    ProjectManager(ProjectManager const&) = delete;
    ProjectManager& operator=(ProjectManager const&) = delete;

    static std::shared_ptr<ProjectManager> get_singleton()
    {
        static std::shared_ptr<ProjectManager> s{new ProjectManager};
        return s;
    }

    void updateFile(const std::string& uri, const std::string& code);

    void setRootPath(const std::string& path) { rootPath = path; }
    std::string resolvePath(const std::string& currentPath, const std::string& includePath);

    std::shared_ptr<ShaderUnit> getUnit(const std::string& uri);
    std::unordered_map<std::string, std::shared_ptr<ShaderUnit>> getUnitsSnapshot() const;

    std::shared_ptr<SymbolTable> getExports(const std::string& uri);
    std::vector<std::string> getDependentFiles(const std::string& uri);
};

} // namespace gdshader_lsp


#endif // PROJECT_MANAGER_HPP