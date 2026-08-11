#pragma once

#include "System.h"

class InventorySystem final : public System {
public:
    void initialize() override;
    void update(float dt) override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView& targetView) override {}
    void onEvent(Event& event) override;

    [[nodiscard]] bool isInventoryOpen() const { return m_inventoryOpen; }
    void openInventory();
    void closeInventory();
    void toggleInventory();

    void syncHotbarToUI() const;
    void syncSelectedSlotToUI() const;
    void syncSelectedVoxel() const;

private:
    bool m_inventoryOpen{false};

    static void injectUI();
};
