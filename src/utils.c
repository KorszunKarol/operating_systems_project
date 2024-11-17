#include "utils.h"
#include "shared.h"

/*************************************************
*
* @Name: readUntil
* @Def: Reads characters from a file descriptor until a specified end character is encountered.
* @Arg: In: fd = file descriptor to read from
*       In: end = character to stop reading at
* @Ret: Returns a dynamically allocated string containing the read characters, or NULL on failure.
*
*************************************************/

char *read_until(int fd, char end) {
    int nIndex = 0;
    ssize_t nCharsRead;
    char c = 0;
    char *psBuffer = NULL;

    while (1) {
        nCharsRead = read(fd, &c, sizeof(char));
        if (nCharsRead == 0) {
            if (nIndex == 0) {
                return NULL;
            }
            break;
        } else if (nCharsRead < 0) {
            free(psBuffer);
            return NULL;
        }

        if (c == end) {
            break;
        }
        psBuffer = (char *)realloc(psBuffer, nIndex + 2);
        if (!psBuffer) {
            return NULL;
        }
        psBuffer[nIndex++] = c;
    }

    if (psBuffer) {
        psBuffer[nIndex] = '\0';
    }
    return psBuffer;
}

/*************************************************
*
* @Name: verifyDirectory
* @Def: Verifies if a directory exists at the given path.
* @Arg: In: path = path of the directory to verify
* @Ret: None
*
*************************************************/

void verify_directory(const char *path) {
    DIR *pDir = opendir(path);
    if (!pDir) {
        char *psError;
        asprintf(&psError, "Error: Directory %s does not exist\n", path);
        vWriteLog(psError);
        free(psError);
        exit(1);
    }
    closedir(pDir);
}

void setup_signal_handlers(void) {
    signal(SIGINT, SIG_IGN);
}

void load_fleck_config(const char *filename, FleckConfig *config) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        vWriteLog(ERROR_MSG_CONFIG);
        exit(1);
    }

    char *line;

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sUsername, line, MAX_USERNAME_LENGTH - 1);
        sanitize_username(config->sUsername);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sFolderPath, line, MAX_PATH_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sGothamIP, line, MAX_IP_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sGothamPort, line, MAX_PORT_LENGTH - 1);
        free(line);
    }

    close(fd);
}

void nothing(int signum) {
    signal(SIGINT, nothing);
    (void)signum;
}

void load_worker_config(const char *filename, WorkerConfig *config) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printF(ERROR_MSG_CONFIG);
        exit(1);
    }

    char *line;

    // Read Gotham IP
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sGothamIP, line, MAX_IP_LENGTH - 1);
        config->sGothamIP[MAX_IP_LENGTH - 1] = '\0';
        free(line);
    }

    // Read Gotham Port
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sGothamPort, line, MAX_PORT_LENGTH - 1);
        config->sGothamPort[MAX_PORT_LENGTH - 1] = '\0';
        free(line);
    }

    // Read Fleck IP
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sFleckIP, line, MAX_IP_LENGTH - 1);
        config->sFleckIP[MAX_IP_LENGTH - 1] = '\0';
        free(line);
    }

    // Read Fleck Port
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sFleckPort, line, MAX_PORT_LENGTH - 1);
        config->sFleckPort[MAX_PORT_LENGTH - 1] = '\0';
        free(line);
    }

    // Read Save Folder
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sSaveFolder, line, MAX_PATH_LENGTH - 1);
        config->sSaveFolder[MAX_PATH_LENGTH - 1] = '\0';
        free(line);
    }

    // Read Worker Type
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sWorkerType, line, MAX_TYPE_LENGTH - 1);
        config->sWorkerType[MAX_TYPE_LENGTH - 1] = '\0';
        free(line);
    }

    close(fd);
}
