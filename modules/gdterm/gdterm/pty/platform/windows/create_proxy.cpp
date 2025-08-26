#include "./pty_proxy_win.h"

PtyProxy *
create_proxy(TermRenderer *renderer, const char *shell_path) {
	PtyProxy *proxy = new PtyProxyWin();
	proxy->set_renderer(renderer);
	return proxy;
}
