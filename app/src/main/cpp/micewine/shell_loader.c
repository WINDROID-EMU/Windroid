#define _GNU_SOURCE
#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define log(prio, ...)                                                         \
  __android_log_print(ANDROID_LOG_##prio, "ShellLoaderNative", __VA_ARGS__)

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp)                                                \
  ({                                                                           \
    __typeof__(exp) _rc;                                                       \
    do {                                                                       \
      _rc = (exp);                                                             \
    } while (_rc == -1 && errno == EINTR);                                     \
    _rc;                                                                       \
  })
#endif

typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} ShellBuffer;

static void bufferInit(ShellBuffer *buf) {
  buf->capacity = 4096;
  buf->data = malloc(buf->capacity);
  buf->size = 0;
  if (buf->data)
    buf->data[0] = '\0';
}

static void bufferAppend(ShellBuffer *buf, const char *text, size_t len) {
  if (!buf->data)
    return;
  if (buf->size + len + 1 > buf->capacity) {
    size_t new_cap = buf->capacity * 2;
    while (buf->size + len + 1 > new_cap)
      new_cap *= 2;
    char *new_data = realloc(buf->data, new_cap);
    if (new_data) {
      buf->data = new_data;
      buf->capacity = new_cap;
    } else {
      return;
    }
  }
  memcpy(buf->data + buf->size, text, len);
  buf->size += len;
  buf->data[buf->size] = '\0';
}

static void bufferFree(ShellBuffer *buf) { free(buf->data); }

static void sanitizeUTF8(char *s) {
  if (!s)
    return;
  unsigned char *p = (unsigned char *)s;
  while (*p) {
    if ((*p & 0x80) == 0x00) {
      p++;
    } else if ((*p & 0xe0) == 0xc0) {
      if (p[1] && (p[1] & 0xc0) == 0x80)
        p += 2;
      else {
        *p = '?';
        p++;
      }
    } else if ((*p & 0xf0) == 0xe0) {
      if (p[1] && (p[1] & 0xc0) == 0x80 && p[2] && (p[2] & 0xc0) == 0x80)
        p += 3;
      else {
        *p = '?';
        p++;
      }
    } else if ((*p & 0xf8) == 0xf0) {
      if (p[1] && (p[1] & 0xc0) == 0x80 && p[2] && (p[2] & 0xc0) == 0x80 &&
          p[3] && (p[3] & 0xc0) == 0x80)
        p += 4;
      else {
        *p = '?';
        p++;
      }
    } else {
      *p = '?';
      p++;
    }
  }
}

static jobject callbackInstance = NULL;
static jmethodID appendLogsMethodID = NULL;

static jmethodID addToSessionLogsMethodID = NULL;

void appendLog(JNIEnv *env, const char *text) {
  if (callbackInstance != NULL && appendLogsMethodID != NULL) {
    jstring jText = (*env)->NewStringUTF(env, text);
    (*env)->CallVoidMethod(env, callbackInstance, appendLogsMethodID, jText);
    (*env)->DeleteLocalRef(env, jText);
  }

  if (addToSessionLogsMethodID != NULL) {
    jclass cls = (*env)->FindClass(env, "com/windroid/emu/core/ShellLoader");
    jstring jText = (*env)->NewStringUTF(env, text);
    (*env)->CallStaticVoidMethod(env, cls, addToSessionLogsMethodID, jText);
    (*env)->DeleteLocalRef(env, jText);
    (*env)->DeleteLocalRef(env, cls);
  }
}

JNIEXPORT void JNICALL Java_com_windroid_emu_core_ShellLoader_connectOutput(
    JNIEnv *env, jobject __unused cls, jobject callback) {
  if (callbackInstance != NULL) {
    (*env)->DeleteGlobalRef(env, callbackInstance);
  }
  callbackInstance = (*env)->NewGlobalRef(env, callback);

  jclass class = (*env)->GetObjectClass(env, callbackInstance);
  appendLogsMethodID =
      (*env)->GetMethodID(env, class, "appendLogs", "(Ljava/lang/String;)V");

  jclass shellLoaderCls = (*env)->FindClass(env, "com/windroid/emu/core/ShellLoader");
  addToSessionLogsMethodID = (*env)->GetStaticMethodID(env, shellLoaderCls, "addToSessionLogs", "(Ljava/lang/String;)V");

  (*env)->DeleteLocalRef(env, shellLoaderCls);
  (*env)->DeleteLocalRef(env, cls);
}

JNIEXPORT void JNICALL Java_com_windroid_emu_core_ShellLoader_cleanup(
    JNIEnv *env, __unused jobject cls) {
  if (callbackInstance != NULL) {
    (*env)->DeleteGlobalRef(env, callbackInstance);
  }

  callbackInstance = NULL;
  appendLogsMethodID = NULL;
}

JNIEXPORT jint JNICALL Java_com_windroid_emu_core_ShellLoader_runCommand(
    JNIEnv *env, __unused jobject cls, jstring command, jboolean log) {
  const char *parsedCommand;
  int pipe_in[2];
  int pipe_out[2];
  pid_t pid;

  parsedCommand = (*env)->GetStringUTFChars(env, command, NULL);

  if (log == JNI_TRUE) {
    log(DEBUG, "Trying to exec '%s'", parsedCommand);
  }

  if (pipe2(pipe_in, O_CLOEXEC) == -1 || pipe2(pipe_out, O_CLOEXEC) == -1) {
    perror("pipe2");
    (*env)->ReleaseStringUTFChars(env, command, parsedCommand);
    return -1;
  }

  pid = fork();
  if (pid == -1) {
    perror("fork");
    (*env)->ReleaseStringUTFChars(env, command, parsedCommand);
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    return -1;
  }

  if (pid == 0) {
    close(pipe_in[1]);
    close(pipe_out[0]);

    dup2(pipe_in[0], STDIN_FILENO);
    close(pipe_in[0]);

    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_out[1], STDERR_FILENO);
    close(pipe_out[1]);

    execl("/system/bin/sh", "sh", NULL);

    perror("execl");
    exit(EXIT_FAILURE);
  } else {
    close(pipe_in[0]);
    close(pipe_out[1]);

    const char *terminator = "\nexit\n";
    size_t cmd_len = strlen(parsedCommand);
    size_t term_len = strlen(terminator);
    size_t size = cmd_len + term_len;

    char *fullCmd = malloc(size + 1);
    if (fullCmd) {
      snprintf(fullCmd, size + 1, "%s%s", parsedCommand, terminator);
      TEMP_FAILURE_RETRY(write(pipe_in[1], fullCmd, size));
      free(fullCmd);
    }
    close(pipe_in[1]);

    char buffer[4096];
    ssize_t n;

    while ((n = TEMP_FAILURE_RETRY(
                read(pipe_out[0], buffer, sizeof(buffer) - 1))) > 0) {
      if (log == JNI_TRUE) {
        buffer[n] = '\0';
        sanitizeUTF8(buffer);
        log(DEBUG, "%s", buffer);
        appendLog(env, buffer);
      }
    }

    int status;
    close(pipe_out[0]);
    TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));

    (*env)->ReleaseStringUTFChars(env, command, parsedCommand);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
  }
}

JNIEXPORT jstring JNICALL
Java_com_windroid_emu_core_ShellLoader_runCommandWithOutput(
    JNIEnv *env, __unused jobject cls, jstring command, jboolean stdErrLog) {
  const char *parsedCommand;
  int pipe_in[2];
  int pipe_out[2];
  pid_t pid;

  parsedCommand = (*env)->GetStringUTFChars(env, command, NULL);

  if (pipe2(pipe_in, O_CLOEXEC) == -1 || pipe2(pipe_out, O_CLOEXEC) == -1) {
    perror("pipe2");
    (*env)->ReleaseStringUTFChars(env, command, parsedCommand);
    return (*env)->NewStringUTF(env, "");
  }

  pid = fork();
  if (pid == -1) {
    perror("fork");
    (*env)->ReleaseStringUTFChars(env, command, parsedCommand);
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    return (*env)->NewStringUTF(env, "");
  }

  if (pid == 0) {
    close(pipe_in[1]);
    close(pipe_out[0]);

    dup2(pipe_in[0], STDIN_FILENO);
    close(pipe_in[0]);

    dup2(pipe_out[1], STDOUT_FILENO);

    if (stdErrLog == JNI_TRUE) {
      dup2(pipe_out[1], STDERR_FILENO);
    }

    close(pipe_out[1]);

    execl("/system/bin/sh", "sh", NULL);

    perror("execl");
    exit(EXIT_FAILURE);
  } else {
    close(pipe_in[0]);
    close(pipe_out[1]);

    const char *terminator = "\nexit\n";
    size_t cmd_len = strlen(parsedCommand);
    size_t term_len = strlen(terminator);
    size_t size = cmd_len + term_len;

    char *fullCmd = malloc(size + 1);
    if (fullCmd) {
      snprintf(fullCmd, size + 1, "%s%s", parsedCommand, terminator);
      TEMP_FAILURE_RETRY(write(pipe_in[1], fullCmd, size));
      free(fullCmd);
    }
    close(pipe_in[1]);

    ShellBuffer outBuf;
    bufferInit(&outBuf);
    char buffer[4096];
    ssize_t n;

    while ((n = TEMP_FAILURE_RETRY(
                read(pipe_out[0], buffer, sizeof(buffer) - 1))) > 0) {
      bufferAppend(&outBuf, buffer, n);
    }

    close(pipe_out[0]);
    TEMP_FAILURE_RETRY(waitpid(pid, NULL, 0));
    (*env)->ReleaseStringUTFChars(env, command, parsedCommand);

    sanitizeUTF8(outBuf.data);
    jstring result = (*env)->NewStringUTF(env, outBuf.data ? outBuf.data : "");
    bufferFree(&outBuf);

    return result;
  }
}
