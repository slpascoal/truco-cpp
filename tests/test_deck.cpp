#include "core/deck.hpp"

#include <random>
#include <set>

#include "test_framework.hpp"

using namespace truco;

namespace {

int cardKey(Card c) {
    return commonStrength(c.rank) * 4 + suitOrder(c.suit);
}

bool contains(const std::array<Card, 3>& hand, Card c) {
    return hand[0] == c || hand[1] == c || hand[2] == c;
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

TEST(deal_excludes_vira_from_hands) {
    std::mt19937 rng(12345);
    for (int i = 0; i < 200; ++i) {
        const Deal d = deal(rng);
        CHECK(!contains(d.human, d.vira));
        CHECK(!contains(d.cpu, d.vira));
    }
}

TEST(deal_has_six_distinct_cards_plus_vira) {
    std::mt19937 rng(999);
    const Deal d = deal(rng);
    std::set<int> keys{cardKey(d.vira)};
    for (const Card& c : d.human) keys.insert(cardKey(c));
    for (const Card& c : d.cpu) keys.insert(cardKey(c));
    CHECK_EQ(keys.size(), size_t{7});
}

TEST(deal_is_deterministic_for_same_seed) {
    std::mt19937 a(777);
    std::mt19937 b(777);
    const Deal da = deal(a);
    const Deal db = deal(b);
    CHECK(da.vira == db.vira);
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
    bool same = da.vira == db.vira;
    for (int i = 0; i < 3; ++i) same = same && da.human[i] == db.human[i];
    CHECK(!same);
}
