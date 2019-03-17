#include <stdio.h>
#include <string.h>

#include "../inc/rescomp.h"
#include "../inc/plugin.h"
#include "../inc/tools.h"
#include "../inc/img_tools.h"
#include "../inc/spr_tools.h"

#include "../inc/sprite.h"
#include "../inc/palette.h"
#include "../inc/tileset.h"


// forward
static int isSupported(char *type);
static int execute(char *info, FILE *fs, FILE *fh);


// SPRITE resource support
Plugin sprite = { isSupported, execute };


static int isSupported(char *type)
{
    if (!strcasecmp(type, "SPRITE")) return 1;

    return 0;
}

static int execute(char *info, FILE *fs, FILE *fh)
{
    char temp[MAX_PATH_LEN];
    char id[50];
    char fileIn[MAX_PATH_LEN];
    char packedStr[256];
    int w, h, bpp;
    int wf, hf;
    int wt, ht;
    int size, psize;
    int packed, time;
    int maxIndex;
    char collision[32];
    int collid;
    int nbElem;
    unsigned char *data;
    unsigned short *palette;
    spriteDefinition_ *sprDef;

    packed = 0;
    time = 0;
    strcpy(collision, "NONE");

    nbElem = sscanf(info, "%s %s \"%[^\"]\" %d %d %s %d %s", temp, id, temp, &wf, &hf, packedStr, &time, collision);

    if (nbElem < 5)
    {
        printf("Wrong SPRITE definition\n");
        printf("SPRITE name \"file\" width heigth [packed [time [collid]]]\n");
        printf("  name      Sprite variable name\n");
        printf("  file      the image file to convert to SpriteDefinition structure (should be a 8bpp .bmp or .png)\n");
        printf("  width     width of a single sprite frame in tile\n");
        printf("  height    heigth of a single sprite frame in tile\n");
        printf("  packed    compression type, accepted values:\n");
        printf("              -1 / BEST / AUTO = use best compression\n");
        printf("               0 / NONE        = no compression\n");
        printf("               1 / APLIB       = aplib library (good compression ratio but slow)\n");
        printf("               2 / FAST / LZ4W = custom lz4 compression (average compression ratio but fast)\n");
        printf("  time      display frame time in 1/60 of second (time between each animation frame).\n");
        printf("  collid    collision type: CIRCLE, BOX or NONE (BOX by default).\n");

        return FALSE;
    }

    if (!strcasecmp(collision, "CIRCLE")) collid = COLLISION_CIRCLE;
    else if (!strcasecmp(collision, "BOX")) collid = COLLISION_BOX;
    else collid = COLLISION_NONE;

    // adjust input file path
    adjustPath(resDir, temp, fileIn);
    // get packed value
    packed = getCompression(packedStr);

    // retrieve basic infos about the image
    if (!Img_getInfos(fileIn, &w, &h, &bpp)) return FALSE;

    // get size in tile
    wt = (w + 7) / 8;
    ht = (h + 7) / 8;

    // inform about incorrect size
    if ((w & 7) != 0)
    {
        printf("Warning: Image %s width is not a multiple of 8 (%d)\n", fileIn, w);
        printf("Width changed to %d\n", wt * 8);
    }
    if ((h & 7) != 0)
    {
        printf("Warning: Image %s height is not a multiple of 8 (%d)\n", fileIn, h);
        printf("Height changed to %d\n", ht * 8);
    }

    // get image data (always 8bpp)
    data = Img_getData(fileIn, &size, 8, 8);
    if (!data) return FALSE;

    // find max color index
    maxIndex = getMaxIndex(data, size);
    // not allowed here
    if (maxIndex >= 64)
    {
        printf("Error: Image %s use color index >= 64\n", fileIn);
        printf("IMAGE resource require image with a maximum of 64 colors.\n");
        return FALSE;
    }

    sprDef = getSpriteDefinition(data, wt, ht, wf, hf, time, collid);
    if (!sprDef) return FALSE;

    //TODO: optimize
    removeEmptyFrame(sprDef);

    // pack data
    if (packed != PACK_NONE)
    {
        if (!packSpriteDef(sprDef, packed)) return FALSE;
    }

    // get palette
    palette = Img_getPalette(fileIn, &psize);
    if (!palette) return FALSE;

    // optimize palette size
    if (maxIndex < 16) psize = 16;
    else if (maxIndex < 32) psize = 32;
    else if (maxIndex < 48) psize = 48;
    else psize = 64;

    // EXPORT PALETTE
    strcpy(temp, id);
    strcat(temp, "_palette");
    outPalette(palette, psize, fs, fh, temp, FALSE);

    // EXPORT SPRITE
    outSpriteDef(sprDef, fs, fh, id, TRUE);

    return TRUE;
}


void outCollision(collision_* collision, FILE* fs, FILE* fh, char* id, int global)
{
    // Collision structure
    decl(fs, fh, "Collision", id, 2, global);
    // type position
    fprintf(fs, "    dc.b    %d\n", collision->typeHit);
    fprintf(fs, "    dc.b    %d\n", collision->typeAttack);

    switch(collision->typeHit)
    {
        case COLLISION_BOX:
            fprintf(fs, "    dc.b    %d\n", collision->hit.box.x);
            fprintf(fs, "    dc.b    %d\n", collision->hit.box.y);
            fprintf(fs, "    dc.b    %d\n", collision->hit.box.w);
            fprintf(fs, "    dc.b    %d\n", collision->hit.box.h);
            break;

        case COLLISION_CIRCLE:
            fprintf(fs, "    dc.b    %d\n", collision->hit.circle.x);
            fprintf(fs, "    dc.b    %d\n", collision->hit.circle.y);
            fprintf(fs, "    dc.w    %d\n", collision->hit.circle.ray);
            break;
    }
    switch(collision->typeAttack)
    {
        case COLLISION_BOX:
            fprintf(fs, "    dc.b    %d\n", collision->attack.box.x);
            fprintf(fs, "    dc.b    %d\n", collision->attack.box.y);
            fprintf(fs, "    dc.b    %d\n", collision->attack.box.w);
            fprintf(fs, "    dc.b    %d\n", collision->attack.box.h);
            break;

        case COLLISION_CIRCLE:
            fprintf(fs, "    dc.b    %d\n", collision->attack.circle.x);
            fprintf(fs, "    dc.b    %d\n", collision->attack.circle.y);
            fprintf(fs, "    dc.w    %d\n", collision->attack.circle.ray);
            break;
    }

    fprintf(fs, "\n");
}

//void outFrameSprite(frameSprite_* frameSprite, FILE* fs, FILE* fh, char* baseid, char* id, int global)
//{
//    char temp[MAX_PATH_LEN];
//    char refid[MAX_PATH_LEN];
//
//    // EXPORT TILESET
//    sprintf(refid, "%s_gtileset", baseid);
//    sprintf(temp, "%s_tileset", id);
//    outTileset(frameSprite->tileset, fs, fh, refid, temp, FALSE);
//
//    // FrameSprite structure
//    decl(fs, fh, "FrameSprite", id, 2, global);
//    // Y position
//    fprintf(fs, "    dc.w    %d\n", frameSprite->y);
//    // size infos
//    fprintf(fs, "    dc.w    %d\n", SPRITE_SIZE(frameSprite->w, frameSprite->h) << 8);
//    // index and attributs
//    fprintf(fs, "    dc.w    %d\n", frameSprite->ind | (frameSprite->attr << 11));
//    // X position
//    fprintf(fs, "    dc.w    %d\n", frameSprite->x);
//    // set tileset pointer
//    fprintf(fs, "    dc.l    %s\n", temp);
//    fprintf(fs, "\n");
//}

void outFrameSprite(frameSprite_* frameSprite, FILE* fs, FILE* fh, char* id, int global)
{
    if ((frameSprite->offsetx < -128) || (frameSprite->offsetx > 127) || (frameSprite->offsety < -128) || (frameSprite->offsety > 127))
        printf("Error: sprite '%s' offset < -128 or > 127 !\n", id);

    // FrameSprite = FrameVDPSprite structure in SGDK
    decl(fs, fh, "FrameVDPSprite", id, 2, global);
    // Num tile
    fprintf(fs, "    dc.b    %d\n", frameSprite->numTile);
    // Y position
    fprintf(fs, "    dc.b    %d\n", frameSprite->offsety);
    // size infos
    fprintf(fs, "    dc.b    %d\n", SPRITE_SIZE(frameSprite->w, frameSprite->h) << 0);
    // X position
    fprintf(fs, "    dc.b    %d\n", frameSprite->offsetx);
    fprintf(fs, "\n");
}

void outFrameInfo(frameInfo_* frameInfo, int numSprite, FILE* fs, FILE* fh, char* id, int global)
{
    int i;
    frameSprite_ **frameSprites;
    char temp[MAX_PATH_LEN];

    // EXPORT FRAME SPRITES
    frameSprites = frameInfo->frameSprites;
    // norm
    for(i = 0; i < numSprite; i++)
    {
        sprintf(temp, "%s_sprite%d", id, i);
        outFrameSprite(*frameSprites, fs, fh, temp, FALSE);
        // next
        frameSprites++;
    }

    fprintf(fs, "\n");

    // sprites data
    sprintf(temp, "%s_sprites", id);
    // declare
    decl(fs, fh, NULL, temp, 2, FALSE);
    // output sprite pointers
    for(i = 0; i < numSprite; i++)
        fprintf(fs, "    dc.l    %s_sprite%d\n", id, i);

    fprintf(fs, "\n");

    collision_* collision = frameInfo->collision;
    // EXPORT COLLISION DATA
    if (collision)
    {
        sprintf(temp, "%s_collision", id);
        outCollision(collision, fs, fh, temp, FALSE);

        fprintf(fs, "\n");
    }
}

void outAnimFrame(animFrame_* animFrame, FILE* fs, FILE* fh, char* id, int global)
{
    int numSprite;
    char temp[MAX_PATH_LEN];

    numSprite = animFrame->numSprite;

    // EXPORT FRAME INFO
    sprintf(temp, "%s_base", id);
    outFrameInfo(&(animFrame->frameInfos[0]), numSprite, fs, fh, temp, FALSE);
    sprintf(temp, "%s_hflip", id);
    outFrameInfo(&(animFrame->frameInfos[1]), numSprite, fs, fh, temp, FALSE);
    sprintf(temp, "%s_vflip", id);
    outFrameInfo(&(animFrame->frameInfos[2]), numSprite, fs, fh, temp, FALSE);
    sprintf(temp, "%s_hvflip", id);
    outFrameInfo(&(animFrame->frameInfos[3]), numSprite, fs, fh, temp, FALSE);

    // EXPORT TILESET
    sprintf(temp, "%s_tileset", id);
    outTileset(animFrame->tileset, fs, fh, temp, FALSE);

    // AnimationFrame structure
    decl(fs, fh, "AnimationFrame", id, 2, global);
    // set number of sprite
    fprintf(fs, "    dc.w    %d\n", animFrame->numSprite);
    // set frame sprites and collision pointer (base)
    fprintf(fs, "    dc.l    %s_base_sprites\n", id);
    if (animFrame->frameInfos[0].collision)
        fprintf(fs, "    dc.l    %s_base_collision\n", id);
    else
        fprintf(fs, "    dc.l    0\n");
    // set frame sprites and collision pointer (hflip)
    fprintf(fs, "    dc.l    %s_hflip_sprites\n", id);
    if (animFrame->frameInfos[1].collision)
        fprintf(fs, "    dc.l    %s_hflip_collision\n", id);
    else
        fprintf(fs, "    dc.l    0\n");
    // set frame sprites and collision pointer (vflip)
    fprintf(fs, "    dc.l    %s_vflip_sprites\n", id);
    if (animFrame->frameInfos[2].collision)
        fprintf(fs, "    dc.l    %s_vflip_collision\n", id);
    else
        fprintf(fs, "    dc.l    0\n");
    // set frame sprites and collision pointer (hvflip)
    fprintf(fs, "    dc.l    %s_hvflip_sprites\n", id);
    if (animFrame->frameInfos[3].collision)
        fprintf(fs, "    dc.l    %s_hvflip_collision\n", id);
    else
        fprintf(fs, "    dc.l    0\n");
    // set tileset pointer
    fprintf(fs, "    dc.l    %s_tileset\n", id);
    // frame width
    fprintf(fs, "    dc.w    %d\n", animFrame->w * 8);
    // frame height
    fprintf(fs, "    dc.w    %d\n", animFrame->h * 8);
    // timer info
    fprintf(fs, "    dc.w    %d\n", animFrame->timer);
    fprintf(fs, "\n");
}

void outAnimation(animation_* animation, FILE* fs, FILE* fh, char* id, int global)
{
    int i;
    animFrame_ **frames;
    char temp[MAX_PATH_LEN];

    // EXPORT FRAME
    frames = animation->frames;
    for(i = 0; i < animation->numFrame; i++)
    {
        sprintf(temp, "%s_frame%d", id, i);
        outAnimFrame(*frames++, fs, fh, temp, FALSE);
    }

    fprintf(fs, "\n");

    // frames data
    strcpy(temp, id);
    strcat(temp, "_frames");
    // declare
    decl(fs, fh, NULL, temp, 2, FALSE);
    // output frame pointers
    for(i = 0; i < animation->numFrame; i++)
        fprintf(fs, "    dc.l    %s_frame%d\n", id, i);

    fprintf(fs, "\n");

    // EXPORT SEQUENCE DATA
    strcpy(temp, id);
    strcat(temp, "_sequence");
    // declare
    decl(fs, fh, NULL, temp, 2, FALSE);
    // output sequence data
    outS(animation->sequence, 0, animation->length, fs, 1);

    fprintf(fs, "\n");

    // Animation structure
    decl(fs, fh, "Animation", id, 2, global);
    // set number of frame
    fprintf(fs, "    dc.w    %d\n", animation->numFrame);
    // set frames pointer
    fprintf(fs, "    dc.l    %s_frames\n", id);
    // set size of sequence
    fprintf(fs, "    dc.w    %d\n", animation->length);
    // set sequence pointer
    fprintf(fs, "    dc.l    %s_sequence\n", id);
    // loop info
    fprintf(fs, "    dc.w    %d\n", animation->loop);
    fprintf(fs, "\n");
}

void outSpriteDef(spriteDefinition_* spriteDef, FILE* fs, FILE* fh, char* id, int global)
{
    int i;
    animation_ **animations;
    char temp[MAX_PATH_LEN];

    // EXPORT ANIMATION
    animations = spriteDef->animations;
    for(i = 0; i < spriteDef->numAnimation; i++)
    {
        sprintf(temp, "%s_animation%d", id, i);
        outAnimation(*animations++, fs, fh, temp, FALSE);
    }

    fprintf(fs, "\n");

    // animations data
    strcpy(temp, id);
    strcat(temp, "_animations");
    // declare
    decl(fs, fh, NULL, temp, 2, FALSE);
    // output animation pointers
    for(i = 0; i < spriteDef->numAnimation; i++)
        fprintf(fs, "    dc.l    %s_animation%d\n", id, i);

    fprintf(fs, "\n");

    // SpriteDefinition structure
    decl(fs, fh, "SpriteDefinition", id, 2, global);
    // set palette pointer
    fprintf(fs, "    dc.l    %s_palette\n", id);
    // set number of animation
    fprintf(fs, "    dc.w    %d\n", spriteDef->numAnimation);
    // set animations pointer
    fprintf(fs, "    dc.l    %s_animations\n", id);
    // set maximum number of tile used by a single animation frame (used for VRAM tile space allocation)
    fprintf(fs, "    dc.w    %d\n", spriteDef->maxNumTile);
    // set maximum number of VDP sprite used by a single animation frame (used for VDP sprite allocation)
    fprintf(fs, "    dc.w    %d\n", spriteDef->maxNumSprite);

    fprintf(fs, "\n");
}
