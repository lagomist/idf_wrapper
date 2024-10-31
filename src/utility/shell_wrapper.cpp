#include "shell_wrapper.h"
#include <esp_log.h>
#include <string.h>
#include <vector>

namespace Wrapper {

namespace Shell {

static Callback _call_cb = nullptr;

static void try_push(std::vector<std::string>& args, std::string& arg) {
	if (arg.empty())
		return;
	args.push_back(arg);
	arg.clear();
	return;
}

// return number of args
static std::vector<std::string> cmd_split(std::string_view exp) {
	std::vector<std::string> args;
	std::string arg;
	while (!exp.empty()) {
		switch (exp[0]) {
			case '\r': case '\n': case ' ':
				try_push(args, arg);
				break;
			default:
				arg.push_back(exp[0]);
				break;
		}
		exp.remove_prefix(1);
	}
	try_push(args, arg);
	return args;
}

void registerCallback(Callback call) {
	_call_cb = call;
}

OBuf response(IBuf rbuf) {
	auto args = cmd_split({(char*)rbuf.data(), rbuf.size()});
	if (args.empty())
		return {};
	char *argptr[args.size()];
	for (size_t idx = 0; idx < args.size(); ++idx)
		argptr[idx] = args[idx].data();
	if (_call_cb != nullptr)
		return _call_cb(args.size(), argptr);
	return {};
}

}

}
