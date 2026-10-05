#include "artillery/match.hpp"
#include "artillery/physics.hpp"
#include "artillery/render.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace artillery;

static void rgb565_to_ppm(FILE* f, const uint16_t* fb)
{
    std::fprintf(f, "P6\n%d %d\n255\n", kWidth, kHeight);
    for (int i = 0; i < kPixelCount; ++i) {
        const uint16_t c = fb[i];
        const uint8_t r = static_cast<uint8_t>(((c >> 11) & 0x1F) << 3);
        const uint8_t g = static_cast<uint8_t>(((c >> 5) & 0x3F) << 2);
        const uint8_t b = static_cast<uint8_t>((c & 0x1F) << 3);
        std::fputc(r, f);
        std::fputc(g, f);
        std::fputc(b, f);
    }
}

static void print_bool(const char* key, bool v, bool last)
{
    std::printf("\"%s\":%s%s", key, v ? "true" : "false", last ? "" : ",");
}

static int cmd_fire(int argc, char** argv)
{
    uint32_t seed = 1;
    int player = 0;
    int angle = 45;
    int power = 70;
    int wind = 0;
    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--player") == 0 && i + 1 < argc) {
            player = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--angle") == 0 && i + 1 < argc) {
            angle = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--power") == 0 && i + 1 < argc) {
            power = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--wind") == 0 && i + 1 < argc) {
            wind = std::atoi(argv[++i]);
        }
    }

    World world;
    world.generate(seed);
    const PlayerId id = player == 1 ? PlayerId::P1 : PlayerId::P0;
    const ShotOutcome out = simulate_shot(world, id, Shot{angle, power}, wind);
    std::printf(
        "{\"cmd\":\"fire\",\"ok\":true,\"seed\":%u,\"player\":%d,\"angle\":%d,\"power\":%d,\"wind\":%d,"
        "\"impact_x\":%.2f,\"impact_y\":%.2f,\"steps\":%d,",
        seed, player, angle, power, wind, static_cast<double>(out.impact.x),
        static_cast<double>(out.impact.y), out.steps);
    print_bool("hit_terrain", out.hit_terrain, false);
    print_bool("left_map", out.left_map, false);
    print_bool("hit_tank0", out.hit_tank[0], false);
    print_bool("hit_tank1", out.hit_tank[1], false);
    std::printf("\"damage0\":%d,\"damage1\":%d}\n", out.damage[0], out.damage[1]);
    return 0;
}

static int cmd_ppm(int argc, char** argv)
{
    uint32_t seed = 1;
    const char* out_path = "artillery.ppm";
    bool title = false;
    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (std::strcmp(argv[i], "--title") == 0) {
            title = true;
        }
    }

    static uint16_t frame[kPixelCount];
    Match match;
    if (title) {
        match.reset_title();
    } else {
        match.start(seed, Mode::VsBot);
    }
    compose_scene(frame, match);
    FILE* f = std::fopen(out_path, "wb");
    if (!f) {
        std::printf("{\"cmd\":\"ppm\",\"ok\":false,\"error\":\"open_failed\"}\n");
        return 1;
    }
    rgb565_to_ppm(f, frame);
    std::fclose(f);
    std::printf("{\"cmd\":\"ppm\",\"ok\":true,\"out\":\"%s\",\"phase\":\"%s\"}\n", out_path,
                phase_name(match.phase()));
    return 0;
}

static int cmd_state(int argc, char** argv)
{
    uint32_t seed = 1;
    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        }
    }
    Match match;
    match.start(seed, Mode::Hotseat);
    const MatchSnapshot s = match.snapshot();
    std::printf(
        "{\"cmd\":\"state\",\"ok\":true,\"seed\":%u,\"phase\":\"%s\",\"turn\":%d,\"active\":%d,"
        "\"angle\":%d,\"power\":%d,\"wind\":%d,\"hp0\":%d,\"hp1\":%d,\"tank0\":[%d,%d],\"tank1\":[%d,%d]}\n",
        s.seed, phase_name(s.phase), s.turn, static_cast<int>(s.active), s.angle, s.power, s.wind, s.hp[0],
        s.hp[1], s.tank_x[0], s.tank_y[0], s.tank_x[1], s.tank_y[1]);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: artillery-sim fire|state|ppm [--seed N] [--angle A] [--power P] "
                     "[--player 0|1] [--wind W] [--out file.ppm]\n");
        return 2;
    }
    const char* cmd = argv[1];
    char** rest = argv + 2;
    const int nrest = argc - 2;
    if (std::strcmp(cmd, "fire") == 0) {
        return cmd_fire(nrest, rest);
    }
    if (std::strcmp(cmd, "state") == 0) {
        return cmd_state(nrest, rest);
    }
    if (std::strcmp(cmd, "ppm") == 0) {
        return cmd_ppm(nrest, rest);
    }
    std::printf("{\"cmd\":\"unknown\",\"ok\":false,\"error\":\"unknown_command\"}\n");
    return 1;
}
