#include "worker.h"
#include "resource.h"
#include <process.h>
#include <stdint.h>
#include <stdlib.h>

static void SendVKey(WORD vk)
{
    INPUT input[2];
    ZeroMemory(input, sizeof(input));

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = vk;

    input[1] = input[0];
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, input, sizeof(INPUT));
}

static void SendChar(wchar_t ch)
{
    if (ch == L'\r') {
        return;
    }
    if (ch == L'\n') {
        INPUT input[4];
        ZeroMemory(input, sizeof(input));

        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_SHIFT;

        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = VK_RETURN;

        input[2].type = INPUT_KEYBOARD;
        input[2].ki.wVk = VK_RETURN;
        input[2].ki.dwFlags = KEYEVENTF_KEYUP;

        input[3].type = INPUT_KEYBOARD;
        input[3].ki.wVk = VK_SHIFT;
        input[3].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(4, input, sizeof(INPUT));
        return;
    }
    if (ch == L'\t') {
        SendVKey(VK_TAB);
        return;
    }

    INPUT input[2];
    ZeroMemory(input, sizeof(input));

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = 0;
    input[0].ki.wScan = (WORD)ch;
    input[0].ki.dwFlags = KEYEVENTF_UNICODE;

    input[1] = input[0];
    input[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    SendInput(2, input, sizeof(INPUT));
}

static BOOL SimulateTyping(WorkerParams *params)
{
    int i;

    for (i = 0; i < params->textLen; ++i) {
        if (InterlockedCompareExchange(&params->stopped, 0, 0) != 0) {
            return FALSE;
        }

        // 暂停时阻塞在这里；继续时事件会恢复为 signaled。
        WaitForSingleObject(params->hEventPause, INFINITE);

        if (InterlockedCompareExchange(&params->stopped, 0, 0) != 0) {
            return FALSE;
        }

        SendChar(params->text[i]);
        PostMessageW(params->hwndMain, WM_WORKER_PROGRESS,
                     (WPARAM)(i + 1), (LPARAM)params->textLen);

        if (WaitForSingleObject(params->hEventStop, params->delayMs) == WAIT_OBJECT_0) {
            InterlockedExchange(&params->stopped, 1);
            return FALSE;
        }
    }

    return TRUE;
}

static unsigned __stdcall WorkerThread(void *arg)
{
    WorkerParams *params = (WorkerParams *)arg;
    BOOL completed = SimulateTyping(params);

    PostMessageW(params->hwndMain, WM_WORKER_DONE,
                 (WPARAM)(completed ? 0 : 1), 0);
    return 0;
}

HANDLE Worker_Start(WorkerParams *params)
{
    uintptr_t threadHandle;

    params->hEventPause = CreateEventW(NULL, TRUE, TRUE, NULL);
    params->hEventStop  = CreateEventW(NULL, FALSE, FALSE, NULL);
    params->stopped     = 0;

    if (!params->hEventPause || !params->hEventStop) {
        Worker_Free(params);
        return NULL;
    }

    threadHandle = _beginthreadex(NULL, 0, WorkerThread, params, 0, NULL);
    if (threadHandle == 0) {
        Worker_Free(params);
        return NULL;
    }

    return (HANDLE)threadHandle;
}

void Worker_Pause(WorkerParams *params)
{
    if (params && params->hEventPause) {
        ResetEvent(params->hEventPause);
    }
}

void Worker_Resume(WorkerParams *params)
{
    if (params && params->hEventPause) {
        SetEvent(params->hEventPause);
    }
}

void Worker_Stop(WorkerParams *params)
{
    if (!params) {
        return;
    }

    InterlockedExchange(&params->stopped, 1);

    // 如果当前暂停中，先恢复，保证线程能检查到 stop。
    if (params->hEventPause) {
        SetEvent(params->hEventPause);
    }
    if (params->hEventStop) {
        SetEvent(params->hEventStop);
    }
}

void Worker_Free(WorkerParams *params)
{
    if (!params) {
        return;
    }

    if (params->hEventPause) {
        CloseHandle(params->hEventPause);
        params->hEventPause = NULL;
    }
    if (params->hEventStop) {
        CloseHandle(params->hEventStop);
        params->hEventStop = NULL;
    }
    if (params->text) {
        HeapFree(GetProcessHeap(), 0, params->text);
        params->text = NULL;
    }

    HeapFree(GetProcessHeap(), 0, params);
}
