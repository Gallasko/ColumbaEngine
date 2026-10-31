#include "stdafx.h"

#include <gtest/gtest.h>

#include "Net/portmapper.h"

using arena::net::PortMapper;

// The outside ports the router is asked for, in order: some routers only open some (a Freebox: above 45000)

TEST(arena_portmapper, a_list_of_ports_and_ranges_reads_in_order_a_range_by_twos)
{
    const auto ports = PortMapper::parsePorts("7777,45000-45006,50000");
    const std::vector<uint16_t> expected = {7777, 45000, 45002, 45004, 45006, 50000};
    EXPECT_EQ(ports, expected);
}

TEST(arena_portmapper, nonsense_and_repeats_are_left_out)
{
    EXPECT_TRUE(PortMapper::parsePorts("").empty());
    EXPECT_TRUE(PortMapper::parsePorts("abc,0,70000").empty());

    const auto ports = PortMapper::parsePorts("7777,7777,45000-45002,45000");
    const std::vector<uint16_t> expected = {7777, 45000, 45002};
    EXPECT_EQ(ports, expected);
}

TEST(arena_portmapper, the_default_range_is_above_45000)
{
    const auto ports = PortMapper::parsePorts(PortMapper::DEFAULT_RANGE);
    ASSERT_FALSE(ports.empty());
    EXPECT_GE(ports.front(), 45000);
    EXPECT_GE(ports.size(), 50u);
}
