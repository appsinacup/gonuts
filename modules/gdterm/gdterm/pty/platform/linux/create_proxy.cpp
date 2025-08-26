#include "pty_proxy_linux.h"

PtyProxy *
create_proxy(TermRenderer *renderer, const char *shell_path) {
	PtyProxy *proxy = new PtyProxyLinux(shell_path);
	proxy->set_renderer(renderer);
	return proxy;
}
