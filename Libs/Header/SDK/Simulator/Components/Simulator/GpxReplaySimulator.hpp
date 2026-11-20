/**
 ******************************************************************************
 * @file    GpxReplaySimulator.hpp
 * @brief   Simulator GPS that plays back a recorded GPX track.
 *
 * Selected by ComponentSimulator when UNA_SIM_GPX names a .gpx file.
 * UNA_SIM_GPX_SPEED (default 1) plays the track that many times faster than
 * real time. Points without <time> are paced at a steady 25 km/h, so route
 * files exported from planners play too. The position is interpolated from
 * the elapsed time on every read, so no thread is needed; the track holds at
 * its last point once it ends.
 *
 * Header-only so that the many simulator build lists (TouchGFX Makefiles and
 * .vcxproj files) need no new source entry.
 ******************************************************************************
 */
#ifndef __GPX_REPLAY_SIMULATOR_HPP
#define __GPX_REPLAY_SIMULATOR_HPP

#include "SDK/Simulator/Components/ISensorsSim/IGps.hpp"
#include "SDK/Simulator/Components/ISensorsSim/IStepCounter.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace Simulator
{
    class GpxReplaySimulator : public Interface::IGps, public Interface::IStepCounter
    {
    public:
        struct Point { double t; float lat; float lon; float alt; };

        GpxReplaySimulator(const char* path, float speedUp)
            : mPoints(parse(path)), mSpeedUp(speedUp > 0.0f ? speedUp : 1.0f)
        {
            if (mPoints.size() < 2) {
                std::fprintf(stderr, "UNA_SIM_GPX: no track points in %s\n", path);
                std::exit(1);
            }
            std::printf("UNA_SIM_GPX: %zu points, %.0f s of ride, playing at %gx\n",
                        mPoints.size(), mPoints.back().t, mSpeedUp);
        }

        void setParamSimulation(float, float, float, uint32_t) override {}
        bool init() override { return true; }
        bool deinit() override { return true; }

        // ponytail: the ride starts on the first enable, so the time the app spends on its own start screen is not skipped
        bool enable() override
        {
            std::call_once(mStartOnce, [this] {
                mStart = std::chrono::steady_clock::now();
                mStarted = true;
            });
            mEnabled = true;
            return true;
        }
        bool disable() override { mEnabled = false; return true; }
        bool isEnabled() override { return mEnabled; }
        void setPeriod(uint32_t) override {}
        bool hasFix() override { return mStarted; }
        time_t getTime() override { return std::time(nullptr); }

        LocationInfo getLocation() override
        {
            const Seg s = segment();
            const Point& a = mPoints[s.i];
            const Point& b = mPoints[s.i + 1];
            LocationInfo loc;
            loc.valid = mStarted;
            loc.lat = a.lat + (b.lat - a.lat) * s.f;
            loc.lon = a.lon + (b.lon - a.lon) * s.f;
            loc.alt = a.alt + (b.alt - a.alt) * s.f;
            loc.precision = 4.0f;
            return loc;
        }

        float getSpeed() override
        {
            const Seg s = segment();
            const Point& a = mPoints[s.i];
            const Point& b = mPoints[s.i + 1];
            if (s.ended || b.t <= a.t) {
                return 0.0f;
            }
            return static_cast<float>(metres(a, b) / (b.t - a.t));
        }

        float getDistance() override { return 0.0f; }
        float getAltitude() override { return getLocation().alt; }

        void startStepCounter() override {}
        void stopStepCounter() override {}
        void setParamStepCounter(float) override {}
        uint32_t getStepCounter() override { return 0; }

        static double metres(const Point& a, const Point& b)
        {
            const double r = M_PI / 180.0;
            const double dLat = (b.lat - a.lat) * r;
            const double dLon = (b.lon - a.lon) * r;
            const double h = std::sin(dLat / 2) * std::sin(dLat / 2) +
                             std::cos(a.lat * r) * std::cos(b.lat * r) * std::sin(dLon / 2) * std::sin(dLon / 2);
            return 2.0 * 6371000.0 * std::asin(std::sqrt(h));
        }

        // ponytail: string scan, not an XML parser; handles Strava/Garmin/planner exports, add a parser if a real file breaks it
        static std::vector<Point> parse(const char* path)
        {
            std::ifstream in(path);
            std::stringstream ss;
            ss << in.rdbuf();
            const std::string x = ss.str();
            const char* tag = x.find("<trkpt") != std::string::npos ? "<trkpt" : "<rtept";

            std::vector<Point> pts;
            bool timed = true;
            double t0 = 0.0;
            for (size_t p = x.find(tag); p != std::string::npos; p = x.find(tag, p + 1)) {
                const size_t tagEnd = x.find('>', p);
                if (tagEnd == std::string::npos) {
                    break;
                }
                const size_t ptEnd = x[tagEnd - 1] == '/' ? tagEnd : x.find("pt>", tagEnd);
                const std::string head = x.substr(p, tagEnd - p);
                const std::string body = x.substr(tagEnd, ptEnd - tagEnd);

                Point pt{};
                pt.lat = std::strtof(attr(head, "lat").c_str(), nullptr);
                pt.lon = std::strtof(attr(head, "lon").c_str(), nullptr);
                pt.alt = std::strtof(element(body, "ele").c_str(), nullptr);

                const std::string iso = element(body, "time");
                std::tm tm{};
                if (timed && !iso.empty() &&
                    std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                                &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
                    tm.tm_year -= 1900;
                    tm.tm_mon -= 1;
                    // GPX times are UTC; timegm is POSIX, _mkgmtime is its Windows name
#ifdef _WIN32
                    const double t = static_cast<double>(_mkgmtime(&tm));
#else
                    const double t = static_cast<double>(timegm(&tm));
#endif
                    if (pts.empty()) {
                        t0 = t;
                    }
                    pt.t = t - t0;
                } else {
                    timed = false;
                }
                pts.push_back(pt);
            }

            if (!timed) {
                for (size_t i = 1; i < pts.size(); ++i) {
                    pts[i].t = pts[i - 1].t + metres(pts[i - 1], pts[i]) / (25.0 / 3.6);
                }
            }
            return pts;
        }

    private:
        struct Seg { size_t i; float f; bool ended; };

        Seg segment() const
        {
            double t = 0.0;
            if (mStarted) {
                t = std::chrono::duration<double>(std::chrono::steady_clock::now() - mStart).count() * mSpeedUp;
            }
            if (t >= mPoints.back().t) {
                return { mPoints.size() - 2, 1.0f, true };
            }
            size_t i = 0;
            while (mPoints[i + 1].t <= t) {
                ++i;
            }
            const double span = mPoints[i + 1].t - mPoints[i].t;
            return { i, static_cast<float>((t - mPoints[i].t) / span), false };
        }

        static std::string attr(const std::string& s, const char* name)
        {
            const size_t a = s.find(std::string(" ") + name + "=");
            if (a == std::string::npos) {
                return "";
            }
            const size_t q = a + std::strlen(name) + 2;
            return s.substr(q + 1, s.find(s[q], q + 1) - q - 1);
        }

        static std::string element(const std::string& s, const char* name)
        {
            const std::string open = std::string("<") + name + ">";
            const size_t a = s.find(open);
            if (a == std::string::npos) {
                return "";
            }
            const size_t b = a + open.size();
            return s.substr(b, s.find('<', b) - b);
        }

        const std::vector<Point> mPoints;
        const float mSpeedUp;
        std::once_flag mStartOnce;
        std::chrono::steady_clock::time_point mStart;
        std::atomic<bool> mStarted{ false };
        std::atomic<bool> mEnabled{ false };
    };
}

#endif
