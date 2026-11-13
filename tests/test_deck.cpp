#include "core/deck.hpp"

#include <random>
#include <set>

#include "test_framework.hpp"

using namespace truco;

namespace {

int cardKey(Card c) {
    return commonStrength(c.rank) * 4 + suitOrder(c.suit);
}

}  // namespace

TEST(deck_has_forty_unique_cards) {
    const auto deck = buildDeck();
    std::set<int> keys;
    for (const Card& c : deck) {
        keys.insert(cardKey(c));
    }
    CHECK_EQ(deck.size(), size_t{40});
    CHECK_EQ(keys.size(), size_t{40});
}

TEST(deck_has_no_eight_nine_ten) {
    const auto labels = buildDeck();
    for (const Card& c : labels) {
        const std::string label = rankLabel(c.rank);
        CHECK(label != "8");
        CHECK(label != "9");
        CHECK(label != "10");
    }
}

TEST(deal_has_six_distinct_cards) {
    std::mt19937 rng(999);
    const Deal d = deal(rng);
    std::set<int> keys;
    for (const Card& c : d.human) keys.insert(cardKey(c));
    for (const Card& c : d.cpu) keys.insert(cardKey(c));
    CHECK_EQ(keys.size(), size_t{6});
}

TEST(deal_is_deterministic_for_same_seed) {
    std::mt19937 a(777);
    std::mt19937 b(777);
    const Deal da = deal(a);
    const Deal db = deal(b);
    for (int i = 0; i < 3; ++i) {
        CHECK(da.human[i] == db.human[i]);
        CHECK(da.cpu[i] == db.cpu[i]);
    }
}

TEST(different_seeds_produce_different_deals) {
    std::mt19937 a(1);
    std::mt19937 b(2);
    const Deal da = deal(a);
    const Deal db = deal(b);
    bool same = true;
    for (int i = 0; i < 3; ++i) {
        same = same && da.human[i] == db.human[i] && da.cpu[i] == db.cpu[i];
    }
    CHECK(!same);
}
