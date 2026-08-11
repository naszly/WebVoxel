#pragma once

#include <array>
#include "application/domain/ItemStack.h"

class Inventory {
public:
    static constexpr int HOTBAR_SIZE = 9;
    static constexpr int MAIN_ROWS = 4;
    static constexpr int MAIN_COLS = 9;
    static constexpr int MAIN_SIZE = MAIN_ROWS * MAIN_COLS;
    static constexpr int TOTAL_SIZE = HOTBAR_SIZE + MAIN_SIZE;

    Inventory() {
        m_slots.fill(ItemStack::empty());

        // Default hotbar contents
        m_slots[0] = ItemStack::of(BlockId::Grass, 64);
        m_slots[1] = ItemStack::of(BlockId::Dirt, 64);
        m_slots[2] = ItemStack::of(BlockId::Stone, 64);
        m_slots[3] = ItemStack::of(BlockId::OakLog, 64);
        m_slots[4] = ItemStack::of(BlockId::OakLeaves, 64);
        m_slots[5] = ItemStack::of(BlockId::BirchLog, 64);
        m_slots[6] = ItemStack::of(BlockId::BirchLeaves, 64);
        m_slots[7] = ItemStack::of(BlockId::Duskstone, 64);
        m_slots[8] = ItemStack::of(BlockId::MoonlitLantern, 64);
    }

    [[nodiscard]] const ItemStack& getSlot(int index) const { return m_slots[index]; }
    [[nodiscard]] ItemStack& getSlot(int index) { return m_slots[index]; }

    [[nodiscard]] const ItemStack& getHotbarSlot(int slot) const { return m_slots[slot]; }
    [[nodiscard]] const ItemStack& getMainSlot(int row, int col) const { return m_slots[HOTBAR_SIZE + row * MAIN_COLS + col]; }

    void setSlot(int index, const ItemStack& item) { m_slots[index] = item; }

    void swapSlots(int a, int b) { std::swap(m_slots[a], m_slots[b]); }

    [[nodiscard]] int getSelectedSlot() const { return m_selectedSlot; }

    void setSelectedSlot(int slot) {
        m_selectedSlot = (slot + HOTBAR_SIZE) % HOTBAR_SIZE;
    }

    void scrollSelectedSlot(int delta) {
        setSelectedSlot(m_selectedSlot + delta);
    }

    [[nodiscard]] const ItemStack& getSelectedItem() const {
        return m_slots[m_selectedSlot];
    }

private:
    std::array<ItemStack, TOTAL_SIZE> m_slots;
    int m_selectedSlot{0};
};
