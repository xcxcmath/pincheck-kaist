#include <string>
#include <array>
#include <vector>
#include <deque>
#include <algorithm>
#include <optional>

#if __GNUC__ > 7
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
using Path = fs::path;

using Buffer = std::array<char, 1024>;
using String = std::string;

template<typename T>
using Optional = std::optional<T>;

template<typename T>
using Vector = std::vector<T>;

template<typename T>
using Deque = std::deque<T>;

template<typename T1, typename T2 = T1>
using Pair = std::pair<T1, T2>;

