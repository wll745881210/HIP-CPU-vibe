#include <algorithm>
#include <execution>
#include <vector>

int main()
{
    std::vector<int> v{1};
    std::for_each(std::execution::par, v.begin(), v.end(), [](int&) {});
    return 0;
}
