#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma ide diagnostic ignored "UnusedParameter"
#pragma ide diagnostic ignored "DanglingPointer"
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "UnreachableCode"
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#pragma ide diagnostic ignored "misc-no-recursion"
#pragma clang diagnostic ignored                                               \
    "-Wincompatible-pointer-types-discards-qualifiers"
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include "lorie.h"
#include "os.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/resource.h>

#define log(...)                                                               \
  __android_log_print(ANDROID_LOG_DEBUG, "gles-renderer", __VA_ARGS__)
#define loge(...)                                                              \
  __android_log_print(ANDROID_LOG_ERROR, "gles-renderer", __VA_ARGS__)

static GLuint create_program(const char *p_vertex_source,
                             const char *p_fragment_source);

static int printEglError(char *msg, int line) {
  char descBuf[32] = {0};
  char *desc;
  int err = eglGetError();
  switch (err) {
#define E(code, text)                                                          \
  case code:                                                                   \
    desc = (char *)text;                                                       \
    break
  case EGL_SUCCESS:
    desc = NULL; // "No error"
    E(EGL_NOT_INITIALIZED, "EGL not initialized or failed to initialize");
    E(EGL_BAD_ACCESS, "Resource inaccessible");
    E(EGL_BAD_ALLOC, "Cannot allocate resources");
    E(EGL_BAD_ATTRIBUTE, "Unrecognized attribute or attribute value");
    E(EGL_BAD_CONTEXT, "Invalid EGL context");
    E(EGL_BAD_CONFIG, "Invalid EGL frame buffer configuration");
    E(EGL_BAD_CURRENT_SURFACE, "Current surface is no longer valid");
    E(EGL_BAD_DISPLAY, "Invalid EGL display");
    E(EGL_BAD_SURFACE, "Invalid surface");
    E(EGL_BAD_MATCH, "Inconsistent arguments");
    E(EGL_BAD_PARAMETER, "Invalid argument");
    E(EGL_BAD_NATIVE_PIXMAP, "Invalid native pixmap");
    E(EGL_BAD_NATIVE_WINDOW, "Invalid native window");
    E(EGL_CONTEXT_LOST, "Context lost");
#undef E
  default:
    snprintf(descBuf, sizeof(descBuf) - 1, "Unknown error (%d)", err);
    desc = descBuf;
  }

  if (desc)
    log("renderer: %s: %s (%s:%d)\n", msg, desc, __FILE__, line);

  return 0;
}

static inline __always_inline void vprintEglError(char *msg, int line) {
  printEglError(msg, line);
}

static void checkGlError(int line) {
  GLenum error;
  char *desc = NULL;
  for (error = glGetError(); error; error = glGetError()) {
    switch (error) {
#define E(code)                                                                \
  case code:                                                                   \
    desc = (char *)#code;                                                      \
    break
      E(GL_INVALID_ENUM);
      E(GL_INVALID_VALUE);
      E(GL_INVALID_OPERATION);
      E(GL_STACK_OVERFLOW_KHR);
      E(GL_STACK_UNDERFLOW_KHR);
      E(GL_OUT_OF_MEMORY);
      E(GL_INVALID_FRAMEBUFFER_OPERATION);
      E(GL_CONTEXT_LOST_KHR);
    default:
      continue;
#undef E
    }
    log("Xlorie: GLES %d ERROR: %s.\n", line, desc);
    return;
  }
}

/* In release builds, glGetError() is a full GPU pipeline sync barrier.
 * Disable it completely to avoid adding latency on every draw call. */
#ifdef NDEBUG
  #define checkGlError() ((void)0)
#else
  #define checkGlError() checkGlError(__LINE__)
#endif

static const char vertex_shader[] = "attribute vec4 position;\n"
                                    "attribute vec2 texCoords;"
                                    "varying vec2 outTexCoords;\n"
                                    "void main(void) {\n"
                                    "   outTexCoords = texCoords;\n"
                                    "   gl_Position = position;\n"
                                    "}\n";

#define COLOR_PROFILE_FRAGMENT_LOGIC                                           \
    "   if (colorProfile == 1) { // Vivid\n"                                     \
    "       lowp float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));\n"   \
    "       color.rgb = mix(vec3(gray), color.rgb, 1.25);\n"                     \
    "   } else if (colorProfile == 2) { // Warm\n"                                \
    "       color.rgb *= vec3(1.10, 1.02, 0.90);\n"                              \
    "   } else if (colorProfile == 3) { // Cool\n"                                \
    "       color.rgb *= vec3(0.90, 1.02, 1.10);\n"                              \
    "   }\n"

#define FRAGMENT_SHADER(swizzle)                                               \
  "precision mediump float;\n"                                                 \
  "varying vec2 outTexCoords;\n"                                               \
  "uniform sampler2D texture;\n"                                               \
  "uniform int colorProfile;\n"                                                \
  "void main(void) {\n"                                                        \
  "   lowp vec4 color = texture2D(texture, outTexCoords)" swizzle ";\n"        \
  COLOR_PROFILE_FRAGMENT_LOGIC                                                 \
  "   gl_FragColor = color;\n"                                                 \
  "}\n"

static const char fragment_shader[] = FRAGMENT_SHADER();
static const char fragment_shader_bgra[] = FRAGMENT_SHADER(".bgra");

#define FSR_FRAGMENT_SHADER(swizzle)                                           \
    "precision mediump float;\n"                                               \
    "varying vec2 outTexCoords;\n"                                             \
    "uniform sampler2D texture;\n"                                             \
    "uniform vec4 srcSize; // {1/w, 1/h, w, h}\n"                              \
    "uniform int colorProfile;\n"                                                \
    "void main() {\n"                                                          \
    "    lowp vec3 e = texture2D(texture, outTexCoords).rgb;\n"                \
    "    lowp vec3 b = texture2D(texture, outTexCoords + vec2(0.0, -srcSize.y)).rgb;\n" \
    "    lowp vec3 d = texture2D(texture, outTexCoords + vec2(-srcSize.x, 0.0)).rgb;\n" \
    "    lowp vec3 f = texture2D(texture, outTexCoords + vec2(srcSize.x, 0.0)).rgb;\n" \
    "    lowp vec3 h = texture2D(texture, outTexCoords + vec2(0.0, srcSize.y)).rgb;\n" \
    "    lowp vec3 res = e + (e - (b + d + f + h) * 0.25) * 0.35;\n"            \
    "    lowp vec4 color = vec4(clamp(res, 0.0, 1.0), 1.0)" swizzle ";\n"         \
    COLOR_PROFILE_FRAGMENT_LOGIC                                                 \
    "    gl_FragColor = color;\n"                                                 \
    "}\n"

static const char fsr_fragment_shader[] = FSR_FRAGMENT_SHADER("");
static const char fsr_fragment_shader_bgra[] = FSR_FRAGMENT_SHADER(".bgra");

#define CAS_FRAGMENT_SHADER(swizzle)                                           \
    "precision mediump float;\n"                                               \
    "varying vec2 outTexCoords;\n"                                             \
    "uniform sampler2D texture;\n"                                             \
    "uniform vec4 srcSize; // {1/w, 1/h, w, h}\n"                              \
    "uniform int colorProfile;\n"                                                \
    "void main() {\n"                                                          \
    "    lowp vec3 e = texture2D(texture, outTexCoords).rgb;\n"                \
    "    lowp vec3 b = texture2D(texture, outTexCoords + vec2(0.0, -srcSize.y)).rgb;\n" \
    "    lowp vec3 d = texture2D(texture, outTexCoords + vec2(-srcSize.x, 0.0)).rgb;\n" \
    "    lowp vec3 f = texture2D(texture, outTexCoords + vec2(srcSize.x, 0.0)).rgb;\n" \
    "    lowp vec3 h = texture2D(texture, outTexCoords + vec2(0.0, srcSize.y)).rgb;\n" \
    "    lowp vec3 res = e + (e - (b + d + f + h) * 0.25) * 0.18;\n"            \
    "    lowp vec4 color = vec4(clamp(res, 0.0, 1.0), 1.0)" swizzle ";\n"         \
    COLOR_PROFILE_FRAGMENT_LOGIC                                                 \
    "    gl_FragColor = color;\n"                                                 \
    "}\n"

static const char cas_fragment_shader[] = CAS_FRAGMENT_SHADER("");
static const char cas_fragment_shader_bgra[] = CAS_FRAGMENT_SHADER(".bgra");

#define INTERPOLATION_FRAGMENT_SHADER(swizzle)                                 \
    "precision mediump float;\n"                                               \
    "varying vec2 outTexCoords;\n"                                             \
    "uniform sampler2D texture;\n"                                             \
    "uniform sampler2D lastTexture;\n"                                         \
    "uniform float alpha;\n"                                                   \
    "uniform int colorProfile;\n"                                                \
    "void main() {\n"                                                          \
    "    lowp vec4 current = texture2D(texture, outTexCoords);\n"              \
    "    lowp vec4 last = texture2D(lastTexture, outTexCoords);\n"             \
    "    lowp vec4 color = mix(last, current, alpha)" swizzle ";\n"             \
    COLOR_PROFILE_FRAGMENT_LOGIC                                                 \
    "    gl_FragColor = color;\n"                                                 \
    "}\n"

static const char interpolation_fragment_shader[] = INTERPOLATION_FRAGMENT_SHADER("");
static const char interpolation_fragment_shader_bgra[] = INTERPOLATION_FRAGMENT_SHADER(".bgra");

static EGLDisplay egl_display = EGL_NO_DISPLAY;
static EGLContext ctx = EGL_NO_CONTEXT;
static EGLSurface sfc = EGL_NO_SURFACE;
static EGLConfig cfg = 0;
static ANativeWindow *win = 0;
#if !RENDERER_IN_ACTIVITY
static jobject surface = NULL;
#endif
static LorieBuffer *buffer = NULL;
static EGLImageKHR image = NULL;

#if !RENDERER_IN_ACTIVITY
static jmethodID Surface_release = NULL;
static jmethodID Surface_destroy = NULL;
#endif

static JNIEnv *renderEnv = NULL;
static volatile bool stateChanged = false, bufferChanged = false,
                     windowChanged = false;
static volatile struct lorie_shared_server_state *pendingState = NULL;
static volatile LorieBuffer *pendingBuffer = NULL;
#if RENDERER_IN_ACTIVITY
static volatile ANativeWindow *pendingWin = NULL;
#else
static volatile jobject pendingSurface = NULL;
#endif

static pthread_mutex_t stateLock;
static pthread_cond_t stateCond;
static volatile struct lorie_shared_server_state *state = NULL;
static struct {
  GLuint id;
  LorieBuffer_Desc desc;
} display;

static GLuint local_display_id = 0;
static GLuint quad_vbo = 0;  // Static VBO for the full-screen quad (never changes)
static GLuint copy_fbo = 0;
static GLuint dst_fbo = 0;  // Pooled destination FBO for blit operations
static uint32_t local_display_width = 0;
static uint32_t local_display_height = 0;

// Double buffering para cursor - evita re-upload da mesma textura
static struct {
  GLuint ids[2];           // Duas texturas para alternância
  int currentIndex;        // Índice atual sendo usado para render
  int pendingIndex;        // Índice pendente de upload
  uint32_t lastWidth;
  uint32_t lastHeight;
  uint32_t lastHash;       // Hash do conteúdo para detectar mudanças reais
  bool needsUpload;        // Flag se precisa upload
} cursor = {{0, 0}, 0, 1, 0, 0, 0, false};

// Performance counters
static struct {
  uint64_t framesRendered;
  uint64_t framesSkipped;
  uint64_t cursorUploads;
  uint64_t bufferSwaps;
  double avgFrameTimeMs;
  double lastFrameTimeMs;
  struct timespec lastFrameTime;
  int gpuLoadLevel;  // 0=low, 1=medium, 2=high
} perf = {0, 0, 0, 0, 0.0, 0.0, {0, 0}, 0};
static int gles_version = 0;
static GLuint pbo = 0;

GLuint g_texture_program = 0, gv_pos = 0, gv_coords = 0;
GLuint g_texture_program_bgra = 0, gv_pos_bgra = 0, gv_coords_bgra = 0;
GLuint g_fsr_program = 0, gv_pos_fsr = 0, gv_coords_fsr = 0, gu_src_size_fsr = 0;
GLuint g_fsr_program_bgra = 0, gv_pos_fsr_bgra = 0, gv_coords_fsr_bgra = 0, gu_src_size_fsr_bgra = 0;
GLuint g_cas_program = 0, gv_pos_cas = 0, gv_coords_cas = 0, gu_src_size_cas = 0;
GLuint g_cas_program_bgra = 0, gv_pos_cas_bgra = 0, gv_coords_cas_bgra = 0, gu_src_size_cas_bgra = 0;
GLuint g_interpolation_program = 0, gv_pos_interp = 0, gv_coords_interp = 0,
       gu_alpha_interp = 0, gu_last_tex_interp = 0;
GLuint g_interpolation_program_bgra = 0, gv_pos_interp_bgra = 0, gv_coords_interp_bgra = 0,
       gu_alpha_interp_bgra = 0, gu_last_tex_interp_bgra = 0;

GLuint gu_color_profile = 0, gu_color_profile_bgra = 0, gu_color_profile_fsr = 0,
       gu_color_profile_fsr_bgra = 0, gu_color_profile_cas = 0, gu_color_profile_cas_bgra = 0,
       gu_color_profile_interp = 0, gu_color_profile_interp_bgra = 0;

static volatile int color_profile = 0;
void renderer_set_color_profile(int profile) { color_profile = profile; }

static volatile int scaling_filter = 0;
void renderer_set_scaling_filter(int filter) { scaling_filter = filter; }

static volatile int renderer_vsync = 1;
void renderer_set_vsync(int vsync) { 
  log("renderer: vsync set to %d", vsync);
  renderer_vsync = vsync;
  if (state)
    state->vsyncOff = (vsync == 0);
}


static volatile int frame_generation = 0;
void renderer_set_frame_generation(int mode) { 
  log("renderer: frame_generation set to %d", mode);
  frame_generation = mode; 
}

static volatile int renderer_frameskip = 0;
static int renderer_frameskip_counter = 0;
void renderer_set_frameskip(int skip) { 
  log("renderer: frameskip set to %d", skip);
  renderer_frameskip = skip; 
}

static struct {
  GLuint id;
  uint32_t width, height;
} last_frame = {0, 0, 0};

// Hash simples para detectar mudanças no cursor
static uint32_t hashCursorBits(const uint32_t *data, int width, int height) {
  uint32_t hash = 0;
  int stride = width * height / 4; // Amostragem a cada 4 pixels
  if (stride < 1) stride = 1;
  for (int i = 0; i < width * height; i += stride) {
    hash = hash * 31 + data[i];
  }
  return hash;
}

static void updatePerfCounters(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  
  if (perf.lastFrameTime.tv_sec != 0 || perf.lastFrameTime.tv_nsec != 0) {
    double frameTimeMs = (now.tv_sec - perf.lastFrameTime.tv_sec) * 1000.0 +
                         (now.tv_nsec - perf.lastFrameTime.tv_nsec) / 1000000.0;
    perf.lastFrameTimeMs = frameTimeMs;
    // Média móvel exponencial
    perf.avgFrameTimeMs = perf.avgFrameTimeMs * 0.9 + frameTimeMs * 0.1;
    
    // Adaptive GPU load detection
    if (perf.avgFrameTimeMs > 16.67) {  // Below 60fps
      perf.gpuLoadLevel = 2;  // High load
    } else if (perf.avgFrameTimeMs > 11.0) {  // Below 90fps
      perf.gpuLoadLevel = 1;  // Medium load
    } else {
      perf.gpuLoadLevel = 0;  // Low load
    }
  }
  perf.lastFrameTime = now;
  perf.framesRendered++;
}

static void *renderer_thread(void *closure);

/* Only bind — texture parameters are persistent state on the texture object.
 * Call initLinearTexture() once at creation time, then just bindTexture(). */
static inline __always_inline void bindTexture(GLuint id) {
  glBindTexture(GL_TEXTURE_2D, id);
}

/* Sets LINEAR filter + CLAMP params on the currently bound texture.
 * Must be called once after glGenTextures, before any use. */
static inline __always_inline void initTextureParams(void) {
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

/* Legacy helper used at texture creation sites — bind + set params. */
static inline __always_inline void bindLinearTexture(GLuint id) {
  glBindTexture(GL_TEXTURE_2D, id);
  initTextureParams();
}

static EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                 EGL_WINDOW_BIT,
                                 EGL_RENDERABLE_TYPE,
                                 EGL_OPENGL_ES2_BIT,
                                 EGL_RED_SIZE,
                                 8,
                                 EGL_GREEN_SIZE,
                                 8,
                                 EGL_BLUE_SIZE,
                                 8,
                                 EGL_ALPHA_SIZE,
                                 0,
                                 EGL_NONE};

const EGLint ctxattribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

const EGLint ctxattribs3[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
                              EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};

int renderer_init(JNIEnv *env) {
  JavaVM *vm;
  pthread_t t;
  EGLint major, minor;
  EGLint numConfigs;
  EGLint *const alphaAttrib = &configAttribs[11];

  if (ctx)
    return 1;

  (*env)->GetJavaVM(env, &vm);

  pthread_mutex_init(&stateLock, NULL);
  pthread_cond_init(&stateCond, NULL);

#if !RENDERER_IN_ACTIVITY
  jclass Surface = (*env)->FindClass(env, "android/view/Surface");
  Surface_release = (*env)->GetMethodID(env, Surface, "release", "()V");
  Surface_destroy = (*env)->GetMethodID(env, Surface, "destroy", "()V");
  if (!Surface_release) {
    loge("Failed to find required Surface.release method. Aborting.\n");
    abort();
  }
  if (!Surface_destroy) {
    loge("Failed to find required Surface.destroy method. Aborting.\n");
    abort();
  }
#endif

  egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (egl_display == EGL_NO_DISPLAY)
    return printEglError("Got no EGL display", __LINE__);

  if (eglInitialize(egl_display, &major, &minor) != EGL_TRUE)
    return printEglError("Unable to initialize EGL", __LINE__);

  log("Xlorie: Initialized EGL version %d.%d\n", major, minor);
  eglBindAPI(EGL_OPENGL_ES_API);

  if (eglChooseConfig(egl_display, configAttribs, &cfg, 1, &numConfigs) !=
          EGL_TRUE &&
      (*alphaAttrib = 8) &&
      eglChooseConfig(egl_display, configAttribs, &cfg, 1, &numConfigs) !=
          EGL_TRUE)
    return printEglError("eglChooseConfig failed", __LINE__);

  ctx = eglCreateContext(egl_display, cfg, NULL, ctxattribs3);
  if (ctx != EGL_NO_CONTEXT)
    gles_version = 3;
  else {
    ctx = eglCreateContext(egl_display, cfg, NULL, ctxattribs);
    gles_version = 2;
  }

  if (ctx == EGL_NO_CONTEXT)
    return printEglError("eglCreateContext failed", __LINE__);

  if (eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                     EGL_NO_CONTEXT) != EGL_TRUE)
    return printEglError("eglMakeCurrent failed", __LINE__);

  if (gles_version == 3) {
    log("Xlorie: Initialized GLES 3.0 context, using PBO for legacy drawing.");
    glGenBuffers(1, &pbo);
  } else {
    log("Xlorie: Fallback to GLES 2.0 context");
  }

  pthread_create(&t, NULL, renderer_thread, vm);
  return 1;
}

void renderer_test_capabilities(int *legacy_drawing, uint8_t *flip) {
  // Some devices do not support sampling from HAL_PIXEL_FORMAT_BGRA_8888, here
  // we are checking it.
  const EGLint imageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
                                    EGL_NONE};
  EGLint numConfigs;
  EGLClientBuffer clientBuffer;
  EGLImageKHR img;
  AHardwareBuffer *new = NULL;
  int status;
  AHardwareBuffer_Desc d0 = {.width = 64,
                             .height = 64,
                             .layers = 1,
                             .usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                                      AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
                                      AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
                             .format = AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM};

  if (egl_display == EGL_NO_DISPLAY) {
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY)
      return vprintEglError("Got no EGL display", __LINE__);
  }

  status = AHardwareBuffer_allocate(&d0, &new);
  if (status != 0 || new == NULL) {
    loge("Failed to allocate native buffer (%p, error %d)", new, status);
    loge("Forcing legacy drawing");
    *legacy_drawing = 1;
    return;
  }

  uint32_t *pixels;
  if (AHardwareBuffer_lock(new,
                           AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
                               AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
                           -1, NULL, (void **)&pixels) == 0) {
    pixels[0] = 0xAABBCCDD;
    AHardwareBuffer_unlock(new, NULL);
  } else {
    loge("Failed to lock native buffer (%p, error %d)", new, status);
    loge("Forcing legacy drawing");
    *legacy_drawing = 1;
    AHardwareBuffer_release(new);
    return;
  }

  clientBuffer = eglGetNativeClientBufferANDROID(new);
  if (!clientBuffer) {
    *legacy_drawing = 1;
    AHardwareBuffer_release(new);
    return vprintEglError("Failed to obtain EGLClientBuffer from "
                          "AHardwareBuffer, forcing legacy drawing",
                          __LINE__);
  }

  if (!(img = eglCreateImageKHR(egl_display, EGL_NO_CONTEXT,
                                EGL_NATIVE_BUFFER_ANDROID, clientBuffer,
                                imageAttributes))) {
    if (eglGetError() == EGL_BAD_PARAMETER) {
      loge("Sampling from HAL_PIXEL_FORMAT_BGRA_8888 is not supported, forcing "
           "AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM");
      *flip = 1;
    } else {
      loge("Failed to obtain EGLImageKHR from EGLClientBuffer");
      loge("Forcing legacy drawing");
      *legacy_drawing = 1;
    }
    AHardwareBuffer_release(new);
  } else {
    // For some reason all devices I checked had no
    // GL_EXT_texture_format_BGRA8888 support, but some of them still provided
    // BGRA extension. EGL does not provide functions to query texture format in
    // runtime. Workarounds are less performant but at least they let us use
    // Termux:X11 on devices with missing BGRA support. We handle two cases. If
    // resulting texture has BGRA format but still drawing RGBA we should flip
    // format to RGBA and flip pixels manually in shader. In the case if for
    // some reason we can not use HAL_PIXEL_FORMAT_BGRA_8888 we should fallback
    // to legacy drawing method (uploading pixels via glTexImage2D).
    configAttribs[1] = EGL_PBUFFER_BIT;
    EGLConfig checkcfg = 0;
    GLuint fbo = 0, texture = 0;
    if (eglChooseConfig(egl_display, configAttribs, &checkcfg, 1,
                        &numConfigs) != EGL_TRUE)
      return vprintEglError("check eglChooseConfig failed", __LINE__);

    EGLContext testctx =
        eglCreateContext(egl_display, checkcfg, NULL, ctxattribs);
    if (testctx == EGL_NO_CONTEXT)
      return vprintEglError("check eglCreateContext failed", __LINE__);

    const EGLint pbufferAttributes[] = {
        EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE,
    };
    EGLSurface checksfc =
        eglCreatePbufferSurface(egl_display, checkcfg, pbufferAttributes);

    if (eglMakeCurrent(egl_display, checksfc, checksfc, testctx) != EGL_TRUE)
      return vprintEglError("check eglMakeCurrent failed", __LINE__);

    glActiveTexture(GL_TEXTURE0);
    checkGlError();
    glGenTextures(1, &texture);
    checkGlError();
    bindLinearTexture(texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, img);
    checkGlError();
    glGenFramebuffers(1, &fbo);
    checkGlError();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    checkGlError();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture, 0);
    checkGlError();
    uint32_t pixel[64 * 64];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
    checkGlError();
    if (pixel[0] == 0xAABBCCDD) {
      log("Xlorie: GLES draws pixels unchanged, probably system does not "
          "support AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM. Forcing bgra.\n");
      *flip = 1;
    } else if (pixel[0] != 0xAADDCCBB) {
      log("Xlorie: GLES receives broken pixels. Forcing legacy drawing. 0x%X\n",
          pixel[0]);
      *legacy_drawing = 1;
    }
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(egl_display, testctx);
    eglDestroyImageKHR(egl_display, img);
    eglDestroySurface(egl_display, checksfc);
    AHardwareBuffer_release(new);
  }
}

__unused void
renderer_set_shared_state(struct lorie_shared_server_state *newState) {
  pthread_mutex_lock(&stateLock);
  if (newState == pendingState || newState == state)
    goto end;

  if (pendingState)
    munmap(pendingState, sizeof(*state));

  pendingState = newState;
  stateChanged = true;

  // We are not sure which conditional variable is used at current moment so
  // let's signal both
  if (state)
    pthread_cond_signal(&state->cond);
  pthread_cond_signal(&stateCond);

end:
  pthread_mutex_unlock(&stateLock);
}

void renderer_set_buffer(LorieBuffer *buf) {
  pthread_mutex_lock(&stateLock);
  if (buf == pendingBuffer)
    goto end;

  if (pendingBuffer)
    LorieBuffer_release(pendingBuffer);

  pendingBuffer = buf;
  bufferChanged = true;

  if (pendingBuffer)
    LorieBuffer_acquire(pendingBuffer);

  // We are not sure which conditional variable is used at current moment so
  // let's signal both
  if (state)
    pthread_cond_signal(&state->cond);
  pthread_cond_signal(&stateCond);

end:
  pthread_mutex_unlock(&stateLock);
}

#if RENDERER_IN_ACTIVITY
void renderer_set_window(ANativeWindow *newWin) {
#else
void renderer_set_window(JNIEnv *env, jobject new_surface) {
#endif
  pthread_mutex_lock(&stateLock);
#if RENDERER_IN_ACTIVITY
  if (newWin && pendingWin == newWin) {
    ANativeWindow_release(newWin);
    pthread_mutex_unlock(&stateLock);
    return;
  }

  if (pendingWin)
    ANativeWindow_release(pendingWin);

  pendingWin = newWin;
#else
  if (pendingSurface && new_surface && pendingSurface != new_surface &&
      (*env)->IsSameObject(env, pendingSurface, new_surface)) {
    (*env)->DeleteGlobalRef(env, new_surface);
    pthread_mutex_unlock(&stateLock);
    return;
  }

  if (pendingSurface && pendingSurface == new_surface) {
    pthread_mutex_unlock(&stateLock);
    return;
  }

  if (pendingSurface)
    (*env)->DeleteGlobalRef(env, pendingSurface);

  pendingSurface = new_surface;
#endif
  windowChanged = TRUE;

  // We are not sure which conditional variable is used at current moment so
  // let's signal both
  if (state)
    pthread_cond_signal(&state->cond);
  pthread_cond_signal(&stateCond);

  pthread_mutex_unlock(&stateLock);
}

#if !RENDERER_IN_ACTIVITY
jobject renderer_get_surface(void) {
  pthread_mutex_lock(&stateLock);
  jobject res = surface;
  pthread_mutex_unlock(&stateLock);
  return res;
}
#endif

#if RENDERER_IN_ACTIVITY
static inline __always_inline void
release_win_and_surface(JNIEnv *env, ANativeWindow **anw, EGLSurface *esfc) {
#else
static inline __always_inline void release_win_and_surface(JNIEnv *env,
                                                           jobject *jsfc,
                                                           ANativeWindow **anw,
                                                           EGLSurface *esfc) {
#endif
  if (esfc && *esfc) {
    if (eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT) != EGL_TRUE)
      return vprintEglError("eglMakeCurrent failed (EGL_NO_SURFACE)", __LINE__);
    if (eglDestroySurface(egl_display, *esfc) != EGL_TRUE)
      return vprintEglError("eglDestoySurface failed", __LINE__);
    *esfc = EGL_NO_SURFACE;
  }

  if (anw && *anw) {
    ANativeWindow_release(*anw);
    *anw = NULL;
  }

#if !RENDERER_IN_ACTIVITY
  if (jsfc && *jsfc) {
    (*env)->CallVoidMethod(env, *jsfc, Surface_release);
    (*env)->CallVoidMethod(env, *jsfc, Surface_destroy);
    (*env)->DeleteGlobalRef(env, *jsfc);
    *jsfc = NULL;
  }
#endif
}

void renderer_refresh_context(JNIEnv *env) {
  uint32_t emptyData = {0};
#if !RENDERER_IN_ACTIVITY
  ANativeWindow *pendingWin =
      pendingSurface ? ANativeWindow_fromSurface(env, pendingSurface) : NULL;
  if ((pendingSurface && surface && pendingSurface != surface &&
       (*env)->IsSameObject(env, pendingSurface, surface)) ||
      (pendingWin && win == pendingWin)) {
    (*env)->DeleteGlobalRef(env, pendingSurface);
    pendingSurface = NULL;
    windowChanged = FALSE;
    return;
  }
#endif
  int width = pendingWin ? ANativeWindow_getWidth(pendingWin) : 0;
  int height = pendingWin ? ANativeWindow_getHeight(pendingWin) : 0;
  log("renderer_set_window %p %d %d", pendingWin, width, height);

#if RENDERER_IN_ACTIVITY
  release_win_and_surface(env, &win, &sfc);
#else
  if (pendingWin)
    ANativeWindow_acquire(pendingWin);

  release_win_and_surface(env, &surface, &win, &sfc);
#endif

  if (pendingWin && (width <= 0 || height <= 0)) {
    log("Xlorie: We've got invalid surface. Probably it became invalid before "
        "we started working with it.\n");
#if RENDERER_IN_ACTIVITY
    release_win_and_surface(env, &pendingWin, NULL);
#else
    release_win_and_surface(env, &pendingSurface, &pendingWin, NULL);
#endif
  }

  win = pendingWin;
#if RENDERER_IN_ACTIVITY
  pendingWin = NULL;
#else
  surface = pendingSurface;
  pendingSurface = NULL;
#endif
  windowChanged = FALSE;

  if (!win) {
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (state)
      state->surfaceAvailable = false;
    return;
  }

  sfc = eglCreateWindowSurface(egl_display, cfg, win, NULL);
  if (sfc == EGL_NO_SURFACE)
    return vprintEglError("eglCreateWindowSurface failed", __LINE__);

  if (eglMakeCurrent(egl_display, sfc, sfc, ctx) != EGL_TRUE) {
    if (state)
      state->surfaceAvailable = false;
    return vprintEglError("eglMakeCurrent failed", __LINE__);
  }

  eglSwapInterval(egl_display, 0); // Force VSync OFF for maximum FPS


  if (state)
    // We should redraw image at least once right after surface change
    state->surfaceAvailable = state->drawRequested = state->cursor.updated =
        true;

  if (!g_texture_program) {
    g_texture_program = create_program(vertex_shader, fragment_shader);
    if (!g_texture_program) {
      log("Xlorie: GLESv2: Unable to create shader program.\n");
      return;
    }

    g_texture_program_bgra =
        create_program(vertex_shader, fragment_shader_bgra);
    if (!g_texture_program_bgra) {
      log("Xlorie: GLESv2: Unable to create bgra shader program.\n");
      return;
    }

    gv_pos = (GLuint)glGetAttribLocation(g_texture_program, "position");
    gv_coords = (GLuint)glGetAttribLocation(g_texture_program, "texCoords");

    gv_pos_bgra = (GLuint)glGetAttribLocation(g_texture_program_bgra, "position");
    gv_coords_bgra = (GLuint)glGetAttribLocation(g_texture_program_bgra, "texCoords");

    gu_color_profile = (GLuint)glGetUniformLocation(g_texture_program, "colorProfile");
    gu_color_profile_bgra = (GLuint)glGetUniformLocation(g_texture_program_bgra, "colorProfile");

    g_fsr_program = create_program(vertex_shader, fsr_fragment_shader);
    if (g_fsr_program) {
      gv_pos_fsr = (GLuint)glGetAttribLocation(g_fsr_program, "position");
      gv_coords_fsr = (GLuint)glGetAttribLocation(g_fsr_program, "texCoords");
      gu_src_size_fsr = (GLuint)glGetUniformLocation(g_fsr_program, "srcSize");
      gu_color_profile_fsr = (GLuint)glGetUniformLocation(g_fsr_program, "colorProfile");
    }

    g_fsr_program_bgra = create_program(vertex_shader, fsr_fragment_shader_bgra);
    if (g_fsr_program_bgra) {
      gv_pos_fsr_bgra = (GLuint)glGetAttribLocation(g_fsr_program_bgra, "position");
      gv_coords_fsr_bgra = (GLuint)glGetAttribLocation(g_fsr_program_bgra, "texCoords");
      gu_src_size_fsr_bgra = (GLuint)glGetUniformLocation(g_fsr_program_bgra, "srcSize");
      gu_color_profile_fsr_bgra = (GLuint)glGetUniformLocation(g_fsr_program_bgra, "colorProfile");
    }

    g_cas_program = create_program(vertex_shader, cas_fragment_shader);
    if (g_cas_program) {
      gv_pos_cas = (GLuint)glGetAttribLocation(g_cas_program, "position");
      gv_coords_cas = (GLuint)glGetAttribLocation(g_cas_program, "texCoords");
      gu_src_size_cas = (GLuint)glGetUniformLocation(g_cas_program, "srcSize");
      gu_color_profile_cas = (GLuint)glGetUniformLocation(g_cas_program, "colorProfile");
    }

    g_cas_program_bgra = create_program(vertex_shader, cas_fragment_shader_bgra);
    if (g_cas_program_bgra) {
      gv_pos_cas_bgra = (GLuint)glGetAttribLocation(g_cas_program_bgra, "position");
      gv_coords_cas_bgra = (GLuint)glGetAttribLocation(g_cas_program_bgra, "texCoords");
      gu_src_size_cas_bgra = (GLuint)glGetUniformLocation(g_cas_program_bgra, "srcSize");
      gu_color_profile_cas_bgra = (GLuint)glGetUniformLocation(g_cas_program_bgra, "colorProfile");
    }

    g_interpolation_program = create_program(vertex_shader, interpolation_fragment_shader);
    if (g_interpolation_program) {
      gv_pos_interp = (GLuint)glGetAttribLocation(g_interpolation_program, "position");
      gv_coords_interp = (GLuint)glGetAttribLocation(g_interpolation_program, "texCoords");
      gu_alpha_interp = (GLuint)glGetUniformLocation(g_interpolation_program, "alpha");
      gu_last_tex_interp = (GLuint)glGetUniformLocation(g_interpolation_program, "lastTexture");
      gu_color_profile_interp = (GLuint)glGetUniformLocation(g_interpolation_program, "colorProfile");
    }

    g_interpolation_program_bgra = create_program(vertex_shader, interpolation_fragment_shader_bgra);
    if (g_interpolation_program_bgra) {
      gv_pos_interp_bgra = (GLuint)glGetAttribLocation(g_interpolation_program_bgra, "position");
      gv_coords_interp_bgra = (GLuint)glGetAttribLocation(g_interpolation_program_bgra, "texCoords");
      gu_alpha_interp_bgra = (GLuint)glGetUniformLocation(g_interpolation_program_bgra, "alpha");
      gu_last_tex_interp_bgra = (GLuint)glGetUniformLocation(g_interpolation_program_bgra, "lastTexture");
      gu_color_profile_interp_bgra = (GLuint)glGetUniformLocation(g_interpolation_program_bgra, "colorProfile");
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &display.id);
    bindLinearTexture(display.id);  // Set params once at creation

    // Double buffering: criar duas texturas para cursor (params setados uma vez)
    glGenTextures(2, cursor.ids);
    for (int i = 0; i < 2; i++) {
      bindLinearTexture(cursor.ids[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &emptyData);
    }
    cursor.currentIndex = 0;
    cursor.pendingIndex = 1;
    cursor.needsUpload = false;

    // Create static VBO for the full-screen quad — uploaded once, reused every frame.
    // Coords: {posX, posY, texU, texV} * 4 vertices (TRIANGLE_STRIP)
    if (!quad_vbo) {
      static const float quad_data[16] = {
        -1.f,  1.f,  0.f, 0.f,   // top-left
         1.f,  1.f,  1.f, 0.f,   // top-right
        -1.f, -1.f,  0.f, 1.f,   // bottom-left
         1.f, -1.f,  1.f, 1.f,   // bottom-right
      };
      glGenBuffers(1, &quad_vbo);
      glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
  }

  glViewport(0, 0, ANativeWindow_getWidth(win), ANativeWindow_getHeight(win));
  log("Xlorie: new surface applied: %p\n", sfc);

  bindTexture(display.id);  // Params already set at creation
  
  if (image)
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
  else {
    LorieBuffer_describe(buffer, &display.desc);

    if (display.desc.data && display.desc.width > 0 &&
        display.desc.height > 0) {
      int format = display.desc.format == AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM
                       ? GL_BGRA_EXT
                       : GL_RGBA;
      // The image will be updated in redraw call because of `drawRequested`
      // flag, so we are not uploading pixels
      glTexImage2D(GL_TEXTURE_2D, 0, format, display.desc.width,
                   display.desc.height, 0, format, GL_UNSIGNED_BYTE, NULL);
    } else {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, &emptyData);
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
    }
  }
}

static void draw(GLuint id, float x0, float y0, float x1, float y1,
                 uint8_t flip, float alpha);
static void draw_cursor(void);

static void renderer_renew_image(void) {
  const EGLint imageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
                                    EGL_NONE};
  uint32_t emptyData = {0};

  if (image)
    eglDestroyImageKHR(egl_display, image);
  if (buffer)
    LorieBuffer_release(buffer);

  buffer = pendingBuffer;
  pendingBuffer = NULL;
  bufferChanged = false;
  image = NULL;

  LorieBuffer_describe(buffer, &display.desc);

  if (display.desc.buffer)
    image = eglCreateImageKHR(
        egl_display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
        eglGetNativeClientBufferANDROID(display.desc.buffer), imageAttributes);

  if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
    if (state)
      // We should redraw image at least once right after buffer change
      state->surfaceAvailable = state->drawRequested = state->cursor.updated =
          true;

    if (local_display_id) {
      glDeleteTextures(1, &local_display_id);
      local_display_id = 0;
      local_display_width = 0;
      local_display_height = 0;
    }

    bindLinearTexture(display.id);
    if (image)
      glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    else if (display.desc.data && display.desc.width > 0 &&
             display.desc.height > 0) {
      int format = display.desc.format == AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM
                       ? GL_BGRA_EXT
                       : GL_RGBA;
      // The image will be updated in redraw call because of `drawRequested`
      // flag, so we are not uploading pixels
      glTexImage2D(GL_TEXTURE_2D, 0, format, display.desc.width,
                   display.desc.height, 0, format, GL_UNSIGNED_BYTE, NULL);
    } else {
      loge("There is no %s, nothing to be bound.",
           !buffer ? "AHardwareBuffer" : "EGLImage");
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, &emptyData);
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
    }
  }

  log("renderer: buffer changed %p %d %d", buffer, display.desc.width,
      display.desc.height);
}

void renderer_redraw_locked(JNIEnv *env) {
  EGLSync fence;
  int err = EGL_SUCCESS;
  bool unlocked_early = false;

  static int current_vsync = -1;
  // VSync is forced to 0 in refresh_context for maximum FPS


  // We should signal X server to not use root window while we actively copy use
  // it
  lorie_mutex_lock(&state->lock, &state->lockingPid);

  if (renderer_frameskip > 0) {
    if (renderer_frameskip_counter < renderer_frameskip) {
      renderer_frameskip_counter++;
      state->drawRequested = FALSE;
      state->cursor.updated = false;
      state->cursor.moved = FALSE;
      lorie_mutex_unlock(&state->lock, &state->lockingPid);
      return;
    }
    renderer_frameskip_counter = 0;
  }
  // Non-null display.desc.data means we have root window created in legacy
  // drawing mode so we should update it on each frame.
  if (display.desc.data && state->drawRequested) {
    state->drawRequested = FALSE;
    bindLinearTexture(display.id);
    GLenum format = display.desc.format == AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM
                        ? GL_BGRA_EXT
                        : GL_RGBA;

    if (gles_version == 3 && pbo) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
      glBufferData(GL_PIXEL_UNPACK_BUFFER,
                   display.desc.width * display.desc.height * 4,
                   display.desc.data, GL_STREAM_DRAW);

      // PBO update is zero-copy (DMA-like) or fast copy. We can release the
      // lock now.
      lorie_mutex_unlock(&state->lock, &state->lockingPid);
      unlocked_early = true;

      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, display.desc.width,
                      display.desc.height, format, GL_UNSIGNED_BYTE, 0);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    } else {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, display.desc.width,
                      display.desc.height, format, GL_UNSIGNED_BYTE,
                      display.desc.data);
    }
  } else if (!display.desc.data && state->drawRequested) {
    state->drawRequested = FALSE;

    if (!local_display_id || local_display_width != display.desc.width || local_display_height != display.desc.height) {
      if (local_display_id) glDeleteTextures(1, &local_display_id);
      glGenTextures(1, &local_display_id);
      bindLinearTexture(local_display_id);  // bindLinearTexture = bind + set params (creation site)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, display.desc.width, display.desc.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      local_display_width = display.desc.width;
      local_display_height = display.desc.height;
    }

    if (!copy_fbo) {
      glGenFramebuffers(1, &copy_fbo);
    }
    if (!dst_fbo) {
      glGenFramebuffers(1, &dst_fbo);  // Create once, reuse forever
    }

#if defined(GL_ES_VERSION_3_0)
    if (gles_version >= 3) {
      /* glBlitFramebuffer is a direct GPU-to-GPU blit — much faster than
       * glCopyTexSubImage2D which reads back through the framebuffer. */
      // Source: display.id
      glBindFramebuffer(GL_READ_FRAMEBUFFER, copy_fbo);
      glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, display.id, 0);

      // Destination: local_display_id (reusing pooled dst_fbo)
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, local_display_id, 0);

      glBlitFramebuffer(0, 0, (GLint)display.desc.width, (GLint)display.desc.height,
                        0, 0, (GLint)display.desc.width, (GLint)display.desc.height,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else
#endif
    {
      // GLES 2 fallback: glCopyTexSubImage2D via read framebuffer
      glBindFramebuffer(GL_FRAMEBUFFER, copy_fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, display.id, 0);
      bindTexture(local_display_id);
      glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, display.desc.width, display.desc.height);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Flush and unlock X server early
    glFlush();
    lorie_mutex_unlock(&state->lock, &state->lockingPid);
    unlocked_early = true;
  }

  // Not a mistake, we reset drawRequested flag even in the case if there is no
  // legacy drawing.
  if (!unlocked_early)
    state->drawRequested = FALSE;

  int win_width = ANativeWindow_getWidth(win);
  int win_height = ANativeWindow_getHeight(win);

  GLuint active_tex_id = (display.desc.data || !local_display_id) ? display.id : local_display_id;

  // Skip frame generation if not needed or if GPU is under high load
  bool use_frame_generation = (frame_generation == 1 && last_frame.id && 
                               last_frame.width == win_width && last_frame.height == win_height &&
                               !state->cursor.updated && !state->cursor.moved &&
                               perf.gpuLoadLevel < 2);  // Disable under high GPU load

  if (use_frame_generation) {
    // Draw interpolated frame (50% previous, 50% current)
    draw(active_tex_id, -1.f, -1.f, 1.f, 1.f,
         display.desc.format != AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM, 0.5f);
    eglSwapBuffers(egl_display, sfc);
    state->renderedFrames++;
  }

  // Draw current frame
  draw(active_tex_id, -1.f, -1.f, 1.f, 1.f,
       display.desc.format != AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM, 1.0f);

  // Sync fence created, but we will not wait FOREVER to avoid blocking the CPU
  fence = eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);


  // Double buffering otimizado para cursor - só faz upload se realmente mudou
  if (state->cursor.updated) {
    lorie_mutex_lock(&state->cursor.lock, &state->cursor.lockingPid);
    
    uint32_t newHash = 0;
    int cursorW = state->cursor.width;
    int cursorH = state->cursor.height;
    
    // Calcular hash do conteúdo do cursor
    if (cursorW > 0 && cursorH > 0 && cursorW <= 512 && cursorH <= 512) {
      newHash = hashCursorBits(state->cursor.bits, cursorW, cursorH);
    }
    
    // Só faz upload se o conteúdo realmente mudou ou se dimensões diferentes
    bool contentChanged = (newHash != cursor.lastHash) || 
                          (cursorW != cursor.lastWidth) || 
                          (cursorH != cursor.lastHeight);
    
    if (contentChanged) {
      // Alternar entre as duas texturas (double buffering)
      int uploadIndex = cursor.pendingIndex;
      cursor.pendingIndex = cursor.currentIndex;
      cursor.currentIndex = uploadIndex;
      
      bindLinearTexture(cursor.ids[uploadIndex]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)cursorW,
                   (GLsizei)cursorH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   state->cursor.bits);
      
      cursor.lastHash = newHash;
      cursor.lastWidth = cursorW;
      cursor.lastHeight = cursorH;
      cursor.needsUpload = false;
      perf.cursorUploads++;
    }
    
    state->cursor.updated = false;
    lorie_mutex_unlock(&state->cursor.lock, &state->cursor.lockingPid);
  }

  state->cursor.moved = FALSE;
  
  // Only draw cursor if it's visible and has valid dimensions
  if (state->cursor.width > 0 && state->cursor.height > 0) {
    draw_cursor();
  }


  // Copy current frame to last_frame for next iteration
  // OPTIMIZATION: Use GPU-to-GPU copy instead of expensive readback
  if (frame_generation == 1 && use_frame_generation) {
    if (!last_frame.id || last_frame.width != win_width || last_frame.height != win_height) {
      if (last_frame.id) glDeleteTextures(1, &last_frame.id);
      glGenTextures(1, &last_frame.id);
      bindLinearTexture(last_frame.id);  // bindLinearTexture = bind + set params (creation site)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, win_width, win_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      last_frame.width = win_width;
      last_frame.height = win_height;
    }
    
    // Use GPU-to-GPU blit instead of readback when possible
    if (gles_version >= 3 && copy_fbo && dst_fbo) {
      // Source: default framebuffer (current rendered frame)
      glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);  // Default framebuffer
      // Destination: last_frame texture via FBO
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, last_frame.id, 0);
 glBlitFramebuffer(0, 0, win_width, win_height,
                        0, 0, win_width, win_height,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
      // Fallback to readback (expensive but necessary for GLES 2)
      bindTexture(last_frame.id);
      glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, win_width, win_height);
    }
  }

  // Wait until root window drawing is finished before giving control back to X
  // server
  // Using a short timeout instead of EGL_FOREVER to prevent CPU stalls
  if (!unlocked_early) {
    /* Wait up to 2ms for the GPU to finish before releasing the X server lock.
     * Timeout 0 returns immediately (no sync), EGL_FOREVER can stall the CPU.
     * 2ms is enough for a simple blit frame on any mobile GPU. */
    eglClientWaitSyncKHR(egl_display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 2000000ULL);
    lorie_mutex_unlock(&state->lock, &state->lockingPid);
  }

  eglDestroySyncKHR(egl_display, fence);

  if (eglSwapBuffers(egl_display, sfc) != EGL_TRUE) {
    printEglError("Failed to swap buffers", __LINE__);
    err = eglGetError();
    if (err == EGL_BAD_NATIVE_WINDOW || err == EGL_BAD_SURFACE) {
      log("The window is to be destroyed. Native window "
          "disconnected/abandoned, probably activity is destroyed or in "
          "background");
#if RENDERER_IN_ACTIVITY
      renderer_set_window(NULL);
#else
      renderer_set_window(env, NULL);
#endif
      // If we unlocked early, we don't own the lock here. But window lost
      // usually implies catastrophic failure anyway. Safe to check
      // unlocked_early flag or just re-lock/check owner? Actually original code
      // unlocked unconditionally. Here we must be careful. If we unlocked
      // early, we can't unlock again.
      if (!unlocked_early)
        lorie_mutex_unlock(&state->lock, &state->lockingPid);
    }
  }

  state->renderedFrames++;
  
  // Atualizar performance counters
  updatePerfCounters();
  
  // Log de performance reduzido - apenas a cada 300 frames (~5 segundos a 60fps)
  if ((perf.framesRendered % 300) == 0) {
    log("Perf: avg=%.2fms gpuLoad=%d cursor=%lu frames=%lu",
        perf.avgFrameTimeMs, perf.gpuLoadLevel, (unsigned long)perf.cursorUploads, (unsigned long)perf.framesRendered);
  }
}

static inline __always_inline bool renderer_should_wait(void) {
  if (stateChanged || windowChanged || bufferChanged)
    // If there are pending changes we should process them immediately.
    return false;

  if (!state || !state->surfaceAvailable)
    // Even in the case if there are pending changes, we can not draw it without
    // rendering surface
    return true;

  if (state->drawRequested || state->cursor.moved || state->cursor.updated)
    // X server reported drawing or cursor changes, no need to wait.
    return false;

  // Probably spurious wake, no changes we can work with.
  return true;
}

__noreturn static void *renderer_thread(void *closure) {
  JavaVM *vm = closure;
  JNIEnv *env;
  (*vm)->AttachCurrentThread(vm, &env, NULL);

  // Set high priority for the renderer thread to ensure smooth frame delivery
  // -10 is a good balance for a high-priority display thread.
  setpriority(PRIO_PROCESS, 0, -10);

  pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_init(&m, NULL);
  // Mutex is needed only for pthread_cond_wait, it is needed only to make
  // thread sleep when it is idle. We check for all event that may change in
  // between so we should not miss any events.
  pthread_mutex_lock(&m);

  while (true) {
    while (renderer_should_wait())
      pthread_cond_wait(state ? &state->cond : &stateCond, &m);

    pthread_mutex_lock(&stateLock);
    if (stateChanged) {
      if (state && pendingState != state)
        munmap(state, sizeof(*state));

      state = pendingState;
      pendingState = NULL;
      stateChanged = false;
    }

    if (windowChanged)
      renderer_refresh_context(env);

    if (bufferChanged)
      renderer_renew_image();
    pthread_mutex_unlock(&stateLock);

    if (state && state->surfaceAvailable && (state->vsyncOff || !state->waitForNextFrame) &&
        (state->drawRequested || state->cursor.moved || state->cursor.updated))

      renderer_redraw_locked(env);
  }
}

static GLuint load_shader(GLenum shaderType, const char *pSource) {
  GLint compiled = 0, infoLen = 0;
  GLuint shader = glCreateShader(shaderType);
  if (!shader)
    return 0;

  glShaderSource(shader, 1, &pSource, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled)
    return shader;

  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
  if (infoLen) {
    char buf[infoLen];
    glGetShaderInfoLog(shader, infoLen, NULL, buf);
    log("renderer: Could not compile shader %d:\n%s\n", shaderType, buf);
  }
  glDeleteShader(shader);

  return 0;
}

static GLuint create_program(const char *p_vertex_source,
                             const char *p_fragment_source) {
  GLuint program, vertexShader, pixelShader;
  GLint linkStatus = GL_FALSE, bufLength = 0;
  vertexShader = load_shader(GL_VERTEX_SHADER, p_vertex_source);
  pixelShader = load_shader(GL_FRAGMENT_SHADER, p_fragment_source);
  if (!pixelShader || !vertexShader) {
    return 0;
  }

  program = glCreateProgram();
  if (!program)
    return 0;

  glAttachShader(program, vertexShader);
  glAttachShader(program, pixelShader);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
  if (linkStatus == GL_TRUE)
    return program;

  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
  if (bufLength) {
    char buf[bufLength];
    glGetProgramInfoLog(program, bufLength, NULL, buf);
    log("renderer: Could not link program:\n%s\n", buf);
  }
  glDeleteProgram(program);

  return 0;
}

static void draw(GLuint id, float x0, float y0, float x1, float y1,
                 uint8_t flip, float alpha) {
  /* Use the static VBO when rendering the full-screen quad (fixed coords -1,-1,1,1).
   * For other quads (e.g. cursor), fall back to immediate vertex data. */
  const bool is_fullscreen_quad = (x0 == -1.f && y0 == -1.f && x1 == 1.f && y1 == 1.f);

  float coords[16];
  if (!is_fullscreen_quad) {
    /* Cursor or custom quad — build vertex data on the stack */
    coords[0]  = x0; coords[1]  = -y0; coords[2]  = 0.f; coords[3]  = 0.f;
    coords[4]  = x1; coords[5]  = -y0; coords[6]  = 1.f; coords[7]  = 0.f;
    coords[8]  = x0; coords[9]  = -y1; coords[10] = 0.f; coords[11] = 1.f;
    coords[12] = x1; coords[13] = -y1; coords[14] = 1.f; coords[15] = 1.f;
  }

  GLuint p = flip ? gv_pos_bgra : gv_pos, c = flip ? gv_coords_bgra : gv_coords;
  GLuint prog = flip ? g_texture_program_bgra : g_texture_program;

  if (id == display.id || (local_display_id && id == local_display_id)) {
    if (frame_generation == 1 && alpha > 0.0f && g_interpolation_program) {
      prog = g_interpolation_program;
      p = gv_pos_interp;
      c = gv_coords_interp;
    } else if (scaling_filter == 1 && perf.gpuLoadLevel < 2) {  // Disable FSR under high GPU load
      if (flip && g_fsr_program_bgra) {
        prog = g_fsr_program_bgra;
        p = gv_pos_fsr_bgra;
        c = gv_coords_fsr_bgra;
      } else if (g_fsr_program) {
        prog = g_fsr_program;
        p = gv_pos_fsr;
        c = gv_coords_fsr;
      }
    } else if (scaling_filter == 2 && perf.gpuLoadLevel < 2) {  // Disable CAS under high GPU load
      if (flip && g_cas_program_bgra) {
        prog = g_cas_program_bgra;
        p = gv_pos_cas_bgra;
        c = gv_coords_cas_bgra;
      } else if (g_cas_program) {
        prog = g_cas_program;
        p = gv_pos_cas;
        c = gv_coords_cas;
      }
    }
  }

  glActiveTexture(GL_TEXTURE0);
  glUseProgram(prog);
  glBindTexture(GL_TEXTURE_2D, id);

  GLuint cp = prog == g_texture_program ? gu_color_profile :
              prog == g_texture_program_bgra ? gu_color_profile_bgra :
              prog == g_fsr_program ? gu_color_profile_fsr :
              prog == g_fsr_program_bgra ? gu_color_profile_fsr_bgra :
              prog == g_cas_program ? gu_color_profile_cas :
              prog == g_cas_program_bgra ? gu_color_profile_cas_bgra :
              prog == g_interpolation_program ? gu_color_profile_interp :
              prog == g_interpolation_program_bgra ? gu_color_profile_interp_bgra : 0;

  if (cp) {
      glUniform1i(cp, color_profile);
  }

  if (id == display.id || (local_display_id && id == local_display_id)) {
    if (frame_generation == 1 && alpha < 1.0f) {
      if (flip && g_interpolation_program_bgra) {
        glUniform1f(gu_alpha_interp_bgra, alpha);
        glUniform1i(gu_last_tex_interp_bgra, 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, last_frame.id);
        glActiveTexture(GL_TEXTURE0);
      } else if (g_interpolation_program) {
        glUniform1f(gu_alpha_interp, alpha);
        glUniform1i(gu_last_tex_interp, 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, last_frame.id);
        glActiveTexture(GL_TEXTURE0);
      }
    } else if (scaling_filter == 1) {
      if (flip && g_fsr_program_bgra) {
        glUniform4f(gu_src_size_fsr_bgra, 1.0f / (float)display.desc.width,
                    1.0f / (float)display.desc.height, (float)display.desc.width,
                    (float)display.desc.height);
      } else if (g_fsr_program) {
        glUniform4f(gu_src_size_fsr, 1.0f / (float)display.desc.width,
                    1.0f / (float)display.desc.height, (float)display.desc.width,
                    (float)display.desc.height);
      }
    } else if (scaling_filter == 2) {
      if (flip && g_cas_program_bgra) {
        glUniform4f(gu_src_size_cas_bgra, 1.0f / (float)display.desc.width,
                    1.0f / (float)display.desc.height, (float)display.desc.width,
                    (float)display.desc.height);
      } else if (g_cas_program) {
        glUniform4f(gu_src_size_cas, 1.0f / (float)display.desc.width,
                    1.0f / (float)display.desc.height, (float)display.desc.width,
                    (float)display.desc.height);
      }
    }
  }

  if (is_fullscreen_quad && quad_vbo) {
    /* Static VBO path: no per-frame upload, just pointer into the resident buffer */
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glVertexAttribPointer(p, 2, GL_FLOAT, GL_FALSE, 16, (void *)0);
    glVertexAttribPointer(c, 2, GL_FLOAT, GL_FALSE, 16, (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(p);
    glEnableVertexAttribArray(c);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  } else {
    /* Immediate path for cursor / dynamic quads */
    glVertexAttribPointer(p, 2, GL_FLOAT, GL_FALSE, 16, coords);
    glVertexAttribPointer(c, 2, GL_FLOAT, GL_FALSE, 16, &coords[2]);
    glEnableVertexAttribArray(p);
    glEnableVertexAttribArray(c);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  checkGlError();
}

__unused static void draw_cursor(void) {
  float x, y, w, h;

  if (!state->cursor.width || !state->cursor.height)
    return;

  x = 2.f * ((float)state->cursor.x - (float)state->cursor.xhot) /
          (float)display.desc.width -
      1.f;
  y = 2.f * ((float)state->cursor.y - (float)state->cursor.yhot) /
          (float)display.desc.height -
      1.f;
  w = 2.f * (float)state->cursor.width / (float)display.desc.width;
  h = 2.f * (float)state->cursor.height / (float)display.desc.height;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // Usar textura atual do double buffering
  draw(cursor.ids[cursor.currentIndex], x, y, x + w, y + h, false, 1.0f);
  glDisable(GL_BLEND);
}

JNIEXPORT void JNICALL
Java_com_micewine_emu_LorieView_setColorProfile(JNIEnv *env, jobject thiz, jint profile) {
  renderer_set_color_profile(profile);
}

JNIEXPORT void JNICALL
Java_com_micewine_emu_LorieView_setFrameSkip(JNIEnv *env, jobject thiz, jint skip) {
  renderer_set_frameskip(skip);
}
