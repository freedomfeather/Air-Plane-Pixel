/*
    Program to print map of ITB
    Assumption: use 32bpp screen
*/

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "headers/VectorPath.h"

#define SCALE 1
#define RUNNING 1
#define WORLD_WIDTH 2000
#define WORLD_HEIGHT 2000
#define VIEWPORT_X 650
#define VIEWPORT_Y 250
#define VIEWPORT_WIDTH 640
#define VIEWPORT_HEIGHT 480
#define VIEWPORT_SPEED 5
#define NUM_OF_OBJECT 4
#define COLOR_CRITICAL rgbaToInt(250,250,250,0)
#define COLOR_FRAME rgbaToInt(247,247,247,0)
#define COLOR_BLUE rgbaToInt(0,0,255,0)

// World
VectorPath** world_data;
unsigned int world_x = 0;
unsigned int world_y = 0;
unsigned int world[WORLD_WIDTH][WORLD_HEIGHT]; 
int viewport_x;
int viewport_y;
int viewport_width = 640;
int viewport_height = 480;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

char *fbp = 0;
int centerX = 0;
int fullY = 0;
int frameColor;
int critColor;

// initFrameBuffer: preparing frame buffer so can be used in this program, return screensize
long int initFrameBuffer();
void drawWindowBorder();
void fillWindow(int color);
int getObjectPosition(VectorPath* object); // 0: didalam, 1: berpotongan, 2: diluar

void render();
void clearViewPort(int color);
void clearScreen();
int isValidPoint(int x, int y);
void drawLineLow(double x0, double y0, double x1, double y1, unsigned int color);
void drawLineHigh(double x0, double y0, double x1, double y1, unsigned int color);
void drawVectorLine(VectorPoint* point1, VectorPoint* point2, unsigned int color, int offsetX, int offsetY);
int drawVectorPath(VectorPath* path, unsigned int boundaryColor, unsigned int color, int offsetX, int offsetY);
void drawCritPoint(VectorPath* path, int offsetX, int offsetY, unsigned int boundaryColor);
int rotatePath(VectorPath* path, float degree, int originX, int originY);
int dilatatePath(VectorPath* path, int originX, int originY, float zoom);
int translatePath(VectorPath* path, int dx, int dy);
void drawPixel(int x, int y, unsigned int color);
unsigned int rgbaToInt(int r, int g, int b, int a);
unsigned int getPixelColor(int x, int y);
void determineCriticalPoint(VectorPath* vecPath);
void fillVector(VectorPath* path, unsigned int fillColor, unsigned int boundaryColor, int offsetX, int offsetY);
int isCritPoint(int i, int j, unsigned int boundaryColor);

// new in tugas_6
void drawCircle(int x0, int y0, int radius, unsigned int boundaryColor, unsigned int fillColor);
void fillCircle(unsigned int boundaryColor, unsigned int fillColor);

// main program ===================================================================================================
int main() {
    long int screensize = initFrameBuffer();

    // Prepare drawing viewport
    if (VIEWPORT_WIDTH > vinfo.xres || VIEWPORT_HEIGHT > vinfo.yres){
        printf("Wrong viewport size!\n");
        exit(5);
    }
    clearScreen();
    fillWindow(COLOR_BLUE);
    drawWindowBorder();

    // Preparing world
    world_data = malloc(sizeof(VectorPath*) * NUM_OF_OBJECT);
    world_data[0] = createVectorPathFromFile("assets/labtek_5.txt");
    world_data[1] = createVectorPathFromFile("assets/labtek_6.txt");
    world_data[2] = createVectorPathFromFile("assets/labtek_7.txt");
    world_data[3] = createVectorPathFromFile("assets/labtek_8.txt");
    // while (1) {
        // Draw only object inside the viewport
        for(int i = 0; i < NUM_OF_OBJECT; i++) {
            drawVectorPath(world_data[i], rgbaToInt(0,255,0,0), rgbaToInt(255,0,0,0), 0, 0);
            // int position = getObjectPosition(world_data[i]);
            // if (position == 1) {
            //     // cliping
            // } else if (position == 0){
            //     drawVectorPath(world_data[i], COLOR_BLUE, COLOR_FRAME, 0, 0);
            // }
        }
    // }
    
    munmap(fbp, screensize);
    return 0;
}


// function implementation  ========================================================================================
long int initFrameBuffer() {
    int fbfd = 0;
    long int screensize = 0;

    // Open the file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        exit(1);
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error reading fixed information");
        exit(2);
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error reading variable information");
        exit(3);
    }
    printf("Detected display: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // Figure out the size of the screen in bytes
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    // Map the device to memory
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        exit(4);
    }
    
    close(fbfd);
    return screensize;
}

void drawWindowBorder() {
    for (int x = VIEWPORT_X; x < VIEWPORT_X + VIEWPORT_WIDTH; x++) {
        drawPixel(x,VIEWPORT_Y,COLOR_FRAME);
        drawPixel(x,VIEWPORT_Y + VIEWPORT_HEIGHT,COLOR_FRAME);
    }
    for (int y = VIEWPORT_Y; y < VIEWPORT_Y + VIEWPORT_HEIGHT; y++) {
        drawPixel(VIEWPORT_X,y,COLOR_FRAME);
        drawPixel(VIEWPORT_X + VIEWPORT_WIDTH,y,COLOR_FRAME);
    }
}

void fillWindow(int color) {
    for (int x = VIEWPORT_X; x < VIEWPORT_X + VIEWPORT_WIDTH; x++)
        for (int y = VIEWPORT_Y; y < VIEWPORT_Y + VIEWPORT_HEIGHT;y++)
            drawPixel(x,y,color);
}

int getObjectPosition(VectorPath* object) {
    int max_x, max_y, min_x, min_y;
    VectorPoint** currentPoint = object->firstPoint;
    VectorPoint** nextPoint = object->firstPoint[0]->nextPoint;

    max_x = min_x = currentPoint[0]->x;
    max_y = min_y = currentPoint[0]->y;
    while (currentPoint != NULL) {
        if (currentPoint[0]->x > max_x)
            max_x = currentPoint[0]->x;
        if (currentPoint[0]->y > max_y)
            max_y = currentPoint[0]->y;
        if (currentPoint[0]->x < min_x)
            min_x = currentPoint[0]->x;
        if (currentPoint[0]->y < min_y)
            min_y = currentPoint[0]->x;
    }

    return 0;
}

void clearScreen(){
    system("clear");
}



























void render(){
    long int location;
    int color;
    int offsetX = (vinfo.xres - viewport_width)/2;
    int offsetY = (vinfo.yres - viewport_height)/2;


    for(int x = 0; x < viewport_width; x++){
        for(int y = 0; y < viewport_height; y++){
            int worldx = x + viewport_x;
            int worldy = y + viewport_y;
            if(isValidPoint(worldx,worldy)){
                color = world[worldx][worldy];
            }
            else{
                color = rgbaToInt(0,0,0,0);
            }
            location = (x+offsetX+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y+offsetY+vinfo.yoffset) * finfo.line_length;
            *(fbp + location) = color;
            *(fbp + location + 1) = color >> 8;
            *(fbp + location + 2) = color >> 16;
            *(fbp + location + 3) = color >> 24;
        }
    }
}

void clearViewPort(int color){
    long int location;

    for(int x = 0; x < viewport_width + viewport_x; x++){
        for(int y = 0; y < viewport_height + viewport_y; y++){
            world[x][y] = color;
        }
    }
}
unsigned int rgbaToInt(int r, int g, int b, int a) {
    return a << 24 | r << 16 | g << 8 | b;
}

unsigned int getPixelColor(int x, int y) {
    if(!isValidPoint(x,y)){
        return 0;
    }
    x = x * SCALE;
    y = y * SCALE;

   return world[x][y];
}

void determineCriticalPoint(VectorPath* vecPath) {

    if (checkIfPathIsClosed(vecPath) == 1) {
        if (vecPath->firstPoint[0] != NULL)
        {
            VectorPoint **currentToCheck = vecPath->firstPoint;
            VectorPoint **nextToCheck = vecPath->firstPoint[0]->nextPoint;
            VectorPoint **prevToCheck = vecPath->firstPoint[0]->prevPoint;

            int nextX, prevX, currentX, nextY, currentY, prevY;
            do
            {    currentToCheck[0]->isCrit = 0;
                nextX = round(nextToCheck[0]->x);
                currentX = round(currentToCheck[0]->x);
                nextY = round(nextToCheck[0]->y);
                currentY = round(currentToCheck[0]->y);
                prevY = round(prevToCheck[0]->y);

                if ((nextY > currentY && prevY > currentY)
                    || (nextY < currentY && prevY < currentY))
                {
                    currentToCheck[0]->isCrit = 1;
                }
                
                if (nextY == currentY) {
                    int nextnextY = round(nextToCheck[0]->nextPoint[0]->y);
                    if ((nextnextY > currentY && prevY > currentY)
                        || (nextnextY < currentY && prevY < currentY))
                        {
                            if (nextX > currentX) {
                                nextToCheck[0]->isCrit = 1;

                            } else {
                                currentToCheck[0]->isCrit = 1;

                            }
                        }
                }

                prevToCheck = currentToCheck;
                currentToCheck = nextToCheck;
                if (currentToCheck[0] != NULL)
                {
                    nextToCheck = currentToCheck[0]->nextPoint;
                }
            } while (currentToCheck[0] != NULL && currentToCheck[0] != vecPath->firstPoint[0]);

        }
        else
        {
            printf("Path is empty\n");
        }

        return;

    } else {
        return;
    }
}

void drawPixel(int x, int y, unsigned int color) {
    long int location;
    int i = 0, j = 0;
    x = x*SCALE; y = y*SCALE;
    for (i = 0; i < SCALE; i++)
        for (j = 0; j < SCALE; j++) {
            location = (x+i+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y+j+vinfo.yoffset) * finfo.line_length;
            *(fbp + location) = color;
            *(fbp + location + 1) = color >> 8;
            *(fbp + location + 2) = color >> 16;
            *(fbp + location + 3) = color >> 24;
        }
}

int isValidPoint(int x, int y) {
    if (x >= 0 && x < WORLD_WIDTH/SCALE && y >=0 && y < WORLD_HEIGHT/SCALE)
        return 1;

    return 0;
}

void drawLineLow(double x0, double y0, double x1, double y1, unsigned int color) {
    double dx, dy, D;
    dx = x1 - x0;
    dy = y1 - y0;
    int yi = 1;
    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }

    D = 2 * dy - dx;
    int y = y0;


    for (double x = x0; x <= x1; x++) {
        if (isValidPoint(x, y) == 0)
            return;
        drawPixel(x, y, color);
        if (D > 0) {
            y += yi;
            D -= 2 * dx;
        }
        D += 2 * dy;
    }

    
    if (isValidPoint(round(x0), round(y0)))
        drawPixel(x0, y0, color);
    if (isValidPoint(round(x1), round(y1)))
        drawPixel(x1, y1, color);
}

void drawLineHigh(double x0, double y0, double x1, double y1, unsigned int color) {
    double dx, dy, D;
    dx = x1 - x0;
    dy = y1 - y0;
    int xi = 1;



    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }


    D = 2 * dx - dy;
    int x = x0;

    for (double y = y0; y <= y1; y++) {
        if (isValidPoint(x, y) == 0)
            return;
        drawPixel(x, y, color);
        if (D > 0) {
            x += xi;
            D -= 2 * dy;
        }

        D += 2 * dx;
    }
        if (isValidPoint(round(x0), round(y0)))
        drawPixel(x0, y0, color);
    if (isValidPoint(round(x1), round(y1)))
        drawPixel(x1, y1,color);
}

void drawVectorLine(VectorPoint* point1, VectorPoint* point2, unsigned int color, int offsetX, int offsetY) {
    double x1,y1,x2,y2;
    x1 = round(point1->x);
    y1 = round(point1->y);
    x2 = round(point2->x);
    y2 = round(point2->y);
    if (abs(y2 - y1) < abs(x2 - x1)) {
        if (x1 > x2) {
            drawLineLow(x2 + offsetX, y2 + offsetY, x1 + offsetX, y1 + offsetY, color);
        } else {
            drawLineLow(x1 + offsetX, y1 + offsetY, x2 + offsetX, y2 + offsetY, color);
        }
    } else {
        if (y1 > y2) {
            drawLineHigh(x2 + offsetX, y2 + offsetY, x1 + offsetX, y1 + offsetY, color);
        } else {
            drawLineHigh(x1 + offsetX, y1 + offsetY, x2 + offsetX, y2 + offsetY, color);
        }
    }
}

int drawVectorPath(VectorPath* path, unsigned int boundaryColor, unsigned int fillColor, int offsetX, int offsetY) {
    if (path != NULL) {

        if (path->firstPoint[0] != NULL && path->firstPoint[0]->nextPoint[0] != NULL) {
            VectorPoint** currentPoint = path->firstPoint;
            VectorPoint** nextPoint = path->firstPoint[0]->nextPoint;

            do {
                if (nextPoint[0] != NULL) {
                    drawVectorLine(currentPoint[0], currentPoint[0]->nextPoint[0], boundaryColor, offsetX, offsetY);
                }


                currentPoint = nextPoint;

                if (currentPoint[0] != NULL) {
                    nextPoint = currentPoint[0]->nextPoint;
                }
            } while (currentPoint[0] != NULL && currentPoint[0] != path->firstPoint[0]);
        } else {
            return 0;
        }
    } else {
        return 0;
    }

//    fillVector(path, fillColor, boundaryColor, offsetX, offsetY);

   
    return 1;
}

int rotatePath(VectorPath* path, float degree, int originX, int originY) {
    if (path != NULL) {
        if (path->firstPoint[0] != NULL && path->firstPoint[0]->nextPoint[0] != NULL) {
            VectorPoint** currentPoint = path->firstPoint;
            VectorPoint** nextPoint = path->firstPoint[0]->nextPoint;

            double t=(22*degree)/(180*7);

            do {
                double x1 = currentPoint[0]->x;
                double y1 = currentPoint[0]->y;

                currentPoint[0]->x = ((x1-originX)*cos(t))-((y1-originY)*sin(t)) + originX;
                currentPoint[0]->y = ((x1-originX)*sin(t))+((y1-originY)*cos(t)) +originY;

                currentPoint = nextPoint;
                if (currentPoint[0] != NULL) {
                    nextPoint = currentPoint[0]->nextPoint;
                }
            } while (currentPoint[0] != NULL && currentPoint[0] != path->firstPoint[0]);
        } else {
            return 0;
        }
    } else {
        return 0;
    }

   checkForMinMaxUpdate(path);
    return 1;
}

int dilatatePath(VectorPath* path, int originX, int originY, float zoom) {
    if (path != NULL) {
        if (path->firstPoint[0] != NULL && path->firstPoint[0]->nextPoint[0] != NULL) {
            VectorPoint** currentPoint = path->firstPoint;
            VectorPoint** nextPoint = path->firstPoint[0]->nextPoint;

            do {
                double x1 = currentPoint[0]->x;
                double y1 = currentPoint[0]->y;
                currentPoint[0]->x = (x1 - originX) * zoom + originX;
                currentPoint[0]->y = (y1 - originY) * zoom + originY;


                currentPoint = nextPoint;
                if (currentPoint[0] != NULL) {
                    nextPoint = currentPoint[0]->nextPoint;
                }
            } while (currentPoint[0] != NULL && currentPoint[0] != path->firstPoint[0]);
        } else {
            return 0;
        }
    } else {
        return 0;
    }

    checkForMinMaxUpdate(path);
    return 1;
}

int translatePath(VectorPath* path, int dx, int dy) {
    if (path != NULL) {
        if (path->firstPoint[0] != NULL && path->firstPoint[0]->nextPoint[0] != NULL) {
            VectorPoint** currentPoint = path->firstPoint;
            VectorPoint** nextPoint = path->firstPoint[0]->nextPoint;

            do {
                double x1 = currentPoint[0]->x;
                double y1 = currentPoint[0]->y;
                currentPoint[0]->x = (x1 + dx);
                currentPoint[0]->y = (y1 + dy);

                currentPoint = nextPoint;
                if (currentPoint[0] != NULL) {
                    nextPoint = currentPoint[0]->nextPoint;
                }
            } while (currentPoint[0] != NULL && currentPoint[0] != path->firstPoint[0]);
        } else {
            return 0;
        }
    } else {
        return 0;
    }

    checkForMinMaxUpdate(path);
    return 1;
}

void drawCritPoint(VectorPath* path, int offsetX, int offsetY, unsigned int boundaryColor){
    if (path != NULL) {
        if (path->firstPoint[0] != NULL && path->firstPoint[0]->nextPoint[0] != NULL) {
            VectorPoint** currentPoint = path->firstPoint;
            VectorPoint** nextPoint = path->firstPoint[0]->nextPoint;

            do {

                if(currentPoint[0]->isCrit){
                    int i = round(currentPoint[0]->x) + offsetX;
                    int j = round(currentPoint[0]->y)+ offsetY;
                    while(isValidPoint(i,j) && getPixelColor(i,j) == boundaryColor){
                        i++;
                    }    
                    drawPixel(i-1, j, critColor);

                }

                currentPoint = nextPoint;
                if (currentPoint[0] != NULL) {
                    nextPoint = currentPoint[0]->nextPoint;
                }
            } while (currentPoint[0] != NULL && currentPoint[0] != path->firstPoint[0]);
        } else {
            return;
        }
    } else {
        return;
    } 
}

void fillVector(VectorPath* path, unsigned int fillColor, unsigned int boundaryColor, int offsetX, int offsetY) {
    int isFilling = -1;

    int count = 0;
    determineCriticalPoint(path);
    drawCritPoint(path, offsetX, offsetY, boundaryColor);

    if (checkIfPathIsClosed(path)) {
        for (int j = path->minY-1 + offsetY; j <= path->maxY + offsetY; j++) {
            isFilling = -1;
            for (int i = path->minX + offsetX -1; i <= path->maxX + offsetX; i++) {
                if (getPixelColor(i, j) == critColor) {
                    drawPixel(i,j, boundaryColor);
                    continue;
                } else {
                    if (getPixelColor(i, j) == boundaryColor || getPixelColor(i, j) == critColor) {
                        while(getPixelColor(i, j) == boundaryColor && i <= path->maxX + offsetX && getPixelColor(i, j) != critColor) {
                            i++;
                        }

                        if (getPixelColor(i, j) == critColor) {
                            drawPixel(i,j, boundaryColor);

                        } else {
                            isFilling *= -1;
                        }
                    }
                    if (i <= path->maxX+ offsetX) {
                        if (isFilling > 0) {
                            drawPixel(i, j, fillColor);
                        }
                    }
                }
            }
        }
    }

}

void drawCircle(int x0, int y0, int radius, unsigned int boundaryColor, unsigned int fillColor) {
    unsigned int circleFrame[radius * 2][radius * 2];

    int isValidPointOnFrame(int x, int y) {
        if (x >= 0 && x < radius * 2/SCALE && y >=0 && y < radius * 2/SCALE)
            return 1;

        return 0;
    }

    void drawPixelOnFrame(int x, int y, unsigned int color) {
        int i = 0, j = 0;
        x = x*SCALE; y = y*SCALE;
        for (i = 0; i < SCALE; i++)
            for (j = 0; j < SCALE; j++) {
                circleFrame[x+i][y+j] = color;
            }
    }

    unsigned int getPixelColorOnFrame(int x, int y) {
        if(!isValidPointOnFrame(x,y)){
            return 0;
        }
        x = x * SCALE;
        y = y * SCALE;

       return circleFrame[x][y];
    }

    int isCircleCritPoint(int x, int y) {
        return ((y == 1 || y == (2 * radius) - 1));
    }

    int isOnBoundary(int x, int y) {
        return getPixelColorOnFrame(x, y) == boundaryColor;
    }
    void fillCircle() {
        int isFilling = -1;

        for (int j = 0; j <= radius * 2; j++) {
            isFilling = -1;
            for (int i = 0; i <= radius * 2; i++) {
                if (getPixelColorOnFrame(i, j)) {
                    while (isOnBoundary(i, j)) {
                        i++;
                    }
                    isFilling *= -1;
                }

                if (isCircleCritPoint(i, j)) { 
                    continue;
                }

                if (i < radius * 2) {
                    if (isFilling > 0) {
                        drawPixelOnFrame(i, j, fillColor);
                    }
                }

                // if (i == 0 || j == 0 || i == (radius * 2) - 1 || j == (radius * 2) - 1) {
                //     drawPixelOnFrame(i, j, rgbaToInt(255,0,0,0));
                // }
            }
        }
    }

    void drawCircle() {
        int minX = x0 - radius;
        int maxX = x0 + radius;
        int minY = y0 - radius;
        int maxY = y0 + radius;

        for (int x = 0; x < radius * 2; x++) {
            for (int y = 0; y < radius * 2; y++) {
                if (circleFrame[x][y] == boundaryColor || circleFrame[x][y] == fillColor) {
                    drawPixel(x + minX, y + minY, circleFrame[x][y]);
                }
            }
        }
    }

    void clearFrame() {
        for (int i = 0; i < radius * 2; i++) {
            for (int j = 0; j < radius * 2; j++) {
                unsigned int clearColor = rgbaToInt(0,0,0,0);
                if (boundaryColor == clearColor || fillColor == clearColor) {
                    clearColor = rgbaToInt(255,255,255,255);
                } else if (boundaryColor == rgbaToInt(255,255,255,255) || fillColor == rgbaToInt(255,255,255,255)) {
                    clearColor = rgbaToInt(0,255,255,255);
                }
                circleFrame[i][j] = clearColor;
            }
        }
    }

    int centerX = radius;
    int centerY = radius;
    int x = radius-1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);

    clearFrame();
    while (x >= y)
    {
        drawPixelOnFrame(centerX + x, centerY + y, boundaryColor);
        drawPixelOnFrame(centerX + y, centerY + x, boundaryColor);
        drawPixelOnFrame(centerX - y, centerY + x, boundaryColor);
        drawPixelOnFrame(centerX - x, centerY + y, boundaryColor);
        drawPixelOnFrame(centerX - x, centerY - y, boundaryColor);
        drawPixelOnFrame(centerX - y, centerY - x, boundaryColor);
        drawPixelOnFrame(centerX + y, centerY - x, boundaryColor);
        drawPixelOnFrame(centerX + x, centerY - y, boundaryColor);

        if (err <= 0)
        {
            y++;
            err += dy;
            dy += 2;
        }
        
        if (err > 0)
        {
            x--;
            dx += 2;
            err += dx - (radius << 1);
        }
    }

    fillCircle();
    drawCircle();
}
