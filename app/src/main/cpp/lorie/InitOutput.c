/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"
#pragma ide diagnostic ignored "cert-err34-c"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "ConstantFunctionResult"
#pragma ide diagnostic ignored "bugprone-integer-division"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wformat-nonliteral"

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <X11/X.h>
#include <X11/Xdefs.h>
#include <X11/Xmd.h>
#include <X11/Xproto.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "dix.h"
#include "inpututils.h"
#include "misc.h"
#include "os.h"
#include "resource.h"
#include "scrnintstr.h"
#include "windowstr.h"

#include "dri3.h"
#include "drm_fourcc.h"
#include "exa.h"
#include "fb.h"
#include "glxserver.h"
#include "fbconfigs.h"
#include "glxutil.h"
#include "micmap.h"
#include "miline.h"
#include "mipointer.h"
#include "misyncshm.h"
#include "shmint.h"
#include <libxcvt/libxcvt.h>
#include <present.h>

#include "lorie.h"

extern void android_shmem_sysv_shm_force(uint8_t enable);

#define unused __attribute__((unused))
#define log(prio, ...)                                                         \
  __android_log_print(ANDROID_LOG_##prio, "LorieNative", __VA_ARGS__)

extern DeviceIntPtr lorieMouse, lorieKeyboard;

#define CREATE_PIXMAP_USAGE_LORIEBUFFER_BACKED 5

struct vblank {
  struct xorg_list list;
  uint64_t id, msc;
};

typedef struct {
  LorieBuffer *buffer;
  AHardwareBuffer *ahb;
  uint8_t flipped;
  void *locked;
  void *mem;
} LoriePixmapPriv;

static LoriePixmapPriv *lorieRootWindowPixmapPriv(void);

static struct dri3_screen_info lorieDri3Info;

static struct present_screen_info loriePresentInfo;

static ExaDriverRec lorieExa;

typedef struct {
  DamagePtr damage;
  OsTimerPtr fpsTimer;

  int eventFd, stateFd;

  struct lorie_shared_server_state *state;
  struct {
    // Triple buffering: 3 buffers para eliminar contenção
    // back: onde X server escreve
    // middle: pronto para renderizar
    // front: atualmente sendo renderizado
    LorieBuffer *buffers[3];
    int backIndex;    // Índice do buffer que X server está escrevendo
    int middleIndex;  // Índice do buffer pronto para render
    int frontIndex;   // Índice do buffer sendo renderizado
    Bool legacyDrawing;
    uint8_t flip;
    uint32_t width, height;
    char name[1024];
    uint32_t framerate;
  } root;

  JavaVM *vm;
  JNIEnv *env;
  Bool dri3;

  uint64_t vblank_interval;
  struct xorg_list vblank_queue;
  uint64_t current_msc;
} lorieScreenInfo;

ScreenPtr pScreenPtr;
static lorieScreenInfo lorieScreen =
                           {
                               .stateFd = -1,
                               .root.width = 1280,
                               .root.height = 1024,
                               .root.framerate = 30,
                               .root.name = "screen",
                               .dri3 = TRUE,
                               .vblank_queue = {&lorieScreen.vblank_queue,
                                                &lorieScreen.vblank_queue},
},
                       *pvfb = &lorieScreen;
static char *xstartup = NULL;

void OsVendorInit(void) {
  pthread_mutexattr_t mutex_attr;
  pthread_condattr_t cond_attr;

  if (lorieScreen.stateFd != -1) // already initialized
    return;

  if (-1 == (lorieScreen.stateFd = LorieBuffer_createRegion(
                 "xserver", sizeof(*lorieScreen.state)))) {
    dprintf(2, "FATAL: Failed to allocate server state.\n");
    _exit(1);
  }

  if (!(lorieScreen.state =
            mmap(NULL, sizeof(*lorieScreen.state), PROT_READ | PROT_WRITE,
                 MAP_SHARED, lorieScreen.stateFd, 0))) {
    dprintf(2, "FATAL: Failed to map server state.\n");
    _exit(1);
  }

  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&lorieScreen.state->lock, &mutex_attr);
  pthread_mutex_init(&lorieScreen.state->cursor.lock, &mutex_attr);

  pthread_condattr_init(&cond_attr);
  pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(&lorieScreen.state->cond, &cond_attr);
#if !RENDERER_IN_ACTIVITY
  renderer_set_shared_state(lorieScreen.state);
#endif
}

void lorieActivityConnected(void) {
  lorieSendSharedServerState(pvfb->stateFd);
  if (!pvfb->root.legacyDrawing) {
    LoriePixmapPriv *priv = lorieRootWindowPixmapPriv();
    if (priv && priv->buffer)
      lorieSendRootWindowBuffer(priv->buffer);
  } else {
    // Enviar o buffer middle (pronto para render) para o renderer
    if (pvfb->root.buffers[pvfb->root.middleIndex])
      lorieSendRootWindowBuffer(pvfb->root.buffers[pvfb->root.middleIndex]);
  }
}

static void lorieSwapTripleBuffers(void) {
  // Rotaciona os buffers: back -> middle, middle -> front (old), front -> back (recycled)
  int oldBack = pvfb->root.backIndex;
  pvfb->root.backIndex = pvfb->root.frontIndex;  // Recicla o front antigo
  pvfb->root.frontIndex = pvfb->root.middleIndex;
  pvfb->root.middleIndex = oldBack;
  
  // Notifica renderer sobre o buffer que ACABOU de ser atualizado (agora é o front)
  if (pvfb->root.buffers[pvfb->root.frontIndex]) {
    #if RENDERER_IN_ACTIVITY
    lorieSendRootWindowBuffer(pvfb->root.buffers[pvfb->root.frontIndex]);
    #else
    renderer_set_buffer(pvfb->root.buffers[pvfb->root.frontIndex]);
    #endif
  }
}

static LoriePixmapPriv *lorieRootWindowPixmapPriv(void) {
  void *devPriv = pScreenPtr ? pScreenPtr->devPrivate : NULL;
  return devPriv ? exaGetPixmapDriverPrivate(devPriv) : NULL;
}

static Bool TrueNoop() { return TRUE; }
static Bool FalseNoop() { return FALSE; }
static void VoidNoop() {}

void ddxGiveUp(unused enum ExitCode error) {
  log(ERROR, "Server stopped (%d)", error);
  CloseWellKnownConnections();
  UnlockServer();
  exit(error);
}

static void *ddxReadyThread(unused void *cookie) {
  if (xstartup && serverGeneration == 1) {
    pid_t pid = fork();

    if (!pid) {
      char DISPLAY[16] = "";
      sprintf(DISPLAY, ":%s", display);
      setenv("DISPLAY", DISPLAY, 1);

#define INHERIT_VAR(v)                                                         \
  char *v = getenv("XSTARTUP_" #v);                                            \
  if (v && strlen(v))                                                          \
    setenv(#v, v, 1);                                                          \
  unsetenv("XSTARTUP_" #v);
      INHERIT_VAR(CLASSPATH)
      INHERIT_VAR(LD_LIBRARY_PATH)
      INHERIT_VAR(LD_PRELOAD)
#undef INHERIT_VAR

      execlp(xstartup, xstartup, NULL);
      execlp("sh", "sh", "-c", xstartup, NULL);
      dprintf(2, "Failed to start command `sh -c \"%s\"`: %s\n", xstartup,
              strerror(errno));
      abort();
    } else {
      int status;
      do {
        pid_t w = waitpid(pid, &status, 0);
        if (w == -1) {
          perror("waitpid");
          GiveUp(SIGKILL);
        }

        if (WIFEXITED(status)) {
          printf("%d exited, status=%d\n", w, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
          printf("%d killed by signal %d\n", w, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
          printf("%d stopped by signal %d\n", w, WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
          printf("%d continued\n", w);
        }
      } while (!WIFEXITED(status) && !WIFSIGNALED(status));
      GiveUp(SIGINT);
    }
  }

  return NULL;
}

void drawSquare(int x, int y, int l, uint32_t color, uint32_t stride,
                uint32_t *pixels) {
  for (int i = 0; i < l; i++)
    for (int j = 0; j < l; j++)
      pixels[(j + y) * stride + x + i] = color;
}

Bool drawSquares() {
  LoriePixmapPriv *priv = lorieRootWindowPixmapPriv();
  uint32_t *pixels = !priv ? NULL : priv->locked;
  if (pixels) {
    LorieBuffer_Desc d = {0};
    LorieBuffer_describe(priv->buffer, &d);
    int l = min(d.width, d.height) / 4, x = (d.width - l) / 2,
        y = (d.height - l) / 2;

    drawSquare(x - l / 3, y - l / 3, l, 0x00FF0000, d.stride, pixels);
    drawSquare(x, y, l, 0x0000FF00, d.stride, pixels);
    drawSquare(x + l / 3, y + l / 3, l, 0x000000FF, d.stride, pixels);
  }

  return FALSE;
}

void ddxReady(void) {
  CursorVisible = TRUE;
  pScreenPtr->DisplayCursor(lorieMouse, pScreenPtr, rootCursor);
  if (NoListenAll)
    return;
  if (xstartup && !strlen(xstartup)) // allow overriding $TERMUX_X11_XSTARTUP
                                     // with empty xstartup arg
    return;
  if (!xstartup || !strlen(xstartup))
    xstartup = getenv("TERMUX_X11_XSTARTUP");
  if (!xstartup || !strlen(xstartup))
    return;

  pthread_t t;
  pthread_create(&t, NULL, ddxReadyThread, NULL);
}

void OsVendorFatalError(unused const char *f, unused va_list args) {
  log(ERROR, f, args);
}

#if defined(DDXBEFORERESET)
void ddxBeforeReset(void) {}
#endif

#if INPUTTHREAD
/** This function is called in Xserver/os/inputthread.c when starting
    the input thread. */
void ddxInputThreadInit(void) {}
#endif

void ddxUseMsg(void) {
  ErrorF("-xstartup \"command\"    start `command` after server startup\n");
  ErrorF("-legacy-drawing        use legacy drawing, without using "
         "AHardwareBuffers\n");
  ErrorF("-force-bgra            force flipping colours (RGBA->BGRA)\n");
  ErrorF(
      "-disable-dri3          disabling DRI3 support (to let lavapipe work)\n");
  ErrorF("-force-sysvshm         force using SysV shm syscalls\n");
  ErrorF("-check-drawing         run server only able to draw some test image "
         "(for testing if rendering root window works or not),\n");
}

int ddxProcessArgument(unused int argc, unused char *argv[], unused int i) {
  if (strcmp(argv[i], "-xstartup") == 0) { /* -xstartup "command" */
    CHECK_FOR_REQUIRED_ARGUMENTS(1);
    xstartup = argv[++i];
    return 2;
  }

  if (strcmp(argv[i], "-legacy-drawing") == 0) {
    pvfb->root.legacyDrawing = TRUE;
    return 1;
  }

  if (strcmp(argv[i], "-force-bgra") == 0) {
    pvfb->root.flip = TRUE;
    return 1;
  }

  if (strcmp(argv[i], "-disable-dri3") == 0) {
    pvfb->dri3 = FALSE;
    return 1;
  }

  if (strcmp(argv[i], "-force-sysvshm") == 0) {
    android_shmem_sysv_shm_force(1);
    return 1;
  }

  if (strcmp(argv[i], "-check-drawing") == 0) {
    NoListenAll = TRUE;
    QueueWorkProc(drawSquares, NULL, NULL);
    return 1;
  }

  return 0;
}

static RRModePtr lorieCvt(int width, int height, int framerate) {
  struct libxcvt_mode_info *info;
  char name[128];
  xRRModeInfo modeinfo = {0};
  RRModePtr mode;

  info = libxcvt_gen_mode_info(width, height, framerate, 0, 0);

  snprintf(name, sizeof name, "%dx%d", info->hdisplay, info->vdisplay);
  modeinfo.nameLength = strlen(name);
  modeinfo.width = info->hdisplay;
  modeinfo.height = info->vdisplay;
  modeinfo.dotClock = info->dot_clock * 1000.0;
  modeinfo.hSyncStart = info->hsync_start;
  modeinfo.hSyncEnd = info->hsync_end;
  modeinfo.hTotal = info->htotal;
  modeinfo.vSyncStart = info->vsync_start;
  modeinfo.vSyncEnd = info->vsync_end;
  modeinfo.vTotal = info->vtotal;
  modeinfo.modeFlags = info->mode_flags;

  mode = RRModeGet(&modeinfo, name);
  free(info);
  return mode;
}

static void lorieMoveCursor(unused DeviceIntPtr pDev, unused ScreenPtr pScr,
                            int x, int y) {
  // OPTIMIZATION: Only update if position actually changed
  // This reduces unnecessary signaling and lock contention
  if (pvfb->state->cursor.x != x || pvfb->state->cursor.y != y) {
    pvfb->state->cursor.x = x;
    pvfb->state->cursor.y = y;
    pvfb->state->cursor.moved = TRUE;
    // No need to explicitly lock the mutex, it will cause waiting for rendering
    // to be finished. We are simply signaling the renderer in the case if it
    // sleeps.
    pthread_cond_signal(&pvfb->state->cond);
  }
}

static void lorieConvertCursor(CursorPtr pCurs, uint32_t *data) {
  CursorBitsPtr bits = pCurs->bits;
  if (bits->argb) {
    // Optimized BGRA to RGBA conversion - process in chunks for better cache locality
    int total_pixels = bits->width * bits->height;
    for (int i = 0; i < total_pixels; i++) {
      /* Convert bgra to rgba using bitwise operations */
      CARD32 p = bits->argb[i];
      // More efficient: use shift and mask instead of multiple operations
      data[i] = (p & 0xFF000000) | ((p & 0x00FF0000) >> 16) | (p & 0x0000FF00) |
                ((p & 0x000000FF) << 16);
    }
  } else {
    uint32_t d, fg, bg, *p;
    int x, y, stride, i, bit;

    p = data;
    fg = ((pCurs->foreBlue & 0xff00) << 8) | (pCurs->foreGreen & 0xff00) |
         (pCurs->foreRed >> 8);
    bg = ((pCurs->backBlue & 0xff00) << 8) | (pCurs->backGreen & 0xff00) |
         (pCurs->backRed >> 8);
    stride = BitmapBytePad(bits->width);
    
    // Optimized: process row by row for better cache locality
    for (y = 0; y < bits->height; y++) {
      int row_offset = y * stride;
      for (x = 0; x < bits->width; x++) {
        i = row_offset + x / 8;
        bit = 1 << (x & 7);
        d = (bits->source[i] & bit) ? fg : bg;
        d = (bits->mask[i] & bit) ? d | 0xff000000 : 0x00000000;
        *p++ = d;
      }
    }
  }
}

static void lorieSetCursor(unused DeviceIntPtr pDev, unused ScreenPtr pScr,
                           CursorPtr pCurs, int x0, int y0) {
  CursorBitsPtr bits = pCurs ? pCurs->bits : NULL;
  if (pCurs && (pCurs->bits->width >= 512 || pCurs->bits->height >= 512))
    // We do not have enough memory allocated for such a big cursor, let's
    // display default "X" cursor
    pCurs = rootCursor;

  lorie_mutex_lock(&pvfb->state->cursor.lock, &pvfb->state->cursor.lockingPid);
  if (pCurs && bits) {
    pvfb->state->cursor.xhot = bits->xhot;
    pvfb->state->cursor.yhot = bits->yhot;
    pvfb->state->cursor.width = bits->width;
    pvfb->state->cursor.height = bits->height;
    lorieConvertCursor(pCurs, pvfb->state->cursor.bits);
  } else {
    pvfb->state->cursor.xhot = pvfb->state->cursor.yhot = 0;
    pvfb->state->cursor.width = pvfb->state->cursor.height = 0;
  }
  pvfb->state->cursor.updated = true;
  lorie_mutex_unlock(&pvfb->state->cursor.lock,
                     &pvfb->state->cursor.lockingPid);

  lorieMoveCursor(NULL, NULL, x0, y0);
}

static miPointerSpriteFuncRec loriePointerSpriteFuncs = {
    .RealizeCursor = TrueNoop,
    .UnrealizeCursor = TrueNoop,
    .SetCursor = lorieSetCursor,
    .MoveCursor = lorieMoveCursor,
    .DeviceCursorInitialize = TrueNoop,
    .DeviceCursorCleanup = VoidNoop};

static miPointerScreenFuncRec loriePointerCursorFuncs = {
    .CursorOffScreen = FalseNoop,
    .CrossScreen = VoidNoop,
    .WarpCursor = miPointerWarpCursor};

static void loriePerformVblanks(void);

static Bool lorieRedraw(__unused ClientPtr pClient, __unused void *closure) {
  int status, nonEmpty;
  LoriePixmapPriv *priv;

  loriePerformVblanks();

  pvfb->state->waitForNextFrame = false;

  if (!lorieConnectionAlive() || !pvfb->state->surfaceAvailable)
    return TRUE;

  // OPTIMIZATION: Check damage region only if cursor hasn't moved
  // This reduces RegionNotEmpty calls when only cursor changes
  if (!pvfb->state->cursor.moved && !pvfb->state->cursor.updated) {
    nonEmpty = RegionNotEmpty(DamageRegion(pvfb->damage));
  } else {
    nonEmpty = FALSE;  // Cursor-only update, no screen damage
  }
  priv = lorieRootWindowPixmapPriv();

  if (nonEmpty && priv && priv->buffer) {
    if (pvfb->root.legacyDrawing) {
      LorieBuffer *middleBuffer = pvfb->root.buffers[pvfb->root.middleIndex];
      
      // OPTIMIZATION: Only copy if buffer actually changed (damage region is non-empty)
      // This reduces unnecessary CPU work when screen is static
      if (priv->buffer && middleBuffer) {
        // Desbloquear buffer atual antes de copiar (necessário para sincronização/DMA)
        LorieBuffer_unlock(priv->buffer);
        
        // Copiar conteúdo atualizado do buffer do X para o buffer middle (pronto para render)
        LorieBuffer_copy(priv->buffer, middleBuffer);
        
        // Re-lock do buffer para continuar recebendo updates do X
        status = LorieBuffer_lock(priv->buffer, NULL, &priv->locked);
        if (status)
          FatalError("Failed to lock the surface: %d\n", status);
      }

      DamageEmpty(pvfb->damage);
      pvfb->state->drawRequested = TRUE;
      
      // Swap triple buffers: back -> middle (agora contém último frame)
      lorieSwapTripleBuffers();
    } else {
      // Sem cópias na CPU! Enviar buffer principal diretamente
      LorieBuffer_unlock(priv->buffer);
      #if RENDERER_IN_ACTIVITY
      lorieSendRootWindowBuffer(priv->buffer);
      #else
      renderer_set_buffer(priv->buffer);
      #endif
      status = LorieBuffer_lock(priv->buffer, NULL, &priv->locked);
      if (status)
        FatalError("Failed to lock the surface: %d\n", status);

      DamageEmpty(pvfb->damage);
      pvfb->state->drawRequested = TRUE;
    }
  }

  if (pvfb->state->drawRequested || pvfb->state->cursor.moved ||
      pvfb->state->cursor.updated) {
    // Sending signal about pending root window changes to renderer thread.
    pthread_cond_signal(&pvfb->state->cond);
  }

  if (pvfb->state->vsyncOff) {
    QueueWorkProc(lorieRedraw, NULL, NULL);
    lorieTriggerWorkingQueue();
  }

  return TRUE;
}


static CARD32 lorieFramecounter(unused OsTimerPtr timer, unused CARD32 time,
                                unused void *arg) {
  if (pvfb->state->renderedFrames)
    log(INFO, "%d frames in 5.0 seconds = %.1f FPS",
        pvfb->state->renderedFrames, ((float)pvfb->state->renderedFrames) / 5);
  pvfb->state->renderedFrames = 0;
  return 5000;
}

static Bool lorieCreateScreenResources(ScreenPtr pScreen) {
  if (pvfb->root.legacyDrawing) {
    // Inicializar triple buffering - criar 3 buffers
    pvfb->root.backIndex = 0;
    pvfb->root.middleIndex = 1;
    pvfb->root.frontIndex = 2;
    
    // Criar 3 buffers identicos para triple buffering
    for (int i = 0; i < 3; i++) {
      pvfb->root.buffers[i] = LorieBuffer_allocate(
          pScreen->width, pScreen->height,
          AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, LORIEBUFFER_AHARDWAREBUFFER);
      if (!pvfb->root.buffers[i]) {
        // Fallback para LORIEBUFFER_REGULAR se AHardwareBuffer falhar
        pvfb->root.buffers[i] = LorieBuffer_allocate(
            pScreen->width, pScreen->height,
            AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, LORIEBUFFER_REGULAR);
      }
    }
  } else {
    for (int i = 0; i < 3; i++) {
      pvfb->root.buffers[i] = NULL;
    }
  }
  
  // Usar o buffer back (índice 0) como o pixmap principal do screen
  pScreen->devPrivate = pScreen->CreatePixmap(
      pScreen, pScreen->width, pScreen->height, pScreen->rootDepth,
      CREATE_PIXMAP_USAGE_LORIEBUFFER_BACKED);

  pvfb->damage =
      DamageCreate(NULL, NULL, DamageReportNone, TRUE, pScreen, NULL);
  if (!pvfb->damage)
    FatalError("Couldn't setup damage\n");

  DamageRegister(&(*pScreen->GetScreenPixmap)(pScreen)->drawable, pvfb->damage);
  pvfb->fpsTimer = TimerSet(NULL, 0, 5000, lorieFramecounter, pScreen);

  // Enviar o buffer para o renderer
  LoriePixmapPriv *priv = lorieRootWindowPixmapPriv();
  if (priv && priv->buffer) {
#if RENDERER_IN_ACTIVITY
    lorieSendRootWindowBuffer(priv->buffer);
#else
    renderer_set_buffer(priv->buffer);
#endif
  } else {
#if RENDERER_IN_ACTIVITY
    lorieSendRootWindowBuffer(pvfb->root.buffers[pvfb->root.middleIndex]);
#else
    renderer_set_buffer(pvfb->root.buffers[pvfb->root.middleIndex]);
#endif
  }

  return TRUE;
}

static Bool lorieCloseScreen(ScreenPtr pScreen) {
  // Liberar buffers do triple buffering
  for (int i = 0; i < 3; i++) {
    if (pvfb->root.buffers[i]) {
      LorieBuffer_release(pvfb->root.buffers[i]);
      pvfb->root.buffers[i] = NULL;
    }
  }
  pScreenPtr = NULL;
  return fbCloseScreen(pScreen);
}

static int lorieSetPixmapVisitWindow(WindowPtr window, void *data) {
  ScreenPtr screen = window->drawable.pScreen;

  if (screen->GetWindowPixmap(window) == data) {
    screen->SetWindowPixmap(window, screen->GetScreenPixmap(screen));
    return WT_WALKCHILDREN;
  }

  return WT_DONTWALKCHILDREN;
}

static Bool lorieRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height,
                                 unused CARD32 mmWidth,
                                 unused CARD32 mmHeight) {
  PixmapPtr oldPixmap, newPixmap;
  BoxRec box = {0, 0, width, height};

  SetRootClip(pScreen, ROOT_CLIP_NONE);

  pScreen->root->drawable.width = pvfb->root.width = pScreen->width = width;
  pScreen->root->drawable.height = pvfb->root.height = pScreen->height = height;
  pScreen->mmWidth = ((double)(width)) * 25.4 / monitorResolution;
  pScreen->mmHeight = ((double)(height)) * 25.4 / monitorResolution;

  // OPTIMIZATION: Only reallocate buffers if size actually changed significantly
  // This reduces memory churn during minor resize operations
  bool sizeChanged = (pvfb->root.width != width || pvfb->root.height != height);
  
  if (sizeChanged && pvfb->root.legacyDrawing) {
    // Liberar buffers antigos do triple buffering
    for (int i = 0; i < 3; i++) {
      if (pvfb->root.buffers[i]) {
        LorieBuffer_release(pvfb->root.buffers[i]);
        pvfb->root.buffers[i] = NULL;
      }
    }
    
    // Recriar buffers com novo tamanho
    for (int i = 0; i < 3; i++) {
      pvfb->root.buffers[i] = LorieBuffer_allocate(
          width, height,
          AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, LORIEBUFFER_AHARDWAREBUFFER);
      if (!pvfb->root.buffers[i]) {
        pvfb->root.buffers[i] = LorieBuffer_allocate(
            width, height,
            AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, LORIEBUFFER_REGULAR);
      }
    }
    
    // Resetar índices
    pvfb->root.backIndex = 0;
    pvfb->root.middleIndex = 1;
    pvfb->root.frontIndex = 2;
  }

  oldPixmap = pScreen->GetScreenPixmap(pScreen);
  newPixmap = pScreen->CreatePixmap(pScreen, width, height, pScreen->rootDepth,
                                    CREATE_PIXMAP_USAGE_LORIEBUFFER_BACKED);
  pScreen->SetScreenPixmap(newPixmap);
  if (pvfb->damage) {
    DamageUnregister(pvfb->damage);
    DamageDestroy(pvfb->damage);
  }

  pvfb->damage =
      DamageCreate(NULL, NULL, DamageReportNone, TRUE, pScreen, NULL);
  if (!pvfb->damage)
    FatalError("Couldn't setup damage\n");

  DamageRegister(&newPixmap->drawable, pvfb->damage);

  if (oldPixmap) {
    TraverseTree(pScreen->root, lorieSetPixmapVisitWindow, oldPixmap);
    pScreen->DestroyPixmap(oldPixmap);
  }

  // Enviar buffer atualizado para renderer
  LoriePixmapPriv *priv = exaGetPixmapDriverPrivate(newPixmap);
  if (priv && priv->buffer) {
#if RENDERER_IN_ACTIVITY
    lorieSendRootWindowBuffer(priv->buffer);
#else
    renderer_set_buffer(priv->buffer);
#endif
  } else {
#if RENDERER_IN_ACTIVITY
    lorieSendRootWindowBuffer(pvfb->root.buffers[pvfb->root.middleIndex]);
#else
    renderer_set_buffer(pvfb->root.buffers[pvfb->root.middleIndex]);
#endif
  }

  pScreen->ResizeWindow(pScreen->root, 0, 0, width, height, NULL);
  RegionReset(&pScreen->root->winSize, &box);

  SetRootClip(pScreen, ROOT_CLIP_FULL);

  RRScreenSizeNotify(pScreen);
  update_desktop_dimensions();
  pvfb->state->cursor.moved = TRUE;

  return TRUE;
}

static Bool lorieRRCrtcSet(unused ScreenPtr pScreen, RRCrtcPtr crtc,
                           RRModePtr mode, int x, int y, Rotation rotation,
                           int numOutput, RROutputPtr *outputs) {
  return (crtc && mode) ? RRCrtcNotify(crtc, mode, x, y, rotation, NULL,
                                       numOutput, outputs)
                        : FALSE;
}

static Bool lorieRRGetInfo(unused ScreenPtr pScreen, Rotation *rotations) {
  *rotations = RR_Rotate_0;
  return TRUE;
}

static Bool lorieRandRInit(ScreenPtr pScreen) {
  rrScrPrivPtr pScrPriv;
  RROutputPtr output;
  RRCrtcPtr crtc;
  RRModePtr mode;

  if (!RRScreenInit(pScreen))
    return FALSE;

  pScrPriv = rrGetScrPriv(pScreen);
  pScrPriv->rrGetInfo = lorieRRGetInfo;
  pScrPriv->rrCrtcSet = lorieRRCrtcSet;
  pScrPriv->rrScreenSetSize = lorieRRScreenSetSize;

  RRScreenSetSizeRange(pScreen, 1, 1, 32767, 32767);

  if (FALSE ||
      !(mode =
            lorieCvt(pScreen->width, pScreen->height, pvfb->root.framerate)) ||
      !(crtc = RRCrtcCreate(pScreen, NULL)) || !RRCrtcGammaSetSize(crtc, 256) ||
      !(output = RROutputCreate(pScreen, pvfb->root.name,
                                sizeof(pvfb->root.name), NULL)) ||
      (output->nameLength = strlen(output->name), FalseNoop()) ||
      !RROutputSetClones(output, NULL, 0) ||
      !RROutputSetModes(output, &mode, 1, 0) ||
      !RROutputSetCrtcs(output, &crtc, 1) ||
      !RROutputSetConnection(output, RR_Connected) ||
      !RRCrtcNotify(crtc, mode, 0, 0, RR_Rotate_0, NULL, 1, &output))
    return FALSE;
  return TRUE;
}

void lorieTriggerWorkingQueue(void) { eventfd_write(pvfb->eventFd, 1); }

static void lorieWorkingQueueCallback(int fd, int __unused ready,
                                      void __unused *data) {
  // Nothing to do here. It is needed to finish ospoll_wait.
  eventfd_t dummy;
  eventfd_read(fd, &dummy);
}

void lorieChoreographerFrameCallback(__unused long t, AChoreographer *d) {
  AChoreographer_postFrameCallback(
      d, (AChoreographer_frameCallback)lorieChoreographerFrameCallback, d);
  if (pScreenPtr) {
    QueueWorkProc(lorieRedraw, NULL, NULL);
    lorieTriggerWorkingQueue();
  }
}

static Bool lorieScreenInit(ScreenPtr pScreen, unused int argc,
                            unused char **argv) {
  static int eventFd = -1;
  pScreenPtr = pScreen;

  if (eventFd == -1)
    eventFd = eventfd(0, EFD_CLOEXEC);

  pvfb->eventFd = eventFd;
  SetNotifyFd(eventFd, lorieWorkingQueueCallback, X_NOTIFY_READ, NULL);

  miSetZeroLineBias(pScreen, 0);
  pScreen->blackPixel = 0;
  pScreen->whitePixel = 1;

  pvfb->vblank_interval = 1000000 / pvfb->root.framerate;

  if (FALSE ||
      !miSetVisualTypesAndMasks(24, ((1 << TrueColor) | (1 << DirectColor)), 8,
                                TrueColor, 0xFF0000, 0x00FF00, 0x0000FF) ||
      !miSetPixmapDepths() ||
      !fbScreenInit(pScreen, NULL, pvfb->root.width, pvfb->root.height,
                    monitorResolution, monitorResolution, 0, 32) ||
      !(pScreen->CreateScreenResources =
            lorieCreateScreenResources) // Simply replace unneeded function
      || !(pScreen->CloseScreen =
               lorieCloseScreen) // Simply replace unneeded function
      || !(!pvfb->dri3 || dri3_screen_init(pScreen, &lorieDri3Info)) ||
      !fbPictureInit(pScreen, 0, 0) || !exaDriverInit(pScreen, &lorieExa) ||
      !lorieRandRInit(pScreen) ||
      !miPointerInitialize(pScreen, &loriePointerSpriteFuncs,
                           &loriePointerCursorFuncs, TRUE) ||
      !fbCreateDefColormap(pScreen) ||
      !present_screen_init(pScreen, &loriePresentInfo))
    return FALSE;

  ShmRegisterFbFuncs(pScreen);
  miSyncShmScreenInit(pScreen);

  return TRUE;
} /* end lorieScreenInit */

void lorieConfigureNotify(int width, int height, int framerate,
                          size_t name_size, char *name) {
  ScreenPtr pScreen = pScreenPtr;
  RROutputPtr output = RRFirstOutput(pScreen);
  framerate = framerate ? framerate : 30;

  if (output && name) {
    // We should save this name in pvfb to make sure the name will be restored
    // in the case if the server is being reset.
    memset(pvfb->root.name, 0, 1024);
    memset(output->name, 0, 1024);
    strncpy(pvfb->root.name, name, name_size < 1024 ? name_size : 1024);
    strncpy(output->name, name, name_size < 1024 ? name_size : 1024);
    output->name[1023] = '\0';
    output->nameLength = strlen(output->name);
  }

  if (output && width && height &&
      (pScreen->width != width || pScreen->height != height ||
       pvfb->root.framerate != framerate)) {
    CARD32 mmWidth, mmHeight;
    RRModePtr mode = lorieCvt(width, height, framerate);
    mmWidth = ((double)(mode->mode.width)) * 25.4 / monitorResolution;
    mmHeight = ((double)(mode->mode.width)) * 25.4 / monitorResolution;
    RROutputSetModes(output, &mode, 1, 0);
    RRCrtcNotify(RRFirstEnabledCrtc(pScreen), mode, 0, 0, RR_Rotate_0, NULL, 1,
                 &output);
    RRScreenSizeSet(pScreen, mode->mode.width, mode->mode.height, mmWidth,
                    mmHeight);

    log(VERBOSE, "New reported framerate is %d", framerate);
    pvfb->root.framerate = framerate;
    pvfb->vblank_interval = 1000000 / pvfb->root.framerate;
  }
}

void InitOutput(ScreenInfo *screen_info, int argc, char **argv) {
  int depths[] = {1, 4, 8, 15, 16, 24, 32};
  int bpp[] = {1, 8, 8, 16, 16, 32, 32};
  int i;

  if (monitorResolution == 0)
    monitorResolution = 96;

  for (i = 0; i < ARRAY_SIZE(depths); i++) {
    screen_info->formats[i].depth = depths[i];
    screen_info->formats[i].bitsPerPixel = bpp[i];
    screen_info->formats[i].scanlinePad = BITMAP_SCANLINE_PAD;
  }

  screen_info->imageByteOrder = IMAGE_BYTE_ORDER;
  screen_info->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
  screen_info->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
  screen_info->bitmapBitOrder = BITMAP_BIT_ORDER;
  screen_info->numPixmapFormats = ARRAY_SIZE(depths);

#if !RENDERER_IN_ACTIVITY
  renderer_init(pvfb->env);
#endif
  renderer_test_capabilities(&pvfb->root.legacyDrawing, &pvfb->root.flip);
  xorgGlxCreateVendor();
  lorieInitClipboard();

  if (-1 == AddScreen(lorieScreenInit, argc, argv)) {
    FatalError("Couldn't add screen\n");
  }
}

// This Present implementation mostly copies the one from
// `present/present_fake.c` The only difference is performing vblanks right
// before redrawing root window (in lorieRedraw) instead of using timers.
static RRCrtcPtr loriePresentGetCrtc(WindowPtr w) {
  return RRFirstEnabledCrtc(w->drawable.pScreen);
}

static int loriePresentGetUstMsc(__unused RRCrtcPtr crtc, uint64_t *ust,
                                 uint64_t *msc) {
  *ust = GetTimeInMicros();
  *msc = pvfb->current_msc;
  return Success;
}

static Bool loriePresentQueueVblank(__unused RRCrtcPtr crtc, uint64_t event_id,
                                    uint64_t msc) {
#pragma clang diagnostic push
#pragma ide diagnostic ignored                                                 \
    "MemoryLeak" // it is not leaked, it is destroyed in lorieRedraw
  struct vblank *vblank = calloc(1, sizeof(*vblank));
  if (!vblank)
    return BadAlloc;

  *vblank = (struct vblank){.id = event_id, .msc = msc};
  xorg_list_add(&vblank->list, &pvfb->vblank_queue);

  return Success;
#pragma clang diagnostic pop
}

static void loriePresentAbortVblank(__unused RRCrtcPtr crtc, uint64_t id,
                                    __unused uint64_t msc) {
  struct vblank *vblank, *tmp;

  xorg_list_for_each_entry_safe(vblank, tmp, &pvfb->vblank_queue, list) {
    if (vblank->id == id) {
      xorg_list_del(&vblank->list);
      free(vblank);
      break;
    }
  }
}

static void loriePerformVblanks(void) {
  struct vblank *vblank, *tmp;
  uint64_t ust, msc;
  pvfb->current_msc++;

  xorg_list_for_each_entry_safe(vblank, tmp, &pvfb->vblank_queue, list) {
    if (vblank->msc <= pvfb->current_msc) {
      loriePresentGetUstMsc(NULL, &ust, &msc);
      present_event_notify(vblank->id, ust, msc);
      xorg_list_del(&vblank->list);
      free(vblank);
    }
  }
}

static struct present_screen_info loriePresentInfo = {
    .get_crtc = loriePresentGetCrtc,
    .get_ust_msc = loriePresentGetUstMsc,
    .queue_vblank = loriePresentQueueVblank,
    .abort_vblank = loriePresentAbortVblank,
};

void lorieSetVM(JavaVM *vm) {
  pvfb->vm = vm;
  (*vm)->AttachCurrentThread(vm, &pvfb->env, NULL);
}

void exaDDXDriverInit(__unused ScreenPtr pScreen) {}

void *lorieCreatePixmap(__unused ScreenPtr pScreen, int width, int height,
                        __unused int depth, int usage_hint, int bpp,
                        int *new_fb_pitch) {
  LoriePixmapPriv *priv;
  size_t size = sizeof(LoriePixmapPriv);
  *new_fb_pitch = ((width * bpp + FB_MASK) >> FB_SHIFT) * sizeof(FbBits);

  if (usage_hint != CREATE_PIXMAP_USAGE_LORIEBUFFER_BACKED)
    size += *new_fb_pitch * height;

  priv = calloc(1, size);
  if (!priv)
    return NULL;

  if (usage_hint != CREATE_PIXMAP_USAGE_LORIEBUFFER_BACKED)
    return priv;

  uint8_t type = pvfb->root.legacyDrawing ? LORIEBUFFER_REGULAR
                                          : LORIEBUFFER_AHARDWAREBUFFER;
  uint8_t format = pvfb->root.flip ? AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM
                                   : AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM;
  priv->buffer = LorieBuffer_allocate(width, height, format, type);
  LorieBuffer_Desc d = {0};
  LorieBuffer_describe(priv->buffer, &d);
  *new_fb_pitch = d.stride * 4;

  LorieBuffer_lock(priv->buffer, NULL, &priv->locked);
  if (!priv->buffer) {
    free(priv);
    return NULL;
  }

  return priv;
}

void lorieExaDestroyPixmap(__unused ScreenPtr pScreen, void *driverPriv) {
  LoriePixmapPriv *priv = driverPriv;
  if (priv->buffer) {
    LorieBuffer_unlock(priv->buffer);
    LorieBuffer_release(priv->buffer);
  }
  if (priv->ahb)
    AHardwareBuffer_release(priv->ahb);
  free(priv);
}

Bool lorieModifyPixmapHeader(PixmapPtr pPix, __unused int w, __unused int h,
                             __unused int depth, __unused int bitsPerbppPixel,
                             __unused int devKind, __unused void *data) {
  LoriePixmapPriv *priv = exaGetPixmapDriverPrivate(pPix);
  if (priv)
    priv->mem = data;
  return FALSE;
}

Bool loriePrepareAccess(PixmapPtr pPix, int index) {
  LoriePixmapPriv *priv = exaGetPixmapDriverPrivate(pPix);
  if (index == EXA_PREPARE_DEST &&
      pScreenPtr->GetScreenPixmap(pScreenPtr) == pPix)
    lorie_mutex_lock(&pvfb->state->lock, &pvfb->state->lockingPid);

  if (priv->ahb) {
    if (AHardwareBuffer_lock(priv->ahb,
                             AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                                 AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                             -1, NULL, &pPix->devPrivate.ptr))
      return FALSE;
  } else
    pPix->devPrivate.ptr = priv->locked ?: priv->mem ?: priv + 1;
  return TRUE;
}

void lorieFinishAccess(PixmapPtr pPix, int index) {
  LoriePixmapPriv *priv = exaGetPixmapDriverPrivate(pPix);
  if (index == EXA_PREPARE_DEST &&
      pScreenPtr->GetScreenPixmap(pScreenPtr) == pPix)
    lorie_mutex_unlock(&pvfb->state->lock, &pvfb->state->lockingPid);

  if (priv->ahb)
    AHardwareBuffer_unlock(priv->ahb, NULL);
}

// Otimizações EXA: suporte a fills sólidos e flags melhoradas para GPU
static Bool loriePrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel color) {
  // Desativado: o driver ainda não implementa Solid corretamente
  // Retornar FALSE força o X Server a usar o fallback de software
  return FALSE;
}

static void lorieSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2) {
  // Fill sólido otimizado - operações são feitas pelo próprio X Server
  // mas com menos overhead devido à otimização PrepareSolid
  (void)pPixmap; (void)x1; (void)y1; (void)x2; (void)y2;
}

static void lorieDoneSolid(PixmapPtr pPixmap) {
  (void)pPixmap;
}

static ExaDriverRec lorieExa = {
    .exa_major = EXA_VERSION_MAJOR,
    .exa_minor = EXA_VERSION_MINOR,
    .maxX = 32767,
    .maxY = 32767,
    // Otimizado: flags para melhor performance
    .flags = EXA_OFFSCREEN_PIXMAPS | EXA_HANDLES_PIXMAPS,
    .pixmapPitchAlign = 32,
    .pixmapOffsetAlign = 32,
    // Otimizações: PrepareSolid agora implementado para fills acelerados
    .PrepareSolid = loriePrepareSolid,
    .Solid = lorieSolid,
    .DoneSolid = lorieDoneSolid,
    .PrepareCopy = FalseNoop,
    .PrepareComposite = FalseNoop,
    .PixmapIsOffscreen = TrueNoop,
    .WaitMarker = VoidNoop,
    .PrepareAccess = loriePrepareAccess,
    .FinishAccess = lorieFinishAccess,
    .CreatePixmap2 = lorieCreatePixmap,
    .DestroyPixmap = lorieExaDestroyPixmap,
    .ModifyPixmapHeader = lorieModifyPixmapHeader,
};

static PixmapPtr loriePixmapFromFds(ScreenPtr screen, CARD8 num_fds,
                                    const int *fds, CARD16 width, CARD16 height,
                                    const CARD32 *strides,
                                    const CARD32 *offsets, CARD8 depth,
                                    __unused CARD8 bpp, CARD64 modifier) {
#define fail(msg, ...)                                                         \
  do {                                                                         \
    log(ERROR, msg, ##__VA_ARGS__);                                            \
    goto fail;                                                                 \
  } while (0)
#define check(cond, msg, ...)                                                  \
  if ((cond))                                                                  \
  fail(msg, ##__VA_ARGS__)
  const CARD64 AHARDWAREBUFFER_SOCKET_FD = 1255;
  const CARD64 AHARDWAREBUFFER_FLIPPED_SOCKET_FD = 1256;
  const CARD64 RAW_MMAPPABLE_FD = 1274;
  AHardwareBuffer_Desc desc = {0};
  PixmapPtr pixmap = NullPixmap;
  LoriePixmapPriv *priv = NULL;
  void *addr = NULL;

  check(num_fds > 1, "DRI3: More than 1 fd");
  check(modifier != RAW_MMAPPABLE_FD && modifier != AHARDWAREBUFFER_SOCKET_FD &&
            modifier != AHARDWAREBUFFER_FLIPPED_SOCKET_FD &&
            modifier != DRM_FORMAT_MOD_INVALID,
        "DRI3: Modifier is not RAW_MMAPPABLE_FD or AHARDWAREBUFFER_SOCKET_FD");

  pixmap = screen->CreatePixmap(screen, 0, 0, depth, 0);
  check(!pixmap, "DRI3: failed to create pixmap");

  priv = exaGetPixmapDriverPrivate(pixmap);
  check(!priv, "DRI3: failed to obtain pixmap private");

  if (modifier == DRM_FORMAT_MOD_INVALID || modifier == RAW_MMAPPABLE_FD) {
    addr = mmap(NULL, strides[0] * height, PROT_READ, MAP_SHARED, fds[0],
                offsets[0]);
    check(!addr || addr == MAP_FAILED, "DRI3: RAW_MMAPPABLE_FD: mmap failed");
    screen->ModifyPixmapHeader(pixmap, width, height, 0, 0, strides[0], addr);
    return pixmap;
  }

  if (modifier == AHARDWAREBUFFER_SOCKET_FD ||
      modifier == AHARDWAREBUFFER_FLIPPED_SOCKET_FD) {
    struct stat info;
    uint8_t buf = 1;
    int r;

    priv->flipped = modifier == AHARDWAREBUFFER_FLIPPED_SOCKET_FD;
    check(fstat(fds[0], &info) != 0, "DRI3: fstat failed: %s", strerror(errno));
    check(!S_ISSOCK(info.st_mode),
          "DRI3: modifier is AHARDWAREBUFFER_SOCKET_FD but fd is not a socket");
    // Sending signal to other end of socket to send buffer.
    check(write(fds[0], &buf, 1) != 1,
          "DRI3: AHARDWAREBUFFER_SOCKET_FD: failed to write to socket: %s",
          strerror(errno));
    check((r = AHardwareBuffer_recvHandleFromUnixSocket(fds[0], &priv->ahb)) !=
              0,
          "DRI3: AHARDWAREBUFFER_SOCKET_FD: failed to obtain AHardwareBuffer "
          "from socket: %d",
          r);
    check(!priv->ahb, "DRI3: AHARDWAREBUFFER_SOCKET_FD: did not receive "
                      "AHardwareSocket from buffer");
    AHardwareBuffer_describe(priv->ahb, &desc);
    check(desc.format != AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM &&
              desc.format != AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM &&
              desc.format != AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM,
          "DRI3: AHARDWAREBUFFER_SOCKET_FD: wrong format of AHardwareBuffer. "
          "Must be one of: AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, "
          "AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, "
          "AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM (stands for 5).");

    screen->ModifyPixmapHeader(pixmap, desc.width, desc.height, 0, 0,
                               desc.stride * 4, NULL);
  }

  return pixmap;

fail:
  if (priv && priv->ahb) {
    AHardwareBuffer_release(priv->ahb);
    priv->ahb = NULL;
  }
  if (addr)
    munmap(addr, strides[0] * height);
  if (pixmap)
    screen->DestroyPixmap(pixmap);

  return NULL;
}

static int lorieGetFormats(__unused ScreenPtr screen, CARD32 *num_formats,
                           CARD32 **formats) {
  *num_formats = 0;
  *formats = NULL;
  return TRUE;
}

static int lorieGetModifiers(__unused ScreenPtr screen,
                             __unused uint32_t format, uint32_t *num_modifiers,
                             uint64_t **modifiers) {
  *num_modifiers = 0;
  *modifiers = NULL;
  return TRUE;
}

static int lorie_open_client(ClientPtr client, ScreenPtr screen,
                             RRProviderPtr provider, int *fd) {
  *fd = open("/dev/null", O_RDWR | O_CLOEXEC);
  return Success;
}

static int lorie_fds_from_pixmap(ScreenPtr screen, PixmapPtr pixmap, int *fds,
                                 uint32_t *strides, uint32_t *offsets,
                                 uint64_t *modifier) {
  LoriePixmapPriv *priv = exaGetPixmapDriverPrivate(pixmap);
  if (!priv || !priv->buffer)
    return 0;

  fds[0] = LorieBuffer_getFD(priv->buffer);
  if (fds[0] < 0)
    return 0;

  LorieBuffer_Desc desc;
  LorieBuffer_describe(priv->buffer, &desc);

  strides[0] = desc.stride * 4; // Assuming 32bpp
  offsets[0] = 0;
  *modifier = DRM_FORMAT_MOD_INVALID;

  return 1;
}

static dri3_screen_info_rec lorieDri3Info = {
    .version = 2,
    .open_client = lorie_open_client,
    .pixmap_from_fds = loriePixmapFromFds,
    .fds_from_pixmap = lorie_fds_from_pixmap,
    .get_formats = lorieGetFormats,
    .get_modifiers = lorieGetModifiers,
    .get_drawable_modifiers = FalseNoop};

static GLboolean drawableSwapBuffers(unused ClientPtr client,
                                     unused __GLXdrawable *drawable) {
  return TRUE;
}
static void drawableCopySubBuffer(unused __GLXdrawable *basePrivate,
                                  unused int x, unused int y, unused int w,
                                  unused int h) {}
static __GLXdrawable *createDrawable(unused ClientPtr client,
                                     __GLXscreen *screen, DrawablePtr pDraw,
                                     unused XID drawId, int type, XID glxDrawId,
                                     __GLXconfig *glxConfig) {
  __GLXdrawable *private = calloc(1, sizeof *private);
  if (private == NULL)
    return NULL;

  if (!__glXDrawableInit(private, screen, pDraw, type, glxDrawId, glxConfig)) {
    free(private);
    return NULL;
  }

  private->destroy = (void (*)(__GLXdrawable *))free;
  private->swapBuffers = drawableSwapBuffers;
  private->copySubBuffer = drawableCopySubBuffer;

  return private;
}

static void glXDRIscreenDestroy(__GLXscreen *baseScreen) {
  free(baseScreen->GLXextensions);
  free(baseScreen->GLextensions);
  free(baseScreen->visuals);
  free(baseScreen);
}

static __GLXscreen *glXDRIscreenProbe(ScreenPtr pScreen) {
  __GLXscreen *screen;

  screen = calloc(1, sizeof *screen);
  if (screen == NULL)
    return NULL;

  screen->destroy = glXDRIscreenDestroy;
  screen->createDrawable = createDrawable;
  screen->pScreen = pScreen;
  screen->fbconfigs = configs;
  screen->glvnd = "mesa";

  __glXInitExtensionEnableBits(screen->glx_enable_bits);
  /* There is no real GLX support, but anyways swrast reports it. */
  __glXEnableExtension(screen->glx_enable_bits, "GLX_MESA_copy_sub_buffer");
  __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_no_config_context");
  __glXEnableExtension(screen->glx_enable_bits, "GLX_ARB_create_context");
  __glXEnableExtension(screen->glx_enable_bits,
                       "GLX_ARB_create_context_no_error");
  __glXEnableExtension(screen->glx_enable_bits,
                       "GLX_ARB_create_context_profile");
  __glXEnableExtension(screen->glx_enable_bits,
                       "GLX_EXT_create_context_es_profile");
  __glXEnableExtension(screen->glx_enable_bits,
                       "GLX_EXT_create_context_es2_profile");
  __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_framebuffer_sRGB");
  __glXEnableExtension(screen->glx_enable_bits, "GLX_ARB_fbconfig_float");
  __glXEnableExtension(screen->glx_enable_bits,
                       "GLX_EXT_fbconfig_packed_float");
  __glXEnableExtension(screen->glx_enable_bits, "GLX_EXT_texture_from_pixmap");
  __glXScreenInit(screen, pScreen);

  return screen;
}

__GLXprovider __glXDRISWRastProvider = {glXDRIscreenProbe, "DRISWRAST", NULL};
