#include <doctest.h>

#include "ui/VersionCheck.h"

TEST_CASE("VersionCheck: only plain release tags count")
{
    CHECK(VersionCheck::plainVersion("v0.21.0") == "0.21.0");
    CHECK(VersionCheck::plainVersion("1.2.3") == "1.2.3");
    CHECK(VersionCheck::plainVersion("v0.22.0-rc1").isEmpty());
    CHECK(VersionCheck::plainVersion("v0.22").isEmpty());
    CHECK(VersionCheck::plainVersion("nightly").isEmpty());
    CHECK(VersionCheck::plainVersion("v..1").isEmpty());
}

TEST_CASE("VersionCheck: numbers compare as numbers")
{
    CHECK(VersionCheck::isNewer("0.22.0", "0.21.0"));
    CHECK(VersionCheck::isNewer("0.21.1", "0.21.0"));
    CHECK(VersionCheck::isNewer("1.0.0", "0.99.9"));
    CHECK(VersionCheck::isNewer("0.10.0", "0.9.0"));
    CHECK_FALSE(VersionCheck::isNewer("0.21.0", "0.21.0"));
    CHECK_FALSE(VersionCheck::isNewer("0.20.5", "0.21.0"));
    CHECK_FALSE(VersionCheck::isNewer("garbage", "0.21.0"));
}

TEST_CASE("VersionCheck: the highest release in GitHub's tag list")
{
    // GitHub lists tags by name, not by version: 0.9 can come before 0.10.
    const auto json = R"([{"name":"v0.9.0"},{"name":"v0.10.0"},{"name":"v0.11.0-rc1"},{"name":"test"}])";
    CHECK(VersionCheck::latestRelease(json) == "0.10.0");
    CHECK(VersionCheck::latestRelease("[]").isEmpty());
    CHECK(VersionCheck::latestRelease(R"({"message":"API rate limit exceeded"})").isEmpty());
    CHECK(VersionCheck::latestRelease("not json").isEmpty());
}
