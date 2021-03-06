/**
 *  \file map.h
 *  \brief MAP (large background map) management unit
 *  \author Stephane Dallongeville
 *  \date 11/2020
 *
 * This unit provides methods to manipulate / scroll large background MAP:<br>
 * - Create / release MAP object<br>
 * - MAP decoding functions<br>
 * - Large MAP scrolling engine<br>
 * <br>
 * Using MAP resource you can encode large image as a #MapDefinition structure which will be
 * used to handle large background scrolling. The #MapDefinition structure is optimized to encode
 * large map efficiently (space wise), here's the encoding format:<br>
 * - background map is encoded as a grid of 128x128 pixels blocks<br>
 * - duplicated 128x128 blocks are optimized (keep only 1 reference)<br>
 * - each 128x128 blocks is encoded internally as a 8x8 grid of metatile<br>
 * - a metatile is a 16x16 pixels block (2x2 tiles block)<br>
 * - each duplicated / flipped metatile are optimized.<br>
 * <br>
 * Knowing that you can draw your level background to optimize its space usage by trying to optimize<br>
 * the number of unique 128x128 pixels block.
 *
 */

#ifndef _MAP_H_
#define _MAP_H_


#include "vdp.h"
#include "vdp_tile.h"
#include "pal.h"


/**
 *  \brief
 *      MapDefinition structure which contains data for large level background.<br>
 *      It's optimized to encode large map using 128x128 block chunk (block chunk are organized in metatile).
 *
 *  \param w
 *      map width in block (128x128 pixels block).
 *  \param h
 *      map height in block (128x128 pixels block).
 *  \param numMetaTile
 *      number of MetaTile
 *  \param numBlock
 *      number of Block (128x128 pixels chunk)
 *  \param palette
 *      Palette data.
 *  \param tileset
 *      TileSet data structure (contains tiles definition for the image).
 *  \param metaTiles
 *      metatiles definition, each metatile is encoded as 2x2 tiles block:<br>
 *      - b15: priority<br>
 *      - b14-b13: palette<br>
 *      - b12: vflip<br>
 *      - b11: hflip<br>
 *      - b10-b0: tile index (from tileset)
 *  \param blocks
 *      blocks definition, each block is encoded as 8x8 metatiles:<br>
 *      - b15: priority override<br>
 *      - b14-b13: free, can be used to encode collision info ?<br>
 *      - b12: combined vflip<br>
 *      - b11: combined hflip<br>
 *      - b10-b0: metatile index
 *  \param blockIndexes
 *      block index array (referencing blocks) for the w * h sized map
 *  \param blockRowOffsets
 *      block row offsets used internally for fast retrieval of block data (index = blockIndexes[blockRowOffsets[y] + x])
 */
typedef struct
{
    u16 w;
    u16 h;
    u16 numMetaTile;
    u16 numBlock;
    Palette *palette;
    TileSet *tileset;
    u16 *metaTiles;
    u16 *blocks;
    u16 *blockIndexes;
    u16 *blockRowOffsets;
} MapDefinition;


/**
 *  \brief
 *      Map structure containing information for large background/plane update based on #MapDefinition
 *
 *  \param w
 *      map width in block (128x128 pixels block)
 *  \param h
 *      map height in block (128x128 pixels block)
 *  \param metaTiles
 *      internal - direct FAR access (see #FAR) to mapDefinition->metaTiles
 *  \param blocks
 *      internal - direct FAR access (see #FAR) to mapDefinition->blocks
 *  \param blockIndexes
 *      internal - direct FAR access (see #FAR) to mapDefinition->blocks
 *  \param blockRowOffsets
 *      internal - direct FAR access (see #FAR) to mapDefinition->blocks
 *  \param plane
 *      VDP plane where MAP is draw
 *  \param baseTile
 *      Base tile attributes used to provide base tile index offset and base palette index (see TILE_ATTR_FULL() macro)
 *  \param posX
 *      current view position X set using #MAP_scrollTo(..) method
 *  \param posY
 *      current view position Y set using #MAP_scrollTo(..) method
 *  \param planeWidthMask
 *      internal
 *  \param planeHeightMask
 *      internal
 *  \param lastXT
 *      internal
 *  \param lastYT
 *      internal
 */
typedef struct
{
    u16 w;
    u16 h;
    u16 *metaTiles;
    u16 *blocks;
    u16 *blockIndexes;
    u16 *blockRowOffsets;
    VDPPlane plane;
    u16 baseTile;
    u32 posX;
    u32 posY;
    u16 planeWidthMask;
    u16 planeHeightMask;
    u16 lastXT;
    u16 lastYT;
} Map;


/**
 *  \brief
 *      Initialize Map structure required to use all MAP_xxx functions
 *
 *  \param mapDef
 *      MapDefinition structure containing background/plane data.
 *  \param plane
 *      Plane where we want to draw the Map (for #MAP_scroll(..) method).<br>
 *      Accepted values are:<br>
 *      - BG_A<br>
 *      - BG_B<br>
 *  \param basetile
 *      Used to provide base tile index and base palette index (see TILE_ATTR_FULL() macro)
 *  \param map
 *      Map structure to initialize
 *
 *  \see #MAP_scrollTo(..)
 */
void MAP_init(const MapDefinition* mapDef, VDPPlane plane, u16 baseTile, Map *map);
/**
 *  \brief
 *      Scroll map to specified position.<vr>
 *      The fonction takes care of updating the VDP tilemap which will be transfered by DMA queue then
 *      VDP background scrolling is automatically set on VBlank (into the SYS_doVBlankProcess() tasks)
 *
 *  \param map
 *      Map structure containing map information.
 *  \param x
 *      view position X we want to scroll on
 *  \param y
 *      view position Y we want to scroll on
 *
 *  \see #MAP_init(..)
 */
void MAP_scrollTo(Map* map, u32 x, u32 y);

/**
 *  \brief
 *      Returns given metatile attribute (a metatile is a block of 2x2 tiles = 16x16 pixels)
 *
 *  \param mapDef
 *      MapDefinition structure containing map information.
 *  \param x
 *      metatile X position
 *  \param x
 *      metatile Y position
 *
 *  \return
 *      metatile attribute:<br>
 *      - b15: priority override<br>
 *      - b14-b13: free, can be used to encode collision info ?<br>
 *      - b12: combined vflip<br>
 *      - b11: combined hflip<br>
 *      - b10-b0: metatile index<br>
 *
 *  \see #MAP_init(..)
 *  \see #MAP_Tile(..)
 *  \see #MAP_getMetaTilemapRect(..)
 */
u16 MAP_getMetaTile(Map* map, u16 x, u16 y);
/**
 *  \brief
 *      Returns given tile attribute (note than map->baseTile isn't added to the result)
 *
 *  \param mapDef
 *      MapDefinition structure containing map information.
 *  \param x
 *      tile X position
 *  \param x
 *      tile Y position
 *
 *  \return
 *      tile attribute:<br>
 *      - b15: priority<br>
 *      - b14-b13: palette<br>
 *      - b12: vflip<br>
 *      - b11: hflip<br>
 *      - b10-b0: tile index
 *
 *  \see #MAP_init(..)
 *  \see #MAP_getMetaTile(..)
 *  \see #MAP_getTilemapRect(..)
 */
u16 MAP_getTile(Map* map, u16 x, u16 y);
/**
 *  \brief
 *      Returns metatiles attribute for the specified region (a metatile is a block of 2x2 tiles = 16x16 pixels)
 *
 *  \param mapDefmap
 *      MapDefinition structure containing map information.
 *  \param basetile
 *      Base index and flag for tile attributes (see TILE_ATTR_FULL() macro).
 *  \param x
 *      Region X start position (in metatile).
 *  \param y
 *      Region Y start position (in metatile).
 *  \param w
 *      Region Width (in metatile).
 *  \param h
 *      Region Heigh (in metatile).
 *  \param dest
 *      destination pointer receiving metatiles attribute data
 *
 *  \return
 *      metatiles attribute:<br>
 *      - b15: priority override<br>
 *      - b14-b13: free, can be used to encode collision info ?<br>
 *      - b12: combined vflip<br>
 *      - b11: combined hflip<br>
 *      - b10-b0: metatile index<br>
 *
 *  \see #MAP_init(..)
 *  \see #MAP_getTilemapRect(..)
 */
void MAP_getMetaTilemapRect(Map* map, u16 x, u16 y, u16 w, u16 h, u16* dest);
/**
 *  \brief
 *      Returns tiles attribute data for the specified region (map->baseTile is used as base tiles attribute, see #MAP_init(..))
 *
 *  \param mapDef
 *      MapDefinition structure containing map information.
 *  \param x
 *      Region X start position <b>(in metatile)</b>
 *  \param y
 *      Region Y start position <b>(in metatile)</b>
 *  \param w
 *      Region Width <b>(in metatile)</b>
 *  \param h
 *      Region Heigh <b>(in metatile)</b>
 *  \param column
 *      if set to TRUE then tilemap data is stored by column order [Y,X] instead of row order [X,Y].
 *  \param dest
 *      destination pointer receiving tiles attribute data
 *
 *  \return
 *      tiles attribute:<br>
 *      - b15: priority<br>
 *      - b14-b13: palette<br>
 *      - b12: vflip<br>
 *      - b11: hflip<br>
 *      - b10-b0: tile index
 *
 *  \see #MAP_init(..)
 *  \see #MAP_Tile(..)
 *  \see #MAP_getMetaTilemapRect(..)
 */
void MAP_getTilemapRect(Map* map, u16 x, u16 y, u16 w, u16 h, bool column, u16* dest);


#endif // _MAP_H_
