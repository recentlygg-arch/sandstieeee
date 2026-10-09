// =====================================================================
//  SANDSTRIKE v2 - 3D шутер в духе CS / Standoff 2 (C++ / raylib)
//
//  * Главное меню с картинкой menu_bg.jpg (лежит рядом с .exe)
//  * Пауза по ESC: продолжить / главное меню / выйти из игры
//  * 10 секунд закупки перед раундом (меню на клавишу B)
//  * Деньги: за убийства, победы, поражения, установку бомбы
//  * 14 видов оружия с моделями и анимацией перезарядки
//  * Карта в стиле Sandstone: текстурированные стены, ящики, бочки,
//    навесы, перемычки, силуэты зданий вдали
//  * Все звуки генерируются кодом (внешних файлов звуков не нужно)
//
//  Управление:
//    WASD - ходьба, Shift - медленно, Space - прыжок, Мышь - обзор
//    ЛКМ - огонь, ПКМ - прицел снайперок, R - перезарядка
//    1 / 2 / Q / колесо - смена оружия, B - меню закупки (в начале раунда)
//    E (удерживать на пленте A/B) - установить бомбу, ESC - пауза
// =====================================================================
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#ifdef PLATFORM_ANDROID
#define PLANT_HINT "Hold PLANT to plant the bomb"
#else
#define PLANT_HINT "Hold [E] to plant the bomb"
#endif

using std::vector;
using std::string;

static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;

enum Side { TERRO = 0, CT = 1 };
enum GState { FREEZE, LIVE, ROUNDEND };
enum AppState { APP_MENU, APP_GAME };

static float Rand01() { return (float)rand() / (float)RAND_MAX; }
static float RandR(float a, float b) { return a + (b - a) * Rand01(); }

static Color Col(float r, float g, float b, float a = 255.0f) {
    return Color{ (unsigned char)Clamp(r, 0.0f, 255.0f), (unsigned char)Clamp(g, 0.0f, 255.0f),
                  (unsigned char)Clamp(b, 0.0f, 255.0f), (unsigned char)Clamp(a, 0.0f, 255.0f) };
}
static Color Mul(Color c, float k) { return Col(c.r * k, c.g * k, c.b * k, (float)c.a); }
static Color MixC(Color a, Color b, float t) {
    return Col(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 255.0f);
}

// =====================================================================
//  ОРУЖИЕ: характеристики
// =====================================================================
enum {
    W_GLOCK, W_P250, W_DEAGLE,
    W_MAC10, W_MP5, W_P90,
    W_AK47, W_M4A4, W_GALIL,
    W_SCOUT, W_AWP,
    W_NOVA, W_XM1014, W_M249,
    NUM_W
};
enum { CAT_PISTOL, CAT_SMG, CAT_RIFLE, CAT_SNIPER, CAT_HEAVY };
enum { SH_PISTOL, SH_DEAGLE, SH_SMG, SH_RIFLE, SH_SNIPER, SH_SHOTGUN };

struct WeaponDef {
    const char* name;
    int cat, price, dmg;
    float rate;
    int mag, reserve;
    bool autof;
    float spread, recoil, reloadT;
    int pellets, reward;
    float speed, range;
    int shot;
    bool scope;
    float pitch;
};

static const WeaponDef WDEF[NUM_W] = {
    { "Glock-18",    CAT_PISTOL,  200,  20, 0.15f, 20, 100, false, 0.004f, 0.030f, 2.2f, 1, 300, 1.00f,   60.f, SH_PISTOL,  false, 1.00f },
    { "P250",        CAT_PISTOL,  300,  28, 0.16f, 13,  52, false, 0.004f, 0.035f, 2.4f, 1, 300, 1.00f,   60.f, SH_PISTOL,  false, 0.90f },
    { "Desert Eagle",CAT_PISTOL,  700,  55, 0.35f,  7,  35, false, 0.005f, 0.080f, 2.2f, 1, 300, 1.00f,   90.f, SH_DEAGLE,  false, 1.00f },
    { "MAC-10",      CAT_SMG,    1050,  22, 0.065f,30, 100, true,  0.012f, 0.014f, 2.2f, 1, 600, 1.00f,   35.f, SH_SMG,     false, 1.15f },
    { "MP5-SD",      CAT_SMG,    1500,  24, 0.08f, 30, 120, true,  0.008f, 0.014f, 2.4f, 1, 600, 1.00f,   45.f, SH_SMG,     false, 1.00f },
    { "P90",         CAT_SMG,    2350,  23, 0.07f, 50, 100, true,  0.009f, 0.012f, 3.3f, 1, 300, 0.97f,   50.f, SH_SMG,     false, 1.20f },
    { "AK-47",       CAT_RIFLE,  2700,  34, 0.10f, 30,  90, true,  0.004f, 0.018f, 2.4f, 1, 300, 0.93f, 1000.f, SH_RIFLE,   false, 1.00f },
    { "M4A4",        CAT_RIFLE,  3100,  31, 0.09f, 30,  90, true,  0.003f, 0.015f, 2.4f, 1, 300, 0.95f, 1000.f, SH_RIFLE,   false, 1.10f },
    { "Galil AR",    CAT_RIFLE,  1800,  29, 0.09f, 35,  90, true,  0.005f, 0.016f, 2.6f, 1, 300, 0.95f, 1000.f, SH_RIFLE,   false, 0.95f },
    { "SSG 08",      CAT_SNIPER, 1700,  85, 1.25f, 10,  90, false, 0.000f, 0.050f, 2.5f, 1, 300, 1.00f, 1000.f, SH_SNIPER,  true,  1.15f },
    { "AWP",         CAT_SNIPER, 4750, 115, 1.45f,  5,  30, false, 0.000f, 0.070f, 3.5f, 1, 100, 0.85f, 1000.f, SH_SNIPER,  true,  1.00f },
    { "Nova",        CAT_HEAVY,  1050,  11, 0.90f,  8,  32, false, 0.050f, 0.060f, 3.0f, 9, 900, 0.95f,   18.f, SH_SHOTGUN, false, 1.00f },
    { "XM1014",      CAT_HEAVY,  2000,   9, 0.25f,  7,  32, true,  0.050f, 0.040f, 3.2f, 6, 900, 0.90f,   18.f, SH_SHOTGUN, false, 1.10f },
    { "M249",        CAT_HEAVY,  5200,  32, 0.08f,100, 200, true,  0.006f, 0.014f, 5.5f, 1, 300, 0.80f, 1000.f, SH_RIFLE,   false, 0.80f }
};

static const int ARMOR_PRICE = 1000;

// =====================================================================
//  ОРУЖИЕ: 3D-модели из блоков (дуло смотрит в +Z)
// =====================================================================
enum { C_METAL, C_WOOD, C_STEEL, C_POLY, C_TAN, C_GREEN, C_GOLD, C_SKIN, C_SLEEVE };
static const Color PAL[9] = {
    { 38, 38, 42, 255 }, { 128, 82, 46, 255 }, { 135, 138, 148, 255 }, { 26, 26, 28, 255 },
    { 150, 132, 92, 255 }, { 62, 78, 52, 255 }, { 205, 175, 70, 255 }, { 222, 178, 142, 255 }, { 86, 76, 56, 255 }
};

struct Part { float x, y, z, w, h, l; uint8_t c, f; };   // f: 1 = магазин (двигается при перезарядке)
struct ModelDef { const Part* p; int n; Vector3 grip, fore; };

static const Part M_GLOCK[] = {
    { 0, 0.020f, 0.08f, 0.034f, 0.040f, 0.26f, C_POLY, 0 }, { 0, -0.012f, 0.06f, 0.030f, 0.030f, 0.22f, C_POLY, 0 },
    { 0, -0.075f, -0.035f, 0.032f, 0.11f, 0.055f, C_POLY, 0 }, { 0, -0.115f, -0.035f, 0.028f, 0.07f, 0.045f, C_METAL, 1 },
    { 0, 0.021f, 0.225f, 0.016f, 0.016f, 0.04f, C_STEEL, 0 }, { 0, 0.045f, 0.19f, 0.008f, 0.012f, 0.012f, C_STEEL, 0 },
    { 0, 0.045f, -0.03f, 0.016f, 0.012f, 0.012f, C_STEEL, 0 }
};
static const Part M_P250[] = {
    { 0, 0.020f, 0.08f, 0.035f, 0.042f, 0.27f, C_STEEL, 0 }, { 0, -0.012f, 0.06f, 0.032f, 0.030f, 0.23f, C_TAN, 0 },
    { 0, -0.075f, -0.035f, 0.034f, 0.11f, 0.06f, C_TAN, 0 }, { 0, -0.115f, -0.035f, 0.028f, 0.07f, 0.045f, C_METAL, 1 },
    { 0, 0.021f, 0.235f, 0.016f, 0.016f, 0.04f, C_METAL, 0 }, { 0, 0.047f, 0.2f, 0.008f, 0.012f, 0.012f, C_METAL, 0 }
};
static const Part M_DEAGLE[] = {
    { 0, 0.028f, 0.10f, 0.040f, 0.055f, 0.32f, C_STEEL, 0 }, { 0, -0.010f, 0.08f, 0.036f, 0.040f, 0.28f, C_STEEL, 0 },
    { 0, -0.085f, -0.03f, 0.040f, 0.12f, 0.07f, C_POLY, 0 }, { 0, -0.125f, -0.03f, 0.030f, 0.07f, 0.05f, C_METAL, 1 },
    { 0, 0.031f, 0.27f, 0.020f, 0.020f, 0.05f, C_METAL, 0 }, { 0, 0.058f, 0.10f, 0.012f, 0.006f, 0.22f, C_GOLD, 0 }
};
static const Part M_MAC10[] = {
    { 0, 0, 0, 0.050f, 0.075f, 0.20f, C_METAL, 0 }, { 0, 0.005f, 0.17f, 0.050f, 0.060f, 0.14f, C_METAL, 0 },
    { 0, 0.010f, 0.28f, 0.020f, 0.020f, 0.10f, C_STEEL, 0 }, { 0, -0.09f, -0.01f, 0.040f, 0.11f, 0.05f, C_POLY, 0 },
    { 0, -0.14f, 0.03f, 0.035f, 0.12f, 0.05f, C_METAL, 1 }, { 0, 0.010f, -0.2f, 0.012f, 0.012f, 0.22f, C_STEEL, 0 },
    { 0, -0.03f, -0.31f, 0.012f, 0.07f, 0.012f, C_STEEL, 0 }
};
static const Part M_MP5[] = {
    { 0, 0, 0, 0.055f, 0.08f, 0.30f, C_POLY, 0 }, { 0, -0.005f, 0.28f, 0.060f, 0.065f, 0.22f, C_POLY, 0 },
    { 0, 0.010f, 0.42f, 0.018f, 0.018f, 0.08f, C_STEEL, 0 }, { 0, -0.01f, -0.28f, 0.045f, 0.09f, 0.26f, C_POLY, 0 },
    { 0, -0.10f, -0.06f, 0.040f, 0.12f, 0.05f, C_POLY, 0 }, { 0, -0.14f, 0.10f, 0.036f, 0.17f, 0.05f, C_METAL, 1 },
    { 0, 0.05f, 0.05f, 0.020f, 0.025f, 0.08f, C_METAL, 0 }
};
static const Part M_P90[] = {
    { 0, 0, 0, 0.065f, 0.10f, 0.50f, C_POLY, 0 }, { 0, 0.075f, 0.05f, 0.050f, 0.030f, 0.32f, C_GREEN, 1 },
    { 0, -0.09f, 0.12f, 0.040f, 0.10f, 0.05f, C_POLY, 0 }, { 0, -0.10f, -0.03f, 0.040f, 0.12f, 0.05f, C_POLY, 0 },
    { 0, 0, 0.30f, 0.030f, 0.030f, 0.10f, C_METAL, 0 }, { 0, -0.02f, -0.28f, 0.060f, 0.10f, 0.10f, C_POLY, 0 },
    { 0, 0.105f, 0.12f, 0.030f, 0.020f, 0.04f, C_METAL, 0 }
};
static const Part M_AK[] = {
    { 0, 0, 0, 0.060f, 0.09f, 0.45f, C_METAL, 0 }, { 0, 0.025f, 0.42f, 0.025f, 0.025f, 0.50f, C_STEEL, 0 },
    { 0, 0.0f, 0.30f, 0.070f, 0.07f, 0.28f, C_WOOD, 0 }, { 0, 0.05f, 0.35f, 0.030f, 0.02f, 0.30f, C_METAL, 0 },
    { 0, 0.06f, 0.65f, 0.015f, 0.04f, 0.015f, C_METAL, 0 }, { 0, -0.02f, -0.40f, 0.050f, 0.11f, 0.38f, C_WOOD, 0 },
    { 0, -0.12f, -0.10f, 0.045f, 0.14f, 0.06f, C_WOOD, 0 }, { 0, -0.15f, 0.10f, 0.045f, 0.15f, 0.07f, C_METAL, 1 },
    { 0, -0.27f, 0.14f, 0.045f, 0.12f, 0.07f, C_METAL, 1 }
};
static const Part M_M4[] = {
    { 0, 0, 0, 0.055f, 0.085f, 0.36f, C_METAL, 0 }, { 0, 0, 0.30f, 0.065f, 0.065f, 0.30f, C_POLY, 0 },
    { 0, 0.010f, 0.52f, 0.020f, 0.020f, 0.16f, C_STEEL, 0 }, { 0, 0.010f, 0.62f, 0.030f, 0.030f, 0.05f, C_METAL, 0 },
    { 0, 0.065f, 0.0f, 0.020f, 0.040f, 0.20f, C_METAL, 0 }, { 0, 0.050f, 0.50f, 0.012f, 0.050f, 0.012f, C_METAL, 0 },
    { 0, -0.015f, -0.30f, 0.050f, 0.09f, 0.28f, C_POLY, 0 }, { 0, -0.11f, -0.10f, 0.040f, 0.12f, 0.05f, C_POLY, 0 },
    { 0, -0.14f, 0.07f, 0.040f, 0.15f, 0.07f, C_METAL, 1 }
};
static const Part M_GALIL[] = {
    { 0, 0, 0, 0.055f, 0.085f, 0.40f, C_METAL, 0 }, { 0, -0.005f, 0.30f, 0.065f, 0.07f, 0.25f, C_POLY, 0 },
    { 0, 0.012f, 0.52f, 0.022f, 0.022f, 0.20f, C_STEEL, 0 }, { 0, 0.05f, 0.62f, 0.012f, 0.04f, 0.012f, C_METAL, 0 },
    { 0, -0.02f, -0.36f, 0.045f, 0.10f, 0.30f, C_POLY, 0 }, { 0, -0.12f, -0.10f, 0.040f, 0.13f, 0.05f, C_POLY, 0 },
    { 0, -0.15f, 0.08f, 0.045f, 0.17f, 0.07f, C_METAL, 1 }, { 0, 0.055f, -0.1f, 0.016f, 0.03f, 0.016f, C_METAL, 0 }
};
static const Part M_SCOUT[] = {
    { 0, 0, 0, 0.050f, 0.075f, 0.50f, C_POLY, 0 }, { 0, 0.010f, 0.55f, 0.022f, 0.022f, 0.50f, C_STEEL, 0 },
    { 0, -0.015f, -0.33f, 0.050f, 0.10f, 0.36f, C_GREEN, 0 }, { 0, 0.08f, 0.05f, 0.040f, 0.040f, 0.22f, C_METAL, 0 },
    { 0.04f, 0.02f, -0.05f, 0.05f, 0.015f, 0.015f, C_STEEL, 0 }, { 0, -0.07f, 0.05f, 0.030f, 0.05f, 0.08f, C_METAL, 1 },
    { 0, -0.09f, -0.12f, 0.040f, 0.10f, 0.05f, C_POLY, 0 }
};
static const Part M_AWP[] = {
    { 0, 0, 0, 0.060f, 0.09f, 0.60f, C_GREEN, 0 }, { 0, 0.010f, 0.70f, 0.026f, 0.026f, 0.60f, C_METAL, 0 },
    { 0, 0.010f, 1.02f, 0.040f, 0.040f, 0.07f, C_METAL, 0 }, { 0, -0.02f, -0.45f, 0.060f, 0.12f, 0.45f, C_GREEN, 0 },
    { 0, 0.095f, 0.05f, 0.050f, 0.050f, 0.35f, C_METAL, 0 }, { 0, 0.095f, 0.25f, 0.070f, 0.070f, 0.06f, C_METAL, 0 },
    { 0, 0.095f, -0.12f, 0.065f, 0.065f, 0.05f, C_METAL, 0 }, { 0, -0.09f, 0.0f, 0.040f, 0.06f, 0.10f, C_METAL, 1 },
    { 0, -0.11f, -0.20f, 0.045f, 0.12f, 0.05f, C_POLY, 0 }, { 0.05f, 0.02f, -0.1f, 0.06f, 0.016f, 0.016f, C_STEEL, 0 }
};
static const Part M_NOVA[] = {
    { 0, 0.020f, 0.50f, 0.025f, 0.025f, 0.70f, C_METAL, 0 }, { 0, -0.02f, 0.50f, 0.030f, 0.030f, 0.60f, C_METAL, 0 },
    { 0, -0.03f, 0.42f, 0.050f, 0.040f, 0.20f, C_POLY, 0 }, { 0, 0, 0, 0.055f, 0.08f, 0.28f, C_METAL, 0 },
    { 0, -0.02f, -0.30f, 0.050f, 0.10f, 0.36f, C_POLY, 0 }, { 0, -0.045f, 0.0f, 0.025f, 0.025f, 0.06f, C_GOLD, 1 }
};
static const Part M_XM[] = {
    { 0, 0.020f, 0.48f, 0.026f, 0.026f, 0.60f, C_METAL, 0 }, { 0, 0, 0, 0.060f, 0.085f, 0.30f, C_POLY, 0 },
    { 0, -0.015f, 0.40f, 0.050f, 0.050f, 0.18f, C_POLY, 0 }, { 0, -0.02f, -0.28f, 0.050f, 0.10f, 0.32f, C_POLY, 0 },
    { 0, -0.03f, 0.55f, 0.030f, 0.030f, 0.40f, C_METAL, 0 }, { 0, -0.05f, 0.0f, 0.025f, 0.025f, 0.06f, C_GOLD, 1 }
};
static const Part M_M249[] = {
    { 0, 0, 0, 0.070f, 0.10f, 0.50f, C_POLY, 0 }, { 0, 0.020f, 0.50f, 0.025f, 0.025f, 0.50f, C_METAL, 0 },
    { 0, 0.025f, 0.42f, 0.045f, 0.045f, 0.22f, C_METAL, 0 }, { 0.03f, -0.06f, 0.55f, 0.010f, 0.10f, 0.010f, C_METAL, 0 },
    { -0.03f, -0.06f, 0.55f, 0.010f, 0.10f, 0.010f, C_METAL, 0 }, { 0, -0.10f, 0.0f, 0.10f, 0.12f, 0.12f, C_GREEN, 1 },
    { 0, -0.01f, -0.34f, 0.055f, 0.10f, 0.34f, C_POLY, 0 }, { 0, -0.10f, -0.08f, 0.040f, 0.12f, 0.05f, C_POLY, 0 },
    { 0, -0.02f, 0.28f, 0.070f, 0.06f, 0.14f, C_POLY, 0 }, { 0, 0.075f, 0.10f, 0.020f, 0.040f, 0.25f, C_METAL, 0 }
};

#define MDL(arr, gx, gy, gz, fx, fy, fz) { arr, (int)(sizeof(arr) / sizeof(arr[0])), { gx, gy, gz }, { fx, fy, fz } }
static const ModelDef MODELS[NUM_W] = {
    MDL(M_GLOCK, 0, -0.08f, -0.035f, 0.045f, -0.07f, -0.03f),
    MDL(M_P250, 0, -0.08f, -0.035f, 0.045f, -0.07f, -0.03f),
    MDL(M_DEAGLE, 0, -0.09f, -0.03f, 0.05f, -0.08f, -0.03f),
    MDL(M_MAC10, 0, -0.09f, -0.01f, 0, -0.05f, 0.18f),
    MDL(M_MP5, 0, -0.10f, -0.06f, 0, -0.06f, 0.30f),
    MDL(M_P90, 0, -0.10f, -0.03f, 0, -0.07f, 0.14f),
    MDL(M_AK, 0, -0.12f, -0.10f, 0, -0.04f, 0.30f),
    MDL(M_M4, 0, -0.11f, -0.10f, 0, -0.04f, 0.32f),
    MDL(M_GALIL, 0, -0.12f, -0.10f, 0, -0.04f, 0.30f),
    MDL(M_SCOUT, 0, -0.09f, -0.12f, 0, -0.04f, 0.35f),
    MDL(M_AWP, 0, -0.11f, -0.20f, 0, -0.05f, 0.40f),
    MDL(M_NOVA, 0, -0.05f, -0.12f, 0, -0.05f, 0.42f),
    MDL(M_XM, 0, -0.05f, -0.12f, 0, -0.05f, 0.40f),
    MDL(M_M249, 0, -0.10f, -0.08f, 0, -0.04f, 0.28f)
};
static float gZmin[NUM_W], gZmax[NUM_W];

static void InitModels() {
    for (int w = 0; w < NUM_W; w++) {
        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < MODELS[w].n; i++) {
            const Part& p = MODELS[w].p[i];
            lo = std::min(lo, p.z - p.l * 0.5f);
            hi = std::max(hi, p.z + p.l * 0.5f);
        }
        gZmin[w] = lo; gZmax[w] = hi;
    }
}

// =====================================================================
//  ЗВУК (всё синтезируется в коде)
// =====================================================================
static const int SR = 44100;
static uint32_t gRng = 12345u;
static float Nz() { gRng = gRng * 1664525u + 1013904223u; return (float)((gRng >> 8) & 0xFFFF) / 32768.0f - 1.0f; }

struct SPool { vector<Sound> s; size_t i = 0; };
struct Snd {
    SPool shotPistol, shotDeagle, shotSmg, shotRifle, shotSniper, shotShotgun;
    SPool empty, magOut, magIn, bolt, hit, headshot, hurt, death, step, beep;
    SPool planted, defused, explode, buy, cash, denied, roundStart, win, lose;
    SPool uiClick, uiTick, impact, switchW;
    Sound music;
    bool musicOK = false;
};
static Snd S;
static bool gAudioOK = false;

static Sound SoundFromSamples(const vector<float>& v, int rate) {
    Wave w;
    w.frameCount = (unsigned int)v.size();
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    short* d = (short*)RL_MALLOC(v.size() * sizeof(short));
    for (size_t i = 0; i < v.size(); i++) d[i] = (short)(Clamp(v[i], -1.0f, 1.0f) * 30000.0f);
    w.data = d;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}
static SPool MakePool(const vector<float>& v, int n, int rate = SR) {
    SPool p;
    for (int i = 0; i < n; i++) p.s.push_back(SoundFromSamples(v, rate));
    return p;
}

static vector<float> SynthShot(float dur, float decay, float noiseAmt, float lp, float thumpHz, float thumpAmt, float crack) {
    int n = (int)(dur * SR);
    vector<float> o(n);
    float y = 0.0f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float env = expf(-t * decay);
        float nz = Nz();
        y += lp * (nz - y);
        float s = env * (noiseAmt * y * 2.0f + thumpAmt * sinf(6.2831853f * thumpHz * t * (1.0f - 0.6f * t))) + crack * expf(-t * 500.0f) * nz;
        o[i] = tanhf(s * 1.6f) * 0.9f;
    }
    return o;
}
static vector<float> SynthNoiseHit(float dur, float decay, float lp, float vol) {
    int n = (int)(dur * SR);
    vector<float> o(n);
    float y = 0.0f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        y += lp * (Nz() - y);
        o[i] = y * expf(-t * decay) * vol * 2.0f;
    }
    return o;
}
static void AddTone(vector<float>& buf, float start, float freq, float dur, float decay, float vol, float h2 = 0.3f) {
    int s0 = (int)(start * SR), n = (int)(dur * SR);
    if ((int)buf.size() < s0 + n) buf.resize(s0 + n, 0.0f);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float env = expf(-t * decay) * std::min(1.0f, t * 400.0f);
        buf[s0 + i] += env * vol * (sinf(6.2831853f * freq * t) + h2 * sinf(12.566371f * freq * t));
    }
}
static vector<float> SynthTone(float freq, float dur, float decay, float vol = 0.7f, float h2 = 0.3f) {
    vector<float> b;
    AddTone(b, 0.0f, freq, dur, decay, vol, h2);
    return b;
}
static vector<float> SynthClick(float freq, float dur, float noiseAmt) {
    int n = (int)(dur * SR);
    vector<float> o(n);
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float env = expf(-t * (7.0f / dur));
        o[i] = env * (0.5f * sinf(6.2831853f * freq * t) + noiseAmt * Nz()) * 0.8f;
    }
    return o;
}
static void AddBuf(vector<float>& a, const vector<float>& b, float start) {
    size_t s0 = (size_t)(start * SR);
    if (a.size() < s0 + b.size()) a.resize(s0 + b.size(), 0.0f);
    for (size_t i = 0; i < b.size(); i++) a[s0 + i] += b[i];
}

static vector<float> SynthMusic(int rate) {
    // восточная гамма (фригийская доминанта), 32 восьмых по 0.3 c
    const int pat[32] = { 0,4,5,4, 1,0,4,7, 8,7,5,4, 5,4,1,0, 0,4,5,7, 8,7,5,4, 7,5,4,1, 0,1,0,-5 };
    float step = 0.3f;
    int n = (int)(32 * step * rate);
    vector<float> o(n + rate, 0.0f);
    for (int k = 0; k < 32; k++) {
        float f = 293.66f * powf(2.0f, pat[k] / 12.0f);
        int s0 = (int)(k * step * rate);
        int len = (int)(1.2f * rate);
        for (int i = 0; i < len; i++) {
            float t = (float)i / rate;
            float env = expf(-t * 4.5f) * std::min(1.0f, t * 200.0f);
            float v = sinf(6.2831853f * f * t) + 0.4f * sinf(12.566371f * f * t) + 0.2f * sinf(18.849556f * f * t);
            int idx = (s0 + i) % n;       // замыкаем цикл
            o[idx] += env * v * 0.16f;
        }
    }
    o.resize(n);
    for (int i = 0; i < n; i++) {
        float t = (float)i / rate;
        o[i] += 0.07f * sinf(6.2831853f * 73.4f * t) + 0.035f * sinf(6.2831853f * 110.1f * t);
        o[i] = Clamp(o[i], -1.0f, 1.0f);
    }
    return o;
}

static void InitSounds() {
    gAudioOK = IsAudioDeviceReady();
    if (!gAudioOK) return;
    S.shotPistol = MakePool(SynthShot(0.22f, 22, 1.0f, 0.55f, 160, 0.7f, 0.8f), 6);
    S.shotDeagle = MakePool(SynthShot(0.35f, 14, 1.1f, 0.40f, 110, 1.0f, 1.0f), 4);
    S.shotSmg = MakePool(SynthShot(0.15f, 30, 0.9f, 0.60f, 200, 0.5f, 0.7f), 8);
    S.shotRifle = MakePool(SynthShot(0.30f, 16, 1.2f, 0.45f, 100, 1.0f, 1.0f), 8);
    S.shotSniper = MakePool(SynthShot(0.90f, 5.5f, 1.4f, 0.25f, 70, 1.3f, 1.2f), 3);
    S.shotShotgun = MakePool(SynthShot(0.45f, 9, 1.5f, 0.22f, 80, 1.3f, 0.9f), 4);

    S.empty = MakePool(SynthClick(1500, 0.05f, 0.3f), 2);
    S.magOut = MakePool(SynthClick(300, 0.12f, 0.6f), 2);
    S.magIn = MakePool(SynthClick(520, 0.10f, 0.5f), 2);
    {
        vector<float> b = SynthClick(900, 0.06f, 0.5f);
        AddBuf(b, SynthClick(650, 0.08f, 0.5f), 0.09f);
        S.bolt = MakePool(b, 2);
    }
    S.hit = MakePool(SynthTone(1800, 0.06f, 40, 0.6f, 0.2f), 3);
    {
        vector<float> b;
        AddTone(b, 0.0f, 1400, 0.10f, 25, 0.6f);
        AddTone(b, 0.05f, 2400, 0.14f, 22, 0.6f);
        S.headshot = MakePool(b, 3);
    }
    S.hurt = MakePool(SynthShot(0.25f, 14, 0.8f, 0.15f, 70, 1.0f, 0.3f), 3);
    S.death = MakePool(SynthShot(0.55f, 6, 0.6f, 0.10f, 55, 1.2f, 0.0f), 3);
    S.step = MakePool(SynthNoiseHit(0.09f, 38, 0.18f, 0.6f), 6);
    S.beep = MakePool(SynthTone(1100, 0.09f, 12, 0.7f, 0.1f), 4);
    {
        vector<float> b;
        for (int i = 0; i < 4; i++) AddTone(b, i * 0.14f, 880 + i * 220, 0.12f, 14, 0.6f);
        AddTone(b, 0.6f, 220, 0.5f, 5, 0.7f);
        S.planted = MakePool(b, 1);
    }
    {
        vector<float> b;
        AddTone(b, 0.0f, 523, 0.25f, 6, 0.6f); AddTone(b, 0.12f, 659, 0.25f, 6, 0.6f);
        AddTone(b, 0.24f, 784, 0.25f, 6, 0.6f); AddTone(b, 0.36f, 1047, 0.5f, 4, 0.6f);
        S.defused = MakePool(b, 1);
    }
    S.explode = MakePool(SynthShot(2.8f, 1.6f, 1.6f, 0.04f, 40, 1.5f, 1.0f), 1);
    {
        vector<float> b;
        AddTone(b, 0.0f, 2093, 0.18f, 18, 0.5f); AddTone(b, 0.07f, 3136, 0.35f, 9, 0.5f);
        S.buy = MakePool(b, 2);
    }
    {
        vector<float> b;
        AddTone(b, 0.0f, 1568, 0.15f, 18, 0.5f); AddTone(b, 0.08f, 2349, 0.3f, 10, 0.5f);
        S.cash = MakePool(b, 3);
    }
    S.denied = MakePool(SynthTone(140, 0.22f, 6, 0.7f, 0.8f), 2);
    {
        vector<float> b;
        AddTone(b, 0.0f, 440, 0.3f, 5, 0.6f); AddTone(b, 0.18f, 660, 0.5f, 4, 0.6f);
        S.roundStart = MakePool(b, 1);
    }
    {
        vector<float> b;
        float f[4] = { 523, 659, 784, 1047 };
        for (int i = 0; i < 4; i++) AddTone(b, i * 0.14f, f[i], 0.4f, 5, 0.5f);
        S.win = MakePool(b, 1);
    }
    {
        vector<float> b;
        float f[4] = { 440, 392, 330, 262 };
        for (int i = 0; i < 4; i++) AddTone(b, i * 0.2f, f[i], 0.45f, 4, 0.5f);
        S.lose = MakePool(b, 1);
    }
    S.uiClick = MakePool(SynthClick(900, 0.05f, 0.2f), 3);
    S.uiTick = MakePool(SynthTone(700, 0.04f, 40, 0.5f, 0.1f), 2);
    S.impact = MakePool(SynthNoiseHit(0.06f, 50, 0.30f, 0.7f), 6);
    S.switchW = MakePool(SynthClick(650, 0.06f, 0.5f), 2);

    vector<float> m = SynthMusic(22050);
    S.music = SoundFromSamples(m, 22050);
    S.musicOK = true;
}

static Vector3 gListenPos = { 0, 0, 0 };
static float gListenYaw = 0.0f;

static void PlayPool(SPool& p, float vol, float pan = 0.5f, float pitch = 1.0f) {
    if (!gAudioOK || p.s.empty()) return;
    Sound& s = p.s[p.i];
    p.i = (p.i + 1) % p.s.size();
    SetSoundVolume(s, Clamp(vol, 0.0f, 1.0f));
    SetSoundPan(s, pan);
    SetSoundPitch(s, pitch);
    PlaySound(s);
}
static void PlayAt(SPool& p, Vector3 pos, float vol, float pitch = 1.0f, float maxDist = 90.0f) {
    Vector3 d = Vector3Subtract(pos, gListenPos);
    float dist = Vector3Length(d);
    if (dist > maxDist) return;
    float att = 1.0f / (1.0f + dist * dist * 0.0015f);
    float pan = 0.5f;
    if (dist > 0.5f) {
        Vector3 right = { -cosf(gListenYaw), 0.0f, sinf(gListenYaw) };
        float dp = (d.x * right.x + d.z * right.z) / dist;
        pan = 0.5f + 0.45f * dp;
    }
    PlayPool(p, vol * att, pan, pitch);
}
static SPool& ShotPool(int t) {
    switch (t) {
        case SH_PISTOL: return S.shotPistol;
        case SH_DEAGLE: return S.shotDeagle;
        case SH_SMG: return S.shotSmg;
        case SH_RIFLE: return S.shotRifle;
        case SH_SNIPER: return S.shotSniper;
        default: return S.shotShotgun;
    }
}
static void PlayShot(int wid, Vector3 pos, bool own) {
    const WeaponDef& wd = WDEF[wid];
    if (own) PlayPool(ShotPool(wd.shot), 0.9f, 0.5f, wd.pitch);
    else PlayAt(ShotPool(wd.shot), pos, 1.0f, wd.pitch, 160.0f);
}

// =====================================================================
//  ТЕКСТУРЫ (процедурные) И ПРИМИТИВЫ РИСОВАНИЯ
// =====================================================================
static Texture2D texPlaster, texBrick, texFloor, texCrate, texBg;
static bool bgLoaded = false;
static Texture2D NOTEX = {};
static RenderTexture2D previewRT;

static uint32_t HashU(int x, int y, int s) {
    uint32_t h = (uint32_t)(x * 374761393) + (uint32_t)(y * 668265263) + (uint32_t)(s * 1274126177);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static float H01(int x, int y, int s) { return (float)(HashU(x, y, s) & 0xFFFF) / 65535.0f; }
static int ModI(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }
static float VN(float x, float y, int px, int py, int s) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    int x0 = ModI(xi, px), x1 = ModI(xi + 1, px), y0 = ModI(yi, py), y1 = ModI(yi + 1, py);
    float a = H01(x0, y0, s), b = H01(x1, y0, s), c = H01(x0, y1, s), d = H01(x1, y1, s);
    return Lerp(Lerp(a, b, fx), Lerp(c, d, fx), fy);
}

static Texture2D MakeTex(int n, vector<Color>& px) {
    Image img;
    img.data = px.data();
    img.width = n;
    img.height = n;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Texture2D t = LoadTextureFromImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_ANISOTROPIC_8X);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}

static Color BrickAt(int x, int y, int bw, int bh, int seed, Color mortar, float baseR, float baseG, float baseB) {
    const int N = 256;
    int row = y / bh;
    int off = (row & 1) ? bw / 2 : 0;
    int xx = (x + off) % bw;
    int yy = y % bh;
    int col = ((x + off) / bw) % (N / bw);
    float v = H01(col, row, seed);
    if (xx < 3 || yy < 3) return mortar;
    Color c = Col(baseR + v * 32, baseG + v * 26, baseB + v * 20);
    if (yy >= bh - 4 || xx >= bw - 4) c = Mul(c, 0.86f);
    if (yy < 7) c = Mul(c, 1.07f);
    return c;
}

static Texture2D GenPlaster() {
    const int N = 256;
    vector<Color> px(N * N);
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            float n1 = VN(x / 64.f, y / 64.f, 4, 4, 1), n2 = VN(x / 16.f, y / 16.f, 16, 16, 2);
            float n3 = VN(x / 8.f, y / 64.f, 32, 4, 3), f = H01(x, y, 4);
            float k = 0.80f + 0.20f * n1 + 0.10f * n2 + 0.05f * f - 0.10f * n3;
            Color base = Mul(Col(222, 192, 138), k);
            float patch = VN(x / 64.f, y / 64.f, 4, 4, 9) * 0.65f + VN(x / 16.f, y / 16.f, 16, 16, 10) * 0.35f;
            if (patch > 0.68f) {
                Color b = BrickAt(x, y, 64, 32, 5, Col(165, 140, 100), 180, 140, 95);
                base = MixC(base, b, Clamp((patch - 0.68f) * 12.0f, 0.0f, 1.0f));
            }
            px[y * N + x] = base;
        }
    }
    for (int c = 0; c < 6; c++) {
        float cx = H01(c, 1, 77) * N, cy = H01(c, 2, 77) * N, ang = H01(c, 3, 77) * 6.28f;
        for (int s = 0; s < 60; s++) {
            ang += (H01(c, s, 78) - 0.5f) * 0.8f;
            cx += cosf(ang); cy += sinf(ang);
            int ix = ModI((int)cx, N), iy = ModI((int)cy, N);
            px[iy * N + ix] = Mul(px[iy * N + ix], 0.6f);
        }
    }
    return MakeTex(N, px);
}
static Texture2D GenBrick() {
    const int N = 256;
    vector<Color> px(N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            Color c = BrickAt(x, y, 64, 32, 11, Col(120, 100, 72), 178, 142, 92);
            float n = 0.88f + 0.18f * VN(x / 32.f, y / 32.f, 8, 8, 12) + 0.06f * H01(x, y, 13);
            px[y * N + x] = Mul(c, n);
        }
    return MakeTex(N, px);
}
static Texture2D GenFloor() {
    const int N = 256;
    vector<Color> px(N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            int tx = x / 64, ty = y / 64, lx = x % 64, ly = y % 64;
            float v = H01(tx, ty, 21);
            Color base = Col(205 + v * 20, 176 + v * 18, 124 + v * 14);
            float k = 0.90f + 0.10f * VN(x / 32.f, y / 32.f, 8, 8, 22) + 0.06f * H01(x, y, 23);
            if (lx < 2 || ly < 2) k *= 0.70f;
            else if (lx < 4 || ly < 4) k *= 1.04f;
            if (VN(x / 64.f, y / 64.f, 4, 4, 24) > 0.70f) k *= 0.90f;
            px[y * N + x] = Mul(base, k);
        }
    return MakeTex(N, px);
}
static Texture2D GenCrate() {
    const int N = 256;
    vector<Color> px(N * N);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            bool border = (x < 14 || x >= N - 14 || y < 14 || y >= N - 14);
            float grain = 0.90f + 0.10f * sinf(y * 0.18f + 6.0f * VN(x / 16.f, y / 32.f, 16, 8, 31)) + 0.05f * H01(x, y, 32);
            Color c;
            if (border) {
                c = Mul(Col(112, 74, 40), grain);
                if (x < 2 || y < 2 || x >= N - 2 || y >= N - 2) c = Mul(c, 0.6f);
            } else {
                int px0 = (x - 14) % 58;
                c = Mul(Col(160, 112, 66), grain);
                if (px0 < 2) c = Mul(c, 0.55f);
                if (abs(x - y) < 8) c = Mul(Col(118, 80, 44), grain);
                if (abs(x - y) < 2 || abs(x - y) == 8) c = Mul(c, 0.7f);
            }
            // гвозди
            int cxs[4] = { 7, N - 8, 7, N - 8 };
            int cys[4] = { 7, 7, N - 8, N - 8 };
            for (int k = 0; k < 4; k++)
                if ((x - cxs[k]) * (x - cxs[k]) + (y - cys[k]) * (y - cys[k]) < 9) c = Col(70, 70, 76);
            px[y * N + x] = c;
        }
    return MakeTex(N, px);
}

static void DrawTexBox(float x0, float y0, float z0, float x1, float y1, float z1, Texture2D tex, float tile, Color tint, bool bottom = false) {
    rlCheckRenderBatchLimit(36);
    float su = (x1 - x0) / tile, sv = (y1 - y0) / tile, sw = (z1 - z0) / tile;
    rlSetTexture(tex.id != 0 ? tex.id : rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    auto col = [&](float k) { rlColor4ub((unsigned char)(tint.r * k), (unsigned char)(tint.g * k), (unsigned char)(tint.b * k), tint.a); };
    // +Z
    col(0.78f); rlNormal3f(0, 0, 1);
    rlTexCoord2f(0, sv); rlVertex3f(x0, y0, z1);
    rlTexCoord2f(su, sv); rlVertex3f(x1, y0, z1);
    rlTexCoord2f(su, 0); rlVertex3f(x1, y1, z1);
    rlTexCoord2f(0, 0); rlVertex3f(x0, y1, z1);
    // -Z
    col(0.66f); rlNormal3f(0, 0, -1);
    rlTexCoord2f(su, sv); rlVertex3f(x0, y0, z0);
    rlTexCoord2f(su, 0); rlVertex3f(x0, y1, z0);
    rlTexCoord2f(0, 0); rlVertex3f(x1, y1, z0);
    rlTexCoord2f(0, sv); rlVertex3f(x1, y0, z0);
    // верх
    col(1.0f); rlNormal3f(0, 1, 0);
    rlTexCoord2f(0, sw); rlVertex3f(x0, y1, z0);
    rlTexCoord2f(0, 0); rlVertex3f(x0, y1, z1);
    rlTexCoord2f(su, 0); rlVertex3f(x1, y1, z1);
    rlTexCoord2f(su, sw); rlVertex3f(x1, y1, z0);
    // +X
    col(0.90f); rlNormal3f(1, 0, 0);
    rlTexCoord2f(sw, sv); rlVertex3f(x1, y0, z0);
    rlTexCoord2f(sw, 0); rlVertex3f(x1, y1, z0);
    rlTexCoord2f(0, 0); rlVertex3f(x1, y1, z1);
    rlTexCoord2f(0, sv); rlVertex3f(x1, y0, z1);
    // -X
    col(0.84f); rlNormal3f(-1, 0, 0);
    rlTexCoord2f(0, sv); rlVertex3f(x0, y0, z0);
    rlTexCoord2f(sw, sv); rlVertex3f(x0, y0, z1);
    rlTexCoord2f(sw, 0); rlVertex3f(x0, y1, z1);
    rlTexCoord2f(0, 0); rlVertex3f(x0, y1, z0);
    if (bottom) {
        col(0.5f); rlNormal3f(0, -1, 0);
        rlTexCoord2f(0, 1); rlVertex3f(x0, y0, z0);
        rlTexCoord2f(1, 1); rlVertex3f(x1, y0, z0);
        rlTexCoord2f(1, 0); rlVertex3f(x1, y0, z1);
        rlTexCoord2f(0, 0); rlVertex3f(x0, y0, z1);
    }
    rlEnd();
    rlSetTexture(0);
}

// рисует части модели оружия в локальных координатах
static void DrawParts(int w, float magDy, bool magVis) {
    const ModelDef& m = MODELS[w];
    for (int i = 0; i < m.n; i++) {
        const Part& p = m.p[i];
        float y = p.y;
        if (p.f == 1) {
            if (!magVis) continue;
            y += magDy;
        }
        DrawTexBox(p.x - p.w * 0.5f, y - p.h * 0.5f, p.z - p.l * 0.5f, p.x + p.w * 0.5f, y + p.h * 0.5f, p.z + p.l * 0.5f, NOTEX, 1.0f, PAL[p.c], true);
    }
}

// =====================================================================
//  КАРТА
// =====================================================================
enum { K_WALL, K_CRATE, K_STONE, K_BARREL };
struct Wall {
    float x0, z0, x1, z1, h;
    int kind;
    float shade;
    BoundingBox bb;
};
static vector<Wall> gWalls;

enum { D_BRICK, D_WOOD, D_AWNING, D_BUILD, D_DOME };
struct Deco { int kind; float x0, y0, z0, x1, y1, z1; Color a, b; };
static vector<Deco> gDecos;

static void AddBlock(float x0, float z0, float x1, float z1, float h, int kind) {
    Wall w;
    w.x0 = x0; w.z0 = z0; w.x1 = x1; w.z1 = z1; w.h = h; w.kind = kind;
    w.shade = 0.92f + 0.12f * H01((int)gWalls.size(), 5, 99);
    w.bb = BoundingBox{ Vector3{ x0, 0.0f, z0 }, Vector3{ x1, h, z1 } };
    gWalls.push_back(w);
}
static void AddDeco(int kind, float x0, float y0, float z0, float x1, float y1, float z1, Color a, Color b) {
    gDecos.push_back(Deco{ kind, x0, y0, z0, x1, y1, z1, a, b });
}

static const Vector2 SITE_A = { 32.0f, -46.0f };
static const Vector2 SITE_B = { -32.0f, -46.0f };
static const float SITE_R = 9.0f;

static void BuildMap() {
    auto Wl = [&](float x0, float z0, float x1, float z1) { AddBlock(x0, z0, x1, z1, 6.5f, K_WALL); };
    auto Bx = [&](float x0, float z0, float x1, float z1, float h, int kind) { AddBlock(x0, z0, x1, z1, h, kind); };
    auto Br = [&](float x, float z) { AddBlock(x, z, x + 1.0f, z + 1.0f, 1.1f, K_BARREL); };

    // внешние стены
    AddBlock(-52, -62, 52, -60, 7.0f, K_WALL);
    AddBlock(-52, 60, 52, 62, 7.0f, K_WALL);
    AddBlock(-52, -62, -50, 62, 7.0f, K_WALL);
    AddBlock(50, -62, 52, 62, 7.0f, K_WALL);

    // стена респауна T: тоннели B, мид, длинная A
    Wl(-50, 39, -46, 41); Wl(-36, 39, -8, 41); Wl(8, 39, 22, 41); Wl(30, 39, 50, 41);
    // стены мида
    Wl(-19, -30, -17, 8); Wl(-19, 16, -17, 39);
    Wl(17, -30, 19, -8); Wl(17, 0, 19, 39);
    // стена тоннелей B (с окном)
    Wl(-35, -30, -33, -20); Wl(-35, -14, -33, 39);
    // стены CT
    Wl(-15, -58, -13, -50); Wl(-15, -44, -13, -40); Wl(13, -58, 15, -50); Wl(13, -44, 15, -40);

    // ящики и каменные блоки
    Bx(6, 10, 10, 14, 2.2f, K_STONE);
    Bx(-8, -22, -4, -18, 1.2f, K_CRATE);
    Bx(-6, -34, -3, -31, 1.2f, K_CRATE);
    Bx(30, 20, 34, 24, 1.2f, K_CRATE);
    Bx(38, 25, 44, 31, 2.5f, K_CRATE);
    Bx(42, 0, 48, 6, 2.5f, K_STONE);
    Bx(22, -24, 26, -20, 1.2f, K_CRATE);
    Bx(40, -14, 44, -10, 1.2f, K_CRATE);
    Bx(38, -52, 44, -46, 2.5f, K_CRATE);
    Bx(24, -56, 28, -52, 1.2f, K_CRATE);
    Bx(20, -44, 23, -41, 1.2f, K_CRATE);
    Bx(-44, -56, -38, -50, 2.5f, K_CRATE);
    Bx(-26, -56, -20, -52, 1.2f, K_CRATE);
    Bx(-42, -47, -39, -44, 1.2f, K_CRATE);
    Bx(-46, 0, -43, 3, 1.2f, K_CRATE);
    Bx(-39, -6, -36, -3, 1.2f, K_CRATE);
    Bx(-31, -4, -28, -1, 1.2f, K_CRATE);

    // колонны-контрфорсы у задних стен
    float px[8] = { -44, -32, -20, -4, 4, 20, 32, 44 };
    for (int i = 0; i < 8; i++) Bx(px[i] - 0.6f, -59.7f, px[i] + 0.6f, -58.5f, 4.2f, K_STONE);
    Bx(-20.6f, 58.5f, -19.4f, 59.7f, 4.2f, K_STONE);
    Bx(19.4f, 58.5f, 20.6f, 59.7f, 4.2f, K_STONE);

    // бочки
    Br(-30, 52); Br(30, 52); Br(47.5f, 10); Br(47.5f, 12); Br(46, -58); Br(47.2f, -58);
    Br(-47, -58); Br(-48.2f, -58); Br(-11, -57); Br(-49, 20); Br(-49, 22); Br(-16, -12);

    // перемычки над проходами (для вида)
    Color wood = Col(110, 74, 42);
    float lint[][4] = { {-46, -36, 39, 41}, {-8, 8, 39, 41}, {22, 30, 39, 41} };
    for (int i = 0; i < 3; i++) {
        AddDeco(D_BRICK, lint[i][0], 4.6f, lint[i][2] - 0.1f, lint[i][1], 6.5f, lint[i][3] + 0.1f, Col(255, 255, 255), Col(255, 255, 255));
        AddDeco(D_WOOD, lint[i][0], 4.3f, lint[i][2] - 0.1f, lint[i][1], 4.6f, lint[i][3] + 0.1f, wood, wood);
    }
    AddDeco(D_BRICK, -19.1f, 4.6f, 8, -16.9f, 6.5f, 16, WHITE, WHITE);
    AddDeco(D_WOOD, -19.1f, 4.3f, 8, -16.9f, 4.6f, 16, wood, wood);
    AddDeco(D_BRICK, 16.9f, 4.6f, -8, 19.1f, 6.5f, 0, WHITE, WHITE);
    AddDeco(D_WOOD, 16.9f, 4.3f, -8, 19.1f, 4.6f, 0, wood, wood);
    AddDeco(D_BRICK, -35.1f, 3.2f, -20, -32.9f, 6.5f, -14, WHITE, WHITE);
    AddDeco(D_WOOD, -35.1f, 2.9f, -20, -32.9f, 3.2f, -14, wood, wood);
    AddDeco(D_BRICK, -15.1f, 4.6f, -50, -12.9f, 6.5f, -44, WHITE, WHITE);
    AddDeco(D_BRICK, 12.9f, 4.6f, -50, 15.1f, 6.5f, -44, WHITE, WHITE);

    // навесы
    Color wh = Col(238, 232, 215);
    AddDeco(D_AWNING, -7, 3.6f, 24, 7, 3.75f, 32, Col(30, 90, 170), wh);
    AddDeco(D_AWNING, 21, 3.4f, 16, 33, 3.55f, 22, Col(190, 50, 40), wh);
    AddDeco(D_AWNING, -48, 3.5f, 16, -37, 3.65f, 24, Col(220, 130, 40), wh);
    AddDeco(D_AWNING, -31, 3.4f, -8, -21, 3.55f, -2, Col(50, 130, 70), wh);

    // здания и купола вдали
    float bd[][6] = {
        { -120, 0, -40, -75, 18, -5 }, { -110, 0, 10, -70, 26, 50 }, { 70, 0, -50, 115, 22, -10 },
        { 72, 0, 20, 120, 30, 60 }, { -60, 0, 75, -20, 16, 110 }, { 20, 0, 80, 70, 24, 120 },
        { -50, 0, -110, -10, 20, -75 }, { 15, 0, -115, 65, 28, -78 }, { -95, 0, -100, -62, 34, -72 },
        { 95, 0, -35, 100, 46, -30 }
    };
    for (int i = 0; i < 10; i++)
        AddDeco(D_BUILD, bd[i][0], bd[i][1], bd[i][2], bd[i][3], bd[i][4], bd[i][5], Col(255, 245, 230), Col(255, 255, 255));
    AddDeco(D_DOME, -92, 26, 30, 14, 0, 0, Col(70, 130, 150), WHITE);
    AddDeco(D_DOME, 96, 30, 40, 16, 0, 0, Col(70, 130, 150), WHITE);
    AddDeco(D_DOME, 40, 24, 100, 12, 0, 0, Col(200, 170, 80), WHITE);
}

// ---------------------------------------------------------------------
//  НАВИГАЦИОННЫЙ ГРАФ ДЛЯ БОТОВ
// ---------------------------------------------------------------------
static vector<Vector2> gNodes;
static vector<vector<int>> gAdj;
static const int NODE_A = 26;
static const int NODE_B = 19;
static const int A_GUARD[5] = { 26, 27, 28, 25, 24 };
static const int B_GUARD[5] = { 19, 20, 21, 12, 17 };

static bool SegClear(Vector2 a, Vector2 b, float m) {
    float d = Vector2Distance(a, b);
    int n = (int)(d / 0.5f) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / (float)n;
        float px = a.x + (b.x - a.x) * t, pz = a.y + (b.y - a.y) * t;
        for (const Wall& w : gWalls)
            if (px > w.x0 - m && px < w.x1 + m && pz > w.z0 - m && pz < w.z1 + m) return false;
    }
    return true;
}
static void BuildGraph() {
    gNodes = {
        {0,50},{-41,48},{26,48},{0,36},{0,20},{0,6},{0,-10},{0,-28},{0,-36},
        {-41,36},{-41,10},{-41,-17},{-38,-34},{-34,-17},{-28,-17},
        {-26,12},{-26,-10},{-26,-28},{-14,12},{-32,-46},{-44,-40},{-22,-38},
        {26,36},{30,12},{28,-12},{28,-30},{32,-46},{44,-40},{22,-38},
        {12,-4},{24,-4},{0,-52},{-10,-47},{-18,-47},{10,-47},{18,-47}
    };
    int n = (int)gNodes.size();
    gAdj.assign(n, vector<int>());
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (Vector2Distance(gNodes[i], gNodes[j]) < 46.0f && SegClear(gNodes[i], gNodes[j], 0.7f)) {
                gAdj[i].push_back(j);
                gAdj[j].push_back(i);
            }
}
static int NearestNode(Vector2 p) {
    int best = 0, bestClear = -1;
    float bd = 1e9f, bdc = 1e9f;
    for (int i = 0; i < (int)gNodes.size(); i++) {
        float d = Vector2Distance(p, gNodes[i]);
        if (d < bd) { bd = d; best = i; }
        if (d < bdc && SegClear(p, gNodes[i], 0.3f)) { bdc = d; bestClear = i; }
    }
    return bestClear >= 0 ? bestClear : best;
}
static vector<int> NodePath(int s, int g) {
    int n = (int)gNodes.size();
    vector<float> dist(n, 1e9f);
    vector<int> prev(n, -1);
    vector<bool> done(n, false);
    dist[s] = 0.0f;
    for (int it = 0; it < n; it++) {
        int u = -1;
        for (int i = 0; i < n; i++) if (!done[i] && (u < 0 || dist[i] < dist[u])) u = i;
        if (u < 0 || dist[u] >= 1e9f) break;
        done[u] = true;
        for (int v : gAdj[u]) {
            float nd = dist[u] + Vector2Distance(gNodes[u], gNodes[v]);
            if (nd < dist[v]) { dist[v] = nd; prev[v] = u; }
        }
    }
    vector<int> p;
    if (dist[g] >= 1e9f) return p;
    for (int c = g; c != -1; c = prev[c]) p.push_back(c);
    std::reverse(p.begin(), p.end());
    return p;
}

// =====================================================================
//  ИГРОВЫЕ СТРУКТУРЫ И СОСТОЯНИЕ
// =====================================================================
struct Actor {
    int team = TERRO;
    bool alive = true;
    string name;
    Vector3 pos = { 0, 0, 0 };
    float vy = 0.0f;
    bool onGround = true;
    float yaw = 0.0f, pitch = 0.0f, recoil = 0.0f;
    int hp = 100, armor = 0, kills = 0, money = 800;
    int pri = -1, sec = W_GLOCK, cur = W_GLOCK;
    int mag[NUM_W];
    int res[NUM_W];
    float cd = 0.0f, reload = 0.0f, rph = 0.0f, plantProg = 0.0f;
    float anim = 0.0f, stepT = 0.0f, flashT = 0.0f;
    // ИИ
    vector<int> path;
    size_t pi = 0;
    int goalNode = -1;
    float think = 0.0f, react = 0.0f, strafe = 0.0f, holdTimer = 0.0f, startDelay = 0.0f, stuckT = 0.0f;
    int strafeDir = 0, shots = 0, target = -1, siteSel = 0;
    bool planter = false, defusing = false;
    Actor() { for (int i = 0; i < NUM_W; i++) { mag[i] = WDEF[i].mag; res[i] = WDEF[i].reserve; } }
};
struct Bomb {
    bool planted = false, exploded = false;
    Vector3 pos = { 0, 0, 0 };
    float timer = 40.0f, defuse = 0.0f, explT = 0.0f, beepT = 0.0f, blinkT = 0.0f;
};
struct Tracer { Vector3 a, b; float t; };
struct Decal { Vector3 p; float t; };
struct Particle { Vector3 p, v; float t, maxT, size; Color c; };
struct FeedItem { string txt; float t; };
struct Popup { int amt; float t; };

static vector<Actor> A;
static Bomb bomb;
static int gBombNode = 0;
static GState state = FREEZE;
static AppState app = APP_MENU;
static float stTimer = 10.0f, roundTime = 115.0f;
static int scoreT = 0, scoreCT = 0, roundNo = 0;
static int lossStreak[2] = { 0, 0 };
static string endMsg, banner;
static float hurtT = 0.0f, hitT = 0.0f, muzzleT = 0.0f, bannerT = 0.0f;
static float gunKick = 0.0f, bobT = 0.0f, bobAmt = 0.0f, drawAnim = 0.0f;
static vector<Tracer> tracers;
static vector<Decal> decals;
static vector<Particle> particles;
static vector<FeedItem> feed;
static vector<Popup> popups;
static bool scoped = false, planting = false;
static bool buyOpen = false, paused = false, quitReq = false, showControls = false;
static int skipMouse = 0, hoverItem = -1;
static float masterVol = 0.8f;

// ---- масштаб интерфейса под любой размер окна / полный экран ----
// Интерфейс рисуется в виртуальных координатах 1280x720 и растягивается по окну.
static float uiScale = 1.0f, uiOffX = 0.0f, uiOffY = 0.0f;
static bool gToggleFS = false;

static RenderTexture2D uiRT = {};
static void UpdateUIScale() {
    int w = GetScreenWidth(), h = GetScreenHeight();
    if (w < 64 || h < 64) { w = SCREEN_W; h = SCREEN_H; }
    uiScale = std::min((float)w / SCREEN_W, (float)h / SCREEN_H);
    if (!(uiScale > 0.1f && uiScale < 20.0f)) uiScale = 1.0f;
    uiOffX = std::floor((w - SCREEN_W * uiScale) * 0.5f);
    uiOffY = std::floor((h - SCREEN_H * uiScale) * 0.5f);
    // мышь -> виртуальные координаты интерфейса
#ifndef PLATFORM_ANDROID
    SetMouseOffset((int)-uiOffX, (int)-uiOffY);
    SetMouseScale(1.0f / uiScale, 1.0f / uiScale);
#endif
}
// Интерфейс рисуется в текстуру 1280x720, потом растягивается на окно.
static void BeginUI() {
    BeginTextureMode(uiRT);
    ClearBackground(BLANK);
    rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}
static void EndUI() {
    EndBlendMode();
    EndTextureMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTexturePro(uiRT.texture, Rectangle{ 0, 0, (float)SCREEN_W, -(float)SCREEN_H },
                   Rectangle{ uiOffX, uiOffY, SCREEN_W * uiScale, SCREEN_H * uiScale }, Vector2{ 0, 0 }, 0.0f, WHITE);
    EndBlendMode();
}

static void CaptureMouse(bool on) {
    if (on) { DisableCursor(); skipMouse = 3; }
    else EnableCursor();
}

// =====================================================================
//  СЛОЙ ВВОДА: ПК (клавиатура + мышь) / Android (сенсорный экран)
// =====================================================================
#ifdef PLATFORM_ANDROID
enum { TB_FIRE, TB_JUMP, TB_RELOAD, TB_SWAP, TB_SCOPE, TB_PLANT, TB_BUY, TB_PAUSE, TB_COUNT };
struct TBtn { float cx, cy, r; const char* label; };
struct TouchSlot { int id; int role; float lx, ly; };   // role: 0 игнор, 1 стик, 2 обзор, 10+i кнопка i
static TouchSlot tSlots[16];
static int  tSlotN = 0;
static bool tVis[TB_COUNT] = {};          // какие кнопки показаны (ставит основной цикл)
static bool tBtnDown[TB_COUNT] = {}, tBtnPressed[TB_COUNT] = {};
static bool tScopeOn = false;
static Vector2 tMove = { 0, 0 };          // x: вправо, y: вперёд, длина 0..1
static float tMoveMag = 0.0f;
static Vector2 tLook = { 0, 0 };          // сдвиг пальца обзора за кадр (пиксели экрана)
static bool tStickOn = false;
static Vector2 tStickO = { 0, 0 }, tStickK = { 0, 0 };
static Vector2 tPtr = { -1000, -1000 };   // указатель для меню (виртуальные координаты 1280x720)
static bool tPtrDown = false, tPtrPressed = false, tPtrPrev = false;

static float TUnit() { return GetScreenHeight() / 720.0f; }
static float TStickR() { return 95.0f * TUnit(); }
static void TouchLayout(TBtn* b) {
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight(), u = TUnit();
    b[TB_FIRE]   = { W - 175 * u, H - 165 * u, 92 * u, "FIRE" };
    b[TB_JUMP]   = { W - 345 * u, H - 95 * u,  54 * u, "JUMP" };
    b[TB_RELOAD] = { W - 345 * u, H - 235 * u, 48 * u, "R" };
    b[TB_SWAP]   = { W - 175 * u, H - 355 * u, 46 * u, "SWAP" };
    b[TB_SCOPE]  = { W - 320 * u, H - 365 * u, 46 * u, "SCOPE" };
    b[TB_PLANT]  = { W * 0.5f,    H - 105 * u, 62 * u, "PLANT" };
    b[TB_BUY]    = { 190 * u,     56 * u,      44 * u, "BUY" };
    b[TB_PAUSE]  = { 62 * u,      56 * u,      38 * u, "II" };
}
static void TouchUpdate(bool controls) {
    tLook = { 0, 0 };
    for (int i = 0; i < TB_COUNT; i++) tBtnPressed[i] = false;
    int n = std::min(GetTouchPointCount(), 10);
    // указатель для меню = первый палец
    tPtrPrev = tPtrDown;
    if (n > 0) {
        Vector2 p = GetTouchPosition(0);
        tPtr = { (p.x - uiOffX) / uiScale, (p.y - uiOffY) / uiScale };
        tPtrDown = true;
    } else { tPtr = { -1000, -1000 }; tPtrDown = false; }
    tPtrPressed = tPtrDown && !tPtrPrev;

    int ids[10]; Vector2 pos[10];
    for (int i = 0; i < n; i++) { ids[i] = GetTouchPointId(i); pos[i] = GetTouchPosition(i); }

    // убрать отпущенные пальцы
    int k = 0;
    for (int s = 0; s < tSlotN; s++) {
        bool alive = false;
        for (int i = 0; i < n; i++) if (ids[i] == tSlots[s].id) { alive = true; break; }
        if (alive) tSlots[k++] = tSlots[s];
    }
    tSlotN = k;

    TBtn btn[TB_COUNT]; TouchLayout(btn);
    float W = (float)GetScreenWidth();
    bool stickTaken = false;
    for (int s = 0; s < tSlotN; s++) if (tSlots[s].role == 1) stickTaken = true;

    // новые пальцы
    for (int i = 0; i < n && tSlotN < 16; i++) {
        bool known = false;
        for (int s = 0; s < tSlotN; s++) if (tSlots[s].id == ids[i]) { known = true; break; }
        if (known) continue;
        TouchSlot ns = { ids[i], 0, pos[i].x, pos[i].y };
        if (controls) {
            int hit = -1;
            for (int b = 0; b < TB_COUNT; b++) {
                if (!tVis[b]) continue;
                float dx = pos[i].x - btn[b].cx, dy = pos[i].y - btn[b].cy, rr = btn[b].r * 1.2f;
                if (dx * dx + dy * dy <= rr * rr) { hit = b; break; }
            }
            if (hit >= 0) { ns.role = 10 + hit; tBtnPressed[hit] = true; }
            else if (tVis[TB_FIRE]) {      // игрок жив: стик слева, обзор справа
                if (pos[i].x < W * 0.4f && !stickTaken) {
                    ns.role = 1; stickTaken = true; tStickOn = true; tStickO = pos[i]; tStickK = pos[i];
                } else if (pos[i].x >= W * 0.4f) ns.role = 2;
            }
        }
        tSlots[tSlotN++] = ns;   // role 0 = игнор до отпускания пальца
    }

    // обработка
    for (int i = 0; i < TB_COUNT; i++) tBtnDown[i] = false;
    tMove = { 0, 0 }; tMoveMag = 0.0f;
    bool stickSeen = false;
    for (int s = 0; s < tSlotN; s++) {
        TouchSlot& sl = tSlots[s];
        Vector2 p = { 0, 0 };
        for (int i = 0; i < n; i++) if (ids[i] == sl.id) p = pos[i];
        if (!controls) { if (sl.role != 0) sl.role = 0; continue; }
        if (sl.role == 1) {
            stickSeen = true;
            float R = TStickR();
            Vector2 d = { p.x - tStickO.x, p.y - tStickO.y };
            float len = sqrtf(d.x * d.x + d.y * d.y);
            float m = std::min(1.0f, len / R);
            if (len > R) { d.x *= R / len; d.y *= R / len; }
            tStickK = { tStickO.x + d.x, tStickO.y + d.y };
            if (m > 0.12f) {
                float mm = (m - 0.12f) / 0.88f;
                tMove.x = (len > 0.0f) ? (p.x - tStickO.x) / std::max(len, 1.0f) * mm : 0.0f;
                tMove.y = (len > 0.0f) ? -(p.y - tStickO.y) / std::max(len, 1.0f) * mm : 0.0f;
                tMoveMag = mm;
            }
        } else if (sl.role == 2) {
            tLook.x += p.x - sl.lx; tLook.y += p.y - sl.ly;
        } else if (sl.role >= 10) {
            tBtnDown[sl.role - 10] = true;
        }
        sl.lx = p.x; sl.ly = p.y;
    }
    if (!stickSeen) tStickOn = false;
    if (tBtnPressed[TB_SCOPE]) tScopeOn = !tScopeOn;
    if (!controls) tScopeOn = false;
}
static void DrawTouchControls() {
    TBtn btn[TB_COUNT]; TouchLayout(btn);
    float u = TUnit();
    // стик
    Vector2 o = tStickOn ? tStickO : Vector2{ 175 * u, GetScreenHeight() - 170 * u };
    Vector2 kn = tStickOn ? tStickK : o;
    DrawCircleV(o, TStickR(), Fade(WHITE, tStickOn ? 0.10f : 0.05f));
    DrawCircleLines((int)o.x, (int)o.y, TStickR(), Fade(WHITE, tStickOn ? 0.45f : 0.2f));
    DrawCircleV(kn, 38 * u, Fade(WHITE, tStickOn ? 0.40f : 0.15f));
    for (int i = 0; i < TB_COUNT; i++) {
        if (!tVis[i]) continue;
        bool on = tBtnDown[i] || (i == TB_SCOPE && tScopeOn);
        Color c = (i == TB_FIRE) ? Color{ 255, 90, 60, 255 } : Color{ 255, 255, 255, 255 };
        DrawCircleV(Vector2{ btn[i].cx, btn[i].cy }, btn[i].r, Fade(c, on ? 0.40f : 0.14f));
        DrawCircleLines((int)btn[i].cx, (int)btn[i].cy, btn[i].r, Fade(c, 0.65f));
        int fs = (int)((i == TB_FIRE ? 30 : 20) * u);
        int tw = MeasureText(btn[i].label, fs);
        DrawText(btn[i].label, (int)(btn[i].cx - tw / 2), (int)(btn[i].cy - fs / 2), fs, Fade(WHITE, 0.9f));
    }
}
static Vector2 UIPtr()     { return tPtr; }
static bool    UIDown()    { return tPtrDown; }
static bool    UIPressed() { return tPtrPressed; }
static bool IKeyDown(int k) { return k == KEY_E && tBtnDown[TB_PLANT]; }
static bool IKeyPressed(int k) {
    switch (k) {
        case KEY_R: return tBtnPressed[TB_RELOAD];
        case KEY_SPACE: return tBtnPressed[TB_JUMP];
        case KEY_Q: return tBtnPressed[TB_SWAP];
        case KEY_B: return tBtnPressed[TB_BUY];
        case KEY_ESCAPE: return tBtnPressed[TB_PAUSE] || IsKeyPressed(KEY_BACK);
    }
    return false;
}
static bool IMouseDown(int b)    { return b == MOUSE_LEFT_BUTTON ? tBtnDown[TB_FIRE] : (b == MOUSE_RIGHT_BUTTON ? tScopeOn : false); }
static bool IMousePressed(int b) { return b == MOUSE_LEFT_BUTTON && tBtnPressed[TB_FIRE]; }
static Vector2 ILook() { return Vector2{ tLook.x * 1.6f / uiScale, tLook.y * 1.6f / uiScale }; }
static bool IWalk() { return tMoveMag > 0.0f && tMoveMag < 0.55f; }
static void INoScope(bool hasScope) { if (!hasScope) tScopeOn = false; }
#else
static Vector2 UIPtr()     { return GetMousePosition(); }
static bool    UIDown()    { return IsMouseButtonDown(MOUSE_LEFT_BUTTON); }
static bool    UIPressed() { return IsMouseButtonPressed(MOUSE_LEFT_BUTTON); }
static bool IKeyDown(int k)        { return IsKeyDown(k); }
static bool IKeyPressed(int k)     { return IsKeyPressed(k); }
static bool IMouseDown(int b)      { return IsMouseButtonDown(b); }
static bool IMousePressed(int b)   { return IsMouseButtonPressed(b); }
static Vector2 ILook()             { return GetMouseDelta(); }
static bool IWalk()                { return IsKeyDown(KEY_LEFT_SHIFT); }
static void INoScope(bool)         {}
#endif

static void Feed(const string& s) {
    feed.push_back({ s, 6.0f });
    if (feed.size() > 6) feed.erase(feed.begin());
}
static Vector3 Eye(const Actor& a) { return Vector3{ a.pos.x, a.pos.y + 1.6f, a.pos.z }; }
static Vector3 Fwd(float yaw, float pitch) { return Vector3{ cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw) }; }
static float TurnToward(float cur, float want, float step) {
    float d = want - cur;
    while (d > PI) d -= 2 * PI;
    while (d < -PI) d += 2 * PI;
    if (fabsf(d) <= step) return want;
    return cur + (d > 0 ? step : -step);
}
static int SiteOf(Vector3 p) {
    if (Vector2Distance(Vector2{ p.x, p.z }, SITE_A) < SITE_R) return 0;
    if (Vector2Distance(Vector2{ p.x, p.z }, SITE_B) < SITE_R) return 1;
    return -1;
}

static void SpawnParticles(Vector3 pos, int n, Color c, float speed, float size, float life) {
    for (int i = 0; i < n; i++) {
        if (particles.size() > 500) break;
        Vector3 v = { RandR(-1, 1), RandR(0.2f, 1.4f), RandR(-1, 1) };
        v = Vector3Scale(Vector3Normalize(v), speed * RandR(0.4f, 1.0f));
        float l = life * RandR(0.6f, 1.0f);
        particles.push_back({ pos, v, l, l, size * RandR(0.6f, 1.2f), c });
    }
}

static void AddMoney(Actor& a, int amt) {
    int before = a.money;
    a.money = std::min(16000, a.money + amt);
    int got = a.money - before;
    if (&a == &A[0] && got > 0) popups.push_back({ got, 2.2f });
}

// =====================================================================
//  ФИЗИКА / КОЛЛИЗИИ
// =====================================================================
static bool BlockedAt(float x, float z, float y, float r) {
    for (const Wall& w : gWalls) {
        if (y >= w.h - 0.4f) continue;
        float cx = Clamp(x, w.x0, w.x1), cz = Clamp(z, w.z0, w.z1);
        float dx = x - cx, dz = z - cz;
        if (dx * dx + dz * dz < r * r) return true;
    }
    return false;
}
static void MoveActor(Actor& a, float dx, float dz) {
    const float r = 0.4f;
    if (!BlockedAt(a.pos.x + dx, a.pos.z, a.pos.y, r)) a.pos.x += dx;
    if (!BlockedAt(a.pos.x, a.pos.z + dz, a.pos.y, r)) a.pos.z += dz;
}
static float GroundAt(float x, float z, float y) {
    float g = 0.0f;
    for (const Wall& w : gWalls)
        if (x > w.x0 - 0.2f && x < w.x1 + 0.2f && z > w.z0 - 0.2f && z < w.z1 + 0.2f && w.h <= y + 0.45f) g = std::max(g, w.h);
    return g;
}
static void UpdateVertical(Actor& a, float dt) {
    a.vy -= 26.0f * dt;
    a.pos.y += a.vy * dt;
    float g = GroundAt(a.pos.x, a.pos.z, a.pos.y);
    if (a.pos.y <= g) { a.pos.y = g; a.vy = 0.0f; a.onGround = true; }
    else a.onGround = false;
}
static bool CanSee(Vector3 from, Vector3 to) {
    Vector3 d = Vector3Subtract(to, from);
    float L = Vector3Length(d);
    if (L < 0.01f) return true;
    d = Vector3Scale(d, 1.0f / L);
    Ray r = { from, d };
    for (const Wall& w : gWalls) {
        RayCollision c = GetRayCollisionBox(r, w.bb);
        if (c.hit && c.distance > 0.0f && c.distance < L - 0.3f) return false;
    }
    return true;
}

// =====================================================================
//  УРОН, ДЕНЬГИ, СТРЕЛЬБА (карта не разрушается)
// =====================================================================
static void Hurt(int victim, int attacker, int dmg, bool head, int wid, Vector3 hitPos) {
    Actor& v = A[victim];
    if (!v.alive) return;
    int hpDmg = dmg;
    if (v.armor > 0) {
        int ab = dmg / 2;
        if (ab > v.armor) { hpDmg = dmg - v.armor; v.armor = 0; }
        else { v.armor -= ab; hpDmg = dmg - ab; }
    }
    v.hp -= hpDmg;
    SpawnParticles(hitPos, 5, Col(150, 20, 20), 3.0f, 0.07f, 0.6f);
    if (victim == 0) { hurtT = 0.35f; PlayPool(S.hurt, 0.8f); }
    if (attacker == 0) {
        hitT = 0.25f;
        if (head) PlayPool(S.headshot, 0.7f); else PlayPool(S.hit, 0.6f);
    }
    if (v.hp <= 0) {
        v.hp = 0;
        v.alive = false;
        v.defusing = false;
        if (victim == 0) PlayPool(S.death, 0.9f); else PlayAt(S.death, v.pos, 0.9f, 1.0f, 70.0f);
        if (attacker >= 0) {
            A[attacker].kills++;
            AddMoney(A[attacker], WDEF[wid].reward);
            if (attacker == 0) PlayPool(S.cash, 0.7f);
            Feed(A[attacker].name + " [" + WDEF[wid].name + "] " + v.name + (head ? " (headshot)" : ""));
        } else {
            Feed(v.name + " died in the explosion");
        }
    }
}

static void FireShot(int shooter, Vector3 origin, Vector3 dir, float dmg, float range, int wid) {
    Ray ray = { origin, dir };
    float best = 1e9f;
    int hitActor = -1;
    bool head = false, wallHit = false;
    Vector3 hp = Vector3Add(origin, Vector3Scale(dir, 200.0f));
    Vector3 hn = { 0, 1, 0 };

    for (const Wall& w : gWalls) {
        RayCollision c = GetRayCollisionBox(ray, w.bb);
        if (c.hit && c.distance > 0.0f && c.distance < best) { best = c.distance; hp = c.point; hn = c.normal; wallHit = true; }
    }
    if (dir.y < -1e-4f) {
        float t = -origin.y / dir.y;
        if (t > 0.0f && t < best) { best = t; hp = Vector3Add(origin, Vector3Scale(dir, t)); hn = Vector3{ 0, 1, 0 }; wallHit = true; }
    }
    for (int i = 0; i < (int)A.size(); i++) {
        if (i == shooter || !A[i].alive || A[i].team == A[shooter].team) continue;
        const Vector3& p = A[i].pos;
        BoundingBox b = { Vector3{ p.x - 0.4f, p.y, p.z - 0.4f }, Vector3{ p.x + 0.4f, p.y + 1.8f, p.z + 0.4f } };
        RayCollision c = GetRayCollisionBox(ray, b);
        if (c.hit && c.distance > 0.0f && c.distance < best) {
            best = c.distance; hitActor = i; hp = c.point;
            head = (c.point.y - p.y) > 1.45f;
            wallHit = false;
        }
    }

    Vector3 muzzle = Vector3Add(origin, Vector3Scale(dir, 0.6f));
    muzzle.y -= 0.12f;
    tracers.push_back({ muzzle, hp, 0.06f });

    if (hitActor >= 0) {
        float dm = dmg;
        if (best > range) dm *= std::max(0.3f, 1.0f - (best - range) / range);
        int d = (int)(dm * (head ? 2.5f : 1.0f));
        if (d < 1) d = 1;
        Hurt(hitActor, shooter, d, head, wid, hp);
    } else if (wallHit) {
        decals.push_back({ Vector3Add(hp, Vector3Scale(hn, 0.02f)), 20.0f });
        if (decals.size() > 150) decals.erase(decals.begin());
        SpawnParticles(Vector3Add(hp, Vector3Scale(hn, 0.05f)), 4, Col(205, 180, 135), 2.2f, 0.06f, 0.7f);
        PlayAt(S.impact, hp, 0.4f, RandR(0.8f, 1.2f), 45.0f);
    }
}

// =====================================================================
//  БОМБА, ЭКОНОМИКА, РАУНД
// =====================================================================
static void PlantBomb(Vector3 p, int who) {
    bomb.planted = true;
    bomb.pos = p;
    bomb.timer = 40.0f;
    bomb.defuse = 0.0f;
    bomb.beepT = 0.0f;
    gBombNode = NearestNode(Vector2{ p.x, p.z });
    AddMoney(A[who], 300);
    int s = SiteOf(p);
    banner = string("BOMB HAS BEEN PLANTED - SITE ") + (s == 1 ? "B" : "A");
    bannerT = 3.0f;
    Feed(banner);
    PlayPool(S.planted, 0.9f);
}

static void EndRound(int winner, const string& msg, int winReward) {
    if (state == ROUNDEND) return;
    state = ROUNDEND;
    stTimer = 5.0f;
    if (winner == TERRO) scoreT++; else scoreCT++;
    endMsg = string(winner == TERRO ? "TERRORISTS WIN - " : "COUNTER-TERRORISTS WIN - ") + msg;
    for (int team = 0; team < 2; team++) {
        int reward;
        if (team == winner) {
            reward = winReward;
            lossStreak[team] = std::max(0, lossStreak[team] - 1);
        } else {
            reward = 1400 + 500 * std::min(lossStreak[team], 4);
            if (team == TERRO && bomb.planted) reward += 800;
            lossStreak[team] = std::min(lossStreak[team] + 1, 5);
        }
        for (Actor& a : A) if (a.team == team) AddMoney(a, reward);
    }
    PlayPool(winner == TERRO ? S.win : S.lose, 0.8f);
}

static void ResetAI(Actor& a) {
    a.path.clear(); a.pi = 0; a.goalNode = -1; a.think = 0; a.react = 0; a.strafe = 0; a.holdTimer = 0;
    a.stuckT = 0; a.strafeDir = 0; a.shots = 0; a.target = -1; a.defusing = false; a.plantProg = 0;
    a.flashT = 0; a.stepT = 0; a.anim = 0;
}

static void BotBuy(Actor& b) {
    if (b.pri < 0) {
        vector<int> aff;
        for (int w = 0; w < NUM_W; w++)
            if (WDEF[w].cat != CAT_PISTOL && w != W_M249 && WDEF[w].price <= b.money) aff.push_back(w);
        if (!aff.empty()) {
            int pick;
            if (Rand01() < 0.6f) {
                pick = aff[0];
                for (int w : aff) if (WDEF[w].price > WDEF[pick].price) pick = w;
            } else pick = aff[rand() % aff.size()];
            b.pri = pick;
            b.money -= WDEF[pick].price;
        } else if (b.sec == W_GLOCK && b.money >= 300 && Rand01() < 0.7f) {
            b.sec = W_P250;
            b.money -= 300;
        }
    }
    if (b.armor < 50 && b.money >= ARMOR_PRICE) { b.armor = 100; b.money -= ARMOR_PRICE; }
}

static bool BuyItem(Actor& p, int item) {
    if (item == NUM_W) {
        if (p.armor >= 100 || p.money < ARMOR_PRICE) { PlayPool(S.denied, 0.7f); return false; }
        p.money -= ARMOR_PRICE;
        p.armor = 100;
        PlayPool(S.buy, 0.8f);
        return true;
    }
    const WeaponDef& d = WDEF[item];
    bool owned = (d.cat == CAT_PISTOL) ? (p.sec == item) : (p.pri == item);
    if (owned || p.money < d.price) { PlayPool(S.denied, 0.7f); return false; }
    if (d.cat == CAT_PISTOL) p.sec = item; else p.pri = item;
    p.money -= d.price;
    p.mag[item] = d.mag;
    p.res[item] = d.reserve;
    p.cur = item;
    p.reload = 0.0f;
    drawAnim = 1.0f;
    PlayPool(S.buy, 0.8f);
    return true;
}

static void StartRound() {
    roundNo++;
    bomb = Bomb();
    tracers.clear(); decals.clear(); particles.clear();
    Vector3 tsp[4] = { {0,0,54}, {-4,0,54}, {4,0,54}, {0,0,57} };
    Vector3 csp[4] = { {-6,0,-54}, {6,0,-54}, {-3,0,-56}, {3,0,-56} };
    int plSite = rand() % 2;
    for (int i = 0; i < (int)A.size(); i++) {
        Actor& a = A[i];
        if (!a.alive) { a.pri = -1; a.sec = W_GLOCK; a.armor = 0; }   // погиб - потерял снаряжение
        a.alive = true;
        a.hp = 100;
        a.vy = 0; a.onGround = true; a.recoil = 0; a.pitch = 0;
        a.cd = RandR(0.0f, 0.3f);
        a.reload = 0; a.rph = 0;
        ResetAI(a);
        for (int w = 0; w < NUM_W; w++) { a.mag[w] = WDEF[w].mag; a.res[w] = WDEF[w].reserve; }
        if (i < 4) {
            a.pos = tsp[i]; a.yaw = PI;
            if (i > 0) {
                a.siteSel = (i == 1) ? plSite : rand() % 2;
                a.planter = (i == 1);
                a.startDelay = RandR(0.0f, 3.0f);
            }
        } else {
            a.pos = csp[i - 4]; a.yaw = 0;
            a.siteSel = (i < 6) ? 0 : 1;
            a.startDelay = RandR(0.0f, 1.0f);
        }
        if (i > 0) BotBuy(a);
        a.cur = (a.pri >= 0) ? a.pri : a.sec;
    }
    state = FREEZE;
    stTimer = 10.0f;
    roundTime = 115.0f;
    hurtT = hitT = 0.0f;
    scoped = false; planting = false;
    drawAnim = 1.0f;
    buyOpen = true;
    CaptureMouse(false);
}

static void NewMatch() {
    A.assign(8, Actor());
    scoreT = scoreCT = 0;
    roundNo = 0;
    lossStreak[0] = lossStreak[1] = 0;
    feed.clear(); popups.clear();
    for (int i = 0; i < 8; i++) {
        A[i].team = (i < 4) ? TERRO : CT;
        if (i == 0) A[i].name = "You";
        else if (i < 4) A[i].name = "T-Bot " + std::to_string(i);
        else A[i].name = "CT-Bot " + std::to_string(i - 3);
        A[i].money = 800;
    }
    StartRound();
}

static void CheckRoundEnd() {
    if (state != LIVE) return;
    int tA = 0, cA = 0;
    for (const Actor& a : A) if (a.alive) { if (a.team == TERRO) tA++; else cA++; }
    if (bomb.planted) {
        if (bomb.timer <= 0.0f) {
            bomb.exploded = true;
            bomb.explT = 0.0f;
            SpawnParticles(bomb.pos, 120, Col(255, 150, 40), 16.0f, 0.5f, 1.8f);
            SpawnParticles(bomb.pos, 80, Col(70, 60, 55), 9.0f, 0.7f, 2.5f);
            PlayPool(S.explode, 1.0f);
            for (int i = 0; i < (int)A.size(); i++)
                if (A[i].alive && Vector3Distance(A[i].pos, bomb.pos) < 22.0f) Hurt(i, -1, 999, false, 0, A[i].pos);
            EndRound(TERRO, "Bomb exploded", 3500);
        } else if (bomb.defuse >= 10.0f) {
            PlayPool(S.defused, 0.9f);
            EndRound(CT, "Bomb defused", 3500);
        } else if (cA == 0) {
            EndRound(TERRO, "Counter-Terrorists eliminated", 3250);
        }
    } else {
        if (cA == 0) EndRound(TERRO, "Counter-Terrorists eliminated", 3250);
        else if (tA == 0) EndRound(CT, "Terrorists eliminated", 3250);
        else if (roundTime <= 0.0f) EndRound(CT, "Time ran out", 3250);
    }
}

// =====================================================================
//  ИИ БОТОВ
// =====================================================================
static void SetGoal(Actor& b, int g) {
    b.goalNode = g;
    b.path = NodePath(NearestNode(Vector2{ b.pos.x, b.pos.z }), g);
    b.pi = 0;
    b.stuckT = 0.0f;
}
static bool StepToward(Actor& b, Vector2 tp, float speed, float dt) {
    float dx = tp.x - b.pos.x, dz = tp.y - b.pos.z;
    float L = sqrtf(dx * dx + dz * dz);
    if (L < 0.001f) return false;
    dx /= L; dz /= L;
    b.yaw = TurnToward(b.yaw, atan2f(dx, dz), dt * 10.0f);
    Vector3 before = b.pos;
    MoveActor(b, dx * speed * dt, dz * speed * dt);
    float moved = Vector2Distance(Vector2{ before.x, before.z }, Vector2{ b.pos.x, b.pos.z });
    if (moved < speed * dt * 0.3f) b.stuckT += dt; else b.stuckT = 0.0f;
    if (b.stuckT > 0.5f && b.onGround) { b.vy = 9.0f; b.stuckT = 0.0f; }
    return true;
}

static void UpdateBot(int idx, float dt) {
    Actor& b = A[idx];
    if (!b.alive) return;
    Vector3 startPos = b.pos;
    UpdateVertical(b, dt);
    b.flashT = std::max(0.0f, b.flashT - dt);
    if (state != LIVE) return;
    if (b.startDelay > 0.0f) { b.startDelay -= dt; return; }

    b.cd -= dt;
    b.think -= dt;

    if (b.think <= 0.0f) {
        b.think = 0.15f;
        int best = -1;
        float bd = 1e9f;
        Vector3 eye = Eye(b);
        for (int i = 0; i < (int)A.size(); i++) {
            const Actor& e = A[i];
            if (!e.alive || e.team == b.team) continue;
            float d = Vector3Distance(b.pos, e.pos);
            if (d > 70.0f || d >= bd) continue;
            float want = atan2f(e.pos.x - b.pos.x, e.pos.z - b.pos.z);
            float diff = want - b.yaw;
            while (diff > PI) diff -= 2 * PI;
            while (diff < -PI) diff += 2 * PI;
            if (fabsf(diff) > 0.95f && d > 12.0f) continue;
            if (!CanSee(eye, Vector3{ e.pos.x, e.pos.y + 1.2f, e.pos.z })) continue;
            best = i; bd = d;
        }
        if (best != b.target) {
            b.target = best;
            if (best >= 0) b.react = RandR(0.25f, 0.5f);
        }
    }

    b.defusing = false;

    if (b.target >= 0 && A[b.target].alive) {
        Actor& t = A[b.target];
        const WeaponDef& wd = WDEF[b.cur];
        float want = atan2f(t.pos.x - b.pos.x, t.pos.z - b.pos.z);
        b.yaw = TurnToward(b.yaw, want, dt * 9.0f);
        b.react -= dt;
        b.strafe -= dt;
        if (b.strafe <= 0.0f) { b.strafe = RandR(0.4f, 1.2f); b.strafeDir = (rand() % 3) - 1; }
        float sx = -cosf(b.yaw), sz = sinf(b.yaw);
        MoveActor(b, sx * b.strafeDir * 2.5f * dt, sz * b.strafeDir * 2.5f * dt);

        if (b.react <= 0.0f && b.cd <= 0.0f) {
            Vector3 eye = Eye(b);
            Vector3 aim = { t.pos.x, t.pos.y + ((rand() % 4 == 0) ? 1.65f : 1.1f), t.pos.z };
            Vector3 dir = Vector3Normalize(Vector3Subtract(aim, eye));
            float dist = Vector3Distance(eye, aim);
            float sp = 0.012f + dist * 0.0011f + wd.spread;
            Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, Vector3{ 0, 1, 0 }));
            Vector3 up = Vector3CrossProduct(right, dir);
            float scale = (wd.cat == CAT_SNIPER) ? 0.8f : 0.45f;
            for (int k = 0; k < wd.pellets; k++) {
                Vector3 d2 = Vector3Normalize(Vector3Add(dir, Vector3Add(Vector3Scale(right, RandR(-sp, sp)), Vector3Scale(up, RandR(-sp, sp)))));
                FireShot(idx, eye, d2, wd.dmg * scale, wd.range, b.cur);
            }
            PlayShot(b.cur, b.pos, false);
            b.flashT = 0.05f;
            b.cd = wd.autof ? wd.rate * 1.4f : std::max(wd.rate, 0.28f) + RandR(0.0f, 0.25f);
            b.shots++;
            if (wd.autof && b.shots >= 6) { b.shots = 0; b.cd = RandR(0.35f, 0.8f); }
        }
    } else {
        b.target = -1;
        bool handled = false;

        if (b.team == TERRO && b.planter && !bomb.planted && !A[0].alive && SiteOf(b.pos) >= 0 && b.pi >= b.path.size()) {
            b.plantProg += dt;
            if (b.plantProg >= 3.2f) PlantBomb(b.pos, idx);
            handled = true;
        } else b.plantProg = 0.0f;

        bool chaseBomb = (b.team == CT && bomb.planted);
        if (!handled) {
            if (chaseBomb) {
                int g = gBombNode;
                if (b.goalNode != g) SetGoal(b, g);
                if (Vector2Distance(Vector2{ b.pos.x, b.pos.z }, Vector2{ bomb.pos.x, bomb.pos.z }) < 2.2f) {
                    b.defusing = true;
                    handled = true;
                }
            } else if (b.goalNode < 0 || (b.pi >= b.path.size() && (b.holdTimer -= dt) <= 0.0f)) {
                int g;
                if (b.team == TERRO && b.goalNode < 0) g = (b.siteSel == 0) ? NODE_A : NODE_B;
                else g = (b.siteSel == 0) ? A_GUARD[rand() % 5] : B_GUARD[rand() % 5];
                SetGoal(b, g);
                b.holdTimer = RandR(3.0f, 7.0f);
            }
        }
        if (!handled) {
            if (b.pi < b.path.size()) {
                Vector2 tp = gNodes[b.path[b.pi]];
                if (Vector2Distance(Vector2{ b.pos.x, b.pos.z }, tp) < 1.2f) b.pi++;
                else StepToward(b, tp, 5.2f, dt);
            } else if (chaseBomb) {
                StepToward(b, Vector2{ bomb.pos.x, bomb.pos.z }, 4.0f, dt);
            }
        }
    }

    // анимация шагов и звук шагов
    float moved = Vector2Distance(Vector2{ startPos.x, startPos.z }, Vector2{ b.pos.x, b.pos.z });
    b.anim += moved * 2.4f;
    b.stepT += moved;
    if (b.stepT > 2.2f && b.onGround) {
        b.stepT = 0.0f;
        PlayAt(S.step, b.pos, 0.5f, RandR(0.85f, 1.1f), 30.0f);
    }
}

// =====================================================================
//  ИГРОК
// =====================================================================
static void SwitchWeapon(Actor& p, int w) {
    if (p.cur == w || w < 0) return;
    p.cur = w;
    p.reload = 0.0f;
    p.cd = 0.4f;
    drawAnim = 1.0f;
    PlayPool(S.switchW, 0.6f);
}

static void UpdatePlayer(float dt) {
    Actor& p = A[0];
    if (!p.alive) { scoped = false; planting = false; return; }

    if (!buyOpen) {
        if (skipMouse > 0) skipMouse--;
        else {
            Vector2 md = ILook();
            float sens = scoped ? 0.0008f : 0.0022f;
            md.x *= uiScale; md.y *= uiScale;   // вернуть реальные пиксели мыши
            p.yaw -= md.x * sens;
            p.pitch = Clamp(p.pitch - md.y * sens, -1.5f, 1.5f);
        }
    }
    p.recoil *= expf(-dt * 5.0f);
    gunKick = std::max(0.0f, gunKick - dt * 12.0f);
    drawAnim = std::max(0.0f, drawAnim - dt * 3.0f);

    // выбор оружия
    float wheel = GetMouseWheelMove();
    if (IKeyPressed(KEY_ONE) && p.pri >= 0) SwitchWeapon(p, p.pri);
    if (IKeyPressed(KEY_TWO)) SwitchWeapon(p, p.sec);
    if (IKeyPressed(KEY_Q) || wheel != 0.0f) {
        int other = (p.cur == p.sec && p.pri >= 0) ? p.pri : p.sec;
        SwitchWeapon(p, other);
    }
    int w = p.cur;
    const WeaponDef& wd = WDEF[w];
    INoScope(wd.scope);
    scoped = (wd.scope && !buyOpen && IMouseDown(MOUSE_RIGHT_BUTTON) && p.reload <= 0.0f);

    // установка бомбы
    planting = false;
    if (state == LIVE && !bomb.planted && SiteOf(p.pos) >= 0 && p.onGround && IKeyDown(KEY_E)) {
        planting = true;
        float before = p.plantProg;
        p.plantProg += dt;
        if ((int)(before / 0.4f) != (int)(p.plantProg / 0.4f)) PlayPool(S.beep, 0.5f, 0.5f, 1.3f);
        if (p.plantProg >= 3.2f) { PlantBomb(p.pos, 0); planting = false; p.plantProg = 0.0f; }
    } else p.plantProg = 0.0f;

    // движение
    float speedXZ = 0.0f;
    bool wasAir = !p.onGround;
    float vyBefore = p.vy;
    if (state != FREEZE && !planting && !buyOpen) {
        Vector3 f = { sinf(p.yaw), 0, cosf(p.yaw) };
        Vector3 r = { -cosf(p.yaw), 0, sinf(p.yaw) };
        Vector3 wish = { 0, 0, 0 };
        if (IKeyDown(KEY_W)) wish = Vector3Add(wish, f);
        if (IKeyDown(KEY_S)) wish = Vector3Subtract(wish, f);
        if (IKeyDown(KEY_D)) wish = Vector3Add(wish, r);
        if (IKeyDown(KEY_A)) wish = Vector3Subtract(wish, r);
#ifdef PLATFORM_ANDROID
        wish = Vector3Add(Vector3Scale(f, tMove.y), Vector3Scale(r, tMove.x));
#endif
        if (Vector3Length(wish) > 0.01f) {
            wish = Vector3Normalize(wish);
            bool walk = IWalk();
            float spd = (walk ? 3.2f : 6.2f) * wd.speed;
            if (scoped) spd *= 0.5f;
            speedXZ = spd;
            MoveActor(p, wish.x * spd * dt, wish.z * spd * dt);
            if (p.onGround && !walk) {
                p.stepT += dt;
                if (p.stepT > 0.38f) { p.stepT = 0.0f; PlayPool(S.step, 0.3f, 0.5f, RandR(0.9f, 1.1f)); }
            }
        }
        if (IKeyPressed(KEY_SPACE) && p.onGround) p.vy = 9.0f;
    }
    UpdateVertical(p, dt);
    if (wasAir && p.onGround && vyBefore < -5.0f) PlayPool(S.step, 0.55f, 0.5f, 0.7f);
    bobAmt = Lerp(bobAmt, (speedXZ > 0.1f && p.onGround) ? Clamp(speedXZ / 6.2f, 0.0f, 1.0f) : 0.0f, dt * 8.0f);
    bobT += dt * speedXZ * 1.9f;

    // перезарядка (с анимацией и звуками по фазам)
    if (p.reload > 0.0f) {
        float oldPh = p.rph;
        p.reload -= dt;
        p.rph = Clamp(1.0f - p.reload / wd.reloadT, 0.0f, 1.0f);
        if (oldPh < 0.2f && p.rph >= 0.2f) PlayPool(S.magOut, 0.6f);
        if (oldPh < 0.58f && p.rph >= 0.58f) PlayPool(S.magIn, 0.7f);
        if (oldPh < 0.82f && p.rph >= 0.82f) PlayPool(S.bolt, 0.6f);
        if (p.reload <= 0.0f) {
            int need = wd.mag - p.mag[w];
            int take = std::min(need, p.res[w]);
            p.mag[w] += take;
            p.res[w] -= take;
            p.reload = 0.0f;
            p.rph = 0.0f;
        }
    } else if (state != FREEZE && !buyOpen && (IKeyPressed(KEY_R) || p.mag[w] == 0) && p.mag[w] < wd.mag && p.res[w] > 0) {
        p.reload = wd.reloadT;
        p.rph = 0.0f;
    }

    // стрельба
    p.cd -= dt;
    muzzleT -= dt;
    bool want = wd.autof ? IMouseDown(MOUSE_LEFT_BUTTON) : IMousePressed(MOUSE_LEFT_BUTTON);
    if (state == LIVE && !buyOpen && want && p.cd <= 0.0f && p.reload <= 0.0f && !planting) {
        if (p.mag[w] > 0) {
            p.mag[w]--;
            p.cd = wd.rate;
            float sp = wd.spread + speedXZ * 0.0008f + (p.onGround ? 0.0f : 0.03f);
            if (wd.scope && !scoped) sp += 0.06f;
            Vector3 f = Fwd(p.yaw, p.pitch + p.recoil);
            Vector3 right = Vector3Normalize(Vector3CrossProduct(f, Vector3{ 0, 1, 0 }));
            Vector3 up = Vector3CrossProduct(right, f);
            for (int k = 0; k < wd.pellets; k++) {
                Vector3 dir = Vector3Normalize(Vector3Add(f, Vector3Add(Vector3Scale(right, RandR(-sp, sp)), Vector3Scale(up, RandR(-sp, sp)))));
                FireShot(0, Eye(p), dir, (float)wd.dmg, wd.range, w);
            }
            PlayShot(w, p.pos, true);
            p.recoil += wd.recoil;
            p.yaw += RandR(-0.004f, 0.004f);
            muzzleT = 0.05f;
            gunKick = 1.0f;
        } else {
            p.cd = 0.3f;
            PlayPool(S.empty, 0.6f);
        }
    }
}

// =====================================================================
//  ОБНОВЛЕНИЕ ИГРЫ
// =====================================================================
static void UpdateGame(float dt) {
    hurtT = std::max(0.0f, hurtT - dt);
    hitT = std::max(0.0f, hitT - dt);
    bannerT = std::max(0.0f, bannerT - dt);

    int prevSec = (int)ceilf(stTimer);
    stTimer -= dt;
    if (state == FREEZE) {
        int sec = (int)ceilf(stTimer);
        if (sec != prevSec && sec <= 3 && sec > 0) PlayPool(S.uiTick, 0.6f);
        if (stTimer <= 0.0f) {
            state = LIVE;
            PlayPool(S.roundStart, 0.8f);
            if (buyOpen) { buyOpen = false; CaptureMouse(true); }
        }
    }
    if (state == ROUNDEND && stTimer <= 0.0f) StartRound();

    if (state == LIVE) {
        if (bomb.planted) bomb.timer -= dt; else roundTime -= dt;
    }

    UpdatePlayer(dt);
    for (int i = 1; i < (int)A.size(); i++) UpdateBot(i, dt);

    if (state == LIVE && bomb.planted) {
        bool any = false;
        for (const Actor& a : A) if (a.alive && a.defusing) any = true;
        if (any) bomb.defuse += dt; else bomb.defuse = 0.0f;
        // писк бомбы
        bomb.beepT -= dt;
        if (bomb.beepT <= 0.0f && bomb.timer > 0.0f) {
            float k = Clamp(bomb.timer / 40.0f, 0.0f, 1.0f);
            bomb.beepT = 0.12f + 0.9f * k * k;
            bomb.blinkT = 0.12f;
            PlayAt(S.beep, bomb.pos, 0.9f, 1.0f, 200.0f);
        }
    }
    bomb.blinkT = std::max(0.0f, bomb.blinkT - dt);
    if (bomb.exploded) bomb.explT += dt;

    CheckRoundEnd();

    for (auto& t : tracers) t.t -= dt;
    tracers.erase(std::remove_if(tracers.begin(), tracers.end(), [](const Tracer& t) { return t.t <= 0.0f; }), tracers.end());
    for (auto& d : decals) d.t -= dt;
    decals.erase(std::remove_if(decals.begin(), decals.end(), [](const Decal& d) { return d.t <= 0.0f; }), decals.end());
    for (auto& f : feed) f.t -= dt;
    feed.erase(std::remove_if(feed.begin(), feed.end(), [](const FeedItem& f) { return f.t <= 0.0f; }), feed.end());
    for (auto& po : popups) po.t -= dt;
    popups.erase(std::remove_if(popups.begin(), popups.end(), [](const Popup& po) { return po.t <= 0.0f; }), popups.end());
    for (auto& pa : particles) {
        pa.t -= dt;
        pa.v.y -= 9.0f * dt;
        pa.p = Vector3Add(pa.p, Vector3Scale(pa.v, dt));
        if (pa.p.y < 0.03f) { pa.p.y = 0.03f; pa.v = Vector3Scale(pa.v, 0.2f); }
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& pa) { return pa.t <= 0.0f; }), particles.end());
}

// =====================================================================
//  МЕНЮ ЗАКУПКИ
// =====================================================================
static const int BUY_COLS[6][3] = {
    { W_GLOCK, W_P250, W_DEAGLE }, { W_MAC10, W_MP5, W_P90 }, { W_AK47, W_M4A4, W_GALIL },
    { W_SCOUT, W_AWP, -1 }, { W_NOVA, W_XM1014, W_M249 }, { NUM_W, -1, -1 }
};
static const char* BUY_TITLES[6] = { "PISTOLS", "SMG", "RIFLES", "SNIPERS", "HEAVY", "EQUIPMENT" };
static const Rectangle BUY_PANEL = { 130, 50, 1020, 620 };

static Rectangle CardRect(int col, int row) {
    return Rectangle{ BUY_PANEL.x + 20 + col * 118.0f, BUY_PANEL.y + 100 + row * 80.0f, 110, 72 };
}
static const char* ItemName(int id) { return id == NUM_W ? "Kevlar+Helm" : WDEF[id].name; }
static int ItemPrice(int id) { return id == NUM_W ? ARMOR_PRICE : WDEF[id].price; }
static bool ItemOwned(const Actor& p, int id) {
    if (id == NUM_W) return p.armor >= 100;
    return (WDEF[id].cat == CAT_PISTOL) ? (p.sec == id) : (p.pri == id);
}

static void UpdateBuyMenu() {
    hoverItem = -1;
    Vector2 m = UIPtr();
    for (int c = 0; c < 6; c++)
        for (int r = 0; r < 3; r++) {
            int id = BUY_COLS[c][r];
            if (id < 0) continue;
            if (CheckCollisionPointRec(m, CardRect(c, r))) {
                hoverItem = id;
                if (UIPressed() && A[0].alive) BuyItem(A[0], id);
            }
        }
}

static void RenderPreview(int w) {
    BeginTextureMode(previewRT);
    ClearBackground(Color{ 20, 22, 30, 255 });
    float zc = (gZmin[w] + gZmax[w]) * 0.5f;
    float len = gZmax[w] - gZmin[w];
    float dist = len * 0.9f + 0.15f;
    Camera3D c = { 0 };
    c.position = Vector3{ -dist, 0.05f, zc };
    c.target = Vector3{ 0.0f, 0.0f, zc };
    c.up = Vector3{ 0, 1, 0 };
    c.fovy = 38.0f;
    c.projection = CAMERA_PERSPECTIVE;
    BeginMode3D(c);
    rlPushMatrix();
    rlTranslatef(0, 0, zc);
    rlRotatef(sinf((float)GetTime() * 0.8f) * 25.0f, 0, 1, 0);
    rlTranslatef(0, 0, -zc);
    DrawParts(w, 0.0f, true);
    rlPopMatrix();
    EndMode3D();
    EndTextureMode();
}

#ifdef PLATFORM_ANDROID
#define BUY_CLOSE_FMT "Time left: %d s"
#else
#define BUY_CLOSE_FMT "Time left: %d s    [B] / [ESC] close"
#endif
static void DrawBuyMenu() {
    const Actor& p = A[0];
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.45f));
    DrawRectangleRec(BUY_PANEL, Color{ 24, 26, 34, 240 });
    DrawRectangleLinesEx(BUY_PANEL, 2, Color{ 230, 160, 50, 255 });
    DrawText("BUY MENU", (int)BUY_PANEL.x + 20, (int)BUY_PANEL.y + 16, 36, Color{ 230, 160, 50, 255 });
    DrawText(TextFormat("$ %d", p.money), (int)BUY_PANEL.x + 260, (int)BUY_PANEL.y + 16, 36, Color{ 120, 230, 120, 255 });
    DrawText(TextFormat(BUY_CLOSE_FMT, (int)ceilf(stTimer)), (int)BUY_PANEL.x + 520, (int)BUY_PANEL.y + 26, 20, LIGHTGRAY);

    for (int c = 0; c < 6; c++) {
        DrawText(BUY_TITLES[c], (int)CardRect(c, 0).x, (int)BUY_PANEL.y + 74, 16, GRAY);
        for (int r = 0; r < 3; r++) {
            int id = BUY_COLS[c][r];
            if (id < 0) continue;
            Rectangle rc = CardRect(c, r);
            bool hov = (id == hoverItem);
            bool owned = ItemOwned(p, id);
            bool afford = p.money >= ItemPrice(id);
            Color bg = owned ? Color{ 36, 90, 52, 255 } : (hov ? Color{ 70, 60, 40, 255 } : Color{ 38, 40, 50, 255 });
            DrawRectangleRec(rc, bg);
            DrawRectangleLinesEx(rc, hov ? 2.0f : 1.0f, hov ? Color{ 230, 160, 50, 255 } : Color{ 90, 92, 104, 255 });
            DrawText(ItemName(id), (int)rc.x + 8, (int)rc.y + 12, 16, afford || owned ? WHITE : Color{ 150, 150, 150, 255 });
            DrawText(owned ? "OWNED" : TextFormat("$%d", ItemPrice(id)), (int)rc.x + 8, (int)rc.y + 42, 20,
                     owned ? Color{ 130, 230, 150, 255 } : (afford ? Color{ 120, 230, 120, 255 } : Color{ 220, 80, 80, 255 }));
        }
    }

    // панель описания
    float ix = BUY_PANEL.x + 20 + 6 * 118.0f + 10, iw = BUY_PANEL.x + BUY_PANEL.width - 20 - ix;
    DrawRectangle((int)ix, (int)BUY_PANEL.y + 74, (int)iw, 150, Color{ 20, 22, 30, 255 });
    if (hoverItem >= 0 && hoverItem < NUM_W) {
        DrawTextureRec(previewRT.texture, Rectangle{ 0, 0, (float)previewRT.texture.width, -(float)previewRT.texture.height },
                       Vector2{ ix, BUY_PANEL.y + 74 }, WHITE);
        const WeaponDef& d = WDEF[hoverItem];
        int y = (int)BUY_PANEL.y + 236;
        DrawText(d.name, (int)ix, y, 26, Color{ 230, 160, 50, 255 });
        if (d.pellets > 1) DrawText(TextFormat("Damage: %d x %d pellets", d.dmg, d.pellets), (int)ix, y + 34, 18, WHITE);
        else DrawText(TextFormat("Damage: %d", d.dmg), (int)ix, y + 34, 18, WHITE);
        DrawText(TextFormat("Fire rate: %.0f / min", 60.0f / d.rate), (int)ix, y + 58, 18, WHITE);
        DrawText(TextFormat("Magazine: %d / %d", d.mag, d.reserve), (int)ix, y + 82, 18, WHITE);
        DrawText(TextFormat("Reload: %.1f s", d.reloadT), (int)ix, y + 106, 18, WHITE);
        DrawText(TextFormat("Kill reward: $%d", d.reward), (int)ix, y + 130, 18, Color{ 120, 230, 120, 255 });
        DrawText(d.autof ? "Mode: full auto" : "Mode: semi auto", (int)ix, y + 154, 18, LIGHTGRAY);
        if (d.scope) DrawText("Right mouse: scope", (int)ix, y + 178, 18, LIGHTGRAY);
    } else if (hoverItem == NUM_W) {
        DrawText("Kevlar + Helmet", (int)ix, (int)BUY_PANEL.y + 236, 24, Color{ 230, 160, 50, 255 });
        DrawText("Absorbs half of the damage", (int)ix, (int)BUY_PANEL.y + 272, 18, WHITE);
        DrawText(TextFormat("Armor: %d / 100", p.armor), (int)ix, (int)BUY_PANEL.y + 298, 18, LIGHTGRAY);
    } else {
        DrawText("Hover an item", (int)ix + 70, (int)BUY_PANEL.y + 140, 20, GRAY);
        DrawText("Click to buy", (int)ix + 80, (int)BUY_PANEL.y + 166, 20, GRAY);
    }

    // инвентарь
    DrawText("YOUR LOADOUT", (int)BUY_PANEL.x + 20, (int)BUY_PANEL.y + 360, 16, GRAY);
    DrawText(TextFormat("Primary: %s", p.pri >= 0 ? WDEF[p.pri].name : "-"), (int)BUY_PANEL.x + 20, (int)BUY_PANEL.y + 384, 24, WHITE);
    DrawText(TextFormat("Secondary: %s", WDEF[p.sec].name), (int)BUY_PANEL.x + 20, (int)BUY_PANEL.y + 414, 24, WHITE);
    DrawText(TextFormat("Armor: %d", p.armor), (int)BUY_PANEL.x + 20, (int)BUY_PANEL.y + 444, 24, WHITE);
    DrawText("Kill rewards: pistol/rifle $300, SMG $600, shotgun $900, AWP $100", (int)BUY_PANEL.x + 20, (int)BUY_PANEL.y + 500, 18, LIGHTGRAY);
    DrawText("Round win: $3250   Loss: $1400 + streak bonus   Bomb plant: +$300", (int)BUY_PANEL.x + 20, (int)BUY_PANEL.y + 526, 18, LIGHTGRAY);
    DrawText("Round starts when the timer reaches zero. Movement is frozen during buy time.", (int)BUY_PANEL.x + 20, (int)BUY_PANEL.y + 560, 18, GRAY);
}

// =====================================================================
//  ОТРИСОВКА МИРА
// =====================================================================
static void DrawActor(const Actor& a) {
    bool isT = (a.team == TERRO);
    Color cloth = isT ? Col(96, 84, 56) : Col(52, 72, 112);
    Color pants = isT ? Col(72, 62, 44) : Col(42, 52, 78);
    Color vest = isT ? Col(60, 52, 36) : Col(34, 38, 48);
    Color headc = isT ? Col(205, 196, 165) : Col(70, 82, 70);
    Color skin = Col(220, 175, 140);
    rlPushMatrix();
    if (a.alive) {
        rlTranslatef(a.pos.x, a.pos.y, a.pos.z);
        rlRotatef(a.yaw * RAD2DEG, 0, 1, 0);
    } else {
        rlTranslatef(a.pos.x, a.pos.y + 0.25f, a.pos.z);
        rlRotatef(a.yaw * RAD2DEG, 0, 1, 0);
        rlRotatef(-90, 1, 0, 0);
    }
    float sw = a.alive ? sinf(a.anim) * 0.6f : 0.0f;
    for (int s = -1; s <= 1; s += 2) {
        rlPushMatrix();
        rlTranslatef(0.13f * s, 0.8f, 0);
        rlRotatef(sw * RAD2DEG * s, 1, 0, 0);
        DrawTexBox(-0.09f, -0.8f, -0.10f, 0.09f, 0.0f, 0.10f, NOTEX, 1, pants, true);
        DrawTexBox(-0.10f, -0.8f, -0.11f, 0.10f, -0.66f, 0.15f, NOTEX, 1, Col(30, 28, 26), true);
        rlPopMatrix();
    }
    DrawTexBox(-0.26f, 0.80f, -0.15f, 0.26f, 1.40f, 0.15f, NOTEX, 1, cloth, true);
    DrawTexBox(-0.28f, 0.95f, -0.17f, 0.28f, 1.38f, 0.17f, NOTEX, 1, vest, true);
    DrawTexBox(-0.27f, 0.80f, -0.16f, 0.27f, 0.88f, 0.16f, NOTEX, 1, Col(40, 32, 22), true);
    // руки
    DrawTexBox(-0.40f, 1.12f, -0.05f, -0.28f, 1.36f, 0.12f, NOTEX, 1, cloth, true);
    DrawTexBox(-0.36f, 1.08f, 0.10f, -0.22f, 1.20f, 0.42f, NOTEX, 1, skin, true);
    DrawTexBox(0.28f, 1.12f, -0.05f, 0.40f, 1.36f, 0.12f, NOTEX, 1, cloth, true);
    DrawTexBox(-0.10f, 1.08f, 0.22f, 0.30f, 1.20f, 0.50f, NOTEX, 1, cloth, true);
    // голова
    DrawSphereEx(Vector3{ 0.0f, 1.58f, 0.0f }, 0.17f, 8, 8, skin);
    if (isT) {
        DrawTexBox(-0.19f, 1.62f, -0.19f, 0.19f, 1.76f, 0.19f, NOTEX, 1, headc, true);
        DrawTexBox(-0.18f, 1.45f, -0.04f, 0.18f, 1.56f, 0.19f, NOTEX, 1, headc, true);
    } else {
        DrawTexBox(-0.20f, 1.60f, -0.20f, 0.20f, 1.78f, 0.20f, NOTEX, 1, headc, true);
        DrawTexBox(-0.15f, 1.56f, 0.12f, 0.15f, 1.63f, 0.20f, NOTEX, 1, Col(20, 24, 30), true);
    }
    if (a.alive) {
        rlPushMatrix();
        rlTranslatef(-0.16f, 1.12f, 0.20f);
        rlScalef(0.9f, 0.9f, 0.9f);
        DrawParts(a.cur, 0.0f, true);
        if (a.flashT > 0.0f) DrawTexBox(-0.04f, -0.04f, gZmax[a.cur], 0.04f, 0.04f, gZmax[a.cur] + 0.18f, NOTEX, 1, Col(255, 220, 90), true);
        rlPopMatrix();
    }
    rlPopMatrix();
}

static void DrawWorld() {
    // земля за пределами карты и пол
    DrawCube(Vector3{ 0, -0.55f, 0 }, 700, 1, 700, Col(196, 166, 114));
    DrawTexBox(-50, -1.0f, -60, 50, 0.0f, 60, texFloor, 4.5f, WHITE);

    // мягкие тени у оснований
    rlSetTexture(rlGetTextureIdDefault());
    for (const Wall& w : gWalls) {
        if (w.kind == K_BARREL) continue;
        DrawPlane(Vector3{ (w.x0 + w.x1) * 0.5f, 0.03f, (w.z0 + w.z1) * 0.5f },
                  Vector2{ (w.x1 - w.x0) + 1.2f, (w.z1 - w.z0) + 1.2f }, Color{ 0, 0, 0, 38 });
    }

    // зоны плентов
    DrawCircle3D(Vector3{ SITE_A.x, 0.06f, SITE_A.y }, SITE_R, Vector3{ 1, 0, 0 }, 90.0f, ORANGE);
    DrawCircle3D(Vector3{ SITE_B.x, 0.06f, SITE_B.y }, SITE_R, Vector3{ 1, 0, 0 }, 90.0f, ORANGE);
    DrawCircle3D(Vector3{ SITE_A.x, 0.07f, SITE_A.y }, SITE_R - 0.3f, Vector3{ 1, 0, 0 }, 90.0f, Fade(ORANGE, 0.5f));
    DrawCircle3D(Vector3{ SITE_B.x, 0.07f, SITE_B.y }, SITE_R - 0.3f, Vector3{ 1, 0, 0 }, 90.0f, Fade(ORANGE, 0.5f));

    // стены, ящики, бочки
    for (const Wall& w : gWalls) {
        Color t = Mul(WHITE, w.shade);
        if (w.kind == K_WALL) {
            DrawTexBox(w.x0 - 0.07f, 0.0f, w.z0 - 0.07f, w.x1 + 0.07f, 1.1f, w.z1 + 0.07f, texBrick, 2.0f, t);
            DrawTexBox(w.x0, 1.1f, w.z0, w.x1, w.h, w.z1, texPlaster, 4.0f, t);
            DrawTexBox(w.x0 - 0.12f, w.h, w.z0 - 0.12f, w.x1 + 0.12f, w.h + 0.22f, w.z1 + 0.12f, texBrick, 2.0f, Mul(t, 0.9f));
        } else if (w.kind == K_STONE) {
            DrawTexBox(w.x0, 0.0f, w.z0, w.x1, w.h, w.z1, texBrick, 2.0f, t);
            DrawTexBox(w.x0 - 0.08f, w.h, w.z0 - 0.08f, w.x1 + 0.08f, w.h + 0.14f, w.z1 + 0.08f, texBrick, 2.0f, Mul(t, 0.85f));
        } else if (w.kind == K_CRATE) {
            DrawTexBox(w.x0, 0.0f, w.z0, w.x1, w.h, w.z1, texCrate, std::min(2.5f, std::max(1.2f, w.h)), t);
        } else {
            float cx = (w.x0 + w.x1) * 0.5f, cz = (w.z0 + w.z1) * 0.5f, r = (w.x1 - w.x0) * 0.5f * 0.95f;
            DrawCylinder(Vector3{ cx, 0.0f, cz }, r, r, w.h, 14, Col(150, 72, 44));
            DrawCylinder(Vector3{ cx, 0.18f, cz }, r * 1.04f, r * 1.04f, 0.08f, 14, Col(60, 56, 54));
            DrawCylinder(Vector3{ cx, w.h - 0.28f, cz }, r * 1.04f, r * 1.04f, 0.08f, 14, Col(60, 56, 54));
            DrawCylinder(Vector3{ cx, w.h, cz }, r * 0.9f, r * 0.9f, 0.03f, 14, Col(70, 64, 60));
        }
    }

    // декор без коллизий
    for (const Deco& d : gDecos) {
        switch (d.kind) {
            case D_BRICK: DrawTexBox(d.x0, d.y0, d.z0, d.x1, d.y1, d.z1, texBrick, 2.0f, d.a, true); break;
            case D_WOOD: DrawTexBox(d.x0, d.y0, d.z0, d.x1, d.y1, d.z1, texCrate, 1.5f, d.a, true); break;
            case D_AWNING: {
                int n = (int)(d.x1 - d.x0);
                for (int i = 0; i < n; i++)
                    DrawTexBox(d.x0 + i, d.y0, d.z0, d.x0 + i + 1.0f, d.y1, d.z1, NOTEX, 1, (i & 1) ? d.b : d.a, true);
                break;
            }
            case D_BUILD: DrawTexBox(d.x0, d.y0, d.z0, d.x1, d.y1, d.z1, texPlaster, 8.0f, d.a); break;
            case D_DOME: DrawSphereEx(Vector3{ d.x0, d.y0, d.z0 }, d.x1, 12, 12, d.a); break;
        }
    }

    for (const Decal& d : decals) DrawSphereEx(d.p, 0.07f, 4, 4, Color{ 40, 30, 20, 255 });

    // бомба (C4)
    if (bomb.planted) {
        Vector3 p = bomb.pos;
        DrawTexBox(p.x - 0.25f, p.y, p.z - 0.15f, p.x + 0.25f, p.y + 0.12f, p.z + 0.15f, NOTEX, 1, Col(66, 70, 54), true);
        DrawTexBox(p.x - 0.12f, p.y + 0.12f, p.z - 0.08f, p.x + 0.12f, p.y + 0.14f, p.z + 0.08f, NOTEX, 1, Col(20, 60, 25), true);
        DrawTexBox(p.x - 0.22f, p.y + 0.12f, p.z + 0.10f, p.x - 0.18f, p.y + 0.15f, p.z + 0.14f, NOTEX, 1, Col(180, 40, 40), true);
        if (bomb.blinkT > 0.0f) {
            DrawSphereEx(Vector3{ p.x + 0.18f, p.y + 0.16f, p.z }, 0.04f, 6, 6, RED);
            DrawCircle3D(Vector3{ p.x, p.y + 0.05f, p.z }, 0.9f, Vector3{ 1, 0, 0 }, 90.0f, Fade(RED, 0.6f));
        }
    }
    if (bomb.exploded) {
        float r = std::min(bomb.explT * 25.0f, 22.0f);
        DrawSphere(bomb.pos, r, Fade(ORANGE, std::max(0.0f, 0.6f - bomb.explT * 0.2f)));
    }

    // игроки (себя не рисуем)
    int viewed = A[0].alive ? 0 : -1;
    for (int i = 0; i < (int)A.size(); i++) {
        if (i == viewed) continue;
        DrawActor(A[i]);
    }

    for (const Particle& pa : particles) {
        float k = Clamp(pa.t / pa.maxT, 0.0f, 1.0f);
        Color c = pa.c;
        c.a = (unsigned char)(255 * k);
        DrawCube(pa.p, pa.size, pa.size, pa.size, c);
    }
    for (const Tracer& t : tracers) DrawLine3D(t.a, t.b, Color{ 255, 230, 120, 255 });
}

static float MagDrop(float ph) {
    if (ph < 0.2f) return 0.0f;
    if (ph < 0.4f) return -(ph - 0.2f) / 0.2f * 0.35f;
    if (ph < 0.55f) return -0.35f;
    if (ph < 0.8f) return -0.35f * (1.0f - (ph - 0.55f) / 0.25f);
    return 0.0f;
}

static void DrawViewModel(const Actor& p) {
    const ModelDef& md = MODELS[p.cur];
    bool rel = (p.reload > 0.0f);
    float ph = rel ? p.rph : -1.0f;
    float tilt = 0.0f, magDy = 0.0f, jerk = 0.0f, handT = 0.0f;
    bool magVis = true;
    if (rel) {
        if (ph < 0.15f) tilt = ph / 0.15f; else if (ph < 0.8f) tilt = 1.0f; else tilt = 1.0f - (ph - 0.8f) / 0.2f;
        magDy = MagDrop(ph);
        magVis = !(ph > 0.4f && ph < 0.55f);
        handT = Clamp(std::min(ph / 0.12f, (1.0f - ph) / 0.15f), 0.0f, 1.0f);
        if (ph > 0.82f && ph < 0.92f) jerk = sinf((ph - 0.82f) / 0.1f * PI) * 0.04f;
    }
    rlPushMatrix();
    rlTranslatef(p.pos.x, p.pos.y + 1.6f, p.pos.z);
    rlRotatef(p.yaw * RAD2DEG, 0, 1, 0);
    rlRotatef(-(p.pitch + p.recoil) * RAD2DEG, 1, 0, 0);
    float bx = -0.20f + cosf(bobT * 0.5f) * 0.008f * bobAmt;
    float by = -0.20f + fabsf(sinf(bobT)) * 0.010f * bobAmt;
    rlTranslatef(bx, by - 0.38f * drawAnim, 0.30f - 0.05f * gunKick - jerk);
    rlRotatef(-30.0f * drawAnim - gunKick * 3.0f, 1, 0, 0);
    rlTranslatef(0.05f * tilt, -0.12f * tilt, -0.06f * tilt);
    rlRotatef(-20.0f * tilt, 1, 0, 0);
    rlRotatef(25.0f * tilt, 0, 0, 1);
    rlScalef(0.9f, 0.9f, 0.9f);

    DrawParts(p.cur, magDy, magVis);

    // руки
    Color skin = PAL[C_SKIN], sleeve = PAL[C_SLEEVE];
    Vector3 g = md.grip;
    DrawTexBox(g.x - 0.03f, g.y - 0.05f, g.z - 0.04f, g.x + 0.03f, g.y + 0.04f, g.z + 0.04f, NOTEX, 1, skin, true);
    DrawTexBox(g.x - 0.035f, g.y - 0.08f, g.z - 0.34f, g.x + 0.035f, g.y + 0.00f, g.z - 0.04f, NOTEX, 1, sleeve, true);
    Vector3 magPos = md.fore;
    for (int i = 0; i < md.n; i++) if (md.p[i].f == 1) { magPos = Vector3{ md.p[i].x, md.p[i].y, md.p[i].z }; break; }
    Vector3 lh = Vector3Lerp(md.fore, Vector3{ magPos.x, magPos.y + magDy - 0.07f, magPos.z }, handT);
    DrawTexBox(lh.x - 0.035f, lh.y - 0.035f, lh.z - 0.05f, lh.x + 0.035f, lh.y + 0.035f, lh.z + 0.05f, NOTEX, 1, skin, true);
    DrawTexBox(lh.x - 0.03f, lh.y - 0.05f, lh.z - 0.40f, lh.x + 0.05f, lh.y + 0.01f, lh.z - 0.05f, NOTEX, 1, sleeve, true);

    if (muzzleT > 0.0f) {
        float z = gZmax[p.cur];
        DrawTexBox(-0.035f, -0.035f, z, 0.035f, 0.035f, z + 0.20f, NOTEX, 1, Col(255, 225, 100), true);
        DrawTexBox(-0.07f, -0.01f, z + 0.02f, 0.07f, 0.01f, z + 0.10f, NOTEX, 1, Col(255, 180, 60), true);
    }
    rlPopMatrix();
}

// =====================================================================
//  HUD
// =====================================================================
static void DrawLabel(Camera3D& cam, Vector3 pos, const char* txt, Color c) {
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 to = Vector3Subtract(pos, cam.position);
    if (Vector3DotProduct(fwd, to) <= 0.1f) return;
    Vector2 s = GetWorldToScreen(pos, cam);
    s.x = (s.x - uiOffX) / uiScale;
    s.y = (s.y - uiOffY) / uiScale;
    int w = MeasureText(txt, 40);
    DrawText(txt, (int)s.x - w / 2, (int)s.y - 20, 40, c);
}

static void DrawRadar() {
    const float s = 0.8f;
    const int ox = 10, oy = 10;
    DrawRectangle(ox - 2, oy - 2, (int)(100 * s) + 4, (int)(120 * s) + 4, Fade(BLACK, 0.6f));
    for (const Wall& w : gWalls)
        DrawRectangle(ox + (int)((w.x0 + 50) * s), oy + (int)((w.z0 + 60) * s), std::max(1, (int)((w.x1 - w.x0) * s)),
                      std::max(1, (int)((w.z1 - w.z0) * s)), w.h > 3 ? Color{ 150, 130, 90, 255 } : Color{ 110, 90, 60, 255 });
    DrawText("A", ox + (int)((SITE_A.x + 50) * s) - 5, oy + (int)((SITE_A.y + 60) * s) - 8, 18, ORANGE);
    DrawText("B", ox + (int)((SITE_B.x + 50) * s) - 5, oy + (int)((SITE_B.y + 60) * s) - 8, 18, ORANGE);
    if (bomb.planted) DrawCircle(ox + (int)((bomb.pos.x + 50) * s), oy + (int)((bomb.pos.z + 60) * s), 3, RED);
    for (int i = 0; i < (int)A.size(); i++) {
        const Actor& a = A[i];
        if (!a.alive || a.team != TERRO) continue;
        int px = ox + (int)((a.pos.x + 50) * s), py = oy + (int)((a.pos.z + 60) * s);
        DrawCircle(px, py, i == 0 ? 4.0f : 3.0f, i == 0 ? WHITE : GREEN);
        if (i == 0) DrawLine(px, py, px + (int)(sinf(a.yaw) * 10), py + (int)(cosf(a.yaw) * 10), WHITE);
    }
}

static void DrawHUD(Camera3D& cam) {
    const Actor& p = A[0];
    DrawLabel(cam, Vector3{ SITE_A.x, 4.0f, SITE_A.y }, "A", Fade(ORANGE, 0.9f));
    DrawLabel(cam, Vector3{ SITE_B.x, 4.0f, SITE_B.y }, "B", Fade(ORANGE, 0.9f));

    if (hurtT > 0.0f) DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(RED, hurtT));
    if (scoped) {
        DrawRing(Vector2{ SCREEN_W / 2.0f, SCREEN_H / 2.0f }, SCREEN_H * 0.45f, SCREEN_W, 0, 360, 64, BLACK);
        DrawLine(0, SCREEN_H / 2, SCREEN_W, SCREEN_H / 2, BLACK);
        DrawLine(SCREEN_W / 2, 0, SCREEN_W / 2, SCREEN_H, BLACK);
    } else if (p.alive && !buyOpen) {
        int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
        DrawLine(cx - 10, cy, cx - 4, cy, LIME); DrawLine(cx + 4, cy, cx + 10, cy, LIME);
        DrawLine(cx, cy - 10, cx, cy - 4, LIME); DrawLine(cx, cy + 4, cx, cy + 10, LIME);
    }
    if (hitT > 0.0f) {
        int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
        DrawLine(cx - 8, cy - 8, cx - 3, cy - 3, WHITE); DrawLine(cx + 8, cy - 8, cx + 3, cy - 3, WHITE);
        DrawLine(cx - 8, cy + 8, cx - 3, cy + 3, WHITE); DrawLine(cx + 8, cy + 8, cx + 3, cy + 3, WHITE);
    }

    DrawRadar();

    float tm = bomb.planted ? std::max(0.0f, bomb.timer) : std::max(0.0f, roundTime);
    string ts = bomb.planted ? TextFormat("BOMB  %02d", (int)ceilf(tm)) : TextFormat("%d:%02d", (int)tm / 60, (int)tm % 60);
    int tw = MeasureText(ts.c_str(), 32);
    DrawRectangle(SCREEN_W / 2 - 110, 8, 220, 70, Fade(BLACK, 0.6f));
    DrawText(ts.c_str(), SCREEN_W / 2 - tw / 2, 12, 32, bomb.planted ? RED : WHITE);
    DrawText(TextFormat("T  %d  :  %d  CT", scoreT, scoreCT), SCREEN_W / 2 - 85, 50, 22, LIGHTGRAY);
    if (bomb.planted && bomb.defuse > 0.0f) {
        DrawRectangle(SCREEN_W / 2 - 110, 82, (int)(220 * bomb.defuse / 10.0f), 8, SKYBLUE);
        DrawText("DEFUSING...", SCREEN_W / 2 - 50, 94, 18, SKYBLUE);
    }

    int fy = 10;
    for (const FeedItem& f : feed) {
        int w = MeasureText(f.txt.c_str(), 18);
        DrawRectangle(SCREEN_W - w - 22, fy - 2, w + 14, 22, Fade(BLACK, 0.55f));
        DrawText(f.txt.c_str(), SCREEN_W - w - 15, fy, 18, WHITE);
        fy += 26;
    }

    // здоровье, броня, деньги
    DrawRectangle(10, SCREEN_H - 120, 230, 106, Fade(BLACK, 0.6f));
    DrawText(TextFormat("HP %d", p.hp), 22, SCREEN_H - 114, 36, p.hp > 30 ? WHITE : RED);
    DrawText(TextFormat("ARMOR %d", p.armor), 22, SCREEN_H - 76, 22, SKYBLUE);
    DrawText(TextFormat("$ %d", p.money), 22, SCREEN_H - 50, 30, Color{ 120, 230, 120, 255 });
    int py = 0;
    for (const Popup& po : popups) {
        unsigned char al = (unsigned char)(255 * Clamp(po.t / 1.0f, 0.0f, 1.0f));
        DrawText(TextFormat("+$%d", po.amt), 250, SCREEN_H - 50 - py * 26 - (int)((2.2f - po.t) * 12), 26, Color{ 120, 255, 140, al });
        py++;
    }

    // оружие и патроны
    const WeaponDef& wd = WDEF[p.cur];
    DrawRectangle(SCREEN_W - 270, SCREEN_H - 150, 260, 136, Fade(BLACK, 0.6f));
    if (p.pri >= 0) DrawText(TextFormat("1  %s", WDEF[p.pri].name), SCREEN_W - 258, SCREEN_H - 144, 18, p.cur == p.pri ? ORANGE : GRAY);
    DrawText(TextFormat("2  %s", WDEF[p.sec].name), SCREEN_W - 258, SCREEN_H - 122, 18, p.cur == p.sec ? ORANGE : GRAY);
    DrawText(wd.name, SCREEN_W - 258, SCREEN_H - 94, 22, ORANGE);
    DrawText(TextFormat("%d / %d", p.mag[p.cur], p.res[p.cur]), SCREEN_W - 258, SCREEN_H - 66, 40, WHITE);
    if (p.reload > 0.0f) {
        DrawRectangle(SCREEN_W / 2 - 80, SCREEN_H / 2 + 70, 160, 10, Fade(BLACK, 0.6f));
        DrawRectangle(SCREEN_W / 2 - 80, SCREEN_H / 2 + 70, (int)(160 * p.rph), 10, YELLOW);
        DrawText("RELOADING", SCREEN_W / 2 - 55, SCREEN_H / 2 + 48, 20, YELLOW);
    }

    if (p.alive && state == LIVE && !bomb.planted && SiteOf(p.pos) >= 0)
        DrawText(PLANT_HINT, SCREEN_W / 2 - 150, SCREEN_H / 2 + 100, 26, ORANGE);
    if (planting) {
        int bw = 260;
        DrawRectangle(SCREEN_W / 2 - bw / 2, SCREEN_H / 2 + 140, bw, 18, Fade(BLACK, 0.7f));
        DrawRectangle(SCREEN_W / 2 - bw / 2, SCREEN_H / 2 + 140, (int)(bw * p.plantProg / 3.2f), 18, ORANGE);
        DrawText("PLANTING...", SCREEN_W / 2 - 60, SCREEN_H / 2 + 118, 22, ORANGE);
    }
    if (!p.alive && state != ROUNDEND)
        DrawText("YOU DIED - spectating (next round soon)", SCREEN_W / 2 - 230, SCREEN_H - 120, 26, RED);

    if (state == FREEZE && !buyOpen) {
        string s = TextFormat("BUY TIME  %d    (press B to open buy menu)", (int)ceilf(stTimer));
        DrawText(s.c_str(), SCREEN_W / 2 - MeasureText(s.c_str(), 28) / 2, 130, 28, WHITE);
    }
    if (state == ROUNDEND)
        DrawText(endMsg.c_str(), SCREEN_W / 2 - MeasureText(endMsg.c_str(), 34) / 2, 130, 34, GOLD);
    if (bannerT > 0.0f && state == LIVE)
        DrawText(banner.c_str(), SCREEN_W / 2 - MeasureText(banner.c_str(), 30) / 2, 130, 30, RED);
}

// =====================================================================
//  МЕНЮ: главное и пауза
// =====================================================================
static bool UIButton(Rectangle r, const char* text, int fs = 30) {
    Vector2 m = UIPtr();
    bool hov = CheckCollisionPointRec(m, r);
    DrawRectangleRec(r, hov ? Color{ 230, 160, 50, 235 } : Color{ 16, 18, 26, 205 });
    DrawRectangleLinesEx(r, 2, hov ? WHITE : Color{ 200, 200, 210, 150 });
    int tw = MeasureText(text, fs);
    DrawText(text, (int)(r.x + (r.width - tw) / 2), (int)(r.y + (r.height - fs) / 2), fs, hov ? BLACK : WHITE);
    if (hov && UIPressed()) { PlayPool(S.uiClick, 0.6f); return true; }
    return false;
}
static void UISlider(Rectangle r, float& v, const char* label) {
    Vector2 m = UIPtr();
    Rectangle hit = { r.x - 8, r.y - 12, r.width + 16, r.height + 24 };
    if (UIDown() && CheckCollisionPointRec(m, hit)) {
        v = Clamp((m.x - r.x) / r.width, 0.0f, 1.0f);
        SetMasterVolume(v);
    }
    DrawText(label, (int)r.x, (int)r.y - 28, 20, LIGHTGRAY);
    DrawRectangleRec(r, Color{ 16, 18, 26, 220 });
    DrawRectangle((int)r.x, (int)r.y, (int)(r.width * v), (int)r.height, Color{ 230, 160, 50, 255 });
    DrawRectangleLinesEx(r, 2, Color{ 200, 200, 210, 150 });
    DrawCircle((int)(r.x + r.width * v), (int)(r.y + r.height / 2), 10, WHITE);
}

static void DrawMenuBackground() {
    if (bgLoaded) {
        float sc = std::max((float)SCREEN_W / texBg.width, (float)SCREEN_H / texBg.height);
        float w = texBg.width * sc, h = texBg.height * sc;
        Rectangle src = { 0, 0, (float)texBg.width, (float)texBg.height };
        Rectangle dst = { (SCREEN_W - w) / 2, (SCREEN_H - h) / 2, w, h };
        DrawTexturePro(texBg, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
    } else {
        DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, Color{ 5, 10, 40, 255 }, Color{ 10, 70, 45, 255 });
    }
}

static void DrawMainMenu() {
    DrawMenuBackground();
    DrawRectangleGradientH(0, 0, (int)(SCREEN_W * 0.62f), SCREEN_H, Color{ 0, 0, 0, 215 }, Color{ 0, 0, 0, 0 });
    DrawText("SANDSTRIKE", 83, 103, 84, Color{ 0, 0, 0, 200 });
    DrawText("SANDSTRIKE", 80, 100, 84, Color{ 255, 190, 70, 255 });
    DrawText("TACTICAL 3D SHOOTER", 84, 190, 26, Color{ 220, 220, 230, 255 });

    if (UIButton(Rectangle{ 80, 280, 340, 64 }, "PLAY")) {
        app = APP_GAME;
        paused = false;
        if (S.musicOK) StopSound(S.music);
        NewMatch();
    }
    if (UIButton(Rectangle{ 80, 360, 340, 64 }, showControls ? "HIDE CONTROLS" : "CONTROLS")) showControls = !showControls;
    if (UIButton(Rectangle{ 80, 440, 340, 64 }, "QUIT")) quitReq = true;
    UISlider(Rectangle{ 80, 560, 340, 18 }, masterVol, "VOLUME");

    if (showControls) {
        DrawRectangle(470, 90, 440, 440, Color{ 10, 12, 18, 215 });
        DrawRectangleLines(470, 90, 440, 440, Color{ 230, 160, 50, 255 });
#ifdef PLATFORM_ANDROID
        const char* lines[] = {
            "CONTROLS", "Left stick - move (light push = walk)", "Drag right side - look", "FIRE - shoot",
            "JUMP - jump", "R - reload", "SWAP - change weapon", "SCOPE - zoom on / off (SSG / AWP)",
            "BUY - shop (start of round)", "PLANT (hold) - plant bomb at A / B", "II (top left) - pause"
        };
#else
        const char* lines[] = {
            "CONTROLS", "WASD - move", "Shift - walk slowly", "Space - jump", "Mouse - look",
            "Left mouse - fire", "Right mouse - scope (SSG / AWP)", "R - reload", "1 / 2 / Q / wheel - weapons",
            "B - buy menu (start of round)", "E (hold) - plant bomb at A / B", "ESC - pause menu"
        };
#endif
        for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) DrawText(lines[i], 494, 112 + i * 34, i == 0 ? 28 : 22, i == 0 ? Color{ 255, 190, 70, 255 } : WHITE);
    }
    DrawText("You play as Terrorist: buy weapons, plant the bomb, survive.", 80, SCREEN_H - 40, 18, Color{ 200, 200, 210, 255 });
}

static void DrawPauseMenu() {
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.6f));
    DrawText("PAUSED", SCREEN_W / 2 - MeasureText("PAUSED", 70) / 2, 130, 70, Color{ 255, 190, 70, 255 });
    float bx = SCREEN_W / 2.0f - 170;
    if (UIButton(Rectangle{ bx, 230, 340, 62 }, "RESUME")) { paused = false; CaptureMouse(true); }
#ifndef PLATFORM_ANDROID
    if (UIButton(Rectangle{ bx, 300, 340, 62 }, IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "WINDOWED (F11)" : "FULLSCREEN (F11)")) gToggleFS = true;
#endif
    if (UIButton(Rectangle{ bx, 370, 340, 62 }, "MAIN MENU")) {
        paused = false; buyOpen = false; app = APP_MENU;
        CaptureMouse(false);
        if (S.musicOK) { SetSoundVolume(S.music, 0.5f); PlaySound(S.music); }
    }
    if (UIButton(Rectangle{ bx, 440, 340, 62 }, "QUIT GAME")) quitReq = true;
    UISlider(Rectangle{ bx, 570, 340, 18 }, masterVol, "VOLUME");
}

// =====================================================================
//  MAIN
// =====================================================================
int main() {
#ifdef PLATFORM_ANDROID
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(0, 0, "SANDSTRIKE");          // на весь экран устройства
#else
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "SANDSTRIKE - 3D shooter");
    SetWindowMinSize(640, 360);
#endif
    rlSetClipPlanes(0.05, 700.0);   // лучше точность буфера глубины (меньше мерцания стен)
    uiRT = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(uiRT.texture, TEXTURE_FILTER_BILINEAR);
    UpdateUIScale();
    InitAudioDevice();
    SetExitKey(KEY_NULL);
#ifdef PLATFORM_ANDROID
    SetTargetFPS(60);
#else
    SetTargetFPS(120);
#endif
    srand((unsigned)GetTime() + 12345u);
    SetMasterVolume(masterVol);

    texPlaster = GenPlaster();
    texBrick = GenBrick();
    texFloor = GenFloor();
    texCrate = GenCrate();
    {
        const char* paths[] = { "menu_bg.jpg", "assets/menu_bg.jpg", "../menu_bg.jpg" };
        for (const char* pth : paths) {
#ifdef PLATFORM_ANDROID
            if (true) {                       // ассеты лежат в APK, FileExists их не видит
#else
            if (FileExists(pth)) {
#endif
                texBg = LoadTexture(pth);
                if (texBg.id > 0) { SetTextureFilter(texBg, TEXTURE_FILTER_BILINEAR); bgLoaded = true; break; }
            }
        }
    }
    previewRT = LoadRenderTexture(262, 150);
    InitModels();
    InitSounds();
    BuildMap();
    BuildGraph();
    A.assign(8, Actor());

    EnableCursor();
    if (S.musicOK) { SetSoundVolume(S.music, 0.5f); PlaySound(S.music); }

    Camera3D cam = { 0 };
    cam.up = Vector3{ 0, 1, 0 };
    cam.fovy = 75.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    while (!quitReq && !WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);

#ifndef PLATFORM_ANDROID
        static int frameNo = 0;
        if (++frameNo == 3) gToggleFS = true;   // старт в полном экране (F11 / Alt+Enter - переключить)
        // полный экран: F11 или Alt+Enter
        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) gToggleFS = true;
        if (gToggleFS) {
            gToggleFS = false;
            ToggleBorderlessWindowed();
            skipMouse = 3;
        }
#endif
        UpdateUIScale();
#ifdef PLATFORM_ANDROID
        {
            bool inGame = (app == APP_GAME), alive = inGame && A[0].alive;
            for (int i = 0; i < TB_COUNT; i++) tVis[i] = false;
            tVis[TB_PAUSE] = inGame && !paused && !buyOpen;
            tVis[TB_FIRE] = tVis[TB_JUMP] = tVis[TB_RELOAD] = tVis[TB_SWAP] = alive && !paused && !buyOpen;
            tVis[TB_SCOPE] = tVis[TB_FIRE] && WDEF[A[0].cur].scope;
            tVis[TB_PLANT] = tVis[TB_FIRE] && state == LIVE && !bomb.planted && SiteOf(A[0].pos) >= 0 && A[0].onGround;
            tVis[TB_BUY] = tVis[TB_FIRE] && state == FREEZE;
            TouchUpdate(inGame && !paused && !buyOpen);
        }
#endif

        if (app == APP_MENU) {
            if (S.musicOK && !IsSoundPlaying(S.music)) PlaySound(S.music);
            BeginDrawing();
            ClearBackground(BLACK);
            BeginUI();
            DrawMainMenu();
            EndUI();
            EndDrawing();
            continue;
        }

        // ---- игра ----
        if (IKeyPressed(KEY_ESCAPE)) {
            if (buyOpen) { buyOpen = false; CaptureMouse(true); }
            else { paused = !paused; CaptureMouse(!paused); }
        }
        if (!paused && IKeyPressed(KEY_B) && state == FREEZE && A[0].alive) {
            buyOpen = !buyOpen;
            CaptureMouse(!buyOpen);
        }
        if (!paused) {
            UpdateGame(dt);
            if (buyOpen) UpdateBuyMenu();
        }

        // камера: свой глаз или наблюдение за живым союзником
        int viewIdx = 0;
        if (!A[0].alive) {
            viewIdx = -1;
            for (int i = 1; i < (int)A.size() && viewIdx < 0; i++) if (A[i].alive && A[i].team == TERRO) viewIdx = i;
            for (int i = 1; i < (int)A.size() && viewIdx < 0; i++) if (A[i].alive) viewIdx = i;
            if (viewIdx < 0) viewIdx = 0;
        }
        const Actor& v = A[viewIdx];
        float pitch = (viewIdx == 0) ? (v.pitch + v.recoil) : 0.0f;
        Vector3 eye = Eye(v);
        if (viewIdx == 0 && !v.alive) eye.y = v.pos.y + 0.4f;
        cam.position = eye;
        cam.target = Vector3Add(eye, Fwd(v.yaw, pitch));
        float targetFov = scoped ? (A[0].cur == W_AWP ? 20.0f : 32.0f) : 75.0f;
        cam.fovy = Lerp(cam.fovy, targetFov, std::min(1.0f, dt * 15.0f));
        gListenPos = eye;
        gListenYaw = v.yaw;

        if (buyOpen && !paused && hoverItem >= 0 && hoverItem < NUM_W) RenderPreview(hoverItem);

        BeginDrawing();
        // ВАЖНО: очищаем цвет И буфер глубины каждый кадр. Без этого старая глубина
        // отсекала всю геометрию и на экране оставалось только небо.
        ClearBackground(BLACK);
        DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 88, 150, 215, 255 }, Color{ 225, 210, 180, 255 });
        BeginMode3D(cam);
        DrawWorld();
        if (A[0].alive && !scoped) DrawViewModel(A[0]);
        EndMode3D();
        BeginUI();
        DrawHUD(cam);
        if (buyOpen && !paused) DrawBuyMenu();
#ifdef PLATFORM_ANDROID
        if (buyOpen && !paused && UIButton(Rectangle{ BUY_PANEL.x + BUY_PANEL.width - 190, BUY_PANEL.y + 10, 170, 52 }, "CLOSE", 26)) {
            buyOpen = false; CaptureMouse(true);
        }
#endif
        if (paused) DrawPauseMenu();
        DrawFPS(SCREEN_W - 90, 10 + (int)feed.size() * 26);
        EndUI();
#ifdef PLATFORM_ANDROID
        DrawTouchControls();
#endif
        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
