#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

// Forward declaration
int main(int argc, char** argv);

// --- Sound Functions (Windows specific) ---

void playArrowRelease1() {
    PlaySound("arrow_release.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void playArrowHit1() {
    PlaySound("arrow_hit.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void playApplause1() {
    PlaySound("applause.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void playGameOver1() {
    PlaySound("gameover.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void playLose1() {
    PlaySound("lose.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void stopAllSounds1() {
    PlaySound(NULL, 0, 0);
}

// --- Constants and Global Variables ---

#define OUTER_RADIUS 80.0f
#define MIDDLE_RADIUS 60.0f
#define INNER_RADIUS 24.0f // Note: Redefined later, using the later definition

int WinW1 = 1024, WinH1 = 720;
int score1 = 0;
int arrowsLeft1 = 10;
int gameState1 = 0; // 0=ready,1=flying,2=hit,3=gameover

typedef struct {
    float x, y, z;
} Vec3;

Vec3 bow1 = {-200.0f, 0.0f, 100.0f};
Vec3 aimPoint1 = {0.0f, 0.0f, -600.0f};
Vec3 targetPos1 = {0.0f, 0.0f, -600.0f};

float targetMoveX = 0.0f;       // current offset
float targetMoveDir = 1.0f;     // +1 or -1 direction
float targetMoveSpeed = 4.0f;   // movement speed
float targetMoveRange = 250.0f; // how far it moves each side
int targetFreezeTimer = 0;      // time for which target stays stopped after hit
int restartRequested = 0;

#undef OUTER_RADIUS // Undefining previous definitions to avoid confusion
#undef MIDDLE_RADIUS
#undef INNER_RADIUS
#define OUTER_RADIUS 80.0f
#define MIDDLE_RADIUS 60.0f
#define INNER_RADIUS 30.0f // This is the value used in checkCollisionAndscore11, so using it here for clarity.

Vec3 arrowPos1;
Vec3 arrowDir1;
float arrowSpeed1 = 32.0f;
float arrowShaftLen1 = 70.0f;
int arrowState1 = 0; // 0=ready, 1=flying, 2=hit
int hitTimer1 = 0;
float aimYaw1 = 0.0f;
float aimPitch1 = 0.0f;
int tickMs1 = 16;

// --- Vector Math Functions ---

static inline Vec3 vadd(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 vsub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 vscale(Vec3 v, float s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

static inline float vlen(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline Vec3 vnorm(Vec3 v) {
    float L = vlen(v);
    if (L < 1e-6f)
        return (Vec3){0, 0, -1};
    return vscale(v, 1.0f / L);
}

// --- Grass Structure and Initialization ---

#define grass1_COUNT 80000 // number of grass1 blades

typedef struct {
    float x, y, z, h;
} grass1Blade;

#define FRUIT_COUNT 8
typedef struct {
    float x, y, z;
} Fruit;

Fruit leftTreeFruits1[FRUIT_COUNT];
Fruit rightTreeFruits1[FRUIT_COUNT];
grass1Blade grass1[grass1_COUNT];

void initgrass11() {
    for (int i = 0; i < grass1_COUNT; i++) {
        grass1[i].x = ((rand() % 4000) - 2000); // X range -2000 to 2000
        grass1[i].z = ((rand() % 4000) - 2000); // Z range -2000 to 2000
        grass1[i].y = -200.0f;                  // ground level
        grass1[i].h = 15.0f + (rand() % 4);     // blade height 15-19
    }
}

// --- Utility Functions ---

float distance1(Vec3 a, Vec3 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

void drawStrokeText1(float x, float y, const char* s, float scale) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, 1.0f);
    glColor3f(1, 1, 1);
    for (int i = 0; s[i]; i++)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, s[i]);
    glPopMatrix();
}

void setupLighting1() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat diffuse[] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat specular[] = {0.6f, 0.6f, 0.6f, 1.0f};
    GLfloat position[] = {300.0f, 400.0f, 300.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
}

// --- Drawing Functions ---

void drawGroundSky1() {
    // Ground
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
    glColor3f(0.25f, 0.65f, 0.25f); // green ground
    glVertex3f(-2000.0f, -200.0f, 2000.0f);
    glVertex3f(2000.0f, -200.0f, 2000.0f);
    glVertex3f(2000.0f, -200.0f, -2000.0f);
    glVertex3f(-2000.0f, -200.0f, -2000.0f);
    glEnd();

    // Grass
    glColor3f(0.1f, 0.6f, 0.1f); // green
    glBegin(GL_LINES);
    for (int i = 0; i < grass1_COUNT; i++) {
        glVertex3f(grass1[i].x, grass1[i].y, grass1[i].z);             // base
        glVertex3f(grass1[i].x, grass1[i].y + grass1[i].h, grass1[i].z); // tip
    }
    glEnd();

    // Sky back wall (backdrop)
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
    glColor3f(0.5f, 0.8f, 1.0f);
    glVertex3f(-2000, 1000, -1500);
    glColor3f(0.4f, 0.75f, 1.0f);
    glVertex3f(2000, 1000, -1500);
    glColor3f(0.6f, 0.85f, 1.0f);
    glVertex3f(2000, -200, -1500);
    glColor3f(0.7f, 0.9f, 1.0f);
    glVertex3f(-2000, -200, -1500);
    glEnd();

    // Clouds - Repositioning main light to simulate cloud lighting
    glEnable(GL_LIGHTING);
    GLfloat lightColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat lightPos[] = {0.0f, 1000.0f, 500.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightColor);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    GLfloat pureWhite[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, pureWhite);

    glPushMatrix();
    glTranslatef(-900, 450, -1000);
    glutSolidSphere(90, 20, 20);
    glPushMatrix();
    glTranslatef(80, 20, -10);
    glutSolidSphere(75, 20, 20);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-70, 10, 15);
    glutSolidSphere(60, 20, 20);
    glPopMatrix();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, 500, -950);
    glutSolidSphere(100, 20, 20);
    glPushMatrix();
    glTranslatef(80, 15, 10);
    glutSolidSphere(85, 20, 20);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-70, -10, -5);
    glutSolidSphere(70, 20, 20);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(20, -25, 10);
    glutSolidSphere(60, 20, 20);
    glPopMatrix();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(800, 400, -1100);
    glutSolidSphere(80, 20, 20);
    glPushMatrix();
    glTranslatef(70, 10, -5);
    glutSolidSphere(70, 20, 20);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-60, 5, 15);
    glutSolidSphere(55, 20, 20);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(25, -15, 10);
    glutSolidSphere(50, 20, 20);
    glPopMatrix();
    glPopMatrix();

    // Sun
    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(900.0f, 550.0f, -1400.0f);
    glColor3f(1.0f, 1.0f, 0.0f);
    glutSolidSphere(100, 40, 40);

    // Sun glow using blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 0.0f, 0.3f);
    glutSolidSphere(140, 36, 36);
    glColor4f(1.0f, 1.0f, 0.0f, 0.15f);
    glutSolidSphere(180, 32, 32);
    glDisable(GL_BLEND);

    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glPopMatrix();
}

void drawTarget1() {
    glPushMatrix();
    glTranslatef(targetMoveX, 0.0f, 0.0f); // move only this part left-right

    // Target Stand Supports
    glColor3f(0.55f, 0.35f, 0.2f); // wood/steel color
    glPushMatrix();
    glTranslatef(targetPos1.x - 40.0f, targetPos1.y + 80.0f, targetPos1.z - 15.0f);
    glScalef(10.0f, 120.0f, 5.0f);
    glutSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(targetPos1.x + 40.0f, targetPos1.y + 80.0f, targetPos1.z - 15.0f);
    glScalef(10.0f, 120.0f, 5.0f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Target Face
    glPushMatrix();
    glTranslatef(targetPos1.x, targetPos1.y, targetPos1.z);
    GLUquadric* q = gluNewQuadric();

    // Red ring (Outer)
    glColor3f(1.0f, 0.1f, 0.1f);
    gluDisk(q, MIDDLE_RADIUS, OUTER_RADIUS, 50, 1);

    // Yellow ring (Middle)
    glColor3f(1.0f, 1.0f, 0.2f);
    gluDisk(q, INNER_RADIUS, MIDDLE_RADIUS, 50, 1);

    // Green zone (Inner)
    glColor3f(0.0f, 0.8f, 0.0f);
    gluDisk(q, 0.0f, INNER_RADIUS, 50, 1);

    gluDeleteQuadric(q);
    glPopMatrix(); // end target

    glPopMatrix(); // end moving group
}

void drawHangingStand1() {
    glEnable(GL_COLOR_MATERIAL);
    glColor3f(0.55f, 0.35f, 0.2f); // brown color for wood
    float horizontalLength = 1000.0f; // length of top horizontal rod
    float horizontalHeight = 140.0f; // height above target center
    float verticalHeight = 500.0f;   // vertical rods at both ends
    float thickness = 10.0f;         // thickness of rods

    glPushMatrix();
    glTranslatef(targetPos1.x, targetPos1.y, targetPos1.z);

    // Top horizontal rod
    glPushMatrix();
    glTranslatef(0, horizontalHeight, 0); // move up above target
    glScalef(horizontalLength, thickness, thickness);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Left vertical rod
    glPushMatrix();
    glTranslatef(-horizontalLength / 2, horizontalHeight - verticalHeight / 2, 0);
    glScalef(thickness, verticalHeight, thickness);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Right vertical rod
    glPushMatrix();
    glTranslatef(horizontalLength / 2, horizontalHeight - verticalHeight / 2, 0);
    glScalef(thickness, verticalHeight, thickness);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPopMatrix();
}

void drawArrow1(Vec3 pos, Vec3 dir) {
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);

    Vec3 n = vnorm(dir);
    float yaw = atan2f(n.x, n.z) * 180.0f / M_PI;
    float pitch = atan2f(n.y, sqrtf(n.x * n.x + n.z * n.z)) * 180.0f / M_PI;
    glRotatef(yaw, 0, 1, 0);
    glRotatef(-pitch, 1, 0, 0); // Arrow is generally drawn pointing down the Z axis

    // Shaft
    GLUquadric* q = gluNewQuadric();
    glColor3f(0.9f, 0.75f, 0.2f); // wooden shaft color
    glPushMatrix();
    glRotatef(90, 1, 0, 0); // Orient cylinder to point along Z (arrow direction)
    gluCylinder(q, 1.5, 1.5, arrowShaftLen1, 10, 1);
    glPopMatrix();

    // Tip (Cone)
    glColor3f(0.7f, 0.1f, 0.1f); // metal tip color
    glPushMatrix();
    glTranslatef(0, 0, -5);
    gluCylinder(q, 2.7, 0.0, 5.0, 10, 1); // Cone from base radius 2.7 to tip 0.0
    glPopMatrix();

    // Fletching (simple triangles)
    glColor3f(1.0f, 1.0f, 1.0f); // white fletching
    float fletchStart = arrowShaftLen1 - 15.0f;
    float fletchSize = 10.0f;
    glBegin(GL_TRIANGLES);
    // Vane 1
    glVertex3f(0.0f, 0.0f, fletchStart);
    glVertex3f(4.0f, 0.0f, fletchStart - fletchSize);
    glVertex3f(-4.0f, 0.0f, fletchStart - fletchSize);
    // Vane 2 (rotated)
    glVertex3f(0.0f, 0.0f, fletchStart);
    glVertex3f(0.0f, 4.0f, fletchStart - fletchSize);
    glVertex3f(0.0f, -4.0f, fletchStart - fletchSize);
    glEnd();

    gluDeleteQuadric(q);
    glPopMatrix();
}

void drawbow1(Vec3 pos, Vec3 aimDir) {
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);

    // Orient the bow towards the aim direction
    Vec3 n = vnorm(aimDir);
    float yaw = atan2f(n.x, n.z) * 180.0f / M_PI;
    float pitch = atan2f(n.y, sqrtf(n.x * n.x + n.z * n.z)) * 180.0f / M_PI;
    glRotatef(yaw, 0, 1, 0);
    glRotatef(-pitch, 1, 0, 0); // Negative pitch because the bow is usually drawn vertical

    GLUquadric* q = gluNewQuadric();
    float limbLength = 70.0f;
    float curveStrength = 35.0f; // 𝖸 More curve inward
    float baseRadius = 2.5f;
    float tipRadius = 1.0f;

    // Center Grip
    glPushMatrix();
    glColor3f(0.36f, 0.18f, 0.05f);
    gluCylinder(q, 3.5, 3.5, 15.0, 16, 2);
    glPopMatrix();

    // Handle decorative band
    glPushMatrix();
    glColor3f(0.9f, 0.75f, 0.2f);
    glTranslatef(0, 0, 3);
    gluCylinder(q, 3.8, 3.8, 8.0, 16, 1);
    glPopMatrix();

    // Bow Limbs (approximated with small cylinders)
    glColor3f(0.55f, 0.27f, 0.07f);

    // Right Limb
    glPushMatrix();
    glRotatef(-90, 1, 0, 0);
    glTranslatef(0, -55.0f, 0);
    for (int i = 0; i < 30; i++) {
        float t = (float)i / 30.0f;
        float x = -t * limbLength;
        float y = sin(t * M_PI * 0.65f) * curveStrength; // smoother deeper curve
        float nextX = -(t + 1.0f / 30.0f) * limbLength;
        float nextY = sin((t + 1.0f / 30.0f) * M_PI * 0.65f) * curveStrength;

        glPushMatrix();
        glTranslatef(x, y, 0);
        Vec3 dirSeg = {nextX - x, nextY - y, 0};
        float angle = atan2f(dirSeg.y, dirSeg.x) * 180.0f / M_PI;
        glRotatef(angle, 0, 0, 1);
        float r1 = baseRadius - t * (baseRadius - tipRadius);
        float r2 = baseRadius - (t + 1.0f / 30.0f) * (baseRadius - tipRadius);
        gluCylinder(q, r1, r2, 2.4, 12, 1);
        glPopMatrix();
    }
    glPopMatrix();

    // Left Limb
    glPushMatrix();
    glRotatef(-90, 1, 0, 0);
    glTranslatef(0, -55.0f, 0);
    for (int i = 0; i < 30; i++) {
        float t = (float)i / 30.0f;
        float x = t * limbLength;
        float y = sin(t * M_PI * 0.65f) * curveStrength;
        float nextX = (t + 1.0f / 30.0f) * limbLength;
        float nextY = sin((t + 1.0f / 30.0f) * M_PI * 0.65f) * curveStrength;

        glPushMatrix();
        glTranslatef(x, y, 0);
        Vec3 dirSeg = {nextX - x, nextY - y, 0};
        float angle = atan2f(dirSeg.y, dirSeg.x) * 180.0f / M_PI;
        glRotatef(angle, 0, 0, 1);
        float r1 = baseRadius - t * (baseRadius - tipRadius);
        float r2 = baseRadius - (t + 1.0f / 30.0f) * (baseRadius - tipRadius);
        gluCylinder(q, r1, r2, 2.4, 12, 1);
        glPopMatrix();
    }
    glPopMatrix();

    // Bow Tips and String
    glPushMatrix();
    glRotatef(-90, 1, 0, 0); // Rotate the whole group 90° around X-axis for easier drawing
    glTranslatef(0, -20, 0);

    float tipY = sin(M_PI * 0.65f) * curveStrength;
    glColor3f(0.9f, 0.75f, 0.2f);

    // Tip 1
    glPushMatrix();
    glTranslatef(-limbLength, tipY - 30, 2);
    glutSolidSphere(2.5, 12, 12);
    glPopMatrix();

    // Tip 2
    glPushMatrix();
    glTranslatef(limbLength, tipY - 30, 2);
    glutSolidSphere(2.5, 12, 12);
    glPopMatrix();

    // Bowstring
    glColor3f(0.95f, 0.95f, 0.9f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 20; i++) {
        float t = (float)i / 20.0f;
        float x = -limbLength + t * (2 * limbLength);
        float y = tipY * cos((t - 0.5f) * M_PI); // smooth inward bowstring curve
        glVertex3f(x, y - 2.0f, 2);
    }
    glEnd();
    glPopMatrix(); // end group rotation

    gluDeleteQuadric(q);
    glPopMatrix();
}

void drawHUD1() {
    static int applausePlayed1 = 0;
    static int losePlayed1 = 0;
    static int gameOverPlayed = 0; // Unused in this snippet but kept for context

    glDisable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WinW1, 0, WinH1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    char buf[128];
    glColor3f(1, 1, 1);

    // --- Level 2 Title ---
    glColor3f(1, 1, 0); // yellow for title
    drawStrokeText1(WinW1 / 2 - 90, WinH1 - 50, "LEVEL 2", 0.25f);

    // --- Score + Arrows ---
    glColor3f(1, 1, 1);
    drawStrokeText1(10, WinH1 - 20, "Score Required: 20", 0.12f);
    sprintf(buf, "Score: %d", score1);
    drawStrokeText1(10, WinH1 - 60, buf, 0.12f);
    sprintf(buf, "Arrows: %d", arrowsLeft1);
    drawStrokeText1(10, WinH1 - 100, buf, 0.12f);

    if (arrowsLeft1 == 0 && arrowState1 == 0) {
        if (score1 >= 20) {
            if (!applausePlayed1) {
                playApplause1();
                applausePlayed1 = 1;
            }
            drawStrokeText1(WinW1 / 2 - 120, WinH1 / 2 + 40, "YOU WON!", 0.28f);
            sprintf(buf, "Final Score: %d", score1);
            drawStrokeText1(WinW1 / 2 - 120, WinH1 / 2 - 10, buf, 0.18f);
        } else {
            if (!losePlayed1) {
                // Sleep(50); // The Sleep function is Windows-specific and usually avoided in GLUT callbacks
                playLose1();
                losePlayed1 = 1;
            }
            drawStrokeText1(WinW1 / 2 - 100, WinH1 / 2 + 40, "YOU LOSE!", 0.28f);
            sprintf(buf, "Final Score: %d", score1);
            drawStrokeText1(WinW1 / 2 - 120, WinH1 / 2 - 10, buf, 0.18f);
        }
    } else {
        drawStrokeText1(WinW1 / 2 - 230, 30, "Move mouse to aim | Space = Fire | R = Restart", 0.1f);
        applausePlayed1 = 0;
        losePlayed1 = 0;
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_LIGHTING);
}

// --- Game Logic Functions ---

void resetGame1() {
    score1 = 0;
    arrowsLeft1 = 10;
    arrowState1 = 0;
    aimPitch1 = 0;
    aimYaw1 = 0;
    arrowPos1 = bow1;
    hitTimer1 = 0;
}

int checkCollisionAndscore11() {
    Vec3 tip = vadd(arrowPos1, vscale(arrowDir1, arrowShaftLen1));
    Vec3 movingTarget = targetPos1;
    movingTarget.x += targetMoveX;

    Vec3 d = vsub(tip, movingTarget);
    float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;

    // These radii are relative to the center of the target face
    // and should be based on the constants used for drawing.
    float innerRadius = INNER_RADIUS;  // green zone (30.0f)
    float middleRadius = MIDDLE_RADIUS; // yellow zone (60.0f)
    float outerRadius = OUTER_RADIUS;  // red zone (80.0f)

    if (dist2 <= innerRadius * innerRadius) {
        score1 += 10; // Assuming 10 points for the center, adjusted for Level 2
        targetFreezeTimer = 50;
        playArrowHit1();
        return 1;
    } else if (dist2 <= middleRadius * middleRadius) {
        score1 += 5; // Yellow ring
        targetFreezeTimer = 50;
        playArrowHit1();
        return 1;
    } else if (dist2 <= outerRadius * outerRadius) {
        score1 += 1; // Red ring
        targetFreezeTimer = 50;
        playArrowHit1();
        return 1;
    }
    return 0; // no hit
}

void update1() {
    if (arrowState1 == 1) {
        // Arrow is flying
        arrowPos1 = vadd(arrowPos1, vscale(arrowDir1, arrowSpeed1));
        if (checkCollisionAndscore11()) {
            arrowState1 = 2; // Hit target
            hitTimer1 = 30;
        }
        // Check if arrow flew too far
        if (arrowPos1.z < -2000.0f || fabsf(arrowPos1.x) > 3000.0f)
            arrowState1 = 0; // Miss/Out of bounds
    } else if (arrowState1 == 2) {
        // Arrow hit target
        if (hitTimer1 > 0)
            hitTimer1--;
        else
            arrowState1 = 0; // Ready for next shot
    }

    // Target Movement
    if (targetFreezeTimer > 0) {
        targetFreezeTimer--;
    } else {
        targetMoveX += targetMoveDir * targetMoveSpeed;
        if (fabs(targetMoveX) > targetMoveRange) {
            targetMoveDir *= -1.0f; // reverse direction at edges
        }
    }

    glutPostRedisplay();
    glutTimerFunc(tickMs1, (void (*)(int))update1, 0);
}

void mouseMotion1(int mx, int my) {
    float nx = (float)mx / (float)WinW1;
    float ny = (float)my / (float)WinH1;
    float spanX = 750.0f;
    float spanY = 160.0f;

    // Map mouse position to a world-space aim point near the target plane
    aimPoint1.x = (nx - 0.5f) * spanX;
    aimPoint1.y = (0.5f - ny) * spanY;
}

void fireArrow1() {
    if (arrowsLeft1 <= 0 || arrowState1 != 0)
        return;

    Vec3 aimWorld = {aimPoint1.x, aimPoint1.y, targetPos1.z};
    arrowDir1 = vnorm(vsub(aimWorld, bow1));
    arrowPos1 = bow1;
    arrowState1 = 1;
    arrowsLeft1--;
    playArrowRelease1();
}

void keyboard1(unsigned char key, int x, int y) {
    if (key == 27)
        exit(0);
    if (key == ' ')
        fireArrow1();
    if (key == 'r' || key == 'R')
        resetGame1();
}

void reshape1(int w, int h) {
    WinW1 = w;
    WinH1 = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w / (double)h, 1.0, 5000.0);
    glMatrixMode(GL_MODELVIEW);
}

void display1() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Camera positioned slightly behind and above the bow for a 3rd person view
    Vec3 camPos = {bow1.x + 250.0f, bow1.y + 50.0f, bow1.z + 400.0f};
    gluLookAt(camPos.x, camPos.y, camPos.z,
              targetPos1.x, targetPos1.y, targetPos1.z,
              0.0f, 1.0f, 0.0f);

    setupLighting1();
    drawGroundSky1();
    drawTarget1();
    drawHangingStand1();

    // Bow and Arrow Drawing
    Vec3 aimWorld = {aimPoint1.x, aimPoint1.y, targetPos1.z};
    Vec3 dir = vnorm(vsub(aimWorld, bow1));
    drawbow1(bow1, dir);

    if (arrowState1 == 0) {
        // Arrow ready to fire, draw it on the bow
        drawArrow1(vadd(bow1, vscale(dir, 15.0f)), dir); // Offset slightly in front of the bow
    } else if (arrowState1 == 1 || arrowState1 == 2) {
        // Arrow flying or hit
        drawArrow1(arrowPos1, arrowDir1);
    }

    drawHUD1();
    glutSwapBuffers();
}

void initGL1() {
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_COLOR_MATERIAL);
    GLfloat globAmb[] = {0.2f, 0.2f, 0.2f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globAmb);
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);
}

void main1() {
    // Note: A GLUT application typically uses glutInit in the main function
    // but this function seems intended to be called after glutInit.

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Level 2 - Advanced Target Shooting");

    initGL1();
    resetGame1();
    initgrass11(); // Initialize grass data

    glutDisplayFunc(display1);
    glutReshapeFunc(reshape1);
    glutKeyboardFunc(keyboard1);
    glutPassiveMotionFunc(mouseMotion1);
    glutTimerFunc(tickMs1, (void (*)(int))update1, 0);

    // glutMainLoop() would typically be called here, but is omitted in this snippet.
}