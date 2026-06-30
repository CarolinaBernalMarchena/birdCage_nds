#include <nds.h>
#include "bird.h"
#include "cage_bg.h"

int main(void) {
    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D_LAYOUT);

    vramSetBankA(VRAM_A_MAIN_BG);      // banco A: para el background
    vramSetBankB(VRAM_B_MAIN_SPRITE);  // banco B: para el sprite del pajaro (banco distinto al del fondo)

    // --- background: la jaula ---
    int bg = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 0, 1);
    dmaCopy(cage_bgTiles, bgGetGfxPtr(bg), cage_bgTilesLen);
    dmaCopy(cage_bgMap, bgGetMapPtr(bg), cage_bgMapLen);
    dmaCopy(cage_bgPal, BG_PALETTE, cage_bgPalLen);

    // --- sprite: el pajaro ---
    oamInit(&oamMain, SpriteMapping_1D_32, false);
    u16* spriteGfxMem = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    dmaCopy(birdTiles, spriteGfxMem, birdTilesLen);
    dmaCopy(birdPal, SPRITE_PALETTE, birdPalLen);

    while (1) {
        scanKeys();
        if (keysDown() & KEY_START) break;

        // posicion aproximada sobre la percha
        oamSet(&oamMain, 0, 116, 84, 0, 0, SpriteSize_16x16, SpriteColorFormat_256Color,
               spriteGfxMem, -1, false, false, false, false, false);

        swiWaitForVBlank();
        oamUpdate(&oamMain);
    }

    return 0;
}