#include "app.h"

#include <spdlog/spdlog.h>

#include <cstdlib>

int main()
{
    // try {
        App app;
        app.run();
    // } catch (const std::exception& e) {
    //     spdlog::error("error: {}", e.what());
    //     return EXIT_FAILURE;
    // }

    return EXIT_SUCCESS;
}