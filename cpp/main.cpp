#include "dmc_reference.hpp"
#include <iostream>

int main() {
    dmc::DMCIndex index(dmc::DMCConfig{});
    std::vector<dmc::TokenRef> refs;
    for (std::int32_t i = 0; i < 128; ++i) {
        refs.push_back(dmc::TokenRef{static_cast<std::uint32_t>(i), i, 0});
    }
    index.rebuild(refs);
    auto selection = index.select(0, 127);
    std::cout << "tokens=" << selection.token_ids.size() << "\n";
    std::cout << "spans=" << selection.spans.size() << "\n";
    return 0;
}
