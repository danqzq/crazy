#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "raylib.h"

#include <stdio.h>

#pragma region Macros

#include <string.h>
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 1024

#define WALL_SIZE 80
#define BOUNDS_X (Vector2) { WALL_SIZE, SCREEN_WIDTH - WALL_SIZE }
#define BOUNDS_Y (Vector2) { WALL_SIZE, SCREEN_HEIGHT - WALL_SIZE }

#define TARGET_FPS 60

#define SCALE_FACTOR 100

#define BACKGROUND_COLOR CLITERAL(Color){ 130, 90, 100, 255 }

#define MAX_NAME_INPUT_CHARS 16

#define LEVEL_COUNT 5

#pragma endregion

#pragma region Types

typedef struct {
    Vector2 position;
    float rotation;
    Vector2 scale;

    Vector2 velocity;
} Entity;

typedef struct {
    Entity* entity;

    int type;
    bool isEnraged;

    float throwTimer;
    Vector2 throwPosition;
} Rat;

typedef struct {
    int initialSanity;
    int maxRatCapacity;
    int maxExplosiveRatCapacity;
    bool isFatRatEnabled;
    bool isPowerGeneratorEnabled;
} LevelData;

#pragma endregion

#pragma region Functions

float max(float a, float b) {
    return a > b ? a : b;
}

float min(float a, float b) {
    return a < b ? a : b;
}

float clamp(float value, float minValue, float maxValue) {
    return max(minValue, min(value, maxValue));
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Vector2 getDirection(Vector2 a, Vector2 b) {
    return (Vector2) { b.x - a.x, b.y - a.y };
}

Vector2 normalize(Vector2 vector) {
    float length = sqrt(vector.x * vector.x + vector.y * vector.y);
    return (Vector2) { vector.x / length, vector.y / length };
}

float distance(Vector2 a, Vector2 b) {
    return sqrt(pow(b.x - a.x, 2) + pow(b.y - a.y, 2));
}

float lookAt(Vector2 pointA, Vector2 pointB) {
    float angle = atan2(pointB.y - pointA.y, pointB.x - pointA.x);
    if (angle < 0) {
        angle += 2 * PI;
    }
    return angle * 180 / PI;
}

#pragma endregion

#pragma region Global Variables

const float PLAYER_SPEED = 100.0f;
const float ENEMY_SPAWN_TIME = 1.0f;
const float CHEESE_DECREASE_RATE = 1.0f;
const float SANITY_DECREASE_RATE = 1.5f;
const float FLASHLIGHT_DECREASE_RATE = 2.0f;
const float FLASHLIGHT_CHARGE_RATE = 10.0f;
const float POWER_GENERATOR_RAT_ESCAPE_TIME = 5.0f;
const float FAT_RAT_SPAWN_TIME = 5.0f;
const float FAT_RAT_TEETH_MAX_POSITION = 500.0f;
const float SCREEN_FLICKER_TIME = 60.0f;
const float SURVIVAL_TIME = 80.0f;
const float HOUR_LENGTH_IN_SECONDS = SURVIVAL_TIME / 9.0f;

const LevelData LEVELS[] = {
        (LevelData) { 100, 2, 0, false, false },
        (LevelData) { 100, 2, 0, false, false },
        (LevelData) { 90, 3, 0, true, false },
        (LevelData) { 90, 3, 0, true, true },
        (LevelData) { 80, 4, 1, false, true },
        (LevelData) { 80, 5, 2, true, true },
};

static int currentLevel = 0;
static float levelTransitionTimer = 0.0f;
static bool isLevelTransitioning = true;

static bool isGameOver = false;

static float currentTime = 0.0f;

static Entity player;
static float sanity = 100.0f;
static float cheese = 100.0f;
static float health = 100.0f;
static float flashlight = 100.0f;
static Rat* currentRatOnPlayer = NULL;

static Rat* enemies;
static int enemiesCount = 0;
static float enemySpawnTimer = 0.0f;
static Rat* currentDraggedRat = NULL;

static Rat* explosiveRats;
static int explosiveRatCount = 0;
static float explosiveRatSpawnTimer = 0.0f;
static float explosionTimer = 0.0f;
static Vector2 lastExplosionLocation = (Vector2) { 0.0f, 0.0f };

static Entity cheeseEntity;
static bool isCheeseDragged;

static Entity powerGenerator;
static Rat* currentRatOnPowerGenerator = NULL;
static float powerGeneratorTimer = 0.0f;

static Entity* electricityParticles;
static Entity* mutateParticles;
static int mutateParticlesCount = 0;
static float mutateParticlesTimer = 0.0f;
static Vector2 lastMutationLocation = (Vector2) { 0.0f, 0.0f };
static Entity* bloodParticles;
static int bloodParticlesCount = 0;
static float bloodParticlesTimer = 0.0f;
static Vector2 lastBloodLocation = (Vector2) { 0.0f, 0.0f };
static float bloodTextureRotation = 0.0f;

static Entity fatRat;
static float fatRatTimer = 0.0f;
static bool isFatRatSpawned = false;
static int numberOfRatsFed = 0;
static float fatRatTeethPosition = 0.0f;
static float lastBiteTime = 0.0f;

static float screenFlickerTimer = SCREEN_FLICKER_TIME;
static bool isScreenFlickering = false;

static float redFlashIntensity = 0.0f;

static int score = 0;
static int highscore = 0;
static float scoreTimer = 0.0f;

static char username[MAX_NAME_INPUT_CHARS + 1] = "\0";
static int usernameSize = 0;
static int inputFieldFrames = 0;
static bool submittedScore = false;

static float cutsceneTimer = 0.0f;

static bool isFinishedGame = false;
static float endingTimer = 0.0f;

static bool isCheeseInsane = false;

static Music ambienceMusic;
static Music cutsceneMusic;

static Sound clockSound, elecSound, explosionSound, nomSound, poofSound, popSound1, popSound2, screamingSound, sniffSound;
static Sound squeakSound1, squeakSound2, squeakSound3, splatSound, crazySound, biteSound;

#pragma endregion

#pragma region Textures

static Texture2D spotlightTexture;
static Texture2D wallsTexture;
static Texture2D cheeseTexture;
static Texture2D cheeseWalkTextures[2];
static Texture2D powerGeneratorTexture;

static Texture2D playerTextureSpritesheet;
static Texture2D playerScarsTextures[2];

static Texture2D ratTextureSpritesheet;
static Texture2D explosiveRatTexture;
static Texture2D fatRatTexture, fatRatHappyTexture;
static Rectangle fatRatRect;

static Texture2D fatRatTeeth;

static Texture2D redFlashTexture;
static Texture2D tutorial[4];

static int currentHandTexture = 0;
static Texture2D handTextures[3];

static Texture2D electricityParticleTexture;
static Texture2D poofTexture;
static Texture2D nomTexture;
static Texture2D bloodTexture;
static Texture2D explosionTexture;

static Texture2D cutscenes[2];
static Texture2D playerFalling;
static bool isCutscenePlaying;

static Texture2D spaceButtonTexture;

static Texture2D endCutscene;

#pragma endregion

#pragma region Networking

char *USER_GUID = NULL;

void authorized(emscripten_fetch_t *fetch) {
    USER_GUID = malloc(sizeof(char) * (strlen(fetch->data) + 1));
    strcpy(USER_GUID, fetch->data);
    emscripten_fetch_close(fetch);
}

void scoreSubmitted(emscripten_fetch_t *fetch) {
    if (fetch->status == 200) {
        printf("Successfully uploaded score.\n");
    }
    else {
        printf("Score upload failed, HTTP failure status code: %d.\n", fetch->status);
    }
    emscripten_fetch_close(fetch);
    submittedScore = false;
}

void requestFailed(emscripten_fetch_t *fetch) {
    printf("Request failed, HTTP failure status code: %d.\n", fetch->status);
    emscripten_fetch_close(fetch);
    submittedScore = false;
}

unsigned int EMSCRIPTEN_KEEPALIVE InitializeLeaderboardCreator() {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = authorized;
    attr.onerror = requestFailed;
    emscripten_fetch(&attr, "https://lcv2-server.danqzq.games/authorize");
    return 1;
}

unsigned int EMSCRIPTEN_KEEPALIVE UploadNewEntry() {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = scoreSubmitted;
    attr.onerror = requestFailed;
    const char * headers[] = {"Content-Type", "multipart/form-data; boundary=ANNKwve0ozXAeZrQFMSbveVVr7Mgj5OU1dRnNtlT", 0};
    attr.requestHeaders = headers;

    const char *params = TextFormat("--ANNKwve0ozXAeZrQFMSbveVVr7Mgj5OU1dRnNtlT\nContent-Disposition: form-data; name=\"publicKey\"\n\nb0a306dcf0a7bbc6559dea064d959b469f49ad1b5b7721e1f187b39ae8cd3a67\n--ANNKwve0ozXAeZrQFMSbveVVr7Mgj5OU1dRnNtlT\nContent-Disposition: form-data; name=\"username\"\n\n%s\n--ANNKwve0ozXAeZrQFMSbveVVr7Mgj5OU1dRnNtlT\nContent-Disposition: form-data; name=\"score\"\n\n%i\n--ANNKwve0ozXAeZrQFMSbveVVr7Mgj5OU1dRnNtlT\nContent-Disposition: form-data; name=\"userGuid\"\n\n%s\n--ANNKwve0ozXAeZrQFMSbveVVr7Mgj5OU1dRnNtlT--\n", username, highscore, USER_GUID);
    attr.requestData = params;
    attr.requestDataSize = strlen(attr.requestData);
    emscripten_fetch(&attr, "https://lcv2-server.danqzq.games/entry/upload");
    return 1;
}

#pragma endregion

void LoadLevelData(void) {
    LevelData currentLevelData = LEVELS[currentLevel];
    sanity = currentLevelData.initialSanity;
    enemies = malloc(sizeof(Rat) * currentLevelData.maxRatCapacity);
    explosiveRats = malloc(sizeof(Rat) * currentLevelData.maxExplosiveRatCapacity);
}

void Start(void) {
    isCutscenePlaying = true;

    player = (Entity) {
        .position = (Vector2) { 400.0f, 400.0f },
        .rotation = 0.0f,
        .scale = (Vector2) { 1.0f, 1.0f },
        .velocity = (Vector2) { 0.0f, 0.0f }
    };

    cheeseEntity = (Entity) {
        .position = (Vector2) { SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f },
        .rotation = 0.0f,
        .scale = (Vector2) { 0.5f, 0.5f },
        .velocity = (Vector2) { 0.0f, 0.0f }
    };

    powerGenerator = (Entity) {
        .position = (Vector2) { SCREEN_WIDTH - 150, SCREEN_HEIGHT - 150 },
        .rotation = 0.0f,
        .scale = (Vector2) { 1.0f, 1.0f },
        .velocity = (Vector2) { 0.0f, 0.0f }
    };

    fatRat = (Entity) {
        .position = (Vector2) { SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f },
        .rotation = 0.0f,
        .scale = (Vector2) { 2.0f, 2.0f },
        .velocity = (Vector2) { 0.0f, 0.0f }
    };

    isCheeseDragged = false;

    cutscenes[0] = LoadTexture("resources/cutscene0.png");
    cutscenes[1] = LoadTexture("resources/cutscene1.png");
    playerFalling = LoadTexture("resources/falling.png");
    endCutscene = LoadTexture("resources/end.png");

    playerScarsTextures[0] = LoadTexture("resources/scars1.png");
    playerScarsTextures[1] = LoadTexture("resources/scars2.png");

    handTextures[0] = LoadTexture("resources/hand.png");
    handTextures[1] = LoadTexture("resources/hand_rat.png");
    handTextures[2] = LoadTexture("resources/hand_cheese.png");

    spotlightTexture = LoadTexture("resources/spotlight.png");
    wallsTexture = LoadTexture("resources/walls.png");
    cheeseTexture = LoadTexture("resources/cheese_normal.png");
    cheeseWalkTextures[0] = LoadTexture("resources/cheese_walk1.png");
    cheeseWalkTextures[1] = LoadTexture("resources/cheese_walk2.png");

    playerTextureSpritesheet = LoadTexture("resources/player_spritesheet.png");

    ratTextureSpritesheet = LoadTexture("resources/rats.png");
    explosiveRatTexture = LoadTexture("resources/explosive_rat.png");
    fatRatTexture = LoadTexture("resources/fatrat.png");
    fatRatHappyTexture = LoadTexture("resources/fatrat_happy.png");
    fatRatRect = (Rectangle) { 0, 0, fatRatTexture.width, fatRatTexture.height };
    fatRatTeeth = LoadTexture("resources/teeth.png");

    redFlashTexture = LoadTexture("resources/red_flash.png");

    powerGeneratorTexture = LoadTexture("resources/power_generator.png");
    electricityParticleTexture = LoadTexture("resources/elec.png");

    poofTexture = LoadTexture("resources/poof.png");
    nomTexture = LoadTexture("resources/nom.png");
    bloodTexture = LoadTexture("resources/blood.png");
    explosionTexture = LoadTexture("resources/boom.png");

    spaceButtonTexture = LoadTexture("resources/space_button.png");

    tutorial[0] = LoadTexture("resources/tutorial1.png");
    tutorial[1] = LoadTexture("resources/tutorial2.png");
    tutorial[2] = LoadTexture("resources/tutorial3.png");
    tutorial[3] = LoadTexture("resources/tutorial4.png");

    InitAudioDevice();

    clockSound = LoadSound("resources/clock.wav");
    biteSound = LoadSound("resources/bite.wav");
    elecSound = LoadSound("resources/elec.wav");
    explosionSound = LoadSound("resources/explosion.wav");
    nomSound = LoadSound("resources/nom.wav");
    poofSound = LoadSound("resources/poof.wav");
    popSound1 = LoadSound("resources/pop1.wav");
    popSound2 = LoadSound("resources/pop2.wav");
    screamingSound = LoadSound("resources/screaming.wav");
    sniffSound = LoadSound("resources/sniff.wav");
    squeakSound1 = LoadSound("resources/squeak1.wav");
    squeakSound2 = LoadSound("resources/squeak2.wav");
    squeakSound3 = LoadSound("resources/squeak3.wav");
    splatSound = LoadSound("resources/splat.wav");
    crazySound = LoadSound("resources/crazy.wav");

    ambienceMusic = LoadMusicStream("resources/ambience.wav");
    cutsceneMusic = LoadMusicStream("resources/music.mp3");
    ambienceMusic.looping = true;
    cutsceneMusic.looping = true;
    PlayMusicStream(ambienceMusic);
    PlayMusicStream(cutsceneMusic);

    electricityParticles = malloc(sizeof(Entity) * 10);
    for (int i = 0; i < 10; i++) {
        electricityParticles[i] = (Entity) {
            .position = powerGenerator.position,
            .rotation = 0.0f,
            .scale = (Vector2) { 0.25f, 0.25f },
            .velocity = (Vector2) { 0.0f, 0.0f }
        };
        electricityParticles[i].position.x += rand() % 100 - 50;
    }

    InitializeLeaderboardCreator();
    HideCursor();

    LoadLevelData();
}

void ResetLevel(bool fullRestart) {
    currentTime = 0.0f;
    enemiesCount = 0;
    if (fullRestart) {
        currentLevel = 0;
        health = 100.0f;
    } else {
        health = clamp(health + cheese, 0.0f, 100.0f);
    }

    cheese = 100.0f;
    flashlight = 100.0f;
    LoadLevelData();

    numberOfRatsFed = 0;
    isFatRatSpawned = false;
    fatRatTimer = 0.0f;
    fatRatTeethPosition = 0.0f;

    screenFlickerTimer = SCREEN_FLICKER_TIME;
    enemies = realloc(enemies, 0);
    currentDraggedRat = NULL;
    currentRatOnPowerGenerator = NULL;
    currentRatOnPlayer = NULL;

    isCheeseDragged = false;
    isCheeseInsane = false;

    player.position = (Vector2) { 400.0f, 400.0f };
    cheeseEntity.position = (Vector2) { SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f };

    HideCursor();
}

void UpdateCutscenes(void) {
    cutsceneTimer += GetFrameTime();
    if (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT))
        cutsceneTimer = 15.0f;

    ClearBackground(BLACK);
    if (cutsceneTimer <= 10.0f) {
        DrawTexturePro(cutscenes[0], (Rectangle) { 0, 0, cutscenes[0].width, cutscenes[0].height },
                       (Rectangle) { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                       (Vector2) { 0, 0 }, 0, (Color) { 255, 255, 255, min(cutsceneTimer * 255, 255) });
        DrawRectanglePro((Rectangle) { 0, SCREEN_HEIGHT * 0.5f, SCREEN_WIDTH, SCREEN_HEIGHT * 0.5f },
                         (Vector2) { 0, 0 }, 0, (Color) { 0, 0, 0, clamp(255 * 2 - cutsceneTimer * 255, 0, 255) });
        DrawRectanglePro((Rectangle) { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                         (Vector2) { 0, 0 }, 0, (Color) { 0, 0, 0, clamp(cutsceneTimer * 255 - 255 * 4, 0, 255) });
        DrawTexturePro(cutscenes[1], (Rectangle) { 0, 0, cutscenes[1].width, cutscenes[1].height },
                       (Rectangle) { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                       (Vector2) { 0, 0 }, 0, (Color) { 255, 255, 255, clamp(cutsceneTimer * 255 - 255 * 5, 0, 255) });
        if (cutsceneTimer >= 5.0f) {
            DrawRectanglePro((Rectangle) {0, SCREEN_HEIGHT * 0.5f, SCREEN_WIDTH, SCREEN_HEIGHT * 0.5f},
                             (Vector2) {0, 0}, 0, (Color) {0, 0, 0, clamp(255 * 8 - cutsceneTimer * 255, 0, 255)});
        }
        return;
    }

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color) { 255, 255, 255, clamp(14 * 255 - cutsceneTimer * 128, 0, 255) });

    DrawTexturePro(cutscenes[1], (Rectangle) { 0, 0, cutscenes[1].width, cutscenes[1].height },
                   (Rectangle) { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                   (Vector2) { 0, 0 }, 0, (Color) { 255, 255, 255, clamp(11 * 255 - cutsceneTimer * 255, 0, 255) });

    float size = -cutsceneTimer + 10.0f + 5.0f;

    float w = SCREEN_WIDTH * size;
    float h = SCREEN_HEIGHT * size;

    DrawTexturePro(playerFalling, (Rectangle) { 0, 0, playerFalling.width, playerFalling.height },
                   (Rectangle) { SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, w, h },
                   (Vector2) { w * 0.5f, h * 0.5f }, size * 50, (Color) { 255, 255, 255, clamp(cutsceneTimer * 255 - 255 * 11, 0, 255)});

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color) { 0, 0, 0, clamp(cutsceneTimer * 255 - 255 * 14, 0, 255) });

    if (cutsceneTimer >= 15.0f) {
        isCutscenePlaying = false;
        cutsceneTimer = 0.0f;
        PlaySound(clockSound);
    }
}

void UpdateStats(void) {
    if (cheese <= 0.0f || sanity <= 0.0f || health <= 0.0f) {
        isGameOver = true;
        ShowCursor();
        if (score > highscore) {
            highscore = score;
            score = 0;
        }
    }

    scoreTimer += GetFrameTime();
    if (scoreTimer >= 1.0f) {
        scoreTimer = 0.0f;
        score++;
    }

    currentTime += GetFrameTime();

    if (currentTime >= SURVIVAL_TIME) {
        isLevelTransitioning = true;
        PlaySound(clockSound);
    }

    if (!LEVELS[currentLevel].isPowerGeneratorEnabled)
        return;

    if (currentRatOnPowerGenerator == NULL && flashlight > 0.0f) {
        flashlight -= FLASHLIGHT_DECREASE_RATE * GetFrameTime();
    } else if (flashlight < 100.0f) {
        flashlight += FLASHLIGHT_CHARGE_RATE * GetFrameTime();
    }
}

void OnDamageTaken(void) {
    redFlashIntensity = 1.0f;
}

void UpdateCheese(void) {
    if (isCheeseDragged) return;

    float w = cheeseTexture.width * 0.125f;
    float h = cheeseTexture.height * 0.125f;
    if (sanity <= 50.0f) {
        if (!isCheeseInsane) {
            isCheeseInsane = true;
            PlaySound(screamingSound);
        }
        Rat* closestRat = NULL;
        float closestDistance = 1000.0f;
        for (int i = 0; i < enemiesCount; ++i) {
            Rat* rat = &enemies[i];

            if (distance(rat->entity->position, cheeseEntity.position) < closestDistance) {
                closestDistance = distance(rat->entity->position, cheeseEntity.position);
                closestRat = rat;
            }
        }

        if (closestRat != NULL) {
            Vector2 direction = normalize(getDirection(cheeseEntity.position, closestRat->entity->position));
            cheeseEntity.velocity.x = -direction.x * 50;
            cheeseEntity.velocity.y = -direction.y * 50;

            cheeseEntity.position.x += cheeseEntity.velocity.x * GetFrameTime();
            cheeseEntity.position.y += cheeseEntity.velocity.y * GetFrameTime();

            cheeseEntity.position.x = clamp(cheeseEntity.position.x, BOUNDS_X.x, BOUNDS_X.y);
            cheeseEntity.position.y = clamp(cheeseEntity.position.y, BOUNDS_Y.x, BOUNDS_Y.y);

            int index = (int) (GetTime() * 5) % 2;

            DrawTexturePro(cheeseWalkTextures[index], (Rectangle) { 0, 0, cheeseWalkTextures[index].width *
                                                                          (fabsf(cheeseEntity.velocity.x - 1.0f) < 1.0f || cheeseEntity.velocity.x > 0 ? 1 : -1), cheeseWalkTextures[index].height },
                           (Rectangle) { cheeseEntity.position.x, cheeseEntity.position.y, w, h },
                           (Vector2) { w * 0.5f, h * 0.5f }, 0, WHITE);

            return;
        }
    }

    DrawTexturePro(cheeseTexture, (Rectangle) {0, 0, cheeseTexture.width, cheeseTexture.height},
                   (Rectangle) {cheeseEntity.position.x, cheeseEntity.position.y, w, h},
                   (Vector2) {w * 0.5f, h * 0.5f}, 0, WHITE);
}

void UpdatePlayer(void) {
    Vector2 mousePosition = GetMousePosition();
    float angle = lookAt(player.position, mousePosition);
    player.rotation = angle + 90;

    if (IsKeyDown(KEY_W)) {
        player.velocity.y = -PLAYER_SPEED;
    } else if (IsKeyDown(KEY_S)) {
        player.velocity.y = PLAYER_SPEED;
    } else {
        player.velocity.y = 0;
    }

    if (IsKeyDown(KEY_A)) {
        player.velocity.x = -PLAYER_SPEED;
    } else if (IsKeyDown(KEY_D)) {
        player.velocity.x = PLAYER_SPEED;
    } else {
        player.velocity.x = 0;
    }

    player.position.x += player.velocity.x * GetFrameTime();
    player.position.y += player.velocity.y * GetFrameTime();

    if (player.position.x < BOUNDS_X.x) {
        player.position.x = BOUNDS_X.x;
    } else if (player.position.x > BOUNDS_X.y) {
        player.position.x = BOUNDS_X.y;
    }

    if (player.position.y < BOUNDS_Y.x) {
        player.position.y = BOUNDS_Y.x;
    } else if (player.position.y > BOUNDS_Y.y) {
        player.position.y = BOUNDS_Y.y;
    }

    float w = playerTextureSpritesheet.width / 4.0f;
    float h = playerTextureSpritesheet.height;
    Rectangle sourceRec = (Rectangle) { 0, 0, w, h };
    if (sanity <= 25.0f) {
        sourceRec.x = 256 * 3;
    } else if (sanity <= 50.0f) {
        sourceRec.x = 256 * 2;
    } else if (sanity <= 75.0f) {
        sourceRec.x = 256 * 1;
    }

    DrawTexturePro(playerTextureSpritesheet, sourceRec,
                   (Rectangle) { player.position.x, player.position.y, w * 0.5f, h * 0.5f },
                   (Vector2) { w * 0.25f, h * 0.25f }, player.rotation - 90, WHITE);

    if (health <= 25.0f) {
        DrawTexturePro(playerScarsTextures[1],
                       (Rectangle) {0, 0, playerScarsTextures[1].width, playerScarsTextures[1].height},
                       (Rectangle) {player.position.x, player.position.y, w * 0.5f, h * 0.5f},
                       (Vector2) {w * 0.25f, h * 0.25f}, player.rotation - 90, WHITE);
    } else if (health <= 50.0f) {
        DrawTexturePro(playerScarsTextures[0],
                       (Rectangle) {0, 0, playerScarsTextures[0].width, playerScarsTextures[0].height},
                       (Rectangle) {player.position.x, player.position.y, w * 0.5f, h * 0.5f},
                       (Vector2) {w * 0.25f, h * 0.25f}, player.rotation - 90, WHITE);
    }

    if (currentRatOnPlayer != NULL) {
        Entity* entity = currentRatOnPlayer->entity;
        w = entity->scale.x * SCALE_FACTOR + 25;
        h = entity->scale.y * SCALE_FACTOR + 25;
        sourceRec = (Rectangle) { clamp(currentRatOnPlayer->type - 1, 0, 4) * 256, 0, ratTextureSpritesheet.width / 4.0f, ratTextureSpritesheet.height };

        DrawTexturePro(ratTextureSpritesheet, sourceRec,
                       (Rectangle) { player.position.x, player.position.y, w, h },
                       (Vector2) { w * 0.5f, h * 0.5f }, player.rotation - 90 + sinf(GetTime() * 20) * 50, WHITE);

        float damage = clamp(2 * currentRatOnPlayer->type, 0, 8);
        health -= damage * GetFrameTime();
        OnDamageTaken();

        if (IsKeyPressed(KEY_SPACE)) {
            Vector2 position = (Vector2) { 0, 0 };
            position.x = player.position.x + cosf((player.rotation - 90) * PI / 180) * 300;
            position.y = player.position.y + sinf((player.rotation - 90) * PI / 180) * 300;
            currentRatOnPlayer->throwPosition = position;
            currentRatOnPlayer->throwTimer = 0.5f;
            currentRatOnPlayer = NULL;
        }
    }
}

void UpdateFatRat(void) {
    fatRatTimer += GetFrameTime();
    if (fatRatTimer < FAT_RAT_SPAWN_TIME) return;
    Vector2 playerToMouse = normalize(getDirection(player.position, GetMousePosition()));
    if (!isFatRatSpawned) {
        isFatRatSpawned = true;
        fatRat.position.x = player.position.x - playerToMouse.x * 500;
        fatRat.position.y = player.position.y - playerToMouse.y * 500;
        PlaySound(sniffSound);
        return;
    }
    Vector2 direction = normalize(getDirection(fatRat.position, player.position));

    if (numberOfRatsFed >= 3) {
        fatRat.velocity.x = -direction.x * 400;
        fatRat.velocity.y = -direction.y * 400;
    } else {
        fatRat.velocity.x = direction.x * 100;
        fatRat.velocity.y = direction.y * 100;
    }

    float w = fatRat.scale.x * SCALE_FACTOR;
    float h = fatRat.scale.y * SCALE_FACTOR;

    float distanceToPlayer = distance(fatRat.position, player.position);
    if (distanceToPlayer < w * 0.5f) {
        health -= 10 * GetFrameTime();
        fatRat.velocity.x = 0;
        fatRat.velocity.y = 0;

        redFlashIntensity = cosf(GetTime() * 10) * 0.5f + 0.5f;

        if (lastBiteTime + 0.5f < GetTime()) {
            lastBiteTime = GetTime();
            PlaySound(biteSound);
        }

        fatRatTeethPosition = cosf(GetTime() * 10) * FAT_RAT_TEETH_MAX_POSITION;
        fatRatTeethPosition = clamp(fatRatTeethPosition, 0, FAT_RAT_TEETH_MAX_POSITION);
    } else if (distanceToPlayer > SCREEN_WIDTH) {
        isFatRatSpawned = false;
        fatRatTimer = 0.0f;
        numberOfRatsFed = 0;
    } else {
        fatRatTeethPosition = lerp(fatRatTeethPosition, 0, 0.1f);
    }

    fatRat.rotation = lookAt(fatRat.position, player.position) + 90;
    fatRat.position.x += fatRat.velocity.x * GetFrameTime();
    fatRat.position.y += fatRat.velocity.y * GetFrameTime();

    DrawTexturePro(numberOfRatsFed >= 3 ? fatRatHappyTexture : fatRatTexture, fatRatRect,
                   (Rectangle) { fatRat.position.x, fatRat.position.y, w, h },
                   (Vector2) { w * 0.5f, h * 0.5f }, fatRat.rotation - 90, WHITE);
}

void UpdateRatSpawner() {
    if (enemiesCount >= LEVELS[currentLevel].maxRatCapacity) return;
    enemySpawnTimer += GetFrameTime();

    if (enemySpawnTimer < ENEMY_SPAWN_TIME) return;
    enemySpawnTimer = 0.0f;
    enemiesCount++;

    Vector2 randomPos;
    if (rand() % 2 == 0) {
        randomPos.x = rand() % (int) BOUNDS_X.y;
        randomPos.y = rand() % 2 == 0 ? BOUNDS_Y.x : BOUNDS_Y.y;
        PlaySound(squeakSound3);
    }
    else {
        randomPos.x = rand() % 2 == 0 ? BOUNDS_X.x : BOUNDS_X.y;
        randomPos.y = rand() % (int) BOUNDS_Y.y;
        PlaySound(squeakSound2);
    }

    enemies = realloc(enemies, sizeof(Rat) * enemiesCount);
    Rat* rat = &enemies[enemiesCount - 1];
    rat->entity = malloc(sizeof(Entity));
    rat->type = 1;
    rat->isEnraged = false;
    rat->entity->position = randomPos;
    rat->entity->rotation = 0.0f;
    rat->entity->scale = (Vector2) { 0.5f, 0.5f };
    rat->entity->velocity = (Vector2) { 0.0f, 0.0f };
    rat->throwTimer = 0.0f;
    rat->throwPosition = (Vector2) { 0.0f, 0.0f };
}

void UpdateExplosiveRatSpawner() {
    if (explosiveRatCount >= LEVELS[currentLevel].maxExplosiveRatCapacity) return;

    explosiveRatSpawnTimer += GetFrameTime();

    if (explosiveRatSpawnTimer < 10.0f) return;
    explosiveRatSpawnTimer = 0.0f;
    explosiveRatCount++;

    Vector2 randomPos;
    if (rand() % 2 == 0) {
        randomPos.x = rand() % (int) BOUNDS_X.y;
        randomPos.y = rand() % 2 == 0 ? BOUNDS_Y.x : BOUNDS_Y.y;
    }
    else {
        randomPos.x = rand() % 2 == 0 ? BOUNDS_X.x : BOUNDS_X.y;
        randomPos.y = rand() % (int) BOUNDS_Y.y;
    }

    explosiveRats = realloc(explosiveRats, sizeof(Rat) * explosiveRatCount);
    Rat* rat = &explosiveRats[explosiveRatCount - 1];
    rat->entity = malloc(sizeof(Entity));
    rat->type = 1;
    rat->entity->position = randomPos;
    rat->entity->rotation = 0.0f;
    rat->entity->scale = (Vector2) { 0.5f, 0.5f };
    rat->entity->velocity = (Vector2) { 0.0f, 0.0f };
    rat->throwTimer = 0.0f;
    rat->throwPosition = (Vector2) { 0.0f, 0.0f };
}

void UpdateRats(void) {
    if (lastBloodLocation.x != 0 && lastBloodLocation.y != 0) {
        float w = bloodTexture.width;
        float h = bloodTexture.height;
        DrawTexturePro(bloodTexture, (Rectangle) { 0, 0, w, h },
                       (Rectangle) { lastBloodLocation.x, lastBloodLocation.y, w, h },
                       (Vector2) { w * 0.5f, h * 0.5f }, bloodTextureRotation, WHITE);
    }

    Vector2 cheesePosition = cheeseEntity.position;
    for (int i = 0; i < enemiesCount; i++) {
        Rat* rat = &enemies[i];
        Entity* entity = rat->entity;
        if (rat == currentDraggedRat || rat == currentRatOnPlayer) {
            continue;
        }
        if (rat->throwTimer > 0.0f) {
            rat->throwTimer -= GetFrameTime();
            Vector2 direction = normalize(getDirection(entity->position, rat->throwPosition));
            entity->velocity.x = direction.x * 300;
            entity->velocity.y = direction.y * 300;
            entity->position.x += entity->velocity.x * GetFrameTime();
            entity->position.y += entity->velocity.y * GetFrameTime();
            float w = entity->scale.x * SCALE_FACTOR + cosf(2.0f - rat->throwTimer * 8.0f) * 50;
            float h = entity->scale.y * SCALE_FACTOR + cosf(2.0f - rat->throwTimer * 8.0f) * 50;
            Rectangle sourceRec = (Rectangle) { (rat->type - 1) * 256, 0, ratTextureSpritesheet.width / 4.0f, ratTextureSpritesheet.height };

            DrawTexturePro(ratTextureSpritesheet, sourceRec,
                           (Rectangle) { entity->position.x, entity->position.y, w, h },
                           (Vector2) { w * 0.5f, h * 0.5f }, entity->rotation - 90, WHITE);
            continue;
        }
        Vector2 direction = normalize(getDirection(entity->position, cheesePosition));
        entity->velocity.x = direction.x * 100;
        entity->velocity.y = direction.y * 100;
        if (rat->isEnraged) {
            entity->velocity.x *= 2;
            entity->velocity.y *= 2;

            float w = electricityParticleTexture.width;
            float h = electricityParticleTexture.height;
            DrawTexturePro(electricityParticleTexture, (Rectangle) { 0, 0, w, h },
                           (Rectangle) { entity->position.x, entity->position.y - 25 + rand() % 10 - 5, w * 0.25f, h * 0.25f },
                           (Vector2) { w * 0.125f, h * 0.125f }, 0, WHITE);

            w = electricityParticleTexture.width;
            h = electricityParticleTexture.height;
            DrawTexturePro(electricityParticleTexture, (Rectangle) { 0, 0, w, h },
                           (Rectangle) { entity->position.x + 25, entity->position.y - 20 + rand() % 10 - 5, w * 0.25f, h * 0.25f },
                           (Vector2) { w * 0.125f, h * 0.125f }, 20, WHITE);

            w = electricityParticleTexture.width;
            h = electricityParticleTexture.height;
            DrawTexturePro(electricityParticleTexture, (Rectangle) { 0, 0, w, h },
                           (Rectangle) { entity->position.x - 25, entity->position.y - 20 + rand() % 10 - 5, w * 0.25f, h * 0.25f },
                           (Vector2) { w * 0.125f, h * 0.125f }, -20, WHITE);
        }
        //entity->velocity.x += (rand() % 100 - 50) * 0.5f;
        //entity->velocity.y += (rand() % 100 - 50) * 0.5f;
        entity->position.x += entity->velocity.x * GetFrameTime() * rat->type;
        entity->position.y += entity->velocity.y * GetFrameTime() * rat->type;

        if (currentRatOnPowerGenerator == rat && currentDraggedRat != rat) {
            entity->velocity.x = 0;
            entity->velocity.y = 0;
            entity->position.x = powerGenerator.position.x;
            entity->position.y = powerGenerator.position.y;
        }

        if (distance(entity->position, cheesePosition) < entity->scale.x * SCALE_FACTOR * 1.5f) {
            entity->velocity.x = 0;
            entity->velocity.y = 0;
        } else {
            entity->rotation = lookAt(entity->position, cheesePosition) + 90;
        }

        float w = entity->scale.x * SCALE_FACTOR;
        float h = entity->scale.y * SCALE_FACTOR;
        Rectangle sourceRec = (Rectangle) { (rat->type - 1) * 256, 0, ratTextureSpritesheet.width / 4.0f, ratTextureSpritesheet.height };

        DrawTexturePro(ratTextureSpritesheet, sourceRec,
                       (Rectangle) { entity->position.x, entity->position.y, w, h },
                       (Vector2) { w * 0.5f, h * 0.5f }, entity->rotation - 90, WHITE);

        if (distance(entity->position, player.position) < w && !currentRatOnPlayer) {
            currentRatOnPlayer = rat;
            PlaySound(squeakSound1);
        }

        if (distance(entity->position, cheesePosition) < w) {
            cheese -= CHEESE_DECREASE_RATE * GetFrameTime() * rat->type;
        }
    }

    if (currentRatOnPowerGenerator == NULL) return;

    powerGeneratorTimer += GetFrameTime();

    if (powerGeneratorTimer >= POWER_GENERATOR_RAT_ESCAPE_TIME) {
        powerGeneratorTimer = 0.0f;
        currentRatOnPowerGenerator->isEnraged = true;
        currentRatOnPowerGenerator = NULL;
    }
}

void UpdateExplosiveRats(void) {
    Vector2 cheesePosition = cheeseEntity.position;
    for (int i = 0; i < explosiveRatCount; ++i) {
        float distanceToCheese = distance(explosiveRats[i].entity->position, cheesePosition);

        if (distanceToCheese < cheeseEntity.scale.x * SCALE_FACTOR) {
            cheese -= CHEESE_DECREASE_RATE * GetFrameTime() * 2;
            explosiveRats[i].entity->velocity.x = 0;
            explosiveRats[i].entity->velocity.y = 0;
        } else {
            Vector2 direction = normalize(getDirection(explosiveRats[i].entity->position, cheesePosition));
            explosiveRats[i].entity->velocity.x = direction.x * 100;
            explosiveRats[i].entity->velocity.y = direction.y * 100;
        }

        explosiveRats[i].entity->rotation = lookAt(explosiveRats[i].entity->position, cheesePosition) + 90;
        explosiveRats[i].entity->position.x += explosiveRats[i].entity->velocity.x * GetFrameTime();
        explosiveRats[i].entity->position.y += explosiveRats[i].entity->velocity.y * GetFrameTime();

        float w = explosiveRats[i].entity->scale.x * SCALE_FACTOR;
        float h = explosiveRats[i].entity->scale.y * SCALE_FACTOR;

        Rectangle sourceRec = (Rectangle) { 0, 0, explosiveRatTexture.width, explosiveRatTexture.height };
        DrawTexturePro(explosiveRatTexture, sourceRec,
                       (Rectangle) { explosiveRats[i].entity->position.x, explosiveRats[i].entity->position.y, w, h },
                       (Vector2) { w * 0.5f, h * 0.5f }, explosiveRats[i].entity->rotation - 90, WHITE);
    }
}

void DestroyRat(Rat* rat) {
    for (int i = 0; i < enemiesCount; i++) {
        if (rat == &enemies[i]) {
            free(rat->entity);
            enemies[i] = enemies[enemiesCount - 1];
            enemiesCount--;
            enemies = realloc(enemies, sizeof(Rat) * enemiesCount);
            break;
        }
    }
}

void OnDropRat(Rat* rat) {
    Vector2 ratPosition = rat->entity->position;
    float scaleX = rat->entity->scale.x * SCALE_FACTOR;
    rat->entity->position = (Vector2) {clamp(ratPosition.x, BOUNDS_X.x, BOUNDS_X.y),
                                       clamp(ratPosition.y, BOUNDS_Y.x, BOUNDS_Y.y)};

    if (currentRatOnPowerGenerator == rat) {
        currentRatOnPowerGenerator = NULL;
    }

    for (int i = 0; i < enemiesCount; ++i) {
        if (rat == &enemies[i]) continue;

        Rat* otherRat = &enemies[i];
        Entity* otherEntity = otherRat->entity;
        if (otherRat->type == 4 || rat->type == 4) continue;
        if (otherRat == currentRatOnPlayer || rat == currentRatOnPlayer) continue;
        if (otherRat == currentRatOnPowerGenerator || rat == currentRatOnPowerGenerator) continue;

        float distanceToOther = distance(rat->entity->position, otherEntity->position);

        if (distanceToOther < scaleX) {
            int highestType = max(rat->type, otherRat->type);
            rat->type = highestType + 1;
            rat->entity->scale = (Vector2) { rat->type * 0.25f, rat->type * 0.25f };
            rat->isEnraged = rat->isEnraged || otherRat->isEnraged;
            score += 20;
            DestroyRat(otherRat);
            PlaySound(poofSound);

            if (mutateParticlesCount > 0) {
                free(mutateParticles);
            }

            lastMutationLocation = rat->entity->position;
            mutateParticles = malloc(sizeof(Entity) * 10);
            mutateParticlesCount = 10;
            mutateParticlesTimer = 0.0f;

            for (int j = 0; j < mutateParticlesCount; ++j) {
                mutateParticles[j] = (Entity) {
                        .position = lastMutationLocation,
                        .rotation = 0.0f,
                        .scale = (Vector2) {0.25f, 0.25f},
                        .velocity = (Vector2) {0.0f, 0.0f}
                };
            }
            return;
        }
    }

    if (LEVELS[currentLevel].isFatRatEnabled && (distance(rat->entity->position, fatRat.position) < scaleX && fatRatTimer >= FAT_RAT_SPAWN_TIME)) {
        numberOfRatsFed++;

        score += 5;
        lastBloodLocation = rat->entity->position;
        bloodTextureRotation = rat->entity->rotation;
        bloodParticles = malloc(sizeof(Entity) * 10);
        bloodParticlesCount = 10;
        bloodParticlesTimer = 0.0f;

        for (int j = 0; j < bloodParticlesCount; ++j) {
            bloodParticles[j] = (Entity) {
                    .position = lastBloodLocation,
                    .rotation = 0.0f,
                    .scale = (Vector2) {0.25f, 0.25f},
                    .velocity = (Vector2) {0.0f, 0.0f}
            };
        }

        DestroyRat(rat);
        PlaySound(nomSound);
        PlaySound(splatSound);
        return;
    }

    if (!LEVELS[currentLevel].isPowerGeneratorEnabled) return;

    if (distance(rat->entity->position, powerGenerator.position) < scaleX) {
        currentRatOnPowerGenerator = rat;
        PlaySound(elecSound);
    }
}

void UpdateMouseLogic(void) {
    Vector2 mousePosition = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        for (int i = 0; i < explosiveRatCount; ++i) {
            if (distance(mousePosition, explosiveRats[i].entity->position) < explosiveRats[i].entity->scale.x * SCALE_FACTOR) {
                explosiveRatCount--;
                explosiveRats[i] = explosiveRats[explosiveRatCount];
                explosiveRats = realloc(explosiveRats, sizeof(Rat) * explosiveRatCount);

                lastExplosionLocation = explosiveRats[i].entity->position;
                explosionTimer = 1.0f;
                score += 5;

                for (int j = 0; j < enemiesCount; ++j) {
                    float distanceToExplosiveRat = distance(enemies[j].entity->position, mousePosition);
                    if (distanceToExplosiveRat < 150) {
                        DestroyRat(&enemies[j]);
                        PlaySound(explosionSound);
                    }
                }

                float distanceToCheese = distance(cheeseEntity.position, mousePosition);
                if (distanceToCheese < 150) {
                    cheese -= 5;
                }

                return;
            }
        }
    }

    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        currentHandTexture = 0;
        if (!IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) return;
        if (currentDraggedRat != NULL) {
            OnDropRat(currentDraggedRat);
            PlaySound(popSound1);
            currentDraggedRat = NULL;
        }
        else if (isCheeseDragged) {
            isCheeseDragged = false;
            PlaySound(popSound1);
        }
        return;
    }

    if (currentDraggedRat != NULL) {
        currentHandTexture = 1;
        currentDraggedRat->entity->position = mousePosition;
        sanity -= SANITY_DECREASE_RATE * GetFrameTime();
        return;
    }

    if (isCheeseDragged) {
        currentHandTexture = 2;
        cheeseEntity.position = mousePosition;
        sanity -= SANITY_DECREASE_RATE * GetFrameTime();
        return;
    }

    for (int i = 0; i < enemiesCount; i++) {
        Rat* rat = &enemies[i];
        if (rat == currentRatOnPlayer || rat->throwTimer > 0.0f) continue;
        Entity* enemy = rat->entity;
        float distanceToMouse = distance(enemy->position, GetMousePosition());
        if (distanceToMouse < enemy->scale.x * SCALE_FACTOR) {
            currentDraggedRat = rat;
            PlaySound(popSound2);
            return;
        }
    }

    float distanceToMouse = distance(cheeseEntity.position, GetMousePosition());
    if (distanceToMouse < cheeseEntity.scale.x * SCALE_FACTOR) {
        isCheeseDragged = true;
        PlaySound(popSound2);
    }
}

void UpdateLevel(void) {
    DrawTexturePro(wallsTexture, (Rectangle) { 0, 0, wallsTexture.width, wallsTexture.height },
                   (Rectangle) { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                   (Vector2) { 0, 0 }, 0, WHITE);

    if (LEVELS[currentLevel].isPowerGeneratorEnabled) {
        DrawTexturePro(powerGeneratorTexture, (Rectangle) { 0, 0, powerGeneratorTexture.width, powerGeneratorTexture.height },
                       (Rectangle) { powerGenerator.position.x, powerGenerator.position.y, powerGeneratorTexture.width * 0.5f, powerGeneratorTexture.height * 0.5f },
                       (Vector2) { powerGeneratorTexture.width * 0.25f, powerGeneratorTexture.height * 0.25f }, 0, WHITE);
    }

    if (currentRatOnPowerGenerator != NULL) {
        for (int i = 0; i < 10; ++i) {
            electricityParticles[i].position.y += (rand() % 100 - 50) * GetFrameTime() * 10;
            if (electricityParticles[i].position.y < powerGenerator.position.y - 50) {
                electricityParticles[i].position.y = powerGenerator.position.y - 50;
            } else if (electricityParticles[i].position.y > powerGenerator.position.y + 50) {
                electricityParticles[i].position.y = powerGenerator.position.y + 50;
            }

            float w = electricityParticleTexture.width;
            float h = electricityParticleTexture.height;
            DrawTexturePro(electricityParticleTexture, (Rectangle) { 0, 0, w, h },
                           (Rectangle) { electricityParticles[i].position.x, electricityParticles[i].position.y, w * 0.25f, h * 0.25f },
                           (Vector2) { w * 0.125f, h * 0.125f }, 0, WHITE);
        }
    }

    float w = spotlightTexture.width;
    float h = spotlightTexture.height;
    Vector2 lightPosition = (Vector2) { player.position.x, player.position.y};
    DrawTexturePro(spotlightTexture, (Rectangle) { 0, 0, w, h },
                   (Rectangle) { lightPosition.x, lightPosition.y, w, h },
                   (Vector2) { w * 0.5f, h * 0.5f }, player.rotation, WHITE);

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color) { 0, 0, 0, 255 - (flashlight * 2.55f) });

    if (explosionTimer >= 0.0f) {
        explosionTimer -= GetFrameTime();
        float size = cosf(explosionTimer) * 400.0f;
        DrawTexturePro(explosionTexture, (Rectangle) { 0, 0, explosionTexture.width, explosionTexture.height },
                       (Rectangle) { lastExplosionLocation.x, lastExplosionLocation.y, size, size },
                       (Vector2) { size * 0.5f, size * 0.5f }, sinf(GetTime() * 30) * 30.0f, (Color) { 255, 255, 255, explosionTimer * 255 });
    }

    if (mutateParticlesTimer >= 1.0f) {
        mutateParticlesCount = 0;
    } else {
        mutateParticlesTimer += GetFrameTime();
    }

    if (bloodParticlesTimer >= 1.0f) {
        bloodParticlesCount = 0;
    } else {
        bloodParticlesTimer += GetFrameTime();
    }

    if (mutateParticlesCount > 0) {
        for (int i = 0; i < mutateParticlesCount; ++i) {
            mutateParticles[i].position.x += rand() % 20 - 10;
            mutateParticles[i].position.y += rand() % 20 - 10;

            DrawCircle(mutateParticles[i].position.x, mutateParticles[i].position.y,
                       50.0f - 50.0f * mutateParticlesTimer,
                       (Color) {255, 255, 255, 255 - mutateParticlesTimer * 255});
        }

        float size = sinf(mutateParticlesTimer * 6.0f) * 150.0f + 150.0f;
        DrawTexturePro(poofTexture, (Rectangle) {0, 0, poofTexture.width, poofTexture.height},
                       (Rectangle) {lastMutationLocation.x, lastMutationLocation.y, size, size},
                       (Vector2) {size * 0.5f, size * 0.5f}, sinf(mutateParticlesTimer * 5.0f) * 30.0f - 30.0f,
                       (Color) {255, 255, 255, min(255, 512 - mutateParticlesTimer * 512)});
    }

    if (bloodParticlesCount > 0) {
        for (int i = 0; i < bloodParticlesCount; ++i) {
            bloodParticles[i].position.x += rand() % 20 - 10;
            bloodParticles[i].position.y += rand() % 20 - 10;

            DrawCircle(bloodParticles[i].position.x, bloodParticles[i].position.y,
                       50.0f - 50.0f * bloodParticlesTimer,
                       (Color) {180, 0, 0, 255 - bloodParticlesTimer * 255});
        }

        float size = sinf(bloodParticlesTimer * 6.0f) * 100.0f + 100.0f;
        DrawTexturePro(nomTexture, (Rectangle) {0, 0, nomTexture.width, nomTexture.height},
                       (Rectangle) {lastBloodLocation.x, lastBloodLocation.y, size, size},
                       (Vector2) {size * 0.5f, size * 0.5f}, sinf(bloodParticlesTimer * 5.0f) * 30.0f - 10.0f,
                       (Color) {255, 0, 0, min(255, 512 - bloodParticlesTimer * 512)});
    }
}

void UpdateScreenEffects(void) {
    if (isScreenFlickering) {
        screenFlickerTimer += GetFrameTime();
        if (screenFlickerTimer >= 1.0f) {
            screenFlickerTimer = 0.0f;
            isScreenFlickering = false;
        }
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color) { 0, 0, 0, sinf(screenFlickerTimer * 25) * 255 });
    }
    else if (sanity <= 50.0f) {
        screenFlickerTimer += GetFrameTime();
        if (screenFlickerTimer >= SCREEN_FLICKER_TIME) {
            screenFlickerTimer = 0.0f;
            isScreenFlickering = true;
        }
    }

    if (explosionTimer > 0.0f) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color) { 255, 255, 255, explosionTimer * 128 });
    }

    if (redFlashIntensity > 0.0f) {
        redFlashIntensity -= GetFrameTime();
        DrawTexturePro(redFlashTexture, (Rectangle) { 0, 0, redFlashTexture.width, redFlashTexture.height },
                       (Rectangle) { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                       (Vector2) { 0, 0 }, 0, (Color) { 255, 255, 255, redFlashIntensity * 255 });
    }

    if (currentRatOnPlayer != NULL && currentLevel <= 2) {
        DrawTexturePro(spaceButtonTexture, (Rectangle) { 0, 0, spaceButtonTexture.width, spaceButtonTexture.height },
                       (Rectangle) { SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.875f + sinf(GetTime() * 20) * 10, 300, 300 },
                       (Vector2) { 150, 150 }, 0, WHITE);
    }

    float w = fatRatTeeth.width;
    float h = fatRatTeeth.height;
    DrawTexturePro(fatRatTeeth, (Rectangle) { 0, 0, w, h },
                   (Rectangle) { SCREEN_WIDTH * 0.5f, fatRatTeethPosition - FAT_RAT_TEETH_MAX_POSITION, SCREEN_WIDTH, SCREEN_HEIGHT },
                   (Vector2) { w * 0.5f, h * 0.5f }, 0, WHITE);
    DrawTexturePro(fatRatTeeth, (Rectangle) { 0, 0, w, h },
                   (Rectangle) { SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT - fatRatTeethPosition + FAT_RAT_TEETH_MAX_POSITION, SCREEN_WIDTH, SCREEN_HEIGHT },
                   (Vector2) { w * 0.5f, h * 0.5f }, 180, WHITE);
}

void UpdateUI(void) {
    DrawText(TextFormat("Score: %i", score), 10, 10, 20, WHITE);
    DrawText(TextFormat("Highscore: %i", highscore), 10, 30, 20, WHITE);

    int currentHour = (int) (currentTime / HOUR_LENGTH_IN_SECONDS) + 8;
    char *c = currentHour > 12 ? "PM" : "AM";
    if (currentHour > 12) currentHour -= 12;
    DrawText(TextFormat("%i %s", currentHour, c), 15, SCREEN_HEIGHT - 30, 20, WHITE);

    const int barWidth = 300;
    const int barHeight = 20;

    const int cheeseBarX = SCREEN_WIDTH / 2 - barWidth / 2;
    const int sanityBarX = 50;
    const int healthBarX = SCREEN_WIDTH - barWidth - 50;
    const int barY = 60;

    const int barPadding = 4;

    const int barInnerWidth = barWidth - barPadding * 2;

    const int cheeseBarInnerX = cheeseBarX + barPadding;
    const int sanityBarInnerX = sanityBarX + barPadding;
    const int healthBarInnerX = healthBarX + barPadding;
    const int barInnerY = barY + barPadding;

    const int fontSize = 20;
    const int fontSpacing = 5;

    Vector2 cheeseTextSize = MeasureTextEx(GetFontDefault(), "Cheese", fontSize, fontSpacing);
    Vector2 cheeseTextPosition = (Vector2) { cheeseBarX + barWidth / 2 - cheeseTextSize.x / 2, barY - cheeseTextSize.y - 5 };

    DrawTextEx(GetFontDefault(), "Cheese", cheeseTextPosition, fontSize, fontSpacing, WHITE);

    DrawRectangle(cheeseBarX, barY, barWidth, barHeight, WHITE);
    DrawRectangle(cheeseBarInnerX, barInnerY, barInnerWidth, barHeight - barPadding * 2, BLACK);
    DrawRectangle(cheeseBarInnerX, barInnerY, barInnerWidth * (cheese / 100.0f), barHeight - barPadding * 2, YELLOW);

    Vector2 sanityTextSize = MeasureTextEx(GetFontDefault(), "Sanity", fontSize, fontSpacing);
    Vector2 sanityTextPosition = (Vector2) { sanityBarX + barWidth / 2 - sanityTextSize.x / 2, barY - sanityTextSize.y - 5 };

    DrawTextEx(GetFontDefault(), "Sanity", sanityTextPosition, fontSize, fontSpacing, WHITE);

    DrawRectangle(sanityBarX, barY, barWidth, barHeight, WHITE);
    DrawRectangle(sanityBarInnerX, barInnerY, barInnerWidth, barHeight - barPadding * 2, BLACK);
    DrawRectangle(sanityBarInnerX, barInnerY, barInnerWidth * (sanity / 100.0f), barHeight - barPadding * 2, RED);

    Vector2 healthTextSize = MeasureTextEx(GetFontDefault(), "Health", fontSize, fontSpacing);
    Vector2 healthTextPosition = (Vector2) { healthBarX + barWidth / 2 - healthTextSize.x / 2, barY - healthTextSize.y - 5 };

    DrawTextEx(GetFontDefault(), "Health", healthTextPosition, fontSize, fontSpacing, WHITE);

    DrawRectangle(healthBarX, barY, barWidth, barHeight, WHITE);
    DrawRectangle(healthBarInnerX, barInnerY, barInnerWidth, barHeight - barPadding * 2, BLACK);
    DrawRectangle(healthBarInnerX, barInnerY, barInnerWidth * (health / 100.0f), barHeight - barPadding * 2, GREEN);
}

void RestartMenu(void) {
    Rectangle inputField = {SCREEN_WIDTH / 2 - 150, 700, 300, 50 };
    bool mouseOverInputField = CheckCollisionPointRec(GetMousePosition(), inputField);

    Rectangle submitButton = {SCREEN_WIDTH / 2 - 150, 800, 300, 40 };
    bool mouseOverSubmitButton = CheckCollisionPointRec(GetMousePosition(), submitButton);

    Rectangle restartButton = {SCREEN_WIDTH / 2 - 100, 900, 200, 40 };
    bool mouseOverRestartButton = CheckCollisionPointRec(GetMousePosition(), restartButton);

    if (mouseOverInputField)
    {
        SetMouseCursor(MOUSE_CURSOR_IBEAM);
        int key = GetCharPressed();
        while (key > 0)
        {
            if ((key >= 32) && (key <= 125) && (usernameSize < MAX_NAME_INPUT_CHARS))
            {
                username[usernameSize] = (char)key;
                username[usernameSize + 1] = '\0';
                usernameSize++;
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE))
        {
            usernameSize--;
            if (usernameSize < 0) usernameSize = 0;
            username[usernameSize] = '\0';
        }
    }
    else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

    if ((!submittedScore && (mouseOverSubmitButton && IsMouseButtonDown(MOUSE_LEFT_BUTTON))) && usernameSize > 0) {
        submittedScore = true;
        UploadNewEntry();
    }

    if (mouseOverRestartButton && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        isGameOver = false;
        isFinishedGame = false;
        ResetLevel(true);
    }

    if (mouseOverInputField) inputFieldFrames++;
    else inputFieldFrames = 0;

    char *enterUsernameText = "Enter username (max. 16 characters, hover to focus in)";
    DrawText(enterUsernameText, SCREEN_WIDTH / 2 - MeasureText(enterUsernameText, 20) / 2, 650, 20, GRAY);

    DrawRectangleRec(inputField, BLACK);
    DrawRectangleLinesEx((Rectangle) {inputField.x, inputField.y, inputField.width, inputField.height}, 2.0f,
                         mouseOverInputField ? BLUE : WHITE);

    DrawText(username, inputField.x + 5, inputField.y + 8, 40, WHITE);

    if (mouseOverInputField && (usernameSize < MAX_NAME_INPUT_CHARS && (inputFieldFrames / 20) % 2 == 0))
        DrawText("|", inputField.x + 8 + MeasureText(username, 40), inputField.y + 12, 40, BLUE);

    char *submitText = "Submit highscore";
    if (!submittedScore) {
        DrawRectangleRec(submitButton, WHITE);
        DrawRectangleLinesEx((Rectangle) {submitButton.x, submitButton.y, submitButton.width, submitButton.height},
                             2.0f,
                             mouseOverSubmitButton ? BLUE : BLACK);
        DrawText(submitText, submitButton.x + submitButton.width / 2 - MeasureText(submitText, 20) / 2,
                 submitButton.y + 8, 20, BLACK);
    }

    char *restartText = "Restart";
    DrawRectangleRec(restartButton, WHITE);
    DrawRectangleLinesEx((Rectangle) {restartButton.x, restartButton.y, restartButton.width, restartButton.height}, 2.0f,
                         mouseOverRestartButton ? RED : BLACK);
    DrawText(restartText, restartButton.x + restartButton.width / 2 - MeasureText(restartText, 20) / 2, restartButton.y + 8, 20, BLACK);
}

void OnGameOver(void) {
    UpdateLevel();
    UpdateUI();
    currentHandTexture = 0;
    Vector2 gameOverTextSize = MeasureTextEx(GetFontDefault(), "Game Over", 100, 10);
    DrawTextEx(GetFontDefault(), "Game Over",
               (Vector2) { SCREEN_WIDTH / 2 - gameOverTextSize.x / 2, SCREEN_HEIGHT / 2 - gameOverTextSize.y / 2 - 200 },
               100, 10, WHITE);

    RestartMenu();
}

void UpdateCursor(void) {
    Vector2 mousePos = GetMousePosition();

    Texture2D texture = handTextures[currentHandTexture];
    DrawTexturePro(texture, (Rectangle) { 0, 0, texture.width, texture.height },
                   (Rectangle) { mousePos.x, mousePos.y, texture.width * 0.5f, texture.height * 0.5f },
                   (Vector2) { texture.width * 0.25f, texture.height * 0.25f }, 0, WHITE);
}

void OnEnding(void) {
    endingTimer += GetFrameTime();

    ClearBackground(BLACK);
    if (endingTimer <= 3.0f) {
        Vector2 text = MeasureTextEx(GetFontDefault(), TextFormat("Day 6"), 50, 5);
        DrawTextEx(GetFontDefault(), TextFormat("Day 6"),
                   (Vector2) { SCREEN_WIDTH * 0.5f - text.x / 2, SCREEN_HEIGHT * 0.5f - text.y / 2 },
                   50, 5, RED);
    } else if (endingTimer <= 6.0f) {
        Vector2 text = MeasureTextEx(GetFontDefault(), TextFormat("Nah, just kidding"), 50, 5);
        DrawTextEx(GetFontDefault(), TextFormat("Nah, just kidding", currentLevel + 1),
                   (Vector2) {SCREEN_WIDTH * 0.5f - text.x / 2, SCREEN_HEIGHT * 0.5f - text.y / 2},
                   50, 5, (Color){255, 255, 255, clamp(255 - (endingTimer - 5.0f) * 255, 0, 255)});
    } else {
        DrawTexturePro(endCutscene, (Rectangle) { 0, 0, endCutscene.width, endCutscene.height },
                       (Rectangle) { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
                       (Vector2) { 0, 0 }, 0, (Color) { 255, 255, 255, clamp((endingTimer - 7.0f) * 255, 0, 255) });

        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color) { 0, 0, 0, clamp((endingTimer - 11.0f) * 128, 0, 200) });
    }

    if (endingTimer >= 12.0f) {
        Vector2 gameOverTextSize = MeasureTextEx(GetFontDefault(), "Thanks for playing!", 100, 10);
        DrawTextEx(GetFontDefault(), "Thanks for playing!",
                   (Vector2) { SCREEN_WIDTH / 2 - gameOverTextSize.x / 2, SCREEN_HEIGHT / 2 - gameOverTextSize.y / 2 - 200 },
                   100, 10, WHITE);

        Vector2 highscoreTextSize = MeasureTextEx(GetFontDefault(), TextFormat("Highscore: %i", highscore), 50, 5);
        DrawTextEx(GetFontDefault(), TextFormat("Highscore: %i", highscore),
                   (Vector2) { SCREEN_WIDTH / 2 - highscoreTextSize.x / 2, SCREEN_HEIGHT / 2 - highscoreTextSize.y / 2 + 100 },
                   50, 5, WHITE);
        RestartMenu();
        UpdateCursor();
    }
}

void LevelTransition(void) {
    if (currentLevel >= LEVEL_COUNT) {
        isFinishedGame = true;
        highscore = max(score, highscore);
        return;
    }

    levelTransitionTimer += GetFrameTime();
    ClearBackground(BLACK);
    Vector2 text = MeasureTextEx(GetFontDefault(), TextFormat("Day %i", currentLevel + 1), 50, 5);
    float y = currentLevel <= 3 ? 400.0f : 0.0f;
    DrawTextEx(GetFontDefault(), TextFormat("Day %i", currentLevel + 1),
               (Vector2) { SCREEN_WIDTH / 2 - text.x / 2, SCREEN_HEIGHT / 2 - text.y / 2 - y },
               50, 5, currentLevel == 4 ? RED : WHITE);

    if (currentLevel <= 3) {
        DrawTexturePro(tutorial[currentLevel], (Rectangle) { 0, 0, tutorial[currentLevel].width, tutorial[currentLevel].height },
                       (Rectangle) { SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, tutorial[currentLevel].width * 1.25f, tutorial[currentLevel].height * 1.25f },
                          (Vector2) { tutorial[currentLevel].width * 0.625f, tutorial[currentLevel].height * 0.625f }, 0, (Color) { 255, 255, 255, clamp(levelTransitionTimer * 255 - 255, 0, 255) });
    }

    if (levelTransitionTimer >= 4.0f) {
        text = MeasureTextEx(GetFontDefault(), "Press ENTER to continue...", 50, 5);
        DrawTextEx(GetFontDefault(), "Press ENTER to continue...",
                   (Vector2) { SCREEN_WIDTH / 2 - text.x / 2, SCREEN_HEIGHT * 0.875f },
                   50, 5, GRAY);
    }

    if (levelTransitionTimer >= 2.0f && IsKeyPressed(KEY_ENTER)) {
        levelTransitionTimer = 0.0f;
        isLevelTransitioning = false;
        currentLevel++;
        ResetLevel(false);
    }
}

static bool isStarted = false;

void StartScreen(void) {
    ClearBackground(BLACK);

    Vector2 crazyTextSize = MeasureTextEx(GetFontDefault(), "Crazy?", 100, 10);
    DrawTextEx(GetFontDefault(), "Crazy?",
               (Vector2) { SCREEN_WIDTH / 2 - crazyTextSize.x / 2, SCREEN_HEIGHT / 2 - crazyTextSize.y / 2 - 200 },
               100, 10, WHITE);

    Vector2 startTextSize = MeasureTextEx(GetFontDefault(), "Press ENTER to start...", 50, 5);
    DrawTextEx(GetFontDefault(), "Press ENTER to start...",
               (Vector2) { SCREEN_WIDTH / 2 - startTextSize.x / 2, SCREEN_HEIGHT * 0.5f },
               50, 5, GRAY);

    Vector2 creditsTextSize = MeasureTextEx(GetFontDefault(), "Made by @danqzq for Ludum Dare 54", 20, 2);
    DrawTextEx(GetFontDefault(), "Made by @danqzq for Ludum Dare 54",
               (Vector2) { SCREEN_WIDTH / 2 - creditsTextSize.x / 2, SCREEN_HEIGHT * 0.875f},
               20, 2, GRAY);

    if (IsKeyPressed(KEY_ENTER)) {
        isStarted = true;
        PlaySound(crazySound);
    }
}

void Update(void) {
    if (!isStarted) {
        StartScreen();
        UpdateMusicStream(cutsceneMusic);
        return;
    }
    if (isFinishedGame) {
        OnEnding();
        return;
    }
    if (isCutscenePlaying) {
        UpdateCutscenes();
        UpdateMusicStream(cutsceneMusic);
        return;
    }
    if (isGameOver) {
        OnGameOver();
        UpdateCursor();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            PlaySound(popSound2);
        }
        return;
    }
    if (isLevelTransitioning) {
        LevelTransition();
        return;
    }
    UpdateMusicStream(ambienceMusic);
    UpdateStats();
    UpdateCheese();
    UpdateExplosiveRatSpawner();
    UpdateExplosiveRats();
    UpdateRatSpawner();
    UpdateRats();
    UpdatePlayer();

    if (LEVELS[currentLevel].isFatRatEnabled)
        UpdateFatRat();

    UpdateMouseLogic();
    UpdateLevel();
    UpdateScreenEffects();
    UpdateUI();
    UpdateCursor();
}

void MainLoop(void) {
    while (!WindowShouldClose()) {
        ClearBackground(BACKGROUND_COLOR);
        BeginDrawing();
        Update();
        EndDrawing();
    }
    UnloadMusicStream(ambienceMusic);
    UnloadMusicStream(cutsceneMusic);
    CloseAudioDevice();
}

int main(void) {
    srand(time(NULL));

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Crazy?");

    Start();

    SetWindowState(FLAG_WINDOW_RESIZABLE);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(MainLoop, 0, 1);
#else
    SetTargetFPS(TARGET_FPS);
#endif

    MainLoop();
    CloseWindow();
    return 0;
}