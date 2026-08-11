#include <android/log.h>
#include <jni.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define FEX_LOG(prio, ...) __android_log_print(ANDROID_LOG_##prio, "FEXCoreWrapper", __VA_ARGS__)

// FEXInterpreter path - will be set from Java
static char fex_interpreter_path[512] = {0};
static char rootfs_path[512] = {0};
static pid_t fex_process_pid = -1;
static int fex_stdout_pipe[2] = {-1, -1};
static int fex_stderr_pipe[2] = {-1, -1};

JNIEXPORT jboolean JNICALL
Java_com_windroid_emu_core_FEXCoreWrapper_setFEXInterpreterPath(JNIEnv *env, jobject thiz, jstring path) {
  if (!path) {
    FEX_LOG(ERROR, "FEXInterpreter path is null");
    return JNI_FALSE;
  }
  
  const char *path_str = env->GetStringUTFChars(path, nullptr);
  strncpy(fex_interpreter_path, path_str, sizeof(fex_interpreter_path) - 1);
  env->ReleaseStringUTFChars(path, path_str);
  
  FEX_LOG(INFO, "FEXInterpreter path set to: %s", fex_interpreter_path);
  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_windroid_emu_core_FEXCoreWrapper_setRootFSPath(JNIEnv *env, jobject thiz, jstring path) {
  if (!path) {
    FEX_LOG(ERROR, "RootFS path is null");
    return JNI_FALSE;
  }
  
  const char *path_str = env->GetStringUTFChars(path, nullptr);
  strncpy(rootfs_path, path_str, sizeof(rootfs_path) - 1);
  env->ReleaseStringUTFChars(path, path_str);
  
  FEX_LOG(INFO, "RootFS path set to: %s", rootfs_path);
  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_windroid_emu_core_FEXCoreWrapper_initialize(JNIEnv *env, jobject thiz) {
  FEX_LOG(INFO, "Initializing FEXCore wrapper (subprocess mode)...");
  
  // Check if FEXInterpreter path is set
  if (strlen(fex_interpreter_path) == 0) {
    FEX_LOG(ERROR, "FEXInterpreter path not set. Call setFEXInterpreterPath first.");
    return JNI_FALSE;
  }
  
  // Check if RootFS path is set
  if (strlen(rootfs_path) == 0) {
    FEX_LOG(ERROR, "RootFS path not set. Call setRootFSPath first.");
    return JNI_FALSE;
  }
  
  FEX_LOG(INFO, "FEXCore wrapper initialized successfully");
  return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_windroid_emu_core_FEXCoreWrapper_shutdown(JNIEnv *env, jobject thiz) {
  FEX_LOG(INFO, "Shutting down FEXCore wrapper...");
  
  // Kill FEXInterpreter process if running
  if (fex_process_pid > 0) {
    FEX_LOG(INFO, "Killing FEXInterpreter process (PID: %d)", fex_process_pid);
    kill(fex_process_pid, SIGTERM);
    
    // Wait for process to terminate
    int status;
    waitpid(fex_process_pid, &status, 0);
    fex_process_pid = -1;
  }
  
  // Close pipes
  if (fex_stdout_pipe[0] != -1) close(fex_stdout_pipe[0]);
  if (fex_stdout_pipe[1] != -1) close(fex_stdout_pipe[1]);
  if (fex_stderr_pipe[0] != -1) close(fex_stderr_pipe[0]);
  if (fex_stderr_pipe[1] != -1) close(fex_stderr_pipe[1]);
  
  fex_stdout_pipe[0] = fex_stdout_pipe[1] = -1;
  fex_stderr_pipe[0] = fex_stderr_pipe[1] = -1;
  
  FEX_LOG(INFO, "FEXCore wrapper shutdown complete");
}

JNIEXPORT jboolean JNICALL
Java_com_windroid_emu_core_FEXCoreWrapper_loadELF(JNIEnv *env, jobject thiz, jstring elf_path) {
  if (strlen(fex_interpreter_path) == 0) {
    FEX_LOG(ERROR, "FEXCore not initialized");
    return JNI_FALSE;
  }
  
  const char *path = env->GetStringUTFChars(elf_path, nullptr);
  FEX_LOG(INFO, "Loading ELF: %s", path);
  
  // In subprocess mode, ELF loading happens when we run the process
  // This function just validates the path exists
  if (access(path, F_OK) != 0) {
    FEX_LOG(ERROR, "ELF file not found: %s", path);
    env->ReleaseStringUTFChars(elf_path, path);
    return JNI_FALSE;
  }
  
  env->ReleaseStringUTFChars(elf_path, path);
  return JNI_TRUE;
}

JNIEXPORT jint JNICALL
Java_com_windroid_emu_core_FEXCoreWrapper_run(JNIEnv *env, jobject thiz, jint argc, jobjectArray argv) {
  if (strlen(fex_interpreter_path) == 0) {
    FEX_LOG(ERROR, "FEXCore not initialized");
    return -1;
  }
  
  FEX_LOG(INFO, "Running FEXCore with %d arguments", argc);
  
  // Create pipes for stdout and stderr
  if (pipe(fex_stdout_pipe) == -1 || pipe(fex_stderr_pipe) == -1) {
    FEX_LOG(ERROR, "Failed to create pipes: %s", strerror(errno));
    return -1;
  }
  
  // Fork process
  pid_t pid = fork();
  if (pid == -1) {
    FEX_LOG(ERROR, "Failed to fork: %s", strerror(errno));
    return -1;
  }
  
  if (pid == 0) {
    // Child process - exec FEXInterpreter
    close(fex_stdout_pipe[0]); // Close read end
    close(fex_stderr_pipe[0]); // Close read end
    
    // Redirect stdout and stderr
    dup2(fex_stdout_pipe[1], STDOUT_FILENO);
    dup2(fex_stderr_pipe[1], STDERR_FILENO);
    close(fex_stdout_pipe[1]);
    close(fex_stderr_pipe[1]);
    
    // Build arguments for FEXInterpreter
    // FEXInterpreter [options] <binary> [args...]
    char* args[argc + 4];
    args[0] = fex_interpreter_path;
    args[1] = (char*)"--rootfs";
    args[2] = rootfs_path;
    
    // Copy argv from Java array
    for (int i = 0; i < argc; i++) {
      jstring arg = (jstring)env->GetObjectArrayElement(argv, i);
      const char* arg_str = env->GetStringUTFChars(arg, nullptr);
      args[i + 3] = strdup(arg_str);
      env->ReleaseStringUTFChars(arg, arg_str);
    }
    args[argc + 3] = nullptr;
    
    // Execute FEXInterpreter
    execv(fex_interpreter_path, args);
    
    // If execv fails
    FEX_LOG(ERROR, "Failed to exec FEXInterpreter: %s", strerror(errno));
    _exit(1);
  } else {
    // Parent process
    fex_process_pid = pid;
    close(fex_stdout_pipe[1]); // Close write end
    close(fex_stderr_pipe[1]); // Close write end
    
    FEX_LOG(INFO, "FEXInterpreter started with PID: %d", pid);
    
    // Wait for process to complete
    int status;
    waitpid(pid, &status, 0);
    
    // Read output from pipes
    char buffer[4096];
    ssize_t bytes_read;
    
    // Read stdout
    while ((bytes_read = read(fex_stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';
      FEX_LOG(INFO, "[FEX stdout] %s", buffer);
    }
    
    // Read stderr
    while ((bytes_read = read(fex_stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';
      FEX_LOG(ERROR, "[FEX stderr] %s", buffer);
    }
    
    // Close pipes
    close(fex_stdout_pipe[0]);
    close(fex_stderr_pipe[0]);
    fex_stdout_pipe[0] = fex_stdout_pipe[1] = -1;
    fex_stderr_pipe[0] = fex_stderr_pipe[1] = -1;
    fex_process_pid = -1;
    
    // Return exit code
    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      FEX_LOG(ERROR, "FEXInterpreter killed by signal: %d", WTERMSIG(status));
      return -1;
    }
    
    return 0;
  }
}

JNIEXPORT jstring JNICALL
Java_com_windroid_emu_core_FEXCoreWrapper_getVersion(JNIEnv *env, jobject thiz) {
  return env->NewStringUTF("FEXCore-Android-Bionic-1.0 (Subprocess Mode)");
}
