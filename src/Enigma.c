#include "worker.h"
#include "utils.h"
#include "protocol.h"
#include <errno.h>
#include <unistd.h>

/*************************************************
* @Name: register_with_gotham
* @Def: Registers worker with Gotham server
* @Arg: In: pWorker = Worker instance
* @Ret: 0 on success, -1 on error
*************************************************/
int register_with_gotham(Worker* pWorker) {
    if (!pWorker) {
        vWriteLog("Error: Invalid worker\n");
        return -1;
    }

    char data[DATA_SIZE];
    if (snprintf(data, sizeof(data), "Text&%s&%s",
                 pWorker->sIP, pWorker->sPort) < 0) {
        vWriteLog("Error: Failed to create registration data\n");
        return -1;
    }

    vWriteLog("Sending registration frame to Gotham\n");

    // Send registration frame
    Frame* frame = create_frame(FRAME_WORKER_REG, data, strlen(data));
    if (send_frame(pWorker->pGothamConn, frame) != 0) {
        free_frame(frame);
        return -1;
    }
    free_frame(frame);

    // Wait for response
    frame = receive_frame(pWorker->pGothamConn);
    if (!frame) {
        return -1;
    }

    // Check response
    if (frame->type == FRAME_NEW_MAIN) {
        vWriteLog("Registration successful as main worker\n");
        pWorker->nIsMainWorker = 1;
        free_frame(frame);
        vWriteLog("Worker registered successfully\n");
        return 0;
    }

    free_frame(frame);
    return -1;
}

/*************************************************
* @Name: handle_client_connection
* @Def: Handles connection from a Fleck client
* @Arg: In: pWorker = Worker instance
*       In: frame = Connection request frame
* @Ret: 0 on success, -1 on error
*************************************************/
int handle_client_connection(Worker* pWorker, Frame* frame) {
    // Parse connection data
    char sUsername[64], sFilename[256], sFileSize[32], sMD5[33], sFactor[16];
    if (sscanf(frame->data, "%[^&]&%[^&]&%[^&]&%[^&]&%s",
               sUsername, sFilename, sFileSize, sMD5, sFactor) != 5) {
        // Send connection rejection
        Frame* response = create_frame(FRAME_WORKER_CONNECT, "CON_KO", 6);
        send_frame(pWorker->pClientConn, response);
        free_frame(response);
        return -1;
    }

    vWriteLog("New request - ");
    vWriteLog(sUsername);
    vWriteLog(" wants to distort some text, with factor ");
    vWriteLog(sFactor);
    vWriteLog("\n");

    // Accept connection
    Frame* response = create_frame(FRAME_WORKER_CONNECT, NULL, 0);
    send_frame(pWorker->pClientConn, response);
    free_frame(response);

    vWriteLog("Receiving original text...\n");
    // For Phase 1/2, just simulate receiving and processing
    vWriteLog("Distorting...\n");
    sleep(1); // Simulate processing
    vWriteLog("Sending distorted text to ");
    vWriteLog(sUsername);
    vWriteLog("...\n");

    return 0;
}

/*************************************************
* @Name: process_messages
* @Def: Main message processing loop
* @Arg: In: pWorker = Worker instance
* @Ret: 0 on success, -1 on error
*************************************************/
int process_messages(Worker* pWorker) {
    vWriteLog("Connected to Mr. J System, ready to listen to Fleck petitions\n");
    vWriteLog("Waiting for connections...\n");

    while (1) {
        // Add debug message before receive
        vWriteLog("Waiting for next frame...\n");

        // Check connection status
        if (!is_connected(pWorker->pGothamConn)) {
            vWriteLog("Connection check failed - socket no longer connected\n");
            break;
        }

        Frame* frame = receive_frame(pWorker->pGothamConn);
        if (!frame) {
            char* psMsg;
            asprintf(&psMsg, "Failed to receive frame (errno: %d)\n", errno);
            vWriteLog(psMsg);
            free(psMsg);
            break;
        }

        // Log received frame
        char* psMsg;
        asprintf(&psMsg, "Received frame - Type: 0x%02X, Length: %d\n",
                frame->type, frame->data_length);
        vWriteLog(psMsg);
        free(psMsg);

        switch (frame->type) {
            case FRAME_NEW_MAIN:
                vWriteLog("Assigned as main worker\n");
                pWorker->nIsMainWorker = 1;
                break;

            case FRAME_WORKER_CONNECT:
                vWriteLog("New user connected\n");
                handle_client_connection(pWorker, frame);
                break;

            case FRAME_DISCONNECT:
                vWriteLog("Received disconnect request\n");
                free_frame(frame);
                return 0;

            default:
                asprintf(&psMsg, "Received unexpected frame type: 0x%02X\n", frame->type);
                vWriteLog(psMsg);
                free(psMsg);

                // Send error frame for unexpected types
                Frame* error = create_frame(FRAME_ERROR, NULL, 0);
                send_frame(pWorker->pGothamConn, error);
                free_frame(error);
                break;
        }

        free_frame(frame);
    }

    return -1;
}

int main(int nArgc, char* psArgv[]) {
    if (nArgc != 2) {
        vWriteLog("Usage: Enigma <config_file>\n");
        return 1;
    }

    vWriteLog("Reading configuration file\n");
    vWriteLog("Connecting Enigma worker to the system...\n");

    /* Create and initialize worker */
    Worker* pWorker = create_worker(psArgv[1]);
    if (!pWorker) {
        vWriteLog("Failed to create worker\n");
        return 1;
    }

    /* Register with Gotham */
    if (register_with_gotham(pWorker) != 0) {
        vWriteLog("Failed to register with Gotham\n");
        destroy_worker(pWorker);
        return 1;
    }

    /* Enter message processing loop */
    int nResult = process_messages(pWorker);

    /* Cleanup */
    if (pWorker->pGothamConn) {
        vWriteLog("Lost connection to Gotham. Finishing current work...\n");

        // Send disconnect notification
        vWriteLog("Sending disconnect notification to Gotham\n");
        Frame* frame = create_frame(FRAME_DISCONNECT, "Text", 4);
        send_frame(pWorker->pGothamConn, frame);
        free_frame(frame);

        close_connection(pWorker->pGothamConn);
    }

    destroy_worker(pWorker);
    return nResult;
}