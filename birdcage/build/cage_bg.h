
//{{BLOCK(cage_bg)

//======================================================================
//
//	cage_bg, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 17 tiles (t|f reduced) not compressed
//	+ regular map (in SBBs), not compressed, 32x32 
//	Total size: 512 + 1088 + 2048 = 3648
//
//	Time-stamp: 2026-07-01, 12:08:31
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_CAGE_BG_H
#define GRIT_CAGE_BG_H

#define cage_bgTilesLen 1088
extern const unsigned int cage_bgTiles[272];

#define cage_bgMapLen 2048
extern const unsigned short cage_bgMap[1024];

#define cage_bgPalLen 512
extern const unsigned short cage_bgPal[256];

#endif // GRIT_CAGE_BG_H

//}}BLOCK(cage_bg)
