// level1.c - Level 1 (properly indented and cleaned)
#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
  #include <mmsystem.h>
  #pragma comment(lib, "winmm.lib")
#endif

/* -------------------- Prototypes -------------------- */
void main1(void);
void display1(void);
void keyboard1(unsigned char key, int x, int y);
void reshape1(int w, int h);
void mouseMotion1(int mx, int my);
void update1(int v);
void initGL1(void);
void resetGame1(void);
void initgrass11(void);

/* -------------------- Sound helpers -------------------- */
#ifdef _WIN32
static void playArrowRelease(void) {
    PlaySound("arrow_release.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}
static void playArrowHit(void) {
    PlaySound("arrow_hit.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}
static void playApplause(void) {
    PlaySound("applause.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}
static void playGameOver(void) {
    PlaySound("gameover.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}
static void playLose(void) {
    PlaySound("lose.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}
static void stopAllSounds(void) {
    PlaySound(NULL, 0, 0);
}
#else
/* On non-Windows platforms, provide stubs so code compiles. */
static void playArrowRelease(void) {}
static void playArrowHit(void) {}
static void playApplause(void) {}
static void playGameOver(void) {}
static void playLose(void) {}
static void stopAllSounds(void) {}
#endif

/* -------------------- Constants & Globals -------------------- */
#define OUTER_RADIUS  80.0f
#define MIDDLE_RADIUS 60.0f
#define INNER_RADIUS  24.0f

int winW = 1024, winH = 720;

/* Game variables */
int score = 0;
int arrowsLeft = 10;
int gameState = 0; /* 0=ready,1=flying,2=hit,3=gameover */

/* Vector type */
typedef struct { float x, y, z; } Vec3;

/* Positions */
Vec3 bow       = { -200.0f, 0.0f, 100.0f };
Vec3 aimPoint  = { 0.0f, 0.0f, -600.0f };
Vec3 targetPos = { 0.0f, 0.0f, -600.0f };

/* Arrow */
Vec3 arrowPos;
Vec3 arrowDir;
float arrowSpeed = 32.0f;
float arrowShaftLen = 70.0f;
int arrowState = 0; /* 0 idle, 1 flying, 2 hit */
int hitTimer = 0;

float aimYaw = 0.0f;
float aimPitch = 0.0f;
int tickMs = 16;

/* Math helpers */
static inline Vec3 vadd(Vec3 a, Vec3 b) { return (Vec3){ a.x+b.x, a.y+b.y, a.z+b.z }; }
static inline Vec3 vsub(Vec3 a, Vec3 b) { return (Vec3){ a.x-b.x, a.y-b.y, a.z-b.z }; }
static inline Vec3 vscale(Vec3 v, float s) { return (Vec3){ v.x*s, v.y*s, v.z*s }; }
static inline float vlen(Vec3 v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
static inline Vec3 vnorm(Vec3 v) {
    float L = vlen(v);
    if (L < 1e-6f) return (Vec3){0,0,-1};
    return vscale(v, 1.0f/L);
}

/* Level flags */
int level = 1;
int levelComplete = 0;

/* Grass and fruits */
#define GRASS_COUNT 80000
typedef struct { float x, y, z, h; } GrassBlade;
GrassBlade grass[GRASS_COUNT];

#define FRUIT_COUNT 8
typedef struct { float x, y, z; } Fruit;
Fruit leftTreeFruits[FRUIT_COUNT];
Fruit rightTreeFruits[FRUIT_COUNT];

/* -------------------- Initialization helpers -------------------- */
void initGrass(void) {
    for (int i = 0; i < GRASS_COUNT; ++i) {
        grass[i].x = (float)((rand() % 4000) - 2000);
        grass[i].z = (float)((rand() % 4000) - 2000);
        grass[i].y = -200.0f;
        grass[i].h = 15.0f + (rand() % 4);
    }
}

void initFruits(Fruit fruits[], Vec3 treePos, int isLeftTree) {
    for (int i = 0; i < FRUIT_COUNT; ++i) {
        if (isLeftTree) {
            /* shift left tree fruits 50 units to the right */
            fruits[i].x = (float)((rand() % 50) + 70 + 10);
            fruits[i].z = (float)((rand() % 101) + 100);
        } else {
            fruits[i].x = (float)((rand() % 50) - 120);
            fruits[i].z = (float)((rand() % 101) + 100);
        }
        fruits[i].y = (float)((rand() % 151) + 150);
    }
}

float distance_vec(Vec3 a, Vec3 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

void printFruitDistances(Fruit fruits[], const char* treeName) {
    printf("Distances between fruits in %s:\n", treeName);
    for (int i = 0; i < FRUIT_COUNT; ++i) {
        for (int j = i+1; j < FRUIT_COUNT; ++j) {
            float d = distance_vec((Vec3){fruits[i].x, fruits[i].y, fruits[i].z},
                                   (Vec3){fruits[j].x, fruits[j].y, fruits[j].z});
            printf("Fruit %d ↔ Fruit %d: %.2f\n", i, j, d);
        }
    }
}

/* -------------------- Drawing helpers -------------------- */
void drawStrokeText(float x, float y, const char* s, float scale) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    glColor3f(1,1,1);
    for (int i = 0; s[i]; ++i) glutStrokeCharacter(GLUT_STROKE_ROMAN, s[i]);
    glPopMatrix();
}

void setupLighting(void) {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat ambient[]  = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat diffuse[]  = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat specular[] = {0.6f, 0.6f, 0.6f, 1.0f};
    GLfloat position[] = {300.0f, 400.0f, 300.0f, 1.0f};

    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
}

void drawGroundSky(void) {
    glDisable(GL_LIGHTING);

    /* Ground quad */
    glBegin(GL_QUADS);
        glColor3f(0.25f, 0.65f, 0.25f);
        glVertex3f(-2000.0f, -200.0f, 2000.0f);
        glVertex3f( 2000.0f, -200.0f, 2000.0f);
        glVertex3f( 2000.0f, -200.0f, -2000.0f);
        glVertex3f(-2000.0f, -200.0f, -2000.0f);
    glEnd();

    /* Grass lines */
    glColor3f(0.1f, 0.6f, 0.1f);
    glBegin(GL_LINES);
    for (int i = 0; i < GRASS_COUNT; ++i) {
        glVertex3f(grass[i].x, grass[i].y, grass[i].z);
        glVertex3f(grass[i].x, grass[i].y + grass[i].h, grass[i].z);
    }
    glEnd();

    /* Sky background */
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
        glColor3f(0.5f, 0.8f, 1.0f); glVertex3f(-2000, 1000, -1500);
        glColor3f(0.4f, 0.75f, 1.0f); glVertex3f(2000, 1000, -1500);
        glColor3f(0.6f, 0.85f, 1.0f); glVertex3f(2000, -200, -1500);
        glColor3f(0.7f, 0.9f, 1.0f); glVertex3f(-2000, -200, -1500);
    glEnd();

    glEnable(GL_LIGHTING);

    /* Decorative spheres/clouds */
    GLfloat lightColor[] = {1,1,1,1};
    GLfloat lightPos[] = {0.0f, 1000.0f, 500.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightColor);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    GLfloat pureWhite[] = {1,1,1,1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, pureWhite);

    glPushMatrix();
        glTranslatef(-900, 450, -1000);
        glutSolidSphere(90, 20, 20);
        glPushMatrix(); glTranslatef(80, 20, -10); glutSolidSphere(75, 20, 20); glPopMatrix();
        glPushMatrix(); glTranslatef(-70, 10, 15); glutSolidSphere(60, 20, 20); glPopMatrix();
    glPopMatrix();

    glPushMatrix();
        glTranslatef(0, 500, -950);
        glutSolidSphere(100, 20, 20);
        glPushMatrix(); glTranslatef(80, 15, 10); glutSolidSphere(85, 20, 20); glPopMatrix();
        glPushMatrix(); glTranslatef(-70, -10, -5); glutSolidSphere(70, 20, 20); glPopMatrix();
        glPushMatrix(); glTranslatef(20, -25, 10); glutSolidSphere(60, 20, 20); glPopMatrix();
    glPopMatrix();

    glPushMatrix();
        glTranslatef(800, 400, -1100);
        glutSolidSphere(80, 20, 20);
        glPushMatrix(); glTranslatef(70, 10, -5); glutSolidSphere(70, 20, 20); glPopMatrix();
        glPushMatrix(); glTranslatef(-60, 5, 15); glutSolidSphere(55, 20, 20); glPopMatrix();
        glPushMatrix(); glTranslatef(25, -15, 10); glutSolidSphere(50, 20, 20); glPopMatrix();
    glPopMatrix();

    /* Sun */
    glDisable(GL_LIGHTING);
    glPushMatrix();
        glTranslatef(900.0f, 550.0f, -1400.0f);
        glColor3f(1.0f, 1.0f, 0.0f);
        glutSolidSphere(100, 40, 40);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 1.0f, 0.0f, 0.3f); glutSolidSphere(140, 36, 36);
        glColor4f(1.0f, 1.0f, 0.0f, 0.15f); glutSolidSphere(180, 32, 32);
        glDisable(GL_BLEND);
    glPopMatrix();
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
}

/* Draw tree with fruits */
void drawTree(Vec3 pos, Fruit fruits[]) {
    glEnable(GL_COLOR_MATERIAL);
    glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        GLUquadric* q = gluNewQuadric();

        /* Trunk */
        glColor3f(0.36f, 0.25f, 0.2f);
        glPushMatrix();
            glRotatef(-90, 1, 0, 0);
            gluCylinder(q, 30.0, 25.0, 250.0, 24, 6);
        glPopMatrix();

        /* Leaves / canopy */
        glColor3f(0.1f, 0.6f, 0.1f);
        glTranslatef(0, 250.0f, 0);
        glutSolidSphere(100.0f, 32, 32);
        glTranslatef(0, 60.0f, 0);
        glutSolidSphere(80.0f, 28.0f, 28.0f);
        glTranslatef(0, 50.0f, 0);
        glutSolidSphere(60.0f, 24.0f, 24.0f);

        /* Fruits */
        glPushMatrix();
            glColor3f(1.0f, 0.0f, 0.0f);
            glTranslatef(0, -300.0f, 0);
            for (int i = 0; i < FRUIT_COUNT; ++i) {
                glPushMatrix();
                    glTranslatef(fruits[i].x, fruits[i].y, fruits[i].z);
                    glutSolidSphere(6.0f, 12, 12);
                glPopMatrix();
            }
        glPopMatrix();

        gluDeleteQuadric(q);
    glPopMatrix();
}

/* Draw the 3-ring target + posts */
void drawTarget(void) {
    glPushMatrix();
        glTranslatef(targetPos.x, targetPos.y, targetPos.z);
        GLUquadric* q = gluNewQuadric();

        /* Outer cylinder + disks */
        float outerHeight = 4.0f, middleHeight = 3.0f, innerHeight = 2.0f;
        glColor3f(1.0f, 0.1f, 0.1f);
        gluCylinder(q, OUTER_RADIUS, OUTER_RADIUS, outerHeight, 100, 1);
        glPushMatrix();
            glTranslatef(0, 0, outerHeight);
            gluDisk(q, MIDDLE_RADIUS, OUTER_RADIUS, 100, 1);
        glPopMatrix();

        /* Middle */
        glTranslatef(0, 0, outerHeight);
        glColor3f(1.0f, 1.0f, 0.2f);
        gluCylinder(q, MIDDLE_RADIUS, MIDDLE_RADIUS, middleHeight, 100, 1);
        glPushMatrix();
            glTranslatef(0, 0, middleHeight);
            gluDisk(q, INNER_RADIUS, MIDDLE_RADIUS, 100, 1);
        glPopMatrix();

        /* Inner */
        glTranslatef(0, 0, middleHeight);
        glColor3f(0.0f, 0.8f, 0.0f);
        gluCylinder(q, INNER_RADIUS, INNER_RADIUS, innerHeight, 100, 1);
        glPushMatrix();
            glTranslatef(0, 0, innerHeight);
            gluDisk(q, 0, INNER_RADIUS, 100, 1);
        glPopMatrix();

        gluDeleteQuadric(q);
    glPopMatrix();

    /* Two wooden posts */
    glEnable(GL_COLOR_MATERIAL);
    glColor3f(0.55f, 0.35f, 0.2f);
    glPushMatrix();
        glTranslatef(targetPos.x - 40.0f, targetPos.y - 60.0f, targetPos.z - 15.0f);
        glScalef(10.0f, 140.0f, 5.0f);
        glutSolidCube(1.2f);
    glPopMatrix();

    glPushMatrix();
        glTranslatef(targetPos.x + 40.0f, targetPos.y - 60.0f, targetPos.z - 15.0f);
        glScalef(10.0f, 140.0f, 5.0f);
        glutSolidCube(1.2f);
    glPopMatrix();
}

/* Draw arrow at given pos pointing in dir */
void drawArrow(Vec3 pos, Vec3 dir) {
    Vec3 n = vnorm(dir);
    float yaw   = atan2f(n.x, n.z) * 180.0f / M_PI;
    float pitch = atan2f(n.y, sqrtf(n.x*n.x + n.z*n.z)) * 180.0f / M_PI;

    glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glRotatef(yaw,   0, 1, 0);
        glRotatef(pitch, 1, 0, 0);

        GLUquadric* q = gluNewQuadric();

        /* Shaft */
        glColor3f(0.8f, 0.8f, 0.85f);
        gluCylinder(q, 2.5, 2.5, arrowShaftLen, 12, 2);

        /* Tip */
        glTranslatef(0, 0, arrowShaftLen);
        glColor3f(1.0f, 0.1f, 0.1f);
        glutSolidCone(5.0, 15.0, 12, 2);

        /* Fletching/back triangle */
        glTranslatef(0, 10, -15.0f);
        glColor3f(0.1f, 0.3f, 0.9f);
        glBegin(GL_TRIANGLES);
            glVertex3f(0, 4, 0);
            glVertex3f(0, -4, 0);
            glVertex3f(-10, 0, -10);
        glEnd();

        gluDeleteQuadric(q);
    glPopMatrix();
}

/* Draw bow at pos, oriented to aimDir */
void drawBow(Vec3 pos, Vec3 aimDir) {
    glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        Vec3 n = vnorm(aimDir);
        float yaw   = atan2f(n.x, n.z) * 180.0f / M_PI;
        float pitch = atan2f(n.y, sqrtf(n.x*n.x + n.z*n.z)) * 180.0f / M_PI;
        glRotatef(yaw,   0, 1, 0);
        glRotatef(pitch, 1, 0, 0);

        GLUquadric* q = gluNewQuadric();

        glColor3f(0.55f, 0.27f, 0.07f); /* wood */
        glPushMatrix();
            glRotatef(-30, 0, 0, 1);
            gluCylinder(q, 3.0, 2.0, 80.0, 12, 2);
        glPopMatrix();

        glPushMatrix();
            glRotatef(30, 0, 0, 1);
            gluCylinder(q, 3.0, 2.0, 80.0, 12, 2);
        glPopMatrix();

        /* String */
        glColor3f(0.9f, 0.9f, 0.9f);
        glBegin(GL_LINES);
            glVertex3f(-50, 0, 0);
            glVertex3f( 50, 0, 0);
        glEnd();

        gluDeleteQuadric(q);
    glPopMatrix();
}

/* Heads-up display */
void drawHUD(void) {
    static int applausePlayed = 0;
    static int losePlayed = 0;

    glDisable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, winW, 0, winH);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
            glLoadIdentity();

            char buf[128];
            glColor3f(1,1,0);
            drawStrokeText(winW/2 - 90, winH - 50, "LEVEL 1", 0.25f);
            drawStrokeText(10, winH - 20, "Score Required: 25", 0.12f);

            sprintf(buf, "Score: %d", score);
            drawStrokeText(10, winH - 60, buf, 0.12f);

            sprintf(buf, "Arrows: %d", arrowsLeft);
            drawStrokeText(10, winH - 100, buf, 0.12f);

            if (arrowsLeft == 0 && arrowState == 0) {
                if (score >= 25) {
                    if (!applausePlayed) { playApplause(); applausePlayed = 1; }
                    drawStrokeText(winW/2 - 200, winH/2 + 40, "LEVEL 1 COMPLETE!", 0.28f);
                    drawStrokeText(winW/2 - 250, winH/2 - 10, "Press 'Y' to continue to Level 2", 0.16f);
                    levelComplete = 1;
                } else {
                    if (!losePlayed) { playLose(); playGameOver(); losePlayed = 1; }
                    drawStrokeText(winW/2 - 100, winH/2 + 40, "YOU LOSE!", 0.28f);
                    sprintf(buf, "Final Score: %d", score);
                    drawStrokeText(winW/2 - 120, winH/2 - 10, buf, 0.18f);
                    drawStrokeText(winW/2 - 230, winH/2 - 60, "Press 'R' to Restart", 0.12f);
                    levelComplete = 0;
                }
            } else {
                drawStrokeText(winW/2 - 230, 30, "Move mouse to aim | Space = Fire | R = Restart", 0.1f);
                applausePlayed = 0;
                losePlayed = 0;
            }

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_LIGHTING);
}

/* -------------------- Game control -------------------- */
void resetGame(void) {
    score = 0;
    arrowsLeft = 10;
    arrowState = 0;
    aimPitch = 0;
    aimYaw = 0;
    arrowPos = bow;
    hitTimer = 0;
}

int checkCollisionAndScore(void) {
    Vec3 tip = vadd(arrowPos, vscale(arrowDir, arrowShaftLen));
    Vec3 d = vsub(tip, targetPos);
    float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;

    if (dist2 <= INNER_RADIUS*INNER_RADIUS) {
        score += 10; playArrowHit(); return 1;
    }
    if (dist2 <= MIDDLE_RADIUS*MIDDLE_RADIUS) {
        score += 5; playArrowHit(); return 1;
    }
    if (dist2 <= OUTER_RADIUS*OUTER_RADIUS) {
        score += 1; playArrowHit(); return 1;
    }
    return 0;
}

/* Timer update callback */
void update(int v) {
    if (arrowState == 1) {
        arrowPos = vadd(arrowPos, vscale(arrowDir, arrowSpeed));
        if (checkCollisionAndScore()) {
            arrowState = 2;
            hitTimer = 30;
        }
        if (arrowPos.z < -2000.0f || fabsf(arrowPos.x) > 3000.0f) arrowState = 0;
    } else if (arrowState == 2) {
        if (hitTimer > 0) hitTimer--;
        else arrowState = 0;
    }

    glutPostRedisplay();
    glutTimerFunc(tickMs, update, 0);
}

/* Mouse motion */
void mouseMotion(int mx, int my) {
    float nx = (float)mx / (float)winW;
    float ny = (float)my / (float)winH;
    float spanX = 220.0f;
    float spanY = 160.0f;
    aimPoint.x = (nx - 0.5f) * spanX;
    aimPoint.y = (0.5f - ny) * spanY;
}

/* Fire arrow */
void fireArrow(void) {
    if (arrowsLeft <= 0 || arrowState != 0) return;
    Vec3 aimWorld = { aimPoint.x, aimPoint.y, targetPos.z };
    arrowDir = vnorm(vsub(aimWorld, bow));
    arrowPos = bow;
    arrowState = 1;
    arrowsLeft--;
    playArrowRelease();
}

/* Main keyboard handler */
void keyboard(unsigned char key, int x, int y) {
    if (key == 27) exit(0);
    if (key == ' ') fireArrow();
    if (key == 'r' || key == 'R') {
        stopAllSounds();
        resetGame();
    }
    if ((key == 'y' || key == 'Y') && score >= 25) {
        /* switch to level 2 handlers (assumes those functions exist in other source) */
        stopAllSounds();
        printf("Starting Level 2...\n");
        glutDisplayFunc(display1);
        glutKeyboardFunc(keyboard1);
        glutReshapeFunc(reshape1);
        glutPassiveMotionFunc(mouseMotion1);
        glutTimerFunc(tickMs, (void (*)(int))update1, 0);
        initGL1();
        resetGame1();
        initgrass11();
        glutPostRedisplay();
    }
}

/* Window reshape */
void reshape(int w, int h) {
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w/(double)h, 1.0, 5000.0);
    glMatrixMode(GL_MODELVIEW);
}

/* Display callback */
void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    Vec3 camPos = { bow.x + 250.0f, bow.y + 120.0f, bow.z + 400.0f };
    gluLookAt(camPos.x, camPos.y, camPos.z,
              targetPos.x, targetPos.y, targetPos.z,
              0.0f, 1.0f, 0.0f);

    setupLighting();
    drawGroundSky();
    drawTree((Vec3){-800.0f, -200.0f, -650.0f}, leftTreeFruits);
    drawTree((Vec3){ 800.0f, -200.0f, -650.0f}, rightTreeFruits);
    drawTarget();

    Vec3 aimWorld = { aimPoint.x, aimPoint.y, targetPos.z };
    Vec3 dir = vnorm(vsub(aimWorld, bow));
    drawBow(bow, dir);

    if (arrowState == 0) drawArrow(bow, dir);
    else                drawArrow(arrowPos, arrowDir);

    drawHUD();
    glutSwapBuffers();
}

/* OpenGL initialization for level 1 */
void initGL(void) {
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_COLOR_MATERIAL);
    GLfloat globAmb[] = {0.2f, 0.2f, 0.2f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globAmb);
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f);

    initFruits(leftTreeFruits,  (Vec3){-800.0f, -200.0f, -650.0f}, 1);
    initFruits(rightTreeFruits, (Vec3){ 800.0f, -200.0f, -650.0f}, 0);
}

/* Main */
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("3D Target Shooter - Level 1");

    srand((unsigned int)time(NULL));

    initGL();
    resetGame();
    initGrass();

    printFruitDistances(leftTreeFruits,  "Left Tree");
    printFruitDistances(rightTreeFruits, "Right Tree");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(mouseMotion);
    glutTimerFunc(tickMs, update, 0);

    glutMainLoop();
    return 0;
}
