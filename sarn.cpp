#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include <ctype.h>

#define TEMP_DIR_TEMPLATE "sarn_repl"
#define MAX_PATH_LEN 4096
#define MAX_CODE_LEN 65536
#define MAX_CMD_LEN 8192

typedef struct {
    char sarn_root[MAX_PATH_LEN];
    char sarnc_exe[MAX_PATH_LEN];
    char llc_exe[MAX_PATH_LEN];
    char clang_exe[MAX_PATH_LEN];
    char sarn_lib[MAX_PATH_LEN];
    char raylib_lib[MAX_PATH_LEN];
    char temp_dir[MAX_PATH_LEN];
} Config;

void get_env_var(const char* name, char* buffer, size_t len) {
    DWORD result = GetEnvironmentVariableA(name, buffer, (DWORD)len);
    if (result == 0) {
        buffer[0] = '\0';
    }
}

int setup_config(Config* cfg) {
    char module_path[MAX_PATH_LEN];
    GetModuleFileNameA(NULL, module_path, MAX_PATH_LEN);
    
    char drive[_MAX_DRIVE], dir[_MAX_DIR];
    _splitpath_s(module_path, drive, sizeof(drive), dir, sizeof(dir), NULL, 0, NULL, 0);
    _makepath_s(cfg->sarn_root, sizeof(cfg->sarn_root), drive, dir, "", "");
    
    get_env_var("SARN_ROOT", cfg->sarn_root, MAX_PATH_LEN);
    if (cfg->sarn_root[0] == '\0') {
        strcpy_s(cfg->sarn_root, MAX_PATH_LEN, ".");
    }
    
    sprintf_s(cfg->sarnc_exe, MAX_PATH_LEN, "%s\\build\\compiler\\Release\\sarnc.exe", cfg->sarn_root);
    strcpy_s(cfg->llc_exe, MAX_PATH_LEN, "C:\\LLVM\\bin\\llc.exe");
    strcpy_s(cfg->clang_exe, MAX_PATH_LEN, "C:\\LLVM\\bin\\clang.exe");
    sprintf_s(cfg->sarn_lib, MAX_PATH_LEN, "%s\\build\\runtime\\sarn.lib", cfg->sarn_root);
    strcpy_s(cfg->raylib_lib, MAX_PATH_LEN, "C:\\vcpkg\\installed\\x64-windows\\lib\\raylib.lib");
    
    char temp_path[MAX_PATH_LEN];
    GetTempPathA(MAX_PATH_LEN, temp_path);
    sprintf_s(cfg->temp_dir, MAX_PATH_LEN, "%s\\%s", temp_path, TEMP_DIR_TEMPLATE);
    
    CreateDirectoryA(cfg->temp_dir, NULL);
    return 1;
}

int file_exists(const char* path) {
    DWORD attribs = GetFileAttributesA(path);
    return (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY));
}

void trim_str(const char* src, char* dst, size_t len) {
    size_t i = 0;
    while (src[i] && i < len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    
    while (i > 0 && (dst[i - 1] == ' ' || dst[i - 1] == '\t' || dst[i - 1] == '\n' || dst[i - 1] == '\r')) {
        dst[--i] = '\0';
    }
}

int count_keyword(const char* text, const char* kw) {
    int count = 0;
    size_t kwlen = strlen(kw);
    const char* p = text;
    
    while ((p = strstr(p, kw)) != NULL) {
        int valid = 1;
        if (p > text && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) valid = 0;
        if (valid && p[kwlen] && (isalnum((unsigned char)p[kwlen]) || p[kwlen] == '_')) valid = 0;
        if (valid) count++;
        p++;
    }
    return count;
}

int should_continue(const char* code) {
    int opens = count_keyword(code, "if") + count_keyword(code, "for") +
                count_keyword(code, "while") + count_keyword(code, "function") +
                count_keyword(code, "try");
    int closes = count_keyword(code, "end");
    return opens > closes;
}

void delete_file(const char* path) {
    DeleteFileA(path);
}

int run_repl(const Config* cfg) {
    char line[MAX_CODE_LEN];
    char code[MAX_CODE_LEN];
    char cmd[MAX_CMD_LEN];
    unsigned int session_id = (unsigned int)time(NULL) % 100000;
    int repl_count = 0;
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║        Sarn Interactive REPL v0.3                      ║\n");
    printf("║        Type 'exit' or 'quit' to exit                   ║\n");
    printf("║        Type 'help' for commands                        ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    while (1) {
        printf("sarn> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        
        trim_str(line, code, MAX_CODE_LEN);
        
        if (strlen(code) == 0) continue;
        if (strcmp(code, "exit") == 0 || strcmp(code, "quit") == 0) break;
        if (strcmp(code, "help") == 0) {
            printf("\nCommands:\n");
            printf("  exit              Exit the REPL\n");
            printf("  quit              Exit the REPL\n");
            printf("  help              Show this message\n");
            printf("  clear             Clear screen\n\n");
            continue;
        }
        if (strcmp(code, "clear") == 0) {
            system("cls");
            continue;
        }
        
        if (strstr(code, "--!!type:") != code) {
            char temp[MAX_CODE_LEN];
            sprintf_s(temp, MAX_CODE_LEN, "--!!type:strict; %s", code);
            strcpy_s(code, MAX_CODE_LEN, temp);
        }
        
        while (should_continue(code)) {
            printf("...> ");
            fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) break;
            
            if (strlen(code) + strlen(line) < MAX_CODE_LEN - 2) {
                strcat_s(code, MAX_CODE_LEN, "\n");
                strcat_s(code, MAX_CODE_LEN, line);
            }
        }
        
        int repl_id = (session_id + repl_count) % 100000;
        repl_count++;
        
        char temp_sarn[MAX_PATH_LEN];
        char temp_ll[MAX_PATH_LEN];
        char temp_obj[MAX_PATH_LEN];
        char temp_exe[MAX_PATH_LEN];
        
        sprintf_s(temp_sarn, MAX_PATH_LEN, "%s\\repl_%d.sarn", cfg->temp_dir, repl_id);
        sprintf_s(temp_ll, MAX_PATH_LEN, "%s\\repl_%d.ll", cfg->temp_dir, repl_id);
        sprintf_s(temp_obj, MAX_PATH_LEN, "%s\\repl_%d.obj", cfg->temp_dir, repl_id);
        sprintf_s(temp_exe, MAX_PATH_LEN, "%s\\repl_%d.exe", cfg->temp_dir, repl_id);
        
        FILE* f = NULL;
        fopen_s(&f, temp_sarn, "w");
        if (f) {
            fprintf(f, "%s", code);
            fclose(f);
        }
        
        sprintf_s(cmd, MAX_CMD_LEN, "\"%s\" \"%s\" -o \"%s\" 2>&1", cfg->sarnc_exe, temp_sarn, temp_ll);
        int compile_result = system(cmd);
        
        if (compile_result != 0) {
            printf("Compilation failed\n\n");
            delete_file(temp_sarn);
            continue;
        }
        
        if (file_exists(cfg->llc_exe)) {
            sprintf_s(cmd, MAX_CMD_LEN, "\"%s\" \"%s\" -o \"%s\" 2>&1", cfg->llc_exe, temp_ll, temp_obj);
            system(cmd);
        } else {
            CopyFileA(temp_ll, temp_obj, FALSE);
        }
        
        sprintf_s(cmd, MAX_CMD_LEN, 
            "\"%s\" \"%s\" \"%s\" \"%s\" -lOpenGL32 -lgdi32 -lwinmm -ladvapi32 -lUser32 -lShell32 -lGdi32 -lmsvcrt -lucrt -lvcruntime -o \"%s\" 2>&1",
            cfg->clang_exe, temp_obj, cfg->sarn_lib, cfg->raylib_lib, temp_exe);
        system(cmd);
        
        if (file_exists(temp_exe)) {
            printf("\n");
            system(temp_exe);
            printf("\n");
        } else {
            printf("Linking failed\n\n");
        }
        
        delete_file(temp_sarn);
        delete_file(temp_ll);
        delete_file(temp_obj);
        delete_file(temp_exe);
    }
    
    printf("\nExiting Sarn REPL.\n");
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc == 2 && strcmp(argv[1], "sarn") == 0) {
        Config cfg;
        setup_config(&cfg);
        return run_repl(&cfg);
    }
    
    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        printf("Sarn C REPL\n");
        printf("Usage: sarn.exe sarn\n");
        return 0;
    }
    
    fprintf(stderr, "Usage: sarn.exe sarn\n");
    return 1;
}
