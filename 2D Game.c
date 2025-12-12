#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int width = 800, height = 600;

float targetRotation = 0.0f;
int score = 0;

int arrowCount = 10;
int arrowState = 0;
float aimArrowY = 300.0f;
int aimDirection = 1;
float arrowX = 100.0f;

float arrowScale = 1.0f;
int arrowHitDelay = 0;

const float aimSpeed = 6.0f;
const float fireSpeed = 20.0f;
const int arrowLen = 150;
const int bowX = 100;

float scoreScaleFactor = 1.0f;
int scoreScaleTimer = 0;

typedef struct { float x, y; } PointF;

void rotatePoint(int cx, int cy, int x, int y, float angle, int *rx, int *ry) {
    float rad = angle * 3.14159f / 180.0f;
    *rx = (int)(cx + (x - cx) * cosf(rad) - (y - cy) * sinf(rad));
    *ry = (int)(cy + (x - cx) * sinf(rad) + (y - cy) * cosf(rad));
}

void drawDDALine(int x1, int y1, int x2, int y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float steps = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
    if (steps == 0) {
        glBegin(GL_POINTS); glVertex2i(x1,y1); glEnd(); return;
    }
    float xInc = dx / steps;
    float yInc = dy / steps;
    float x = x1, y = y1;

    glBegin(GL_POINTS);
    for(int i = 0; i <= (int)steps; i++) {
        glVertex2i((int)roundf(x), (int)roundf(y));
        x += xInc; y += yInc;
    }
    glEnd();
}

void scanlineFill(int x[], int y[], int n, float r, float g, float b) {
    if (n < 3) return;
    int i, j, k, temp, xi[1024];
    float slope[1024];
    int ymin = y[0], ymax = y[0];
    for (i=1;i<n;i++) { if (y[i] < ymin) ymin = y[i]; if (y[i] > ymax) ymax = y[i]; }
    for (i=0;i<n-1;i++) {
        if (y[i] == y[i+1]) slope[i] = 0.0f;
        else slope[i] = (float)(x[i+1] - x[i]) / (float)(y[i+1] - y[i]);
    }
    if (y[n-1] == y[0]) slope[n-1] = 0.0f;
    else slope[n-1] = (float)(x[0] - x[n-1]) / (float)(y[0] - y[n-1]);

    for (int scan=ymin; scan<=ymax; scan++) {
        k = 0;
        for (i=0;i<n;i++) {
            int jidx = (i+1)%n;
            if ( ((y[i] <= scan) && (y[jidx] > scan)) || ((y[i] > scan) && (y[jidx] <= scan)) ) {
                xi[k] = (int)roundf(x[i] + (scan - y[i]) * slope[i]);
                k++;
            }
        }
        // sort xi
        for (i=0;i<k-1;i++) for (j=0;j<k-1;j++) if (xi[j] > xi[j+1]) { temp = xi[j]; xi[j] = xi[j+1]; xi[j+1] = temp; }
        glColor3f(r,g,b);
        glBegin(GL_LINES);
        for (i=0;i<k;i+=2) {
            if (i+1 < k) {
                glVertex2i(xi[i], scan);
                glVertex2i(xi[i+1], scan);
            }
        }
        glEnd();
    }
}

// Circle scanline fill used for target rings (keeps rotation visual)
void scanlineFillCircle(int xc, int yc, int r, float angle, float rcol, float gcol, float bcol) {
    for (int y=-r; y<=r; y++) {
        int xLimit = (int)(sqrtf((float)r*r - (float)y*y));
        int x1 = xc - xLimit, x2 = xc + xLimit;
        int yscan = yc + y;

        int rx1, ry1, rx2, ry2;
        rotatePoint(xc, yc, x1, yscan, angle, &rx1, &ry1);
        rotatePoint(xc, yc, x2, yscan, angle, &rx2, &ry2);

        glColor3f(rcol,gcol,bcol);
        drawDDALine(rx1, ry1, rx2, ry2);
    }
}

// Build a polygon approximating a circle (CCW)
void buildCirclePolygon(PointF *polyOut, int *countOut, float cx, float cy, float r, int sides) {
    if (sides < 4) sides = 4;
    float step = 2.0f * M_PI / (float)sides;
    int idx = 0;
    for (int i=0;i<sides;i++) {
        float a = i * step;
        polyOut[idx].x = cx + r * cosf(a);
        polyOut[idx].y = cy + r * sinf(a);
        idx++;
    }
    *countOut = idx;
}

// Helper: cross product (B-A) x (P-A)
float cross(const PointF *A, const PointF *B, const PointF *P) {
    return (B->x - A->x) * (P->y - A->y) - (B->y - A->y) * (P->x - A->x);
}

// Compute intersection between segment P->Q and edge A->B
// returns 1 if intersection found, fills outPoint
int lineIntersection(const PointF *P, const PointF *Q, const PointF *A, const PointF *B, PointF *outPoint) {
    float a1 = Q->y - P->y;
    float b1 = P->x - Q->x;
    float c1 = a1 * P->x + b1 * P->y;

    float a2 = B->y - A->y;
    float b2 = A->x - B->x;
    float c2 = a2 * A->x + b2 * A->y;

    float det = a1 * b2 - a2 * b1;
    if (fabsf(det) < 1e-6f) return 0; // parallel or nearly so

    outPoint->x = (b2 * c1 - b1 * c2) / det;
    outPoint->y = (-a2 * c1 + a1 * c2) / det;
    return 1;
}

// Sutherland-Hodgman polygon clipping: subject polygon clipped by convex clip polygon (both as arrays of PointF)
void sutherlandHodgman(const PointF *subject, int subjCount, const PointF *clip, int clipCount, PointF *outPoly, int *outCount) {
    // We'll work with temporary buffers (max 2048 points)
    static PointF input[2048], output[2048];
    int inCount = subjCount;
    if (inCount <= 0) { *outCount = 0; return; }
    for (int i=0;i<inCount;i++) input[i] = subject[i];

    for (int e=0;e<clipCount; e++) {
        PointF A = clip[e];
        PointF B = clip[(e+1)%clipCount];
        int outIdx = 0;
        for (int i=0;i<inCount;i++) {
            PointF P = input[i];
            PointF Q = input[(i+1)%inCount];
            float cpP = cross(&A, &B, &P);
            float cpQ = cross(&A, &B, &Q);
            int insideP = (cpP >= 0.0f);
            int insideQ = (cpQ >= 0.0f);

            if (insideP && insideQ) {
                // both inside: output Q
                output[outIdx++] = Q;
            } else if (insideP && !insideQ) {
                // leaving: output intersection
                PointF I;
                if (lineIntersection(&P, &Q, &A, &B, &I)) output[outIdx++] = I;
            } else if (!insideP && insideQ) {
                // entering: output intersection then Q
                PointF I;
                if (lineIntersection(&P, &Q, &A, &B, &I)) output[outIdx++] = I;
                output[outIdx++] = Q;
            } else {
                // both outside: output nothing
            }
            if (outIdx >= 2047) break;
        }
        // move output to input for next edge
        inCount = outIdx;
        if (inCount == 0) break;
        for (int k=0;k<inCount;k++) input[k] = output[k];
    }

    // copy to outPoly
    *outCount = inCount;
    for (int i=0;i<inCount;i++) outPoly[i] = input[i];
}

// Build arrow polygon (shaft as rectangle + triangular head). CCW polygon.
void buildArrowPolygon(PointF *polyOut, int *countOut, float baseX, float midY, float scale) {
    float x1 = baseX;
    float x2 = baseX + arrowLen * scale;
    float halfWidth = 4.0f * scale;

    // rectangle shaft: A,B,C,D then triangle head points
    // define CCW
    int idx = 0;
    // shaft rectangle top-left
    polyOut[idx++] = (PointF){ x1, midY + halfWidth };
    polyOut[idx++] = (PointF){ x2 - 20.0f*scale, midY + halfWidth };
    // arrowhead triangle top
    polyOut[idx++] = (PointF){ x2 - 20.0f*scale, midY + 10.0f*scale };
    polyOut[idx++] = (PointF){ x2, midY };
    polyOut[idx++] = (PointF){ x2 - 20.0f*scale, midY - 10.0f*scale };
    // shaft bottom back
    polyOut[idx++] = (PointF){ x2 - 20.0f*scale, midY - halfWidth };
    polyOut[idx++] = (PointF){ x1, midY - halfWidth };
    *countOut = idx;
}

// draws polygon outline
void drawPolygonOutline(const PointF *poly, int n) {
    glBegin(GL_LINE_LOOP);
    for (int i=0;i<n;i++) glVertex2f(poly[i].x, poly[i].y);
    glEnd();
}

// Fill polygon via scanlineFill: convert float poly to int arrays
void fillPolygonFloat(const PointF *poly, int n, float r, float g, float b) {
    if (n < 3) return;
    int xi[1024], yi[1024];
    for (int i=0;i<n;i++) {
        xi[i] = (int)roundf(poly[i].x);
        yi[i] = (int)roundf(poly[i].y);
    }
    scanlineFill(xi, yi, n, r,g,b);
}

void drawArrowAt(int baseX, int midY, float scaleFactor) {
    // draw full arrow (outline + filled head)
    int x1 = baseX, y1 = midY;
    int x2 = baseX + (int)(arrowLen * scaleFactor), y2 = midY;

    // shaft line
    glColor3f(1,1,1);
    drawDDALine(x1, y1, x2, y2);

    // triangle head fill
    int triX[3] = { x2, x2 - (int)(15*scaleFactor), x2 - (int)(15*scaleFactor) };
    int triY[3] = { y2, y2 + (int)(10*scaleFactor), y2 - (int)(10*scaleFactor) };
    scanlineFill(triX, triY, 3, 1,1,1);

    for (int i=0;i<3;i++) drawDDALine(triX[i], triY[i], triX[(i+1)%3], triY[(i+1)%3]);
}

void drawTarget() {
    int cx = 600, cy = 300;
    float angle = targetRotation;

    // Bullseye color pattern
    scanlineFillCircle(cx, cy, 80, angle, 1.0f, 0.0f, 0.0f); // outer red
    scanlineFillCircle(cx, cy, 60, angle, 1.0f, 1.0f, 0.0f); // middle yellow
    scanlineFillCircle(cx, cy, 24, angle, 0.0f, 0.8f, 0.0f); // inner green

    // Stand
    int standX[4] = {cx-60, cx+60, cx+60, cx-60};
    int standY[4] = {120, 120, 140, 140};
    scanlineFill(standX, standY, 4, 0.5f,0.25f,0.1f);
    drawDDALine(cx-40, cy-65, cx-40, 140);
    drawDDALine(cx+40, cy-65, cx+40, 140);
}

void drawBackground() {
    int skyX[4] = {0,width,width,0};
    int skyY[4] = {height*0.2f,height*0.2f,height,height};
    scanlineFill(skyX, skyY, 4, 0.4f,0.8f,1.0f);

    int grassX[4] = {0,width,width,0};
    int grassY[4] = {0,0,height*0.2f,height*0.2f};
    scanlineFill(grassX, grassY, 4, 0.2f,0.8f,0.2f);
}

void drawStrokeText(float x,float y,const char* s,float scale){
    glPushMatrix();
    glTranslatef(x,y,0);
    glScalef(scale,scale,1);
    glColor3f(1,1,1);
    for(int i=0;s[i];i++) glutStrokeCharacter(GLUT_STROKE_ROMAN,s[i]);
    glPopMatrix();
}

void drawHUD() {
    char scoreStr[64]; sprintf(scoreStr,"Score: %d",score);
    drawStrokeText(10,height-50,scoreStr,0.15f*scoreScaleFactor);
    char arrowStr[64]; sprintf(arrowStr,"Arrows: %d",arrowCount);
    drawStrokeText(10,height-90,arrowStr,0.15f);
}

void drawGameOver() {
    if(arrowCount==0 && arrowState==0){
        drawStrokeText(width/2-200,height/2+20,"GAME OVER",0.3f);
        char finalStr[64]; sprintf(finalStr,"Final Score: %d",score);
        drawStrokeText(width/2-220,height/2-40,finalStr,0.2f);
        drawStrokeText(width/2-280,height/2-90,"Press 'R' to Restart",0.15f);
    }
}

// bullseye collision (center small circle)
int checkBullseyeHit(int tipX,int tipY){
    int cx=600, cy=300, r=20;
    int dx = tipX-cx, dy=tipY-cy;
    return (dx*dx + dy*dy)<=r*r;
}
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // If game is over, apply clipping
    if (arrowCount == 0 && arrowState == 0) {
        // Define clipping window to show only the Game Over region
        // (adjust these based on the screenshot)
        float clipXmin = 100.0f;
float clipYmin = 200.0f;
float clipXmax = 700.0f;
float clipYmax = 360.0f;

        PointF clipPoly[4] = {
            {clipXmin, clipYmin},
            {clipXmax, clipYmin},
            {clipXmax, clipYmax},
            {clipXmin, clipYmax}
        };

        // ---- Draw scene but clip each polygon with this region ----
        // Background polygons
        int skyX[4] = {0,width,width,0};
        int skyY[4] = {height*0.2f,height*0.2f,height,height};
        PointF skyPoly[4], clipped[2048]; int cnt;
        for (int i=0;i<4;i++) skyPoly[i] = (PointF){skyX[i], skyY[i]};
        sutherlandHodgman(skyPoly,4,clipPoly,4,clipped,&cnt);
        if (cnt>2) fillPolygonFloat(clipped,cnt,0.4f,0.8f,1.0f);

        int grassX[4] = {0,width,width,0};
        int grassY[4] = {0,0,height*0.2f,height*0.2f};
        for (int i=0;i<4;i++) skyPoly[i] = (PointF){grassX[i], grassY[i]};
        sutherlandHodgman(skyPoly,4,clipPoly,4,clipped,&cnt);
        if (cnt>2) fillPolygonFloat(clipped,cnt,0.2f,0.8f,0.2f);

        // Target (clipped)
        PointF circlePoly[128];
        int n;
        buildCirclePolygon(circlePoly, &n, 600, 300, 80, 64);
        sutherlandHodgman(circlePoly, n, clipPoly, 4, clipped, &cnt);
        if (cnt>2) fillPolygonFloat(clipped,cnt,1,0,0); // outer red

        buildCirclePolygon(circlePoly, &n, 600, 300, 60, 64);
        sutherlandHodgman(circlePoly, n, clipPoly, 4, clipped, &cnt);
        if (cnt>2) fillPolygonFloat(clipped,cnt,1,1,0); // middle yellow

        buildCirclePolygon(circlePoly, &n, 600, 300, 24, 64);
        sutherlandHodgman(circlePoly, n, clipPoly, 4, clipped, &cnt);
        if (cnt>2) fillPolygonFloat(clipped,cnt,0,0.8f,0); // inner green

        // ---- Stand base (brown rectangle) ----
        // base was previously standX/standY with values {cx-60, cx+60, cx+60, cx-60} and y {120,120,140,140}
        PointF standBase[4] = { {540.0f, 120.0f}, {660.0f, 120.0f}, {660.0f, 140.0f}, {540.0f, 140.0f} };
        sutherlandHodgman(standBase, 4, clipPoly, 4, clipped, &cnt);
        if (cnt > 2) fillPolygonFloat(clipped, cnt, 0.5f, 0.25f, 0.1f);

        // ---- Stand posts (make them small quads so they clip correctly) ----
        // left post at x = cx-40, vertical from cy-65 to 140
        {
            float cx = 600.0f, cy = 300.0f;
            float postHalfW = 3.0f;             // thickness of post
            float topY = cy - 65.0f;
            float botY = 140.0f;
            float postX = cx - 40.0f;

            PointF leftPost[4] = {
                {postX - postHalfW, topY},
                {postX + postHalfW, topY},
                {postX + postHalfW, botY},
                {postX - postHalfW, botY}
            };
            sutherlandHodgman(leftPost, 4, clipPoly, 4, clipped, &cnt);
            if (cnt > 2) fillPolygonFloat(clipped, cnt, 0.12f, 0.12f, 0.12f); // near-black posts
        }

        // right post at x = cx+40
        {
            float cx = 600.0f, cy = 300.0f;
            float postHalfW = 3.0f;
            float topY = cy - 65.0f;
            float botY = 140.0f;
            float postX = cx + 40.0f;

            PointF rightPost[4] = {
                {postX - postHalfW, topY},
                {postX + postHalfW, topY},
                {postX + postHalfW, botY},
                {postX - postHalfW, botY}
            };
            sutherlandHodgman(rightPost, 4, clipPoly, 4, clipped, &cnt);
            if (cnt > 2) fillPolygonFloat(clipped, cnt, 0.12f, 0.12f, 0.12f);
        }
        // ---- HUD text inside clipped region ----
        drawGameOver();

    } else {
        // Normal gameplay drawing
        drawBackground();
        drawTarget();
        glColor3f(1.0f, 1.0f, 1.0f);
        if (arrowState == 0)
            drawArrowAt(bowX, (int)aimArrowY, 1.0f);
        else
            drawArrowAt((int)arrowX, (int)aimArrowY, arrowScale);
        drawHUD();
    }

    glFlush();
}

void timer(int v){
    if(scoreScaleTimer>0) scoreScaleTimer--; else scoreScaleFactor=1.0f;

    targetRotation += 2.0f; if(targetRotation>=360) targetRotation-=360;

    if(!(arrowCount==0 && arrowState==0)){
        if(arrowState==0){
            aimArrowY += aimDirection*aimSpeed;
            if(aimArrowY>height-50){ aimArrowY=height-50; aimDirection=-1; }
            if(aimArrowY<50){ aimArrowY=50; aimDirection=1; }
        } else if(arrowState==1){
            arrowX+=fireSpeed;
            int tipX=(int)(arrowX + arrowLen*arrowScale);
            int tipY=(int)aimArrowY;
            if(checkBullseyeHit(tipX, tipY)){
                score+=10; scoreScaleFactor=1.5f; scoreScaleTimer=8;
                arrowState=2; arrowHitDelay=20;
            }
            if(arrowX>width){ arrowState=0; arrowX=(float)bowX; }
        } else if(arrowState==2){
            if(--arrowHitDelay<=0){ arrowState=0; arrowX=(float)bowX; }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(33,timer,0);
}

void keyboard(unsigned char key,int x,int y){
    if(key==' '){
        if(arrowState==0 && arrowCount>0){
            arrowState=1; arrowX=(float)bowX; arrowScale=1.0f; arrowCount--;
        }
    } else if(key=='r' || key=='R'){
        if(arrowCount==0 && arrowState==0){
            score=0; arrowCount=10; arrowState=0; aimArrowY=300.0f; aimDirection=1; arrowX=(float)bowX;
            arrowScale=1.0f; scoreScaleFactor=1.0f; scoreScaleTimer=0;
        }
    }
}

void init(){
    glClearColor(0,0,0,1);
    gluOrtho2D(0,width,0,height);
}

int main(int argc, char** argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(width,height);
    glutInitWindowPosition(100,100);
    glutCreateWindow("Target Shooting Game with Clipped Arrow (Sutherland-Hodgman)");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0,timer,0);
    glutMainLoop();
    return 0;
}
