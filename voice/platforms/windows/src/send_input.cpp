#include "send_input.h"
#include <windows.h>
#include <cstdio>

void send_text(const std::wstring &text)
{
    // printf("[SEND_INPUT] Sending %zu chars: ", text.size());
    // for (wchar_t ch : text) {
    //     printf("%04X ", (unsigned int)ch);
    // }
    // printf("\n");
    // fflush(stdout);

    for (wchar_t ch : text)
    {
        INPUT in[2]{};

        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wScan = ch;
        in[0].ki.dwFlags = KEYEVENTF_UNICODE;

        in[1] = in[0];
        in[1].ki.dwFlags |= KEYEVENTF_KEYUP;

        UINT sent = SendInput(2, in, sizeof(INPUT));
        if (sent != 2)
        {
            printf("[SEND_INPUT] SendInput failed for char %04X, sent: %u, error: %lu\n", (unsigned int)ch, sent, GetLastError());
            fflush(stdout);
        }

        // Small delay to prevent overwhelming the target input queue
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    printf("[SEND_INPUT] Done sending.\n");
    fflush(stdout);
}