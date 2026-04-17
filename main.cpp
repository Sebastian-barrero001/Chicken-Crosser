#include "raylib.h"
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

// --- Constants & Configuration ---
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const float GRID_SIZE = 40.0f;
const int LANES_COUNT = 15;

enum GameState { MENU, PLAYING, GAME_OVER };
enum LaneType { GRASS, ROAD, WATER };

// --- Utility Functions ---
float GetRandomSpeed(int level) {
    return (float)GetRandomValue(100, 200) + (level * 20);
}

// --- High Score System ---
void SaveHighScore(int score) {
    std::ofstream file("highscore.txt");
    if (file.is_open()) {
        file << score;
        file.close();
    }
}

int LoadHighScore() {
    int score = 0;
    std::ifstream file("highscore.txt");
    if (file.is_open()) {
        file >> score;
        file.close();
    }
    return score;
}

// --- Game Objects ---

struct Particle {
    Vector2 pos;
    Vector2 speed;
    Color color;
    float lifeTime;
};

class Entity {
public:
    Rectangle rect;
    float speed;
    bool directionRight;
    Color color;

    Entity(float x, float y, float w, float h, float s, bool dir, Color c) {
        rect = { x, y, w, h };
        speed = s;
        directionRight = dir;
        color = c;
    }

    void Update(float dt) {
        if (directionRight) {
            rect.x += speed * dt;
            if (rect.x > SCREEN_WIDTH) rect.x = -rect.width;
        } else {
            rect.x -= speed * dt;
            if (rect.x < -rect.width) rect.x = SCREEN_WIDTH;
        }
    }

    void Draw() {
        DrawRectangleRec(rect, color);
        // Add a small "window" for cars
        if (rect.width > GRID_SIZE) {
            DrawRectangle(rect.x + (directionRight ? rect.width - 10 : 5), rect.y + 5, 5, 10, RAYWHITE);
        }
    }
};

class Player {
public:
    Vector2 gridPos;
    Rectangle rect;
    bool isDead;
    bool isInvincible;
    float powerUpTimer;

    Player() { Reset(); }

    void Reset() {
        gridPos = { (float)SCREEN_WIDTH / 2, (float)SCREEN_HEIGHT - GRID_SIZE };
        rect = { gridPos.x, gridPos.y, GRID_SIZE - 10, GRID_SIZE - 10 };
        isDead = false;
        isInvincible = false;
        powerUpTimer = 0;
    }

    void Update(Sound moveSound) {
        if (isDead) return;

        if (IsKeyPressed(KEY_UP) && gridPos.y > 0) {
            gridPos.y -= GRID_SIZE;
            PlaySound(moveSound);
        }
        if (IsKeyPressed(KEY_DOWN) && gridPos.y < SCREEN_HEIGHT - GRID_SIZE) {
            gridPos.y += GRID_SIZE;
            PlaySound(moveSound);
        }
        if (IsKeyPressed(KEY_LEFT) && gridPos.x > 0) {
            gridPos.x -= GRID_SIZE;
            PlaySound(moveSound);
        }
        if (IsKeyPressed(KEY_RIGHT) && gridPos.x < SCREEN_WIDTH - GRID_SIZE) {
            gridPos.x += GRID_SIZE;
            PlaySound(moveSound);
        }

        rect.x = gridPos.x + 5;
        rect.y = gridPos.y + 5;

        if (powerUpTimer > 0) {
            powerUpTimer -= GetFrameTime();
        } else {
            isInvincible = false;
        }
    }

    void Draw() {
        Color c = isInvincible ? GOLD : ORANGE;
        DrawRectangleRec(rect, c);
        DrawRectangle(rect.x + 5, rect.y + 5, 5, 5, BLACK); // Eye
    }
};

class Lane {
public:
    LaneType type;
    float y;
    std::vector<Entity> obstacles;
    Rectangle powerUp;
    bool hasPowerUp;

    Lane(float yPos, LaneType t, int level) {
        y = yPos;
        type = t;
        hasPowerUp = (GetRandomValue(0, 10) > 8);
        if (hasPowerUp) {
            powerUp = { (float)GetRandomValue(50, SCREEN_WIDTH - 50), y + 10, 20, 20 };
        }

        if (type != GRASS) {
            bool dir = GetRandomValue(0, 1);
            float speed = GetRandomSpeed(level);
            int spacing = GetRandomValue(200, 400);
            
            for (int i = 0; i < 3; i++) {
                Color carColor = (type == ROAD) ? Color{ (unsigned char)GetRandomValue(50, 255), 0, 0, 255 } : DARKBLUE;
                obstacles.push_back(Entity(i * spacing, y + 5, (type == ROAD ? 60 : 100), GRID_SIZE - 10, speed, dir, carColor));
            }
        }
    }

    void Update(float dt, Sound beep) {
        for (auto& obs : obstacles) {
            obs.Update(dt);
            if (type == ROAD && GetRandomValue(0, 1000) == 1) PlaySound(beep);
        }
    }

    void Draw() {
        Color laneColor = (type == GRASS) ? DARKGREEN : (type == ROAD ? GRAY : BLUE);
        DrawRectangle(0, y, SCREEN_WIDTH, GRID_SIZE, laneColor);
        for (auto& obs : obstacles) obs.Draw();
        if (hasPowerUp) DrawCircleV({powerUp.x + 10, powerUp.y + 10}, 8, GOLD);
    }
};

// --- Main Game Class ---

class Game {
public:
    Player player;
    std::vector<Lane> lanes;
    std::vector<Particle> particles;
    GameState state;
    int score;
    int highScore;
    int level;

    Sound moveSnd, dieSnd, beepSnd, powerSnd;

    Game() {
        InitAudioDevice();
        moveSnd = LoadSound("move.wav"); // Ensure these files exist or use dummy waves
        dieSnd = LoadSound("die.wav");
        beepSnd = LoadSound("beep.wav");
        powerSnd = LoadSound("powerup.wav");
        
        highScore = LoadHighScore();
        InitGame();
        state = MENU;
    }

    void InitGame() {
        player.Reset();
        lanes.clear();
        score = 0;
        level = 1;
        for (int i = 0; i < LANES_COUNT; i++) {
            LaneType t = (i == 0 || i == LANES_COUNT - 1) ? GRASS : (LaneType)GetRandomValue(0, 2);
            lanes.push_back(Lane(i * GRID_SIZE, t, level));
        }
    }

    void CreateDeathEffect() {
        for (int i = 0; i < 20; i++) {
            particles.push_back({{player.rect.x, player.rect.y}, 
                                 {(float)GetRandomValue(-100, 100), (float)GetRandomValue(-100, 100)}, 
                                 ORANGE, 1.0f});
        }
    }

    void Update() {
        if (state == MENU) {
            if (IsKeyPressed(KEY_ENTER)) state = PLAYING;
            return;
        }

        if (state == GAME_OVER) {
            if (IsKeyPressed(KEY_ENTER)) {
                InitGame();
                state = PLAYING;
            }
            return;
        }

        float dt = GetFrameTime();
        player.Update(moveSnd);

        // Update Level & Score
        int currentScore = (SCREEN_HEIGHT - player.gridPos.y) / GRID_SIZE;
        if (currentScore > score) {
            score = currentScore;
            if (score % 5 == 0) level++; 
        }

        bool onLog = false;
        for (auto& lane : lanes) {
            lane.Update(dt, beepSnd);

            // Power-up collection
            if (lane.hasPowerUp && CheckCollisionRecs(player.rect, lane.powerUp)) {
                lane.hasPowerUp = false;
                player.isInvincible = true;
                player.powerUpTimer = 5.0f;
                PlaySound(powerSnd);
            }

            // Collision Logic
            if (std::abs(lane.y - player.gridPos.y) < 5) {
                for (auto& obs : lane.obstacles) {
                    if (CheckCollisionRecs(player.rect, obs.rect)) {
                        if (lane.type == ROAD && !player.isInvincible) player.isDead = true;
                        if (lane.type == WATER) {
                            onLog = true;
                            player.gridPos.x += obs.speed * (obs.directionRight ? 1 : -1) * dt;
                        }
                    }
                }
                if (lane.type == WATER && !onLog && !player.isInvincible) player.isDead = true;
            }
        }

        // Particle update
        for (int i = particles.size() - 1; i >= 0; i--) {
            particles[i].pos.x += particles[i].speed.x * dt;
            particles[i].pos.y += particles[i].speed.y * dt;
            particles[i].lifeTime -= dt;
            if (particles[i].lifeTime <= 0) particles.erase(particles.begin() + i);
        }

        if (player.isDead) {
            PlaySound(dieSnd);
            CreateDeathEffect();
            if (score > highScore) {
                highScore = score;
                SaveHighScore(highScore);
            }
            state = GAME_OVER;
        }
    }

    void Draw() {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (state == MENU) {
            DrawText("CROSSY CHICKEN", SCREEN_WIDTH/2 - 150, 200, 40, DARKGREEN);
            DrawText("Press ENTER to Start", SCREEN_WIDTH/2 - 120, 300, 20, DARKGRAY);
        } else {
            for (auto& lane : lanes) lane.Draw();
            player.Draw();
            for (auto& p : particles) DrawCircleV(p.pos, 3, p.color);

            DrawText(TextFormat("Score: %i", score), 10, 10, 20, WHITE);
            DrawText(TextFormat("Level: %i", level), 10, 35, 20, WHITE);
            DrawText(TextFormat("High Score: %i", highScore), 10, 60, 20, GOLD);
            
            if (player.isInvincible) DrawText("INVINCIBLE!", SCREEN_WIDTH - 150, 10, 20, GOLD);

            if (state == GAME_OVER) {
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.7f));
                DrawText("GAME OVER", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 40, 40, RED);
                DrawText("Press ENTER to Restart", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 + 20, 20, RAYWHITE);
            }
        }
        EndDrawing();
    }

    ~Game() {
        UnloadSound(moveSnd);
        UnloadSound(dieSnd);
        UnloadSound(beepSnd);
        UnloadSound(powerSnd);
        CloseAudioDevice();
    }
};

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "RayLib Crossy Road");
    SetTargetFPS(60);

    Game game;

    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }

    CloseWindow();
    return 0;
}