#pragma once

#include "../../pty_proxy.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006 // NTDDI_WIN10_RS5
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // _WIN32_WINNT_WIN10
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Include base Windows types first
#include <windef.h>
#include <windows.h>
#include <wincon.h>
#include <condition_variable>
#include <mutex>
#ifdef __MINGW64__
#include <cstring>
// Add missing pseudo-console API definitions for MinGW
#ifndef HPCON
typedef VOID* HPCON;
#endif
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif
// Function declarations for pseudo-console API
extern "C" {
HRESULT WINAPI CreatePseudoConsole(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC);
HRESULT WINAPI ResizePseudoConsole(HPCON hPC, COORD size);
void WINAPI ClosePseudoConsole(HPCON hPC);
}
#endif

static const int TO_BUFFER_MAX_SIZE = 1024;

class PtyProxyWin : public PtyProxy {
public:
	PtyProxyWin();
	virtual ~PtyProxyWin();

	virtual int send_string(const char *data);
	virtual int available_to_send();
	virtual void resize_screen(int nrows, int ncols);

	// Called from internal threads
	void _run_to_thread();
	void _run_from_thread();

	static void _print_last_error(const char *prefix);

private:
	HPCON _pty_console;
	HANDLE _to_pty;
	HANDLE _from_pty;
	STARTUPINFOEX _startup_info;
	PROCESS_INFORMATION _client;

	std::mutex _state_mutex;
	int _state;

	std::mutex _to_mutex;
	std::condition_variable _to_ready;
	HANDLE _to_thread;
	unsigned char _to_buffer[TO_BUFFER_MAX_SIZE];
	int _to_buffer_pos;
	bool _to_thread_started;

	HANDLE _from_thread;
	bool _from_thread_started;

	std::mutex _start_complete_mutex;
	std::condition_variable _start_complete;

	void _create_pipes_and_pty();
	void _create_process();
	void _init_startup_info();
	void _create_to_pty_thread();
	void _create_from_pty_thread();
	bool _can_write();
	bool _can_read();
	void _exit_pty();
	void _set_state(int p_state);
	void _wake_to_thread();
	void _wake_from_thread();
	void _join_threads();
	void _terminate_process();
	void _close_pty();
	void _notify_to_started();
	void _notify_from_started();
};
