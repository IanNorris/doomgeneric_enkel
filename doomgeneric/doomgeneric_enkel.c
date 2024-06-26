//doomgeneric for soso os

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include <termios.h>
#include <time.h>

static int FrameBufferFd = -1;
static int* FrameBuffer = 0;

static int KeyboardFd = -1;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned int s_PositionX = 600;
static unsigned int s_PositionY = 200;

static unsigned int s_ScreenWidth = 0;
static unsigned int s_ScreenHeight = 0;

static unsigned char convertToDoomKey(unsigned char scancode)
{
    unsigned char key = 0;

    switch (scancode)
    {
    case 0x9C:
    case 0x1C:
        key = KEY_ENTER;
        break;
    case 0x01:
        key = KEY_ESCAPE;
        break;
    case 0xCB:
    case 0x4B:
        key = KEY_LEFTARROW;
        break;
    case 0xCD:
    case 0x4D:
        key = KEY_RIGHTARROW;
        break;
    case 0xC8:
    case 0x48:
        key = KEY_UPARROW;
        break;
    case 0xD0:
    case 0x50:
        key = KEY_DOWNARROW;
        break;
    case 0x1D:
        key = KEY_FIRE;
        break;
    case 0x39:
        key = KEY_USE;
        break;
    case 0x2A:
    case 0x36:
        key = KEY_RSHIFT;
        break;
    case 0x15:
        key = 'y';
        break;
    default:
        break;
    }

    return key;
}

static void addKeyToQueue(int pressed, unsigned char keyCode)
{
    //printf("key hex %x decimal %d\n", keyCode, keyCode);

    unsigned char key = convertToDoomKey(keyCode);

    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}


struct termios orig_termios;

void disableRawMode()
{
    //printf("returning original termios\n");
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO);
    raw.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void DG_Init()
{
    FrameBufferFd = open("/dev/fb0", 0);

    static struct fb_var_screeninfo vinfo;
    static struct fb_fix_screeninfo finfo;

    if (FrameBufferFd >= 0)
    {
        /* Get fixed screen information */
        if (ioctl(FrameBufferFd, FBIOGET_FSCREENINFO, &finfo)) {
            printf("Error reading fixed information.\n");
            exit(2);
        }

        /* Get variable screen information */
        if (ioctl(FrameBufferFd, FBIOGET_VSCREENINFO, &vinfo)) {
            printf("Error reading variable information.\n");
            exit(3);
        }

        s_ScreenWidth = vinfo.xres;
        s_ScreenHeight = vinfo.yres;

        /* Figure out the size of the screen in bytes */
        uint64_t screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

        /* Map the device to memory */
        FrameBuffer = (int*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, FrameBufferFd, 0);
        if ((int64_t)FrameBuffer == -1) {
            printf("Error: failed to map framebuffer device to memory.\n");
            exit(4);
        }
    }
    else
    {
        printf("Opening FrameBuffer device failed!\n");
    }

    enableRawMode();

    KeyboardFd = open("keyboard", 0);

    if (KeyboardFd >= 0)
    {
        //enter non-blocking mode
        ioctl(KeyboardFd, 1, (void*)1);
    }
}

static void handleKeyInput()
{
    if (KeyboardFd < 0)
    {
        return;
    }

    unsigned char scancode = 0;

    if (read(KeyboardFd, &scancode, 1) > 0)
    {
        unsigned char keyRelease = (0x80 & scancode);

        scancode = (0x7F & scancode);

        //printf("scancode:%x pressed:%d\n", scancode, 0 == keyRelease);

        if (0 == keyRelease)
        {
            addKeyToQueue(1, scancode);
        }
        else
        {
            addKeyToQueue(0, scancode);
        }
    }
}

void DG_DrawFrame()
{
    if (FrameBuffer)
    {
        for (int i = 0; i < DOOMGENERIC_RESY; ++i)
        {
            memcpy(FrameBuffer + s_PositionX + (i + s_PositionY) * s_ScreenWidth, DG_ScreenBuffer + i * DOOMGENERIC_RESX, DOOMGENERIC_RESX * 4);
        }
    }

    handleKeyInput();
}

static int uptime = 0;

void DG_SleepMs(uint32_t ms)
{
    uint64_t sec = ms / 1000;
    uint64_t ns = (ms - (sec * 1000)) * 1000;

    struct timespec ts;
    ts.tv_sec = sec;
    ts.tv_nsec = ns;
    nanosleep(&ts, NULL);

    uptime += ms;
}

uint32_t DG_GetTicksMs()
{
    return uptime;
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
    {
        //key queue is empty

        return 0;
    }
    else
    {
        unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
        s_KeyQueueReadIndex++;
        s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

void DG_SetWindowTitle(const char* title)
{
}

int main(int argc, char** argv)
{
    doomgeneric_Create(argc, argv);

    for (int i = 0; ; i++)
    {
        doomgeneric_Tick();
    }


    return 0;
}
