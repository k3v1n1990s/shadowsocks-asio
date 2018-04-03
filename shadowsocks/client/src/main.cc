#include <boost/asio.hpp>

#include <common_utils/common.h>
#include <plugin_utils/plugin.h>

#include "server.h"
#include "parse_args.h"

int main(int argc, char *argv[]) {
    int log_level;
    Plugin plugin;
    StreamServerArgs args;

    ParseArgs(argc, argv, &log_level, &args, &plugin);

    InitialLogLevel(argv[0], log_level);

    boost::asio::io_context ctx;

    Socks5ProxyServer server(ctx, args.bind_ep, args.generator, args.timeout);

    std::unique_ptr<boost::process::child> plugin_process;
    std::thread([&plugin_process, &plugin, &main_ctx(ctx), &server]() {
        boost::asio::io_context ctx;
        plugin_process = StartPlugin(ctx, plugin, [&main_ctx, &server]() {
            if (!server.Stopped()) {
                LOG(ERROR) << "server will terminate due to plugin exited";
                main_ctx.stop();
            }
        });
        ctx.run();
    }).detach();

    boost::asio::signal_set signals(ctx, SIGINT, SIGTERM);

    signals.async_wait(
        [&server, &plugin_process](boost::system::error_code ec, int sig) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }
            LOG(INFO) << "Signal: " << sig << " received";
            server.Stop();
            if (plugin_process && plugin_process->running()) {
                plugin_process->terminate();
            }
        }
    );

    ctx.run();

    if (plugin_process && plugin_process->running()) {
        plugin_process->terminate();
    }

    return 0;
}

