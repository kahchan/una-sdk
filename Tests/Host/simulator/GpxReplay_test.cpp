/**
 * Host unit tests for the simulator's GPX replay GPS (UNA_SIM_GPX).
 */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "SDK/Simulator/Components/Simulator/GpxReplaySimulator.hpp"

using Simulator::GpxReplaySimulator;

namespace {

std::string writeGpx(const char* name, const std::string& body)
{
    const auto path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream(path) << body;
    return path;
}

TEST(GpxReplay, ParsesTimedTrackRelativeToFirstPoint)
{
    const auto path = writeGpx("gpx_timed.gpx", R"(<?xml version="1.0"?>
<gpx><trk><trkseg>
 <trkpt lat="-41.2900" lon="174.7800"><ele>5.0</ele><time>2026-11-01T02:00:00Z</time></trkpt>
 <trkpt lon="174.7810" lat="-41.2901"><ele>7.5</ele><time>2026-11-01T02:00:10Z</time></trkpt>
 <trkpt lat="-41.2902" lon="174.7820"><ele>9.0</ele><time>2026-11-01T02:01:00.000Z</time></trkpt>
</trkseg></trk></gpx>)");

    const auto pts = GpxReplaySimulator::parse(path.c_str());
    ASSERT_EQ(pts.size(), 3u);
    EXPECT_FLOAT_EQ(pts[1].lat, -41.2901f);
    EXPECT_FLOAT_EQ(pts[1].lon, 174.7810f);
    EXPECT_FLOAT_EQ(pts[1].alt, 7.5f);
    EXPECT_DOUBLE_EQ(pts[0].t, 0.0);
    EXPECT_DOUBLE_EQ(pts[1].t, 10.0);
    EXPECT_DOUBLE_EQ(pts[2].t, 60.0);
}

TEST(GpxReplay, PacesUntimedRouteAt25KmH)
{
    const auto path = writeGpx("gpx_route.gpx", R"(<gpx><rte>
 <rtept lat="-41.0" lon="174.0"/>
 <rtept lat="-41.0" lon="174.01"/>
</rte></gpx>)");

    const auto pts = GpxReplaySimulator::parse(path.c_str());
    ASSERT_EQ(pts.size(), 2u);
    const double metres = GpxReplaySimulator::metres(pts[0], pts[1]);
    EXPECT_NEAR(metres, 839.0, 5.0);
    EXPECT_NEAR(pts[1].t, metres / (25.0 / 3.6), 1e-6);
}

}  // namespace
