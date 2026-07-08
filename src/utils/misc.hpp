#include <string>
#include <vector>
#include <numeric>
#include <algorithm>

namespace gdshader_lsp
{
    /**
     * @brief Calculates the minimum number of single-character edits required to change s1 into s2
     * 
     * @param s1 
     * @param s2 
     * @return int 
     */
    static int levenshteinDistance(const std::string& s1, const std::string& s2) 
    {
        const size_t m = s1.size();
        const size_t n = s2.size();
        if (m == 0) return n;
        if (n == 0) return m;

        std::vector<size_t> costs(n + 1);
        std::iota(costs.begin(), costs.end(), 0);
        
        size_t i = 0;
        for (char c1 : s1) {
            costs[0] = i + 1;
            size_t corner = i;
            size_t j = 0;
            for (char c2 : s2) {
                size_t upper = costs[j + 1];
                if (c1 == c2) {
                    costs[j + 1] = corner;
                } else {
                    costs[j + 1] = std::min({costs[j], upper, corner}) + 1;
                }
                corner = upper;
                j++;
            }
            i++;
        }
        return costs[n];
    }
}
