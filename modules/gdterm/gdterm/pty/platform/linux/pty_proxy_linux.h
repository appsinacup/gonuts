#pragma once

#include "../../pty_proxy.h"
#include "poller.h"

class PtyProxyLinux : public PtyProxy, PollHandler {
public:
	PtyProxyLinux(const char *shell_path = nullptr);
	virtual ~PtyProxyLinux();

	int send_string(const char *data) override;
	int available_to_send() override;
	void resize_screen(int nrows, int ncols) override;

	virtual void handle(unsigned char *data, int data_len) override;
	virtual void apply_size() override;
	virtual void exited() override;

private:
	Poller *_poller;
	int _pty_fd;
	char *_shell_path;

	void _init_pty();
};
