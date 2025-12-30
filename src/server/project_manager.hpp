
#ifndef PROJECT_MANAGER_HPP
#define PROJECT_MANAGER_HPP

#include <server/types.hpp>

#include <unordered_map>
#include <memory>
#include <string>

namespace gdshader_lsp
{
    
class ProjectManager {
private:

    ProjectManager() {}

    std::string rootPath;
    std::unordered_map<std::string, std::shared_ptr<ShaderUnit>> units;

    std::vector<std::string> includeStack;

    std::string loadSource(const std::string& path);

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
    std::shared_ptr<SymbolTable> getExports(const std::string& uri);
    std::vector<std::string> getAffectedFiles(const std::string& uri);
};

} // namespace gdshader_lsp


#endif // PROJECT_MANAGER_HPP