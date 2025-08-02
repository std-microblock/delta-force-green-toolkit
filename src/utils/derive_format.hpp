#include <concepts>
#include <print>
#include <string>

#include "reflect.hpp"

struct derive_format {};

template <typename T>
concept DerivedFormat = std::derived_from<T, derive_format>;

namespace std {
template <DerivedFormat T>
struct formatter<T> : public formatter<std::string> {
    template <class FmtContext>
    FmtContext::iterator format(T s, FmtContext& ctx) const {
        std::string res;

        reflect::for_each(
            [&](auto I) {
                if constexpr (reflect::size(s) > I - 1) {
                    res += ", ";
                }

                res += std::format("{}: {}", reflect::member_name<I>(s),
                                   reflect::get<I>(s));
            },
            s);

        return std::ranges::copy(std::format("{} {{{}}}", reflect::type_name(s), res), ctx.out()).out;
    }
};
}  // namespace std
