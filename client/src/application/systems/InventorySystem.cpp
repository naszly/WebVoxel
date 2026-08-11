#include "InventorySystem.h"
#include "application/Application.h"
#include "application/Inventory.h"
#include "core/events/KeyEvent.h"
#include "core/events/MouseEvent.h"

#include <magic_enum.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

// ── C → JS ───────────────────────────────────────────────────────────────────

EM_JS(void, js_initInventoryUI, (), {
    // Build block-name → texture-file map
    const blockTextures = {
        0:  null,                         // Air
        1:  'textures/grass_top.png',     // Grass
        2:  'textures/dirt.png',          // Dirt
        3:  'textures/stone.png',         // Stone
        4:  'textures/duskstone.png',     // Duskstone
        5:  'textures/blackrock.png',     // Blackrock
        6:  'textures/eclipse_crystal.png',
        7:  'textures/moonlit_lantern.png',
        8:  'textures/oak_log_top.png',   // OakLog
        9:  'textures/oak_leaves.png',    // OakLeaves
        10: 'textures/birch_wood_top.png',// BirchLog
        11: 'textures/birch_leaves.png',  // BirchLeaves
    };

    // Load textures from Emscripten FS and create Object URLs
    const textureUrls = {};
    for (const [id, path] of Object.entries(blockTextures)) {
        if (!path) continue;
        try {
            const data = FS.readFile(path);
            const blob = new Blob([data], { type: 'image/png' });
            textureUrls[id] = URL.createObjectURL(blob);
        } catch(e) {}
    }

    // ── Hotbar HTML ──────────────────────────────────────────────────────────
    const hotbar = document.createElement('div');
    hotbar.id = 'vx-hotbar';
    for (let i = 0; i < 9; i++) {
        const slot = document.createElement('div');
        slot.className = 'vx-slot' + (i === 0 ? ' selected' : '');
        slot.id = `vx-hotbar-slot-${i}`;
        slot.innerHTML = `<span class="vx-slot-count"></span>`;
        hotbar.appendChild(slot);
    }
    document.body.appendChild(hotbar);

    // ── Inventory overlay HTML ───────────────────────────────────────────────
    const overlay = document.createElement('div');
    overlay.id = 'vx-inventory-overlay';
    overlay.innerHTML = `
        <div id="vx-inventory-panel">
            <h3>Inventory</h3>
            <div class="vx-inv-grid" id="vx-inv-main"></div>
            <div class="vx-hotbar-row-label">Hotbar</div>
            <div class="vx-inv-grid" id="vx-inv-hotbar"></div>
            <div id="vx-close-hint">Press E to close</div>
        </div>
    `;
    document.body.appendChild(overlay);

    // ── Drag-and-drop ────────────────────────────────────────────────────────
    let dragSrcSlot = -1;
    let dragSrcEl   = null;
    const makeDraggable = (el, idx) => {
        el.draggable = false; // vxUpdateSlot enables dragging when slot has content
        el.addEventListener('dragstart', e => {
            if (!el.querySelector('img')) {
                e.preventDefault();
                return;
            }
            dragSrcSlot = idx;
            dragSrcEl   = el;
            el.classList.add('dragging');
            e.dataTransfer.effectAllowed = 'move';
            e.dataTransfer.setData('text/plain', idx);
        });
        el.addEventListener('dragend', () => {
            dragSrcSlot = -1;
            dragSrcEl   = null;
            el.classList.remove('dragging');
            document.querySelectorAll('.vx-inv-slot.drag-over')
                    .forEach(s => s.classList.remove('drag-over'));
        });
        el.addEventListener('dragover', e => {
            e.preventDefault();
            e.dataTransfer.dropEffect = 'move';
            el.classList.add('drag-over');
        });
        el.addEventListener('dragleave', () => {
            el.classList.remove('drag-over');
        });
        el.addEventListener('drop', e => {
            e.preventDefault();
            el.classList.remove('drag-over');
            if (dragSrcSlot >= 0 && dragSrcSlot !== idx) {
                // Remove dim from source immediately, before the C++ sync repaints
                if (dragSrcEl) dragSrcEl.classList.remove('dragging');
                Module._swapInventorySlots(dragSrcSlot, idx);
            }
        });
    };

    // Populate main inventory slots
    const mainGrid = document.getElementById('vx-inv-main');
    for (let i = 0; i < 36; i++) {
        const slot = document.createElement('div');
        slot.className = 'vx-inv-slot';
        slot.id = `vx-inv-slot-${i + 9}`;
        slot.innerHTML = `<span class="vx-inv-slot-count"></span>`;
        slot.addEventListener('click', () => Module._selectInventorySlot(i + 9));
        makeDraggable(slot, i + 9);
        mainGrid.appendChild(slot);
    }

    // Populate hotbar row inside inventory
    const invHotbar = document.getElementById('vx-inv-hotbar');
    for (let i = 0; i < 9; i++) {
        const slot = document.createElement('div');
        slot.className = 'vx-inv-slot';
        slot.id = `vx-inv-hotbar-slot-${i}`;
        slot.innerHTML = `<span class="vx-inv-slot-count"></span>`;
        slot.addEventListener('click', () => Module._selectInventorySlot(i));
        makeDraggable(slot, i);
        invHotbar.appendChild(slot);
    }

    // ── Slot update helper ───────────────────────────────────────────────────
    window.vxUpdateSlot = function(slotIndex, blockId, count) {
        const url = textureUrls[blockId] || null;
        const countEl = (el) => el ? el.querySelector('.vx-slot-count, .vx-inv-slot-count') : null;

        // Update hotbar
        if (slotIndex < 9) {
            const el = document.getElementById(`vx-hotbar-slot-${slotIndex}`);
            if (el) {
                const img = el.querySelector('img');
                if (url && count > 0) {
                    if (!img) {
                        const i = document.createElement('img');
                        i.src = url;
                        el.prepend(i);
                    } else {
                        img.src = url;
                    }
                    countEl(el).textContent = count > 1 ? count : '';
                } else {
                    if (img) img.remove();
                    countEl(el).textContent = '';
                }
            }
            // Also update inventory hotbar row
            const invEl = document.getElementById(`vx-inv-hotbar-slot-${slotIndex}`);
            if (invEl) {
                const img = invEl.querySelector('img');
                if (url && count > 0) {
                    if (!img) {
                        const i = document.createElement('img');
                        i.src = url;
                        invEl.prepend(i);
                    } else {
                        img.src = url;
                    }
                    invEl.querySelector('.vx-inv-slot-count').textContent = count > 1 ? count : '';
                } else {
                    if (img) img.remove();
                    invEl.querySelector('.vx-inv-slot-count').textContent = '';
                }
                invEl.draggable = !!(url && count > 0);
            }
        } else {
            // Main inventory
            const el = document.getElementById(`vx-inv-slot-${slotIndex}`);
            if (el) {
                const img = el.querySelector('img');
                if (url && count > 0) {
                    if (!img) {
                        const i = document.createElement('img');
                        i.src = url;
                        el.prepend(i);
                    } else {
                        img.src = url;
                    }
                    el.querySelector('.vx-inv-slot-count').textContent = count > 1 ? count : '';
                } else {
                    if (img) img.remove();
                    el.querySelector('.vx-inv-slot-count').textContent = '';
                }
                el.draggable = !!(url && count > 0);
            }
        }
    };

    window.vxSetSelectedHotbarSlot = function(slot) {
        for (let i = 0; i < 9; i++) {
            const el = document.getElementById(`vx-hotbar-slot-${i}`);
            if (el) el.classList.toggle('selected', i === slot);
        }
    };

    window.vxOpenInventory = function() {
        document.getElementById('vx-inventory-overlay').classList.add('open');
        document.getElementById('vx-hotbar').style.display = 'none';
    };

    window.vxCloseInventory = function() {
        document.getElementById('vx-inventory-overlay').classList.remove('open');
        document.getElementById('vx-hotbar').style.display = 'flex';
    };
});

EM_JS(void, js_updateSlot, (int slotIndex, unsigned int blockId, int count), {
    if (window.vxUpdateSlot) window.vxUpdateSlot(slotIndex, blockId, count);
});

EM_JS(void, js_setSelectedHotbarSlot, (int slot), {
    if (window.vxSetSelectedHotbarSlot) window.vxSetSelectedHotbarSlot(slot);
});

EM_JS(void, js_openInventory, (), {
    if (window.vxOpenInventory) window.vxOpenInventory();
});

EM_JS(void, js_closeInventory, (), {
    if (window.vxCloseInventory) window.vxCloseInventory();
});

// ── JS → C ───────────────────────────────────────────────────────────────────

static InventorySystem* g_inventorySystem = nullptr;

extern "C" {

EMSCRIPTEN_KEEPALIVE void selectInventorySlot(int slotIndex) {
    if (!g_inventorySystem) return;
    auto& inventory = g_inventorySystem->getApplication().getApplicationData().inventory;
    if (slotIndex < Inventory::HOTBAR_SIZE) {
        inventory.setSelectedSlot(slotIndex);
        g_inventorySystem->syncSelectedSlotToUI();
        g_inventorySystem->syncSelectedVoxel();
        g_inventorySystem->closeInventory();
    }
}

EMSCRIPTEN_KEEPALIVE void swapInventorySlots(int slotA, int slotB) {
    if (!g_inventorySystem) return;
    auto& inventory = g_inventorySystem->getApplication().getApplicationData().inventory;
    inventory.swapSlots(slotA, slotB);
    g_inventorySystem->syncHotbarToUI();
    g_inventorySystem->syncSelectedVoxel();
}

}  // extern "C"

#endif // __EMSCRIPTEN__


void InventorySystem::initialize() {
#ifdef __EMSCRIPTEN__
    g_inventorySystem = this;
    js_initInventoryUI();
#endif
    syncHotbarToUI();
    syncSelectedSlotToUI();
    syncSelectedVoxel();
}

void InventorySystem::update(float /*dt*/) {}

void InventorySystem::onEvent(Event& event) {
    EventDispatcher dispatcher(event);

    dispatcher.dispatch<KeyPressedEvent>([&](const KeyPressedEvent& e) {
        const auto key = e.getKeyCode();

        // 1-9 hotbar selection
        if (key >= KeyCode::Num1 && key <= KeyCode::Num9 && !e.isRepeat()) {
            const int slot = static_cast<int>(key) - static_cast<int>(KeyCode::Num1);
            auto& inventory = getApplication().getApplicationData().inventory;
            inventory.setSelectedSlot(slot);
            syncSelectedSlotToUI();
            syncSelectedVoxel();
            event.handled = true;
            return true;
        }

        // E key: toggle inventory
        if (key == KeyCode::E && !e.isRepeat()) {
            toggleInventory();
            event.handled = true;
            return true;
        }

        return false;
    });

    dispatcher.dispatch<MouseScrolledEvent>([&](const MouseScrolledEvent& e) {
        if (m_inventoryOpen) return false;
        auto& inventory = getApplication().getApplicationData().inventory;
        const int delta = e.getYOffset() > 0.0f ? -1 : 1;
        inventory.scrollSelectedSlot(delta);
        syncSelectedSlotToUI();
        syncSelectedVoxel();
        event.handled = true;
        return true;
    });
}

void InventorySystem::openInventory() {
    m_inventoryOpen = true;
    getApplicationData().inventoryOpen = true;
    getInput().setCursorMode(Normal);
#ifdef __EMSCRIPTEN__
    js_openInventory();
#endif
}

void InventorySystem::closeInventory() {
    m_inventoryOpen = false;
    getApplicationData().inventoryOpen = false;
    getInput().setCursorMode(Disabled);
#ifdef __EMSCRIPTEN__
    js_closeInventory();
#endif
}

void InventorySystem::toggleInventory() {
    if (m_inventoryOpen) closeInventory();
    else openInventory();
}

void InventorySystem::syncHotbarToUI() const {
#ifdef __EMSCRIPTEN__
    const auto& inventory = getApplication().getApplicationData().inventory;
    for (int i = 0; i < Inventory::TOTAL_SIZE; i++) {
        const auto& slot = inventory.getSlot(i);
        js_updateSlot(i, static_cast<unsigned int>(slot.blockId), slot.isEmpty() ? 0 : slot.count);
    }
#endif
}

void InventorySystem::syncSelectedSlotToUI() const {
#ifdef __EMSCRIPTEN__
    const auto& inventory = getApplication().getApplicationData().inventory;
    js_setSelectedHotbarSlot(inventory.getSelectedSlot());
#endif
}

void InventorySystem::syncSelectedVoxel() const {
    auto& appData = getApplication().getApplicationData();
    const auto& selected = appData.inventory.getSelectedItem();
    if (!selected.isEmpty()) {
        appData.selectedVoxel = VoxelData(selected.blockId);
    }
}
