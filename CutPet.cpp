#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <cmath>
#include <time.h>

#pragma comment(lib, "gdiplus.lib")

constexpr int WINDOW_SIZE = 200;
constexpr int ANIMATION_FPS = 30;
constexpr int SLEEP_INTERVAL_MS = 60000;
constexpr int GREET_INTERVAL_MS = 30000;
constexpr int PET_FEED_DURATION = 5000;
constexpr int PET_PET_DURATION = 3000;

enum class PetState { IDLE, FEEDING, SLEEPING, PETTED, HAPPY, COUNT };
constexpr int FRAMES_PER_STATE[] = { 4, 2, 2, 2, 4 };

// 饰品类型
enum class AccessoryType { NONE, HAT, GLASSES, SCARF, CROWN, COUNT };
const wchar_t* AccessoryNames[] = { L"无", L"帽子", L"眼镜", L"围巾", L"皇冠" };

// 仓鼠颜色预设
enum class FurColorType { BROWN, WHITE, BLACK, GOLDEN, PINK, CUSTOM, COUNT };
const wchar_t* FurColorNames[] = { L"棕色", L"白色", L"黑色", L"金色", L"粉色", L"自定义" };

struct Particle {
    float x, y, vx, vy, life, maxLife;
    bool active;
};

// ============================================================================
// Level System
// ============================================================================

struct LevelInfo {
    int level;
    int xpNeeded;
    const wchar_t* title;
    const wchar_t* evolutionName;
    int bodyScale;       // 80~150, scale factor percentage
    int hueShift;        // color shift for evolution
    int glowRadius;      // aura radius
    DWORD glowColor;     // aura color
};

// XP curve: fixed XP requirement per level
constexpr int XP_PER_LEVEL = 200;

inline int GetXPForLevel(int) {
    return XP_PER_LEVEL;
}

LevelInfo GetLevelInfo(int level) {
    LevelInfo info{};
    info.level = level;
    info.xpNeeded = GetXPForLevel(level);
    info.bodyScale = 100;  // Fixed scale, images will replace shapes
    info.glowRadius = level / 5;

    if (level <= 4) {
        // Lv.1-4: Baby hamster (cute, small, pinkish)
        info.title = L"幼崽";
        info.evolutionName = L"仓鼠宝宝";
        info.hueShift = 0;
        info.glowColor = 0x00FFFFFF;  // white glow
    } else if (level <= 9) {
        // Lv.5-9: Juvenile (bigger, golden)
        info.title = L"少年";
        info.evolutionName = L"黄金仓鼠";
        info.hueShift = 15;
        info.glowColor = 0x40FFD700;  // gold glow
    } else if (level <= 19) {
        // Lv.10-19: Young adult (large, royal purple)
        info.title = L"青年";
        info.evolutionName = L"皇家仓鼠";
        info.hueShift = 30;
        info.glowColor = 0x609370DB;  // purple glow
    } else if (level <= 29) {
        // Lv.20-29: Adult (massive, legendary red)
        info.title = L"成年";
        info.evolutionName = L"传说仓鼠";
        info.hueShift = 50;
        info.glowColor = 0x80FF4500;  // red-orange glow
    } else {
        // Lv.30+: Mythical (cosmic rainbow)
        info.title = L"传奇";
        info.evolutionName = L"星河仓鼠";
        info.hueShift = 80;
        info.glowColor = 0xA0FF69B4;  // pink cosmic glow
    }
    return info;
}

// ============================================================================
// Hamster with Level System
// ============================================================================

class Hamster {
public:
    int pos_x = 0, pos_y = 0;
    int width = WINDOW_SIZE, height = WINDOW_SIZE;
    PetState state = PetState::IDLE;
    int currentFrame = 0;
    float frameTimer = 0;
    float animSpeed = 1.0f / ANIMATION_FPS;
    int happiness = 50;
    int hunger = 30;
    float sleepTimer = static_cast<float>(SLEEP_INTERVAL_MS);
    float greetTimer = static_cast<float>(GREET_INTERVAL_MS);
    std::vector<Particle> particles;
    float bounceOffset = 0.0f;
    float bounceTimer = 0.0f;
    float zzzOffset = 0.0f;
    bool blinking = false;
    float blinkTimer = 2.0f;

    // 饰品
    AccessoryType accessory = AccessoryType::NONE;

    // 毛色
    FurColorType furColor = FurColorType::BROWN;
    Gdiplus::Color customColor;  // 自定义颜色

    // Level system
    int level = 1;
    int currentXP = 0;
    int totalXP = 0;
    float xpTimer = 0.0f;
    bool leveledUp = false;
    float levelUpTimer = 0.0f;
    std::wstring lastEvolutionName;

    // Toy system
    int toysRemaining = 1;  // Daily toy grant
    float lastToyReset = 0.0f;
    float petCooldown = 0.0f;  // 15 minutes
    float playCooldown = 0.0f;
    int dailyPlayCount = 0;
    int dailyMaxPlays = 3;

    void AddXP(int amount) {
        currentXP += amount;
        totalXP += amount;
        LevelInfo info = GetLevelInfo(level);
        if (currentXP >= info.xpNeeded) {
            currentXP -= info.xpNeeded;
            level++;
            leveledUp = true;
            levelUpTimer = 3.0f;
            LevelInfo newInfo = GetLevelInfo(level);
            lastEvolutionName = newInfo.evolutionName;
            for (int i = 0; i < 10; i++) {
                SpawnHeart(width / 2.0f + (rand() % 100 - 50), height / 2.0f);
            }
        }
    }

    void UpdateDailyReset(float dt) {
        // Reset toys every 60 seconds (simulating daily reset for demo)
        lastToyReset += dt;
        if (lastToyReset >= 86400.0f) {  // 24 hours
            lastToyReset = 0.0f;
            toysRemaining = 1;
            dailyPlayCount = 0;
            petCooldown = 0.0f;
            playCooldown = 0.0f;
        }
    }


    void Update(float dt) {
        frameTimer += dt;
        if (frameTimer >= animSpeed) {
            frameTimer = 0.0f;
            int maxFrames = FRAMES_PER_STATE[static_cast<int>(state)];
            currentFrame = (currentFrame + 1) % maxFrames;
        }
        blinkTimer -= dt;
        if (blinkTimer <= 0) {
            blinking = !blinking;
            blinkTimer = blinking ? 0.1f : (2.0f + static_cast<float>(rand() % 200) / 100.0f);
        }
        if (state == PetState::IDLE) {
            bounceTimer += dt * 3.0f;
            bounceOffset = sinf(bounceTimer) * 3.0f;
        }
        if (state == PetState::SLEEPING) {
            zzzOffset = sinf(frameTimer * 2.0f) * 0.3f + 0.3f;
        }

        // Passive XP gain (1 XP per 60 seconds)
        xpTimer += dt;
        if (xpTimer >= 60.0f) {
            xpTimer = 0.0f;
            AddXP(1);
        }

        // Cooldown timers
        if (petCooldown > 0) petCooldown -= dt;
        if (playCooldown > 0) playCooldown -= dt;

        // Level up timer countdown
        if (leveledUp) {
            levelUpTimer -= dt;
            if (levelUpTimer <= 0) {
                leveledUp = false;
            }
        }

        switch (state) {
        case PetState::IDLE:
            sleepTimer -= dt * 1000.0f;
            greetTimer -= dt * 1000.0f;
            hunger += static_cast<int>(dt * 0.5f);
            if (hunger > 100) hunger = 100;
            if (sleepTimer <= 0) { state = PetState::SLEEPING; currentFrame = 0; }
            if (greetTimer <= 0) { state = PetState::HAPPY; currentFrame = 0; greetTimer = static_cast<float>(GREET_INTERVAL_MS); }
            break;
        case PetState::FEEDING:
            hunger -= static_cast<int>(dt * 20.0f);
            if (hunger < 0) hunger = 0;
            if (frameTimer > static_cast<float>(PET_FEED_DURATION) / 1000.0f) {
                state = PetState::IDLE; currentFrame = 0; frameTimer = 0;
                sleepTimer = static_cast<float>(SLEEP_INTERVAL_MS); greetTimer = static_cast<float>(GREET_INTERVAL_MS);
            }
            break;
        case PetState::SLEEPING:
            happiness = (happiness + static_cast<int>(dt * 5.0f)) > 100 ? 100 : (happiness + static_cast<int>(dt * 5.0f));
            if (frameTimer > 15.0f) { state = PetState::IDLE; currentFrame = 0; frameTimer = 0; sleepTimer = static_cast<float>(SLEEP_INTERVAL_MS); }
            break;
        case PetState::PETTED:
            happiness = (happiness + static_cast<int>(dt * 10.0f)) > 100 ? 100 : (happiness + static_cast<int>(dt * 10.0f));
            if (frameTimer > static_cast<float>(PET_PET_DURATION) / 1000.0f) {
                state = PetState::HAPPY; currentFrame = 0; frameTimer = 0;
            }
            break;
        case PetState::HAPPY:
            if (frameTimer > 2.0f) {
                state = PetState::IDLE; currentFrame = 0; frameTimer = 0;
                sleepTimer = static_cast<float>(SLEEP_INTERVAL_MS); greetTimer = static_cast<float>(GREET_INTERVAL_MS);
            }
            break;
        default: break;
        }
        UpdateParticles(dt);
    }

    void SpawnHeart(float px, float py) {
        for (auto& p : particles) {
            if (!p.active) {
                p.active = true; p.x = px; p.y = py;
                p.vx = (rand() % 100 - 50) / 100.0f;
                p.vy = -1.5f - (rand() % 50) / 100.0f;
                p.life = p.maxLife = 1.0f + (rand() % 30) / 100.0f;
                return;
            }
        }
        if (particles.size() < 50) {
            Particle p{};
            p.active = true; p.x = px; p.y = py;
            p.vx = (rand() % 100 - 50) / 100.0f;
            p.vy = -1.5f; p.life = p.maxLife = 1.5f;
            particles.push_back(p);
        }
    }

    void UpdateParticles(float dt) {
        for (auto& p : particles) {
            if (!p.active) continue;
            p.x += p.vx; p.y += p.vy;
            p.vy -= 0.5f * dt;
            p.life -= dt;
            if (p.life <= 0) p.active = false;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return !p.active; }), particles.end());
    }

    void Draw(Gdiplus::Graphics* g,
              Gdiplus::SolidBrush* brown, Gdiplus::SolidBrush* lightBrown,
              Gdiplus::SolidBrush* darkBrown, Gdiplus::SolidBrush* pink,
              Gdiplus::SolidBrush* eyeC, Gdiplus::SolidBrush* white,
              Gdiplus::SolidBrush* orange, Gdiplus::SolidBrush* green,
              Gdiplus::SolidBrush* sparkle, Gdiplus::SolidBrush* zzz,
              Gdiplus::SolidBrush* blush) {
        float cx = static_cast<float>(width) / 2.0f;
        float cy = static_cast<float>(height) / 2.0f + bounceOffset;

        LevelInfo info = GetLevelInfo(level);

        // Draw evolution glow/aura
        if (info.glowRadius > 0) {
            Gdiplus::Color glowClr(info.glowColor);
            Gdiplus::SolidBrush auraBrush(glowClr);
            Gdiplus::RectF aura(cx - 70.0f - static_cast<float>(info.glowRadius), cy - 70.0f - static_cast<float>(info.glowRadius),
                                140.0f + static_cast<float>(info.glowRadius) * 2.0f, 140.0f + static_cast<float>(info.glowRadius) * 2.0f);
            g->FillEllipse(&auraBrush, aura);
        }

        // Scale factor based on level
        float scale = static_cast<float>(info.bodyScale) / 100.0f;

        // Choose colors based on fur color
        Gdiplus::Color furColor_obj(209, 158, 92);
        Gdiplus::Color lightFurColor_obj(235, 199, 140);
        Gdiplus::Color darkFurColor_obj(153, 102, 51);
        switch (furColor) {
        case FurColorType::BROWN:
            furColor_obj = Gdiplus::Color(209, 158, 92);
            lightFurColor_obj = Gdiplus::Color(235, 199, 140);
            darkFurColor_obj = Gdiplus::Color(153, 102, 51);
            break;
        case FurColorType::WHITE:
            furColor_obj = Gdiplus::Color(245, 245, 245);
            lightFurColor_obj = Gdiplus::Color(255, 255, 255);
            darkFurColor_obj = Gdiplus::Color(200, 200, 200);
            break;
        case FurColorType::BLACK:
            furColor_obj = Gdiplus::Color(50, 50, 50);
            lightFurColor_obj = Gdiplus::Color(80, 80, 80);
            darkFurColor_obj = Gdiplus::Color(20, 20, 20);
            break;
        case FurColorType::GOLDEN:
            furColor_obj = Gdiplus::Color(255, 215, 0);
            lightFurColor_obj = Gdiplus::Color(255, 235, 100);
            darkFurColor_obj = Gdiplus::Color(200, 160, 0);
            break;
        case FurColorType::PINK:
            furColor_obj = Gdiplus::Color(255, 182, 193);
            lightFurColor_obj = Gdiplus::Color(255, 210, 218);
            darkFurColor_obj = Gdiplus::Color(220, 150, 160);
            break;
        case FurColorType::CUSTOM:
            furColor_obj = customColor;
            {
                int lr = customColor.GetR() + 30; if (lr > 255) lr = 255;
                int lg = customColor.GetG() + 30; if (lg > 255) lg = 255;
                int lb = customColor.GetB() + 30; if (lb > 255) lb = 255;
                lightFurColor_obj = Gdiplus::Color(customColor.GetAlpha(), static_cast<unsigned char>(lr), static_cast<unsigned char>(lg), static_cast<unsigned char>(lb));
                int dr = customColor.GetR() - 30; if (dr < 0) dr = 0;
                int dg = customColor.GetG() - 30; if (dg < 0) dg = 0;
                int db = customColor.GetB() - 30; if (db < 0) db = 0;
                darkFurColor_obj = Gdiplus::Color(customColor.GetAlpha(), static_cast<unsigned char>(dr), static_cast<unsigned char>(dg), static_cast<unsigned char>(db));
            }
            break;
        }

        // Shadow (scaled)
        Gdiplus::RectF shadow(cx - 45 * scale, cy + 60, 90 * scale, 20);
        g->FillEllipse(blush, shadow);

        // Body (scaled)
        Gdiplus::SolidBrush furBrush(furColor_obj);
        Gdiplus::RectF body(cx - 55 * scale, cy - 30 * scale, 110 * scale, 100 * scale);
        g->FillEllipse(&furBrush, body);

        // Belly (scaled)
        Gdiplus::SolidBrush lightFurBrush(lightFurColor_obj);
        Gdiplus::RectF belly(cx - 30 * scale, cy - 3 * scale, 60 * scale, 56 * scale);
        g->FillEllipse(&lightFurBrush, belly);

        // Head (scaled)
        Gdiplus::RectF head(cx - 45 * scale, cy - 57 * scale, 90 * scale, 84 * scale);
        g->FillEllipse(&furBrush, head);

        // Ears (scaled)
        Gdiplus::SolidBrush darkFurBrush(darkFurColor_obj);
        Gdiplus::RectF earL(cx - 47 * scale, cy - 57 * scale, 24 * scale, 24 * scale);
        Gdiplus::RectF earR(cx + 23 * scale, cy - 57 * scale, 24 * scale, 24 * scale);
        g->FillEllipse(&darkFurBrush, earL);
        g->FillEllipse(&darkFurBrush, earR);

        Gdiplus::RectF earLInner(cx - 42 * scale, cy - 52 * scale, 14 * scale, 14 * scale);
        Gdiplus::RectF earRInner(cx + 28 * scale, cy - 52 * scale, 14 * scale, 14 * scale);
        g->FillEllipse(pink, earLInner);
        g->FillEllipse(pink, earRInner);

        // Eyes (scaled)
        if (state != PetState::SLEEPING && !blinking) {
            Gdiplus::RectF eyeL(cx - 26 * scale, cy - 30 * scale, 16 * scale, 20 * scale);
            Gdiplus::RectF eyeR(cx + 10 * scale, cy - 30 * scale, 16 * scale, 20 * scale);
            g->FillEllipse(eyeC, eyeL);
            g->FillEllipse(eyeC, eyeR);

            Gdiplus::RectF shineL(cx - 21 * scale, cy - 34 * scale, 6 * scale, 6 * scale);
            Gdiplus::RectF shineR(cx + 15 * scale, cy - 34 * scale, 6 * scale, 6 * scale);
            g->FillEllipse(white, shineL);
            g->FillEllipse(white, shineR);
        } else {
            Gdiplus::RectF eyeL(cx - 26 * scale, cy - 28 * scale, 16 * scale, 3 * scale);
            Gdiplus::RectF eyeR(cx + 10 * scale, cy - 28 * scale, 16 * scale, 3 * scale);
            g->FillEllipse(eyeC, eyeL);
            g->FillEllipse(eyeC, eyeR);
        }

        // Nose (scaled)
        Gdiplus::RectF nose(cx - 4 * scale, cy - 16 * scale, 8 * scale, 6 * scale);
        g->FillEllipse(pink, nose);

        // Mouth (scaled)
        Gdiplus::Pen mouthPen(darkBrown, 1.5f);
        g->DrawLine(&mouthPen, Gdiplus::PointF(cx - 10 * scale, cy - 10 * scale), Gdiplus::PointF(cx, cy - 4 * scale));
        g->DrawLine(&mouthPen, Gdiplus::PointF(cx, cy - 4 * scale), Gdiplus::PointF(cx + 10 * scale, cy - 10 * scale));

        // Cheeks (scaled)
        Gdiplus::RectF cheekL(cx - 40 * scale, cy - 16 * scale, 20 * scale, 14 * scale);
        Gdiplus::RectF cheekR(cx + 20 * scale, cy - 16 * scale, 20 * scale, 14 * scale);
        g->FillEllipse(blush, cheekL);
        g->FillEllipse(blush, cheekR);

        // Feet (scaled)
        Gdiplus::RectF footL(cx - 40 * scale, cy + 45 * scale, 30 * scale, 20 * scale);
        Gdiplus::RectF footR(cx + 10 * scale, cy + 45 * scale, 30 * scale, 20 * scale);
        g->FillEllipse(darkBrown, footL);
        g->FillEllipse(darkBrown, footR);

        // Arms (scaled)
        Gdiplus::RectF armL(cx - 54 * scale, cy - 5 * scale, 24 * scale, 36 * scale);
        Gdiplus::RectF armR(cx + 30 * scale, cy - 5 * scale, 24 * scale, 36 * scale);
        g->FillEllipse(brown, armL);
        g->FillEllipse(brown, armR);

        // Tail (scaled)
        Gdiplus::RectF tail(cx - 8 * scale, cy + 57 * scale, 16 * scale, 16 * scale);
        g->FillEllipse(lightBrown, tail);

        // Feeding: carrot (scaled)
        if (state == PetState::FEEDING) {
            Gdiplus::RectF carrotRect(cx - 8 * scale, cy + 5 * scale, 16 * scale, 25 * scale);
            g->FillRectangle(orange, carrotRect);
            Gdiplus::RectF carrotTopRect(cx - 5 * scale, cy - 7 * scale, 10 * scale, 12 * scale);
            g->FillRectangle(green, carrotTopRect);
        }

        // Happy / petted: sparkles (scaled)
        if (state == PetState::PETTED || state == PetState::HAPPY) {
            struct SP { float x, y; } sp[] = {
                {cx - 60 * scale, cy - 30 * scale}, {cx + 60 * scale, cy - 40 * scale},
                {cx - 50 * scale, cy + 20 * scale}, {cx + 55 * scale, cy + 10 * scale}, {cx, cy - 65 * scale}
            };
            for (auto& s : sp) {
                Gdiplus::RectF se(s.x - 3, s.y - 3, 6, 6);
                g->FillEllipse(sparkle, se);
            }
        }

        // Sleeping: Zzz (scaled)
        if (state == PetState::SLEEPING) {
            float zSize = (12.0f + zzzOffset) * scale;
            Gdiplus::RectF z1(cx + 28 * scale, cy - 52 * scale - zzzOffset * 20, zSize * 2, zSize * 2);
            Gdiplus::RectF z2(cx + 42 * scale, cy - 72 * scale - zzzOffset * 25, zSize * 2.6f, zSize * 2.6f);
            Gdiplus::RectF z3(cx + 58 * scale, cy - 94 * scale - zzzOffset * 30, zSize * 3.2f, zSize * 3.2f);
            g->FillEllipse(zzz, z1);
            g->FillEllipse(zzz, z2);
            g->FillEllipse(zzz, z3);
        }

        // Accessories
        if (accessory == AccessoryType::HAT) {
            Gdiplus::SolidBrush hatBrush(Gdiplus::Color(255, 139, 69));
            Gdiplus::RectF hatRect(cx - 30 * scale, cy - 80 * scale, 60 * scale, 25 * scale);
            g->FillRectangle(&hatBrush, hatRect);
            Gdiplus::RectF hatTop(cx - 20 * scale, cy - 90 * scale, 40 * scale, 15 * scale);
            g->FillRectangle(&hatBrush, hatTop);
        } else if (accessory == AccessoryType::GLASSES) {
            Gdiplus::SolidBrush glassBrush(Gdiplus::Color(200, 0, 0, 0));
            Gdiplus::RectF gL(cx - 30 * scale, cy - 35 * scale, 20 * scale, 15 * scale);
            Gdiplus::RectF gR(cx + 10 * scale, cy - 35 * scale, 20 * scale, 15 * scale);
            g->DrawRectangle(new Gdiplus::Pen(Gdiplus::Color(50, 50, 50), 2.0f), gL);
            g->DrawRectangle(new Gdiplus::Pen(Gdiplus::Color(50, 50, 50), 2.0f), gR);
            g->DrawLine(new Gdiplus::Pen(Gdiplus::Color(50, 50, 50), 2.0f),
                        Gdiplus::PointF(cx - 10 * scale, cy - 28 * scale),
                        Gdiplus::PointF(cx + 10 * scale, cy - 28 * scale));
        } else if (accessory == AccessoryType::SCARF) {
            Gdiplus::SolidBrush scarfBrush(Gdiplus::Color(255, 220, 60));
            Gdiplus::RectF scarfRect(cx - 40 * scale, cy - 5 * scale, 80 * scale, 15 * scale);
            g->FillRectangle(&scarfBrush, scarfRect);
        } else if (accessory == AccessoryType::CROWN) {
            Gdiplus::SolidBrush crownBrush(Gdiplus::Color(255, 255, 215, 0));
            Gdiplus::RectF crownRect(cx - 25 * scale, cy - 85 * scale, 50 * scale, 20 * scale);
            g->FillRectangle(&crownBrush, crownRect);
            // Crown points
            Gdiplus::SolidBrush goldBrush(Gdiplus::Color(255, 255, 215, 0));
            Gdiplus::Point crownPoints[5] = {
                Gdiplus::Point(static_cast<int>(cx - 25 * scale), static_cast<int>(cy - 85 * scale)),
                Gdiplus::Point(static_cast<int>(cx - 25 * scale), static_cast<int>(cy - 100 * scale)),
                Gdiplus::Point(static_cast<int>(cx - 12 * scale), static_cast<int>(cy - 90 * scale)),
                Gdiplus::Point(static_cast<int>(cx), static_cast<int>(cy - 100 * scale)),
                Gdiplus::Point(static_cast<int>(cx + 12 * scale), static_cast<int>(cy - 90 * scale))
            };
            g->FillPolygon(&goldBrush, crownPoints, 5);
        }

        // Particles
        for (const auto& p : particles) {
            if (!p.active) continue;
            float alpha = p.life / p.maxLife;
            float size = 8.0f * alpha;
            Gdiplus::Color hc(static_cast<unsigned char>(alpha * 255), 255, 105, 180);
            Gdiplus::SolidBrush hb(hc);
            Gdiplus::RectF pp(p.x - size, p.y - size, size * 2, size * 2);
            g->FillEllipse(&hb, pp);
        }
    }

    void OnMouseDown(int mx, int my) {
        float hcx = static_cast<float>(width) / 2.0f;
        float hcy = static_cast<float>(height) / 2.0f + bounceOffset;
        float dx = static_cast<float>(mx) - hcx;
        float dy = static_cast<float>(my) - hcy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < 60.0f && state != PetState::SLEEPING) {
            // Check pet cooldown for XP gain
            if (petCooldown <= 0) {
                state = PetState::PETTED;
                currentFrame = 0;
                frameTimer = 0;
                petCooldown = 900.0f;  // 15 minutes cooldown
                AddXP(3);
                SpawnHeart(static_cast<float>(mx), static_cast<float>(my));
            } else {
                // Just play animation, no XP
                state = PetState::PETTED;
                currentFrame = 0;
                frameTimer = 0;
                SpawnHeart(static_cast<float>(mx), static_cast<float>(my));
            }
        }
    }

    void OnMouseMove(int mx, int my, bool dragging, int dragOffX, int dragOffY) {
        if (dragging) {
            pos_x = mx - dragOffX;
            pos_y = my - dragOffY;
        }
    }

    void CycleAccessory() {
        accessory = static_cast<AccessoryType>((static_cast<int>(accessory) + 1) % static_cast<int>(AccessoryType::COUNT));
    }

    void CycleFurColor() {
        furColor = static_cast<FurColorType>((static_cast<int>(furColor) + 1) % static_cast<int>(FurColorType::COUNT));
    }
};

class App {
public:
    bool running = false;
    HWND hwnd = nullptr;
    bool isDragging = false;
    int dragOffsetX = 0, dragOffsetY = 0;
    int hamsterDragOffsetX = 0, hamsterDragOffsetY = 0;
    std::chrono::steady_clock::time_point lastFrame;
    Hamster hamster;
    ULONG_PTR gdiPlusToken = 0;

    Gdiplus::SolidBrush* brown = nullptr;
    Gdiplus::SolidBrush* lightBrown = nullptr;
    Gdiplus::SolidBrush* darkBrown = nullptr;
    Gdiplus::SolidBrush* pink = nullptr;
    Gdiplus::SolidBrush* eyeC = nullptr;
    Gdiplus::SolidBrush* white = nullptr;
    Gdiplus::SolidBrush* orange = nullptr;
    Gdiplus::SolidBrush* green = nullptr;
    Gdiplus::SolidBrush* sparkle = nullptr;
    Gdiplus::SolidBrush* zzz = nullptr;
    Gdiplus::SolidBrush* blush = nullptr;

    bool Initialize(HINSTANCE hInstance) {
        Gdiplus::GdiplusStartupInput gdiplusInput;
        GdiplusStartup(&gdiPlusToken, &gdiplusInput, nullptr);

        brown = new Gdiplus::SolidBrush(Gdiplus::Color(209, 158, 92));
        lightBrown = new Gdiplus::SolidBrush(Gdiplus::Color(235, 199, 140));
        darkBrown = new Gdiplus::SolidBrush(Gdiplus::Color(153, 102, 51));
        pink = new Gdiplus::SolidBrush(Gdiplus::Color(255, 191, 191));
        eyeC = new Gdiplus::SolidBrush(Gdiplus::Color(25, 25, 25));
        white = new Gdiplus::SolidBrush(Gdiplus::Color(255, 255, 255));
        orange = new Gdiplus::SolidBrush(Gdiplus::Color(255, 153, 51));
        green = new Gdiplus::SolidBrush(Gdiplus::Color(51, 204, 51));
        sparkle = new Gdiplus::SolidBrush(Gdiplus::Color(204, 204, 204));
        zzz = new Gdiplus::SolidBrush(Gdiplus::Color(255, 255, 255));
        blush = new Gdiplus::SolidBrush(Gdiplus::Color(128, 102, 77, 77));

        WNDCLASSEXW wcex{};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = nullptr;
        wcex.lpszClassName = L"CutPetWindowClass";

        if (!RegisterClassExW(&wcex)) return false;

        hamster.pos_x = 50;  // Far left for visibility
        hamster.pos_y = 50;  // Top for visibility

        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED,
            L"CutPetWindowClass",
            L"",
            WS_POPUP,
            hamster.pos_x, hamster.pos_y, WINDOW_SIZE, WINDOW_SIZE,
            nullptr, nullptr, hInstance, nullptr);

        if (!hwnd) return false;

        // Set window transparency (black = transparent)
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
            GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, RGB(255, 255, 255), 0, LWA_COLORKEY);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        srand(static_cast<unsigned>(time(nullptr)));
        hamster.toysRemaining = 1;  // Daily toy grant
        running = true;
        lastFrame = std::chrono::steady_clock::now();

        return true;
    }

    void Run() {
        MSG msg{};
        auto lastTick = std::chrono::steady_clock::now();
        while (running) {
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            // Tick animation every 16ms (~60fps)
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() >= 16) {
                lastTick = now;
                if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
    }

    void Shutdown() {
        running = false;
        if (hwnd) DestroyWindow(hwnd);
        delete brown; delete lightBrown; delete darkBrown; delete pink;
        delete eyeC; delete white; delete orange; delete green;
        delete sparkle; delete zzz; delete blush;
        Gdiplus::GdiplusShutdown(gdiPlusToken);
        UnregisterClassW(L"CutPetWindowClass", GetModuleHandle(nullptr));
    }

    void Render() {
        if (!hwnd) return;

        HDC hdc = GetDC(hwnd);
        if (!hdc) return;

        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // Double buffer: create memory DC
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldObj = SelectObject(memDC, memBM);

        // Transparent background
        HBRUSH hTransBrush = CreateSolidBrush(RGB(255, 255, 255));  // Will be masked
        FillRect(memDC, &rc, hTransBrush);
        DeleteObject(hTransBrush);

        // Draw hamster on memory DC
        Gdiplus::Graphics g(memDC);

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame = now;
        if (dt > 0.1f) dt = 0.016f;
        hamster.Update(dt);

        hamster.Draw(&g, brown, lightBrown, darkBrown, pink, eyeC, white,
                     orange, green, sparkle, zzz, blush);

        // Draw HUD
        DrawHUD(&g);

        // Blit memory DC to screen
        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        // Cleanup
        SelectObject(memDC, oldObj);
        DeleteObject(memBM);
        DeleteDC(memDC);
        ReleaseDC(hwnd, hdc);
    }

    void DrawHUD(Gdiplus::Graphics* g) {
        LevelInfo info = GetLevelInfo(hamster.level);

        // HUD background bar (bottom of window)
        Gdiplus::RectF hudBar(0.0f, static_cast<float>(WINDOW_SIZE) - 35.0f, static_cast<float>(WINDOW_SIZE) + 40.0f, 35.0f);
        Gdiplus::SolidBrush hudBg(Gdiplus::Color(180, 0, 0, 0));
        g->FillRectangle(&hudBg, hudBar);

        // Level text
        std::wstring levelText = std::wstring(L"Lv.") + std::to_wstring(hamster.level) + L" " + info.title;
        Gdiplus::SolidBrush hudText(Gdiplus::Color(255, 255, 255, 255));
        g->SetTextRenderingHint(Gdiplus::TextRenderingHintSingleBitPerPixel);
        g->DrawString(levelText.c_str(), -1,
                      new Gdiplus::Font(L"Segoe UI", 10),
                      Gdiplus::PointF(5.0f, static_cast<float>(WINDOW_SIZE) - 30.0f), &hudText);

        // XP bar background
        Gdiplus::RectF xpBar(5.0f, static_cast<float>(WINDOW_SIZE) - 12.0f, static_cast<float>(WINDOW_SIZE) + 30.0f, 8.0f);
        Gdiplus::SolidBrush xpBg(Gdiplus::Color(100, 100, 100, 100));
        g->FillRectangle(&xpBg, xpBar);

        // XP bar fill
        int xpNeeded = info.xpNeeded;
        float xpRatio = static_cast<float>(hamster.currentXP) / xpNeeded;
        if (xpRatio > 1.0f) xpRatio = 1.0f;
        Gdiplus::RectF xpFill(5.0f, static_cast<float>(WINDOW_SIZE) - 12.0f, (static_cast<float>(WINDOW_SIZE) + 30.0f) * xpRatio, 8.0f);
        Gdiplus::SolidBrush xpFillBrush(Gdiplus::Color(255, 255, 215, 0));  // Gold
        g->FillRectangle(&xpFillBrush, xpFill);

        // XP text
        std::wstring xpText = std::to_wstring(hamster.currentXP) + L"/" + std::to_wstring(xpNeeded) + L" XP";
        Gdiplus::SolidBrush xpTextBrush(Gdiplus::Color(255, 255, 255, 255));
        g->DrawString(xpText.c_str(), -1,
                      new Gdiplus::Font(L"Segoe UI", 7),
                      Gdiplus::PointF(static_cast<float>(WINDOW_SIZE) + 15.0f, static_cast<float>(WINDOW_SIZE) - 10.0f), &xpTextBrush);

        // Toy info
        std::wstring toyText = std::wstring(L"玩具: ") + std::to_wstring(hamster.toysRemaining) + L"/3";
        Gdiplus::SolidBrush toyTextBrush(Gdiplus::Color(200, 200, 200, 200));
        g->DrawString(toyText.c_str(), -1,
                      new Gdiplus::Font(L"Segoe UI", 7),
                      Gdiplus::PointF(5.0f, 15.0f), &toyTextBrush);

        // Accessory info
        std::wstring accText = std::wstring(L"饰品: ") + AccessoryNames[static_cast<int>(hamster.accessory)];
        Gdiplus::SolidBrush accTextBrush(Gdiplus::Color(200, 200, 200, 200));
        g->DrawString(accText.c_str(), -1,
                      new Gdiplus::Font(L"Segoe UI", 7),
                      Gdiplus::PointF(5.0f, 28.0f), &accTextBrush);

        // Fur color info
        std::wstring colorText = std::wstring(L"毛色: ") + FurColorNames[static_cast<int>(hamster.furColor)];
        Gdiplus::SolidBrush colorTextBrush(Gdiplus::Color(200, 200, 200, 200));
        g->DrawString(colorText.c_str(), -1,
                      new Gdiplus::Font(L"Segoe UI", 7),
                      Gdiplus::PointF(5.0f, 41.0f), &colorTextBrush);

        // Evolution name
        if (!hamster.lastEvolutionName.empty()) {
            Gdiplus::SolidBrush evolText(Gdiplus::Color(200, 255, 215, 0));  // Gold
            g->DrawString(hamster.lastEvolutionName.c_str(), -1,
                          new Gdiplus::Font(L"Segoe UI", 8, Gdiplus::FontStyleBold),
                          Gdiplus::PointF(5.0f, 5.0f), &evolText);
        }

        // Level up notification
        if (hamster.leveledUp && hamster.levelUpTimer > 0) {
            Gdiplus::RectF notif(static_cast<float>(WINDOW_SIZE) / 2.0f - 80.0f, static_cast<float>(WINDOW_SIZE) / 2.0f - 20.0f, 160.0f, 40.0f);
            Gdiplus::SolidBrush notifBg(Gdiplus::Color(200, 255, 215, 0));
            g->FillRectangle(&notifBg, notif);
            g->DrawRectangle(new Gdiplus::Pen(Gdiplus::Color(255, 255, 215, 0), 2.0f), notif);

            std::wstring lvlUpText = L"升级了！";
            Gdiplus::SolidBrush lvlUpTextBrush(Gdiplus::Color(255, 0, 0, 0));
            g->DrawString(lvlUpText.c_str(), -1,
                          new Gdiplus::Font(L"Segoe UI", 14, Gdiplus::FontStyleBold),
                          Gdiplus::PointF(static_cast<float>(WINDOW_SIZE) / 2.0f - 40.0f, static_cast<float>(WINDOW_SIZE) / 2.0f - 5.0f), &lvlUpTextBrush);
        }
    }

    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_DESTROY:
            running = false;
            PostQuitMessage(0);
            break;
        case WM_PAINT:
            Render();
            break;
        case WM_LBUTTONDOWN:
            isDragging = true;
            hamsterDragOffsetX = LOWORD(lParam);
            hamsterDragOffsetY = HIWORD(lParam);
            hamster.OnMouseDown(LOWORD(lParam), HIWORD(lParam));
            SetCapture(hWnd);
            break;
        case WM_MOUSEMOVE: {
            int mx = LOWORD(lParam);
            int my = HIWORD(lParam);
            if (isDragging) {
                SetWindowPos(hWnd, HWND_TOPMOST,
                    hamster.pos_x + mx - hamsterDragOffsetX,
                    hamster.pos_y + my - hamsterDragOffsetY,
                    0, 0, SWP_NOSIZE);
                hamster.pos_x = hamster.pos_x + mx - hamsterDragOffsetX;
                hamster.pos_y = hamster.pos_y + my - hamsterDragOffsetY;
            }
            break;
        }
        case WM_LBUTTONUP:
            isDragging = false;
            ReleaseCapture();
            break;
        case WM_RBUTTONDOWN: {
            // Show context menu
            HMENU hMenu = CreatePopupMenu();
            if (hMenu) {
                // Check play cooldown
                if (hamster.playCooldown > 0) {
                    std::wstring playText = std::wstring(L"玩耍 (") + std::to_wstring(static_cast<int>(hamster.playCooldown)) + L"s)";
                    AppendMenuW(hMenu, MF_GRAYED, 1003, playText.c_str());
                } else if (hamster.toysRemaining <= 0) {
                    AppendMenuW(hMenu, MF_GRAYED, 1003, L"玩耍 (无玩具)");
                } else {
                    AppendMenuW(hMenu, MF_STRING, 1003, L"玩耍 (+5XP)");
                }

                AppendMenuW(hMenu, MF_STRING, 1002, L"喂食");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

                // 饰品子菜单
                HMENU hAccMenu = CreatePopupMenu();
                for (int i = 0; i < static_cast<int>(AccessoryType::COUNT); i++) {
                    AppendMenuW(hAccMenu, MF_STRING | (hamster.accessory == static_cast<AccessoryType>(i) ? MF_CHECKED : 0),
                        2001 + i, AccessoryNames[i]);
                }
                AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hAccMenu), L"饰品");

                // 颜色子菜单
                HMENU hColorMenu = CreatePopupMenu();
                for (int i = 0; i < static_cast<int>(FurColorType::COUNT); i++) {
                    AppendMenuW(hColorMenu, MF_STRING | (hamster.furColor == static_cast<FurColorType>(i) ? MF_CHECKED : 0),
                        3001 + i, FurColorNames[i]);
                }
                AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hColorMenu), L"颜色");

                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 1004, L"关闭");

                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hWnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY,
                                         pt.x, pt.y, 0, hWnd, nullptr);

                if (cmd == 1002) {
                    // Feed
                    if (hamster.state != PetState::SLEEPING) {
                        hamster.state = PetState::FEEDING;
                        hamster.currentFrame = 0;
                        hamster.frameTimer = 0;
                    }
                } else if (cmd == 1003 && hamster.playCooldown <= 0 && hamster.toysRemaining > 0) {
                    // Play - consumes toy, +5 XP
                    hamster.playCooldown = 0.0f;
                    hamster.toysRemaining--;
                    hamster.dailyPlayCount++;
                    if (hamster.state != PetState::SLEEPING) {
                        hamster.state = PetState::HAPPY;
                        hamster.currentFrame = 0;
                        hamster.frameTimer = 0;
                        hamster.AddXP(5);
                    }
                } else if (cmd >= 2001 && cmd < 2005) {
                    // Switch accessory
                    hamster.accessory = static_cast<AccessoryType>(cmd - 2001);
                } else if (cmd >= 3001 && cmd < 3006) {
                    // Switch fur color
                    hamster.furColor = static_cast<FurColorType>(cmd - 3001);
                    if (hamster.furColor == FurColorType::CUSTOM) {
                        // 自定义颜色：随机取一个颜色作为演示
                        hamster.customColor = Gdiplus::Color(255, rand() % 256, rand() % 256, rand() % 256);
                    }
                } else if (cmd == 1004) {
                    running = false;
                    PostQuitMessage(0);
                }

                DestroyMenu(hMenu);
            }
            break;
        }
        case WM_ERASEBKGND:
            return FALSE;  // Let Render handle background
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
        return 0;
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        App* pApp = reinterpret_cast<App*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (pApp) {
            return pApp->HandleMessage(hWnd, msg, wParam, lParam);
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    App app;
    if (!app.Initialize(hInstance)) {
        MessageBoxW(nullptr, L"初始化失败！", L"CutPet", MB_ICONERROR);
        return 1;
    }
    // Store app pointer in WndProc via GWLP_USERDATA
    SetWindowLongPtrW(app.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));
    app.Run();
    app.Shutdown();
    return 0;
}
