/*********************************
*
* @File: Harley.c
* @Purpose: Media distortion worker implementation
* @Author: Karol Korszun
* @Date: 2024-03-19
*
*********************************/

#include "worker.h"
#include "utils.h"

int main(int nArgc, char* psArgv[]) {
    if (nArgc != 2) {
        vWriteLog("Usage: Harley <config_file>\n");
        return 1;
    }

    /* Create and initialize worker */
    Worker* pWorker = create_worker(psArgv[1]);
    if (!pWorker) {
        vWriteLog("Failed to create worker\n");
        return 1;
    }

    /* Run worker */
    int nResult = run_worker(pWorker);

    /* Cleanup */
    destroy_worker(pWorker);

    return nResult;
}
