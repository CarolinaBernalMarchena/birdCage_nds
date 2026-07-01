#include <nds.h>
#include <stdio.h>
#include <stdarg.h>
#include "bird_orange.h"
#include "bird_blue.h"
#include "bird_green.h"
#include "toy_bell.h"
#include "perch.h"
#include "feeder.h"
#include "drinker.h"
#include "cage_bg.h"

typedef enum { SCREEN_MAIN, SCREEN_SHOP, SCREEN_PLACE } GameScreen;
typedef enum { BIRD_ORANGE, BIRD_BLUE, BIRD_GREEN, BIRD_COLOR_COUNT } BirdColor;
typedef enum { ITEM_TOY, ITEM_PERCH, ITEM_FEEDER, ITEM_DRINKER, ITEM_COUNT } ItemType;

// --- Filas de texto (una unica fuente de verdad para dibujar Y para detectar toques) ---
#define ROW_FEED         2
#define ROW_SHOP         4
#define ROW_FLIP         6

#define ROW_BACK         2
#define ROW_LABEL_COLOR  4
#define ROW_OPT_ORANGE   6
#define ROW_OPT_BLUE     8
#define ROW_OPT_GREEN    10
#define ROW_LABEL_DECOR  12
#define ROW_OPT_FIRST_ITEM 14   // cada item ocupa 2 filas: 14, 16, 18, 20...

#define ROW_Y_START(row) ((row) * 8)
#define ROW_Y_END(row)   (((row) + 1) * 8)

#define BIRD_BASE_X 116
#define BIRD_BASE_Y 84
#define FEED_ANIM_FRAMES 20

#define PAL_BANK_BIRD 0 // los items decorativos usan bancos 1, 2, 3, 4 (ver tabla mas abajo)

#define CAGE_LEFT   56
#define CAGE_RIGHT  200
#define CAGE_TOP    24
#define CAGE_BOTTOM 160

#define GUIDE_COL_LEFT   (CAGE_LEFT / 8)
#define GUIDE_COL_RIGHT  (CAGE_RIGHT / 8)
#define GUIDE_ROW_TOP    (CAGE_TOP / 8)
#define GUIDE_ROW_BOTTOM (CAGE_BOTTOM / 8)

typedef struct {
    const unsigned int* tiles;
    unsigned int tilesLen;
    const unsigned short* pal;
    unsigned int palLen;
} SpriteAsset;

typedef struct {
    SpriteAsset asset;
    const char* label;
    u16* gfxMem;     // se rellena en tiempo de ejecucion
    int palBank;
    int oamId;
    int menuRow;     // fila en la Shop donde aparece su boton
    bool placed;
    int x, y;
} DecorItem;

SpriteAsset birdAssets[BIRD_COLOR_COUNT] = {
    { bird_orangeTiles, bird_orangeTilesLen, bird_orangePal, bird_orangePalLen },
    { bird_blueTiles,   bird_blueTilesLen,   bird_bluePal,   bird_bluePalLen   },
    { bird_greenTiles,  bird_greenTilesLen,  bird_greenPal,  bird_greenPalLen  },
};

DecorItem decorItems[ITEM_COUNT] = {
    { { toy_bellTiles, toy_bellTilesLen, toy_bellPal, toy_bellPalLen }, "JUGUETE",  NULL, 1, 1, ROW_OPT_FIRST_ITEM + 0, false, 0, 0 },
    { { perchTiles,    perchTilesLen,    perchPal,    perchPalLen    }, "PERCHA",   NULL, 2, 2, ROW_OPT_FIRST_ITEM + 2, false, 0, 0 },
    { { feederTiles,   feederTilesLen,   feederPal,   feederPalLen   }, "COMEDERO", NULL, 3, 3, ROW_OPT_FIRST_ITEM + 4, false, 0, 0 },
    { { drinkerTiles,  drinkerTilesLen,  drinkerPal,  drinkerPalLen  }, "BEBEDERO", NULL, 4, 4, ROW_OPT_FIRST_ITEM + 6, false, 0, 0 },
};

void gotoRC(int row, int col) {
    iprintf("\x1b[%d;%dH", row, col);
}

void printMainScreen(void) {
    consoleClear();
    gotoRC(ROW_FEED, 3); iprintf("[   FEED   ]");
    gotoRC(ROW_SHOP, 3); iprintf("[   SHOP   ]");
    gotoRC(ROW_FLIP, 3); iprintf("[ FLIP SCR ]");
}

void printShopScreen(BirdColor current) {
    consoleClear();
    gotoRC(ROW_BACK, 3);        iprintf("[   BACK   ]");
    gotoRC(ROW_LABEL_COLOR, 1); iprintf("-- BIRD COLOR --");
    gotoRC(ROW_OPT_ORANGE, 1);  iprintf("%c NARANJA", current == BIRD_ORANGE ? '>' : ' ');
    gotoRC(ROW_OPT_BLUE, 1);    iprintf("%c AZUL",    current == BIRD_BLUE   ? '>' : ' ');
    gotoRC(ROW_OPT_GREEN, 1);   iprintf("%c VERDE",   current == BIRD_GREEN  ? '>' : ' ');
    gotoRC(ROW_LABEL_DECOR, 1); iprintf("-- DECORACION --");

    for (int i = 0; i < ITEM_COUNT; i++) {
        gotoRC(decorItems[i].menuRow, 1);
        iprintf("[ %s ]", decorItems[i].label);
    }
}

void printPlacementScreen(const char* itemLabel) {
    consoleClear();
    gotoRC(0, 1); iprintf("TOCA DENTRO PARA COLOCAR");
    gotoRC(1, 1); iprintf("%s - B: CANCELAR", itemLabel);

    for (int col = GUIDE_COL_LEFT; col <= GUIDE_COL_RIGHT; col++) {
        gotoRC(GUIDE_ROW_TOP, col);    iprintf("-");
        gotoRC(GUIDE_ROW_BOTTOM, col); iprintf("-");
    }
    for (int row = GUIDE_ROW_TOP + 1; row < GUIDE_ROW_BOTTOM; row++) {
        gotoRC(row, GUIDE_COL_LEFT);  iprintf("|");
        gotoRC(row, GUIDE_COL_RIGHT); iprintf("|");
    }
}

int clampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int main(void) {
    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D_LAYOUT);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);
    vramSetBankC(VRAM_C_SUB_BG);

    int bg = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 0, 1);
    dmaCopy(cage_bgTiles, bgGetGfxPtr(bg), cage_bgTilesLen);
    dmaCopy(cage_bgMap, bgGetMapPtr(bg), cage_bgMapLen);
    dmaCopy(cage_bgPal, BG_PALETTE, cage_bgPalLen);

    oamInit(&oamMain, SpriteMapping_1D_32, false);

    // --- pajaro ---
    u16* birdGfxMem = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_16Color);
    BirdColor currentBird = BIRD_ORANGE;
    dmaCopy(birdAssets[currentBird].tiles, birdGfxMem, birdAssets[currentBird].tilesLen);
    dmaCopy(birdAssets[currentBird].pal, &SPRITE_PALETTE[PAL_BANK_BIRD * 16], birdAssets[currentBird].palLen);

    // --- decoraciones: se cargan todas al inicio, cada una en su banco de paleta ---
    for (int i = 0; i < ITEM_COUNT; i++) {
        decorItems[i].gfxMem = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_16Color);
        dmaCopy(decorItems[i].asset.tiles, decorItems[i].gfxMem, decorItems[i].asset.tilesLen);
        dmaCopy(decorItems[i].asset.pal, &SPRITE_PALETTE[decorItems[i].palBank * 16], decorItems[i].asset.palLen);
    }

    consoleDemoInit();

    GameScreen currentScreen = SCREEN_MAIN;
    printMainScreen();

    bool wasTouching = false;
    bool flipped = false;
    int feedAnimCounter = 0;
    int placingItem = -1; // indice en decorItems[] que se esta colocando

    while (1) {
        scanKeys();
        if (keysDown() & KEY_START) break;

        if (flipped && (keysDown() & KEY_B)) {
            lcdSwap();
            flipped = false;
        }
        if (currentScreen == SCREEN_PLACE && (keysDown() & KEY_B)) {
            currentScreen = SCREEN_SHOP;
            printShopScreen(currentBird);
        }

        bool isTouching = keysHeld() & KEY_TOUCH;

        if (isTouching && !wasTouching) {
            if (flipped) {
                lcdSwap();
                flipped = false;
            } else {
                touchPosition touch;
                touchRead(&touch);

                if (currentScreen == SCREEN_MAIN) {
                    if (touch.py >= ROW_Y_START(ROW_FEED) && touch.py < ROW_Y_END(ROW_FEED)) {
                        feedAnimCounter = FEED_ANIM_FRAMES;
                    } else if (touch.py >= ROW_Y_START(ROW_SHOP) && touch.py < ROW_Y_END(ROW_SHOP)) {
                        currentScreen = SCREEN_SHOP;
                        printShopScreen(currentBird);
                    } else if (touch.py >= ROW_Y_START(ROW_FLIP) && touch.py < ROW_Y_END(ROW_FLIP)) {
                        lcdSwap();
                        flipped = true;
                    }
                } else if (currentScreen == SCREEN_SHOP) {
                    BirdColor chosenColor = currentBird;
                    bool colorChanged = false;
                    bool handled = false;

                    if (touch.py >= ROW_Y_START(ROW_BACK) && touch.py < ROW_Y_END(ROW_BACK)) {
                        currentScreen = SCREEN_MAIN;
                        printMainScreen();
                        handled = true;
                    } else if (touch.py >= ROW_Y_START(ROW_OPT_ORANGE) && touch.py < ROW_Y_END(ROW_OPT_ORANGE)) {
                        chosenColor = BIRD_ORANGE; colorChanged = true;
                    } else if (touch.py >= ROW_Y_START(ROW_OPT_BLUE) && touch.py < ROW_Y_END(ROW_OPT_BLUE)) {
                        chosenColor = BIRD_BLUE; colorChanged = true;
                    } else if (touch.py >= ROW_Y_START(ROW_OPT_GREEN) && touch.py < ROW_Y_END(ROW_OPT_GREEN)) {
                        chosenColor = BIRD_GREEN; colorChanged = true;
                    }

                    if (!handled && colorChanged && chosenColor != currentBird) {
                        currentBird = chosenColor;
                        dmaCopy(birdAssets[currentBird].tiles, birdGfxMem, birdAssets[currentBird].tilesLen);
                        dmaCopy(birdAssets[currentBird].pal, &SPRITE_PALETTE[PAL_BANK_BIRD * 16], birdAssets[currentBird].palLen);
                        printShopScreen(currentBird);
                        handled = true;
                    }

                    // comprobar si se toco algun boton de decoracion
                    if (!handled) {
                        for (int i = 0; i < ITEM_COUNT; i++) {
                            int row = decorItems[i].menuRow;
                            if (touch.py >= ROW_Y_START(row) && touch.py < ROW_Y_END(row)) {
                                placingItem = i;
                                currentScreen = SCREEN_PLACE;
                                printPlacementScreen(decorItems[i].label);
                                break;
                            }
                        }
                    }
                } else if (currentScreen == SCREEN_PLACE && placingItem >= 0) {
                    decorItems[placingItem].x = clampInt(touch.px - 8, CAGE_LEFT, CAGE_RIGHT - 16);
                    decorItems[placingItem].y = clampInt(touch.py - 8, CAGE_TOP, CAGE_BOTTOM - 16);
                    decorItems[placingItem].placed = true;
                    placingItem = -1;

                    currentScreen = SCREEN_MAIN;
                    printMainScreen();
                }
            }
        }
        wasTouching = isTouching;

        int birdY = BIRD_BASE_Y;
        if (feedAnimCounter > 0) {
            int half = FEED_ANIM_FRAMES / 2;
            int progress = FEED_ANIM_FRAMES - feedAnimCounter;
            int bump = (progress <= half) ? progress : (FEED_ANIM_FRAMES - progress);
            birdY += bump;
            feedAnimCounter--;
        }

        oamSet(&oamMain, 0, BIRD_BASE_X, birdY, 0, PAL_BANK_BIRD, SpriteSize_16x16, SpriteColorFormat_16Color,
               birdGfxMem, -1, false, false, false, false, false);

        for (int i = 0; i < ITEM_COUNT; i++) {
            DecorItem* item = &decorItems[i];
            oamSet(&oamMain, item->oamId, item->x, item->y, 0, item->palBank, SpriteSize_16x16, SpriteColorFormat_16Color,
                   item->gfxMem, -1, false, !item->placed, false, false, false);
        }

        swiWaitForVBlank();
        oamUpdate(&oamMain);
    }

    return 0;
}